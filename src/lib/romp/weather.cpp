/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"

#include "weather.hpp"

#include "cstdmf/stdmf.hpp"
#include "cstdmf/watcher.hpp"
#include "math/vector2.hpp"

#include <string>


// ----------------------------------------------------------------------------
// Section: WeatherSettings
// ----------------------------------------------------------------------------

/**
 *	Initialise these settings. This is nothing to do with 'clear weather'.
 */
void WeatherSettings::clear()
{
	for (float * pF = (float*)this; pF < (float*)(this+1); pF++)
	{
		*pF = 0.f;
	}
}


/**
 *	Accumulate 'bit' into our settings, weighting ourselves at 'ownProp'
 *	and bit's at 'bitProp'.
 */
void WeatherSettings::acc( float ownProp,
	const WeatherSettings & bit, float bitProp )
{
	float sumProp = ownProp + bitProp;
	if (sumProp <= 0.f) return;

	float * pO = (float*)this;
	float * pB = (float*)&bit;
	for (int i = 0; i < sizeof(*this)/sizeof(float); i++)
	{
		*pO = ( (*pO) * ownProp + (*pB) * bitProp ) / sumProp;
		pO++;
		pB++;
	}
}


// ----------------------------------------------------------------------------
// Section: WeatherSystem Base Class
// ----------------------------------------------------------------------------

PY_TYPEOBJECT( WeatherSystem );

PY_BEGIN_METHODS( WeatherSystem )
	/*~ function WeatherSystem.direct
	 *
	 *	This function changes the configuration of this weather system.
	 *	It specifies (a) The weighting of this weather system relative to
	 *	the other three systems, (b) the parameters to this system, and (c)
	 *	The time to move the weighting and parameters from their present 
	 *	values to the newly specified values.
	 *
	 *	The parameters for the system are a Vector4, with the interpretation
	 *	of each component being dependent on the type of the weather system.
	 *
	 *	The "CLEAR" WeatherSystem ignores all the parameters.
	 *
	 *	The "CLOUD" WeatherSystem interprets the x component as the cloud
	 *	coverage, where 0 means a clear sky, and 1 means fully cloudy. It
	 *	interprets the y component as cloud cohesion, with 0 meaning there
	 *	is no tendency for clouds to be grouped, and higher numbers increase
	 *	the chance of clouds overlapping.
	 *	
	 *	The "RAIN" WeatherSystem interprets the x component as the darkness of
	 *	the clouds, where 0 is the brightest, and 1 is the darkest.  It 
	 *	interprets the y component as cloud cohesion, with 0 meaning there
	 *	is no tendency for clouds to be grouped, and higher numbers increase
	 *	the chance of clouds overlapping.
	 *	
	 *	The "STORM" WeatherSystem ignores all the parameters.
	 *
	 *	The z and w components of the parameters are for future expansion.
	 *
	 *	An example of starting up rain would be:
	 *	@{
	 *	w = BigWorld.weather()
	 *	ws = w.system( "RAIN" )
	 *	ws.direct( 2, (1, 10, 0, 0), 1)
	 *	@}
	 *	This weights the "RAIN" system at 2, compared to the others which default to
	 *	zero, specifies a darkness of 1, and a cohesion of 10.  It will take 1 second
	 *	for these parameters to kick in.
	 *
	 *	@param	weight	a Float. The relative weight to apply to this WeatherSystem
	 *			relative to the other WeatherSystems.  This is 0 by default.
	 *	@param	params	a Vector4.  This is the parameters to the WeatherSystem
	 *			which are interpreted in a system dependent way, as discussed above.
	 *	@param	afterTime	a float.  The time to change from the current settings
	 *			to the new settings, measured in seconds.
	 */
	PY_METHOD( direct )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( WeatherSystem )
PY_END_ATTRIBUTES()

WeatherSystem::WeatherSystem( const std::string & name ) :
	PyObjectPlus( &WeatherSystem::s_type_ ),
	name_( name ),
	propensity_( 0.f ),
	targetTime_( 0.f )
{
	args_[0] = 0.8f;
	args_[1] = 0.8f;
	args_[2] = 0.f;
	args_[3] = 0.f;

	settings_.clear();
}


