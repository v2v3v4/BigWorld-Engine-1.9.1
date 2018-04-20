/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WEATHER_HPP
#define WEATHER_HPP


#include <vector>
#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"


/**
 *	This struct contains the settings for underlying variables used by
 *	the modules that actually implement the weather.
 *
 *	The methods defined on it assume it is composed entirely of floats.
 */
struct WeatherSettings
{
	float	colourMin;
	float	colourMax;
	float	cover;
	float	cohesion;
	float	conflict;

	float	windX;
	float	windZ;

	float	temperature;

	void clear();
	void acc( float ownProp, const WeatherSettings & bit, float bitProp );
};


/*~ class BigWorld.WeatherSystem
 *
 *	This class is used to configure a particular type of weather, and to
 *	weight between the different types.  There are exactly four 
 *	WeatherSystems, all accessable from the global Weather
 *	object using the Weather.system function.  They are accessed by name, and 
 *	the four are called: "CLEAR", "CLOUD", "RAIN" and "STORM".  These systems
 *	are created automatically by the engine, and you cannot create any more.
 *	Each WeatherSystem is set up to play the named type of weather.  
 *	
 *	WeatherSystems are configured by calling the direct function.
 *	This specifies the relative weighting of that WeatherSystem, and the
 *	system specific parameters.  
 *
 *	Changes to the weather occur 1000m upwind from the camera, and blow in
 *	at the wind speed, which is configured from Weather.  It can take several
 *	minutes for changes to appear on the client after parameters are changed.
 */
/**
 *	This interface defines the base class for all types of weather system
 *
 *	Weather systems should use only their arguments (and any random
 *	influences) to form their output settings. Propensity is used only
 *	as a blending factor and has no scale (so prop of 1 means nothing)
 */
class WeatherSystem : public PyObjectPlus
{
	Py_Header( WeatherSystem, PyObjectPlus)
public:
	WeatherSystem( const std::string & name );
	virtual ~WeatherSystem() {}

	void direct( float propensity, float args[4], float afterTime );

	PY_METHOD_DECLARE(py_direct);

	virtual void tick( float dTime );
	virtual void apply() = 0;

	const std::string & name() const	{ return name_; }

	float propensity() const			{ return propensity_; }

	const WeatherSettings & output()	{ return settings_; }

#if ENABLE_WATCHERS
	static class Watcher & watcher();
#endif

	PyObject *			pyGetAttribute( const char * attr );
	int					pySetAttribute( const char * attr, PyObject * value );

protected:
	float			args_[4];

	WeatherSettings	settings_;

private:
	std::string		name_;

	float			propensity_;

	float			targetProp_;
	float			targetArgs_[4];
	float			targetTime_;
};


/*~ class BigWorld.Weather
 *
 *	This class is used to control the weather in the clients world.
 *
 *	It allows access to the WeatherSystem@@s, of which there are exactly four,
 *	which are precreated, and cannot be added to.  They are:
 *	"CLEAR", "CLOUD", "RAIN" and "STORM".  A WeatherSystem contains the
 *	configuration parameters to play a particular type of weather.
 *
 *	The influence of each of these
 *	systems can be weighted, and each system can be independently configured
 *	with the actual weather being the weighted combination of their 
 *	influences.
 *
 *	The weather object is also used to set the temperature, which controls
 *	what kind of precipitation (snow, hail or rain) falls, if the weather
 *	systems decide that precipitation should fall.  The conditions for this
 *	are complex, but basically comes down to enough weight being given to the
 *	"RAIN" and/or "STORM" weather systems.
 *
 *	It also controls the wind speed, which effects the rate at which clouds
 *	move across the sky, and the pertubations of detail objects such as grass.
 *	
 *	Wind speed also effects the rate at which changes in the weather systems blow in
 *	from 1000m from the user, where they originate.
 *
 *	It is worth reiterating that changes in weather happen slowly, and blow in
 *	with the wind.  It can take several minutes for weather changes to appear
 *	on the client.
 */
/**
 *	This class determines what the final WeatherSettings will be, from the
 *	competing influences of every WeatherSystem it owns.
 *
 *	Call tick on it every frame, then read the settings it has decided
 *	with the 'settings' accessor. The output is a 'momentary' output,
 *	i.e. no smoothing need be done on it.
 */
class Weather : public PyObjectPlus
{
	Py_Header( Weather, PyObjectPlus )
public:
	Weather();
	~Weather();

	void tick( float dTime );
	const WeatherSettings & settings() const		{ return settings_; }

	WeatherSystem * system( const std::string & name );

	PyObject *			pyGetAttribute( const char * attr );
	int					pySetAttribute( const char * attr, PyObject * value );

	PY_METHOD_DECLARE(py_system);
	PY_METHOD_DECLARE(py_windAverage);
	PY_METHOD_DECLARE(py_windGustiness);
	PY_METHOD_DECLARE(py_temperature);

	void windAverage( float xv, float yv )	{ windVelX_ = xv; windVelY_ = yv; }
	void windGustiness( float amount )		{ windGustiness_ = amount; }

	void temperature( float degrees, float afterTime );

private:
	typedef std::vector<WeatherSystem*> Systems;
	Systems			systems_;

	WeatherSettings	settings_;

	float			windVelX_;
	float			windVelY_;
	float			windGustiness_;

	float			temperatureTarget_;
	float			temperatureTime_;
};


#endif // WEATHER_HPP