void WeatherSystem::direct( float propensity, float args[4], float afterTime )
{
	targetProp_ = propensity;
	targetArgs_[0] = args[0];
	targetArgs_[1] = args[1];
	targetArgs_[2] = args[2];
	targetArgs_[3] = args[3];
	targetTime_ = afterTime;
}


PyObject * WeatherSystem::py_direct( PyObject * pyargs )
{
	float propensity, args[4], afterTime;

	if(!PyArg_ParseTuple(pyargs, "f(ffff)f", &propensity, &args[0],
		&args[1], &args[2], &args[3], &afterTime))
	{
		// PyArg_ParseTuple sets exception.
		return NULL;
	}

	this->direct(propensity, args, afterTime);
	Py_Return;
}


void WeatherSystem::tick( float dTime )
{
	if (targetTime_ > 0.f)
	{
		float portion = min( dTime, targetTime_ ) / targetTime_;

		propensity_ += (targetProp_ - propensity_) * portion;
		args_[0] += (targetArgs_[0] - args_[0]) * portion;
		args_[1] += (targetArgs_[1] - args_[1]) * portion;
		args_[2] += (targetArgs_[2] - args_[2]) * portion;
		args_[3] += (targetArgs_[3] - args_[3]) * portion;

		targetTime_ = max( targetTime_ - dTime, 0.f );
	}
};


#if ENABLE_WATCHERS
Watcher & WeatherSystem::watcher()
{
	static DirectoryWatcherPtr pWatcher = NULL;
	if (pWatcher == NULL)
	{
		pWatcher = new DirectoryWatcher;

		WeatherSystem * pNULL = NULL;

		// TODO: BC: boundschecker will report bad pointer read 
		// here. DataWatcher should be using a more elegant and 
		// obvious way to get the offset to a member of a structure, 
		// as opposed to dereferencing a NULL pointer.
		pWatcher->addChild( "name", new DataWatcher<std::string>(
			pNULL->name_, Watcher::WT_READ_ONLY ) );

		pWatcher->addChild( "propensity", new DataWatcher<float>(
			pNULL->propensity_, Watcher::WT_READ_WRITE ) );
		pWatcher->addChild( "arg0", new DataWatcher<float>(
			pNULL->args_[0], Watcher::WT_READ_WRITE ) );
		pWatcher->addChild( "arg1", new DataWatcher<float>(
			pNULL->args_[1], Watcher::WT_READ_WRITE ) );
	}
	return *pWatcher;
}
#endif


/**
 *	This method allows scripts to get various properties.
 */
PyObject * WeatherSystem::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();
	return PyObjectPlus::pyGetAttribute( attr );
}

/**
 *	This method allows scripts to set various properties.
 */
int WeatherSystem::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();
	return PyObjectPlus::pySetAttribute( attr, value );
}


// ----------------------------------------------------------------------------
// Section: WeatherSystems
// ----------------------------------------------------------------------------



/**
 * TODO: to be documented.
 */
class ClearWeather: public WeatherSystem
{
public:
	ClearWeather() : WeatherSystem( "CLEAR" ) {}

	void apply()
	{
		settings_.colourMin = 0.95f;
		settings_.colourMax = 1.0f;
		settings_.cover = 0.f;
		settings_.cohesion = 0.5f;
		settings_.conflict = 0.f;
	}
};


/**
 * TODO: to be documented.
 */
class CloudWeather: public WeatherSystem
{
public:
	CloudWeather() : WeatherSystem( "CLOUD" ) {}

	void apply()
	{
		settings_.colourMin = 0.90f;	// should dawdle between
		settings_.colourMax = 1.0f;		// 0.5 and 1.0
		settings_.cover = args_[0];
		settings_.cohesion = args_[1];
		settings_.conflict = 0.f;
	}
};


/**
 * TODO: to be documented.
 */
class RainWeather: public WeatherSystem
{
public:
	RainWeather() : WeatherSystem( "RAIN" ) {}

	void apply()
	{
		settings_.colourMin = 0.4f - (args_[0] * 0.4f);
		settings_.colourMax = 0.5f - (args_[0] * 0.5f);
		settings_.cover = 1.f;
		settings_.cohesion = args_[1];
		settings_.conflict = 0.f;
	}
};


/**
 * TODO: to be documented.
 */
class StormWeather: public WeatherSystem
{
public:
	StormWeather() : WeatherSystem( "STORM" ) {}

	void apply()
	{
		settings_.colourMin = 0.0f;
		settings_.colourMax = 0.1f;
		settings_.cover = 1.f;
		settings_.cohesion = 1.f;
		settings_.conflict = 1.f;
	}
};





// ----------------------------------------------------------------------------
// Section: Weather
// ----------------------------------------------------------------------------

PY_TYPEOBJECT( Weather );

PY_BEGIN_METHODS( Weather )
	/*~ function Weather.system
	 *
	 *	This function returns the named WeatherSystem.  The valid names are 
	 *	"CLEAR", "CLOUD", "RAIN" and "STORM".  If an invalid name is specified
	 *	then a Python error occurs.  The returned weather system can be used
	 *	to configure that particular type of weather, and to change the 
	 *	prevailing weather system to that type of weather.
	 *
	 *	@param	name	a String. The name of the WeatherSystem to return.
	 *
	 *	@return			the named WeatherSystem.
	 */
	PY_METHOD( system )
	/*~ function Weather.windAverage
	 *
	 *	This function sets the average velocity of the wind, specified as
	 *	x and z coordinates.  The wind on average blows at this velocity
	 *	but deviates around it by a random amount which is capped at 
	 *	windGustiness.
	 *
	 *	The wind determines the direction and speed that clouds move across
	 *	the sky.  This is important, because when weather systems are changed
	 *	the changes appear 1000m upwind of the user, and are blown across the
	 *	sky at a rate specified by the wind velocity.
	 *
	 *	If the wind is changed, then existing clouds continue with their existing
	 *	velocities, and new clouds are produced with the new velocity, so that the
	 *	weather gradually switches to blow from a different direction.
	 *
	 *	The wind velocity is also used to perturb the detail objects, allowing
	 *	for swaying grass and the like.
	 *
	 *	@param	xWind	a float.  The x component of the winds average velocity.
	 *	@param	zWind	a float.  The z component of the winds average velocity.
	 *	
	 */
	PY_METHOD( windAverage )
	/*~ function Weather.windGustiness
	 *
	 *	This function specifies the gustiness of the wind.  This is the
	 *	largest magnitude by which the actual wind speed can vary from that
	 *	specified by windAverage, as it varies randomly.
	 *
	 *	The wind determines the direction and speed that clouds move across
	 *	the sky.  This is important, because when weather systems are changed
	 *	the changes appear 1000m upwind of the user, and are blown across the
	 *	sky at a rate specified by the wind velocity.
	 *
	 *	The wind velocity is also used to perturb the detail objects, allowing
	 *	for swaying grass and the like.
	 *
	 *	@param gust	a float.  The cap on the random speed of the wind.
	 */
	PY_METHOD( windGustiness )
	/*~ function Weather.temperature
	 *
	 *	This function starts the temperature changing to the specified
	 *	temperature.  It takes the specified number of seconds to change
	 *	from its temperature at the time of the call to the new temperature.
	 *
	 *	If the WeatherSystems decide that there will be precipitation,
	 *	based on the parameters they are given using the WeatherSystem.direct
	 *	function, then the temperature determines what kind of
	 *	precipitation it will be.  If the temperature is below zero, it will
	 *	snow.  If it is between zero and five degrees celsius, it will hail,
	 *	otherwise it will rain.
	 *
	 *	@param	degrees		a Float.  The temperature to move towards. 
	 *						Measured in degrees Celsius.
	 *	@param	afterTime	a Float.  The number of seconds to take to 
	 *						move from the current temperature to the 
	 *						specified temperature.
	 */
	PY_METHOD( temperature )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( Weather )
PY_END_ATTRIBUTES()

/// Constructor
Weather::Weather() :
	PyObjectPlus( &Weather::s_type_ )
{
	settings_.clear();

	// add the weather systems we want to have
	systems_.push_back( new ClearWeather() );
	systems_.push_back( new CloudWeather() );
	systems_.push_back( new RainWeather() );
	systems_.push_back( new StormWeather() );

	// give the first one some chance
	float args[4] = {0,0,0,0};
	systems_[0]->direct( 1.f, args, 0.1f );

#if ENABLE_WATCHERS
	// add watcher
	DirectoryWatcher * pWW = new DirectoryWatcher;
	for (uint i=0; i < systems_.size(); i++)
	{
		pWW->addChild( systems_[i]->name().c_str(),
			&WeatherSystem::watcher(), systems_[i] );
	}
	Watcher::rootWatcher().removeChild( "Client Settings/Weather" );
	Watcher::rootWatcher().addChild( "Client Settings/Weather", pWW );
#endif

	windVelX_ = 0.f;
	windVelY_ = 0.f;
	windGustiness_ = 0.f;

	temperatureTarget_ = 25.f;
	temperatureTime_ = 0.f;
	settings_.temperature = temperatureTarget_;

	MF_WATCH( "Client Settings/Weather/windVelX",
		windVelX_,
		Watcher::WT_READ_WRITE,
		"Wind velocity on the X axis" );

	MF_WATCH( "Client Settings/Weather/windVelY",
		windVelY_,
		Watcher::WT_READ_WRITE,
		"Wind velocity on the Z axis" );

	MF_WATCH( "Client Settings/Weather/windGustiness",
		windGustiness_,
		Watcher::WT_READ_WRITE,
		"Wind gustiness" );

	MF_WATCH( "Client Settings/Weather/temperature",
		temperatureTarget_,
		Watcher::WT_READ_WRITE,
		"Desired air temperature" );

	MF_WATCH( "Client Settings/Weather/out: colourMin", settings_.colourMin,
		Watcher::WT_READ_ONLY, "Current minimum colour weather value." );

	MF_WATCH( "Client Settings/Weather/out: colourMax", settings_.colourMax,
		Watcher::WT_READ_ONLY, "Current maximum colour weather value." );

	MF_WATCH( "Client Settings/Weather/out: cover", settings_.cover,
		Watcher::WT_READ_ONLY, "Current cloud cover" );

	MF_WATCH( "Client Settings/Weather/out: cohesion", settings_.cohesion,
		Watcher::WT_READ_ONLY, "Current cloud cohesion" );

	MF_WATCH( "Client Settings/Weather/out: conflict", settings_.conflict,
		Watcher::WT_READ_ONLY, "Current Lightning conflict value. "
		"When conflict is full on, there is a 70% chance of "
		"lighting/thunder every second." );

	MF_WATCH( "Client Settings/Weather/out: windX", settings_.windX,
		Watcher::WT_READ_ONLY, "Current wind velocity on the X axis" );

	MF_WATCH( "Client Settings/Weather/out: windZ", settings_.windZ,
		Watcher::WT_READ_ONLY, "Current wind velocity on the Z axis" );

	MF_WATCH( "Client Settings/Weather/out: temp", settings_.temperature,
		Watcher::WT_READ_ONLY, "Current air temperature" );
}


/// Destructor
Weather::~Weather()
{
	for (uint i=0; i < systems_.size(); i++)
	{
		Py_DECREF(systems_[i]);
	}
	systems_.clear();
}


/**
 *	This method calculates the new weather for this frame.
 */
void Weather::tick( float dTime )
{
	float temperatureLast = settings_.temperature;
	Vector2 windLast( settings_.windX, settings_.windZ );

	// call all our systems' ticks
	for (uint i=0; i < systems_.size(); i++)
	{
		systems_[i]->tick( dTime );
	}

	// now apply each in turn and add it proportionally to our settings
	float	totProp = 0.f;
	settings_.clear();	// not strictly necessary
	for (uint i=0; i < systems_.size(); i++)
	{
		float bitProp = systems_[i]->propensity();

		systems_[i]->apply();

		settings_.acc( totProp, systems_[i]->output(), bitProp );
		totProp += bitProp;
	}

	// overwrite the wind with our calculation
	Vector2 windWant = Vector2( windVelX_, windVelY_ ) + Vector2(
		2.f*rand()*windGustiness_/RAND_MAX - windGustiness_,
		2.f*rand()*windGustiness_/RAND_MAX - windGustiness_ );
	Vector2 windNow = windLast + (windWant - windLast) * (1.f * dTime);
		// wind acceleration is here hardcoded to 1m/s/s
	settings_.windX = windNow[0];
	settings_.windZ = windNow[1];

	// set the temperature
	if (temperatureTime_ > 0.f)
	{
		float portion = min( dTime, temperatureTime_ ) / temperatureTime_;
		temperatureLast += (temperatureTarget_ - temperatureLast) * portion;
		temperatureTime_ = max( temperatureTime_ - dTime, 0.f );
	}
	else if (temperatureTarget_ != temperatureLast)
	{
		temperatureLast = temperatureTarget_;
	}
	settings_.temperature = temperatureLast;

	// and that's all there is to it.
}


/**
 *	Sets a new temperature target to be reached after 'afterTime' seconds
 */
void Weather::temperature( float degrees, float afterTime )
{
	temperatureTarget_ = degrees;
	temperatureTime_ = afterTime;
}


/**
 *	Sets a new temperature target to be reached after 'afterTime' seconds
 */
PyObject * Weather::py_temperature( PyObject * args )
{
	float degrees, afterTime;

	if(!PyArg_ParseTuple( args, "ff", &degrees, &afterTime))
	{
		// PyArg_ParseTuple sets the exception.
		return NULL;
	}

	this->temperature(degrees, afterTime);
	Py_Return;
}

/**
 *	This method allows scripts to get various properties.
 */
PyObject * Weather::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();
	return PyObjectPlus::pyGetAttribute( attr );
}

/**
 *	This method allows scripts to set various properties.
 */
int Weather::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();
	return PyObjectPlus::pySetAttribute( attr, value );
}

/**
 *	Returns the named system, or NULL on failure
 */
WeatherSystem * Weather::system( const std::string & name )
{
	for (uint i = 0; i < systems_.size(); i++)
	{
		if (systems_[i]->name() == name) return systems_[i];
	}
	return NULL;
}

/**
 *	Returns the named system, or throws a KeyError exception
 *	if it does not exist.
 */
PyObject * Weather::py_system( PyObject * args )
{
	char* name;

	if(!PyArg_ParseTuple( args, "s", &name))
	{
		// PyArg_ParseTuple sets the exception.
		return NULL;
	}

	WeatherSystem* pSystem = this->system(name);

	if(!pSystem)
	{
		PyErr_SetString( PyExc_KeyError, "Unknown weather system");
		return NULL;
	}

	Py_INCREF(pSystem);
	return pSystem;
}

/**
 *	Sets new wind velocity.
 */	
PyObject * Weather::py_windAverage( PyObject* args )
{
	float xv, yv;

	if(!PyArg_ParseTuple( args, "ff", &xv, &yv ))
	{
		// PyArg_ParseTuple sets the exception.
		return NULL;
	}

	this->windAverage(xv, yv);
	Py_Return;
}

/**
 *	Sets new wind gustiness.
 */	
PyObject * Weather::py_windGustiness( PyObject* args )
{
	float amount;

	if(!PyArg_ParseTuple( args, "f", &amount ))
	{
		// PyArg_ParseTuple sets the exception.
		return NULL;
	}

	this->windGustiness(amount);
	Py_Return;
}

// weather.cpp
