/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pysound.hpp"

#if FMOD_SUPPORT
# include "pysoundparameter.hpp"
# include <fmod_errors.h>
#endif

DECLARE_DEBUG_COMPONENT2( "PySound", 0 );

/*~ class NoModule.PySound
 *
 *  A PySound is a handle to a sound event.  It can be used to play, stop,
 *  and adjust parameters on sounds.  It is basically an interface to
 *  FMOD::Event.  For more information about the concept of sound events and
 *  other API intricacies, please see the FMOD Ex API documentation, available
 *  from www.fmod.org.
 *
 *  The one major point of difference between a PySound and an FMOD::Event is
 *  that a PySound can be retriggered by calling play() without calling stop()
 *  in between.  What actually happens when you do this is determined by the
 *  "Max Playbacks" property of the particular underlying Event.  Method calls
 *  and/or attribute accesses (other than play()) on a PySound will always refer
 *  to the most recently triggered Event; if you want to retain handles to
 *  multiple instances of the same Event, you will need to create multiple
 *  PySound objects instead of triggering all the events from a single PySound
 *  object.
 */
PY_TYPEOBJECT( PySound )

PY_BEGIN_METHODS( PySound )

	/*~ function PySound.param
	 *
	 *  Returns the PySoundParameter object corresponding to the parameter of
	 *  the given name.
	 *
	 *  @param	name	The name of the parameter.
	 *
	 *  @return			A PySoundParameter object.
	 */
	PY_METHOD( param )

	/*~ function PySound.play
	 *
	 *  Start playing the sound event.
	 *
	 *  @return		A boolean indicating success.
	 */
	PY_METHOD( play )

	/*~ function PySound.stop
	 *
	 *  Stop the sound event.
	 *
	 *  @return		A boolean indicating success.
	 */
	PY_METHOD( stop )

PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( PySound )

	/*~ attribute PySound.volume
	 *
	 *  The volume that this sound is (or will be) playing at.
	 *
	 *  @type	Float in the range [0.0, 1.0]
	 */
	PY_ATTRIBUTE( volume )

	/*~ attribute PySound.duration
	 *
	 *  Returns the length of this sound event in seconds.  Will return < 0 on
	 *  error, which can be caused by asking for the duration of a looping
	 *  sound.
	 *
	 *  @type	Float
	 */
	PY_ATTRIBUTE( duration )

	/*~ attribute PySound.name
	 *
	 *  The name of the underlying FMOD::Event.
	 */
	PY_ATTRIBUTE( name )

	/*~ attribute PySound.state
	 *
	 *  Read-only access to the FMOD state of this event.
	 */
	PY_ATTRIBUTE( state )

	/*~ attribute PySound.position
	 *
	 *  Read-write access to the 3D position of this sound event.  Writing to
	 *  this attribute will only have an effect if the event's mode has been set
	 *  to 3D in the FMOD Designer tool.  Reading this attribute on a 2D sound
	 *  will return (0,0,0).
	 */
	PY_ATTRIBUTE( position )

PY_END_ATTRIBUTES()

PY_SCRIPT_CONVERTERS_DECLARE( PySound );
PY_SCRIPT_CONVERTERS( PySound );

PySound::PySound( SoundManager::Event *pEvent, const std::string &path,
	PyTypePlus* pType ) :
	PyObjectPlus( pType ),
	pEvent_( pEvent ),
	pModel_( NULL ),
	pPosition_( NULL ),
	played_( false ),
	reset_( false )
{
	BW_GUARD;
#if FMOD_SUPPORT
	MF_ASSERT( pEvent );

	FMOD_RESULT result;

	result = pEvent->setUserData( this );

	if ( result == FMOD_OK )
	{
		// If unloading is enabled, then we need to store the absolute path to the
		// underlying FMOD::Event* so we can reacquire handles when retriggered.
		if (SoundManager::instance().allowUnload())
		{
			if (!SoundManager::instance().absPath( path, path_ ))
			{
				ERROR_MSG( "PySound::PySound: "
					"Couldn't get absolute path to sound event\n" );
			}
		}

		// Otherwise, we just store a reference to the parent sound group and the
		// index of this event in that group, as it makes for faster handle
		// re-acquisition.
		else
		{
			result = pEvent_->getInfo( &index_, NULL, NULL );
			if (result != FMOD_OK)
			{
				ERROR_MSG( "PySound::PySound: "
					"Couldn't get event index: %s\n",
					FMOD_ErrorString( result ) );
			}

			result = pEvent_->getParentGroup( &pGroup_ );
			if (result != FMOD_OK)
			{
				ERROR_MSG( "PySound::PySound: "
					"Couldn't get parent group: %s\n",
					FMOD_ErrorString( result ) );
			}
		}
	}
	else
	{
		ERROR_MSG( "PySound::PySound(): Unable to setUserData: %s\n",
			FMOD_ErrorString( result ) );
	}

#endif
}

PySound::~PySound()
{
	BW_GUARD;

	if ( pEvent_ )
	{
		SoundManager::instance().release( pEvent_ );
#if FMOD_SUPPORT
		pEvent_->setUserData( NULL );
#endif
	}

	if (pPosition_)
	{
		delete pPosition_;
	}
}

#if FMOD_SUPPORT

/**
 *  Returns a PySoundParameter* for the provided parameter name.
 */
PyObject* PySound::param( const std::string& name )
{
	BW_GUARD;
	if (!this->refresh( PySoundParameter::REFRESH_MASK ))
		return SoundManager::error();

	FMOD::EventParameter *p;
	FMOD_RESULT result = pEvent_->getParameter( name.c_str(), &p );

	if (result == FMOD_OK)
	{
		return new PySoundParameter( p, this );
	}
	else
	{
		PyErr_Format( PyExc_ValueError,
			"FMOD::Event::getParameter() failed: %s",
			FMOD_ErrorString( result ) );

		return SoundManager::error();
	}
}


/**
 *  Play this sound.
 */
bool PySound::play()
{
	BW_GUARD;
	FMOD_RESULT result;

	if (!this->refresh())
		return false;

	result = pEvent_->start();

	if (result == FMOD_OK)
	{
		played_ = true;
	}
	else
	{
		ERROR_MSG( "PySound::play: %s\n", FMOD_ErrorString( result ) );
	}

	return result == FMOD_OK;
}


/**
 *  Stop this sound.
 */
bool PySound::stop()
{
	BW_GUARD;
	FMOD_RESULT result = pEvent_->stop();

	if (result != FMOD_OK)
	{
		ERROR_MSG( "PySound::stop: %s\n", FMOD_ErrorString( result ) );
	}

	return result == FMOD_OK;
}


/**
 *  Ensure that the Event* handle inside this PySound is ready to be played.
 *  The first time this is called, this is pretty much a no-op, so it is cheap
 *  for one-shot sounds.
 */
bool PySound::refresh( SoundManager::EventState okMask )
{
	BW_GUARD;
	FMOD_RESULT result;
	FMOD_EVENT_STATE state;

	result = pEvent_->getState( &state );

	// Cheap breakout if the event is already in an acceptable state.
	if ( !reset_ && (result == FMOD_OK) && (state & okMask) )
	{
		void * userData = NULL;
		// also, check that the userdata has not been reset by anyone
		result = pEvent_->getUserData( &userData );

		if ( ( result == FMOD_OK ) && ( userData == this ) )
		{
			// If the event has already been played we need to re-attach it, as the
			// sound's "on stop" callback will have removed it from the list of
			// attached sounds and without this its 3D attributes will not be set.
			return (played_ && pModel_) ? pModel_->attachSound( pEvent_ ) : true;
		}
	}


	// Get a new Event
	if (SoundManager::instance().allowUnload())
	{
		SoundManager::Event *pEvent = SoundManager::instance().get( path_ );
		if ((PyObject*)pEvent != SoundManager::error())
		{
			pEvent_ = pEvent;
		}
		else
		{
			ERROR_MSG( "PySound::refresh: "
				"Couldn't re-acquire Event handle\n" );
			return false;
		}
	}
	else
	{

		SoundManager::instance().release( pEvent_ );
		pEvent_ = SoundManager::instance().get( pGroup_, index_ );
		if ( !pEvent_ )
		{
			ERROR_MSG( "PySound::refresh: "
				"Couldn't re-acquire Event handle: %s\n",
				FMOD_ErrorString( result ) );
			return false;
		}
	}

	result = pEvent_->setUserData( this );
	if ( result == FMOD_OK )
	{
		reset_ = false;
	}
	else
	{
		ERROR_MSG( "PySound::PySound(): Unable to setUserData: %s\n",
			FMOD_ErrorString( result ) );
	}

	// Attach the new event to the model if there is one
	if (pModel_ && !pModel_->attachSound( pEvent_ ))
	{
		return false;
	}

	// Set the 3D position for the new event if necessary
	if (pPosition_ && !SoundManager::instance().set3D( pEvent_, *pPosition_ ))
	{
		return false;
	}

	return true;
}


/**
 * 
 */
void PySound::reset()
{
	reset_ = true;
}


/**
 *  Getter for event volume.
 */
float PySound::volume()
{
	BW_GUARD;
	float vol;
	FMOD_RESULT result = pEvent_->getVolume( &vol );

	if (result != FMOD_OK)
	{
		ERROR_MSG( "PySound::volume: %s\n", FMOD_ErrorString( result ) );
		return -1;
	}
	else
	{
		return vol;
	}
}


/**
 *  Setter for event volume.
 */
void PySound::volume( float vol )
{
	BW_GUARD;
	FMOD_RESULT result = pEvent_->setVolume( vol );

	if (result != FMOD_OK)
	{
		ERROR_MSG( "PySound::volume: %s\n", FMOD_ErrorString( result ) );
	}
}


/**
 *  Returns the length of this sound event in seconds.  Will return < 0 on error,
 *  which can be caused by asking for the duration of a looping sound.
 */
float PySound::duration()
{
	BW_GUARD;
	FMOD_EVENT_INFO eventInfo;
	memset( &eventInfo, 0, sizeof( eventInfo ) );
	FMOD_RESULT result = pEvent_->getInfo( NULL, NULL, &eventInfo );

	if (result != FMOD_OK)
	{
		ERROR_MSG( "PySound::duration: %s\n", FMOD_ErrorString( result ) );
		return -1;
	}

	return eventInfo.lengthms / 1000.0f;
}


/**
 *  Returns the name of the underlying FMOD::Event.  Uses memory managed by FMOD
 *  so don't hang onto this for long.
 */
const char *PySound::name() const
{
	BW_GUARD;
	static const char *err = "<error>";
	char *name;

	FMOD_RESULT result = pEvent_->getInfo( NULL, &name, NULL );
	if (result == FMOD_OK)
		return name;
	else
	{
		ERROR_MSG( "PySoundParameter::name: %s\n", FMOD_ErrorString( result ) );
		return err;
	}
}


/**
 *  Returns a string containing the names of all the FMOD_EVENT_STATE_* flags 
 *	that are set for this Event.
 */
std::string PySound::state()
{
	BW_GUARD;
	FMOD_EVENT_STATE state;
	pEvent_->getState( &state );
	std::string ret;

	if (state & FMOD_EVENT_STATE_READY) ret += "ready ";
	if (state & FMOD_EVENT_STATE_LOADING) ret += "loading ";
	if (state & FMOD_EVENT_STATE_ERROR) ret += "error ";
	if (state & FMOD_EVENT_STATE_PLAYING) ret += "playing ";
	if (state & FMOD_EVENT_STATE_CHANNELSACTIVE) ret += "channelsactive ";
	if (state & FMOD_EVENT_STATE_INFOONLY) ret += "infoonly ";

	if (!ret.empty())
		ret.erase( ret.size() - 1 );

	return ret;
}


/**
 *  This method returns the 3D position this sound is playing at.
 */
PyObject * PySound::pyGet_position()
{
	BW_GUARD;
	// Just return the position of our model if we're attached.
	if (pModel_)
	{
		return pModel_->pyGet_position();
	}

	if (pPosition_)
	{
		return Script::getData( *pPosition_ );
	}
	else
	{
		PyErr_Format( PyExc_AttributeError,
			"PySound '%s' has no 3D position set",
			SoundManager::name( pEvent_ ) );

		return NULL;
	}
}


/**
 *  This method sets the 3D position this sound is playing at.
 */
int PySound::pySet_position( PyObject * position )
{
	BW_GUARD;
	// Not allowed to set 3D attributes for attached sounds
	if (pModel_)
	{
		PyErr_Format( PyExc_AttributeError,
			"Can't set 3D position for PySound '%s' "
			"(it is already attached to %s)",
			SoundManager::name( pEvent_ ), pModel_->name().c_str() );

		return -1;
	}

	// Create position vector if necessary
	Vector3 * pPosition = pPosition_ ? pPosition_ : new Vector3();

	if (Script::setData( position, *pPosition, "position" ) == -1)
	{
		// Clean up position vector if necessary
		if (pPosition != pPosition_)
		{
			delete pPosition;
		}

		return -1;
	}

	if (SoundManager::instance().set3D( pEvent_, *pPosition ))
	{
		pPosition_ = pPosition;
		return 0;
	}
	else
	{
		PyErr_Format( PyExc_RuntimeError,
			"Failed to set 3D position for %s",
			SoundManager::name( pEvent_ ) );

		delete pPosition;
		pPosition_ = NULL;

		return -1;
	}
}


#else // FMOD_SUPPORT

PyObject* PySound::param( const std::string& name )
{
	PyErr_SetString( PyExc_NotImplementedError,
		"FMOD support disabled, all sound calls will fail" );
	return SoundManager::error();
}

bool PySound::play()
{
	return false;
}

bool PySound::stop()
{
	return false;
}

float PySound::volume()
{
	return -1;
}

void PySound::volume( float vol )
{
}

float PySound::duration()
{
	return -1;
}

const char *PySound::name() const
{
	static const char *err =
		"<FMOD support disabled, all sounds calls will fail>";

	return err;
}

std::string PySound::state()
{
	return "<FMOD support disabled, all sounds calls will fail>";
}

int PySound::pySet_position( PyObject * position )
{
	return -1;
}

PyObject * PySound::pyGet_position()
{
	return NULL;
}

#endif // FMOD_SUPPORT

PyObject* PySound::pyGetAttribute( const char* attr )
{
	BW_GUARD;
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}

int PySound::pySetAttribute( const char* attr, PyObject* value )
{
	BW_GUARD;
	PY_SETATTR_STD();

	return PyObjectPlus::pySetAttribute( attr, value );
}

/*~ function BigWorld.playSound
 *
 *  Plays a sound.
 *
 *  The semantics for sound naming are similar to filesystem paths, in that
 *  there is support for both 'relative' and 'absolute' paths.  If a path is
 *  passed without a leading '/', the sound is considered to be part of the
 *  default project (see BigWorld.setDefaultProject()), for example 'guns/fire'
 *  would refer to the 'guns/fire' event from the default project.  If your game
 *  uses only a single FMOD project, you should refer to events using relative
 *  paths, as it is (slightly) more efficient than the absolute method.
 *
 *  If your game uses multiple projects and you want to refer to a sample in a
 *  project other than the default, you can say '/project2/guns/fire' instead.
 *
 *  @param	path	The path to the sound event you want to trigger.
 *
 *  @return			The PySound for the sound event.
 */
static PyObject* py_playSound( PyObject * args )
{
	BW_GUARD;
	char* tag;

	if (!PyArg_ParseTuple( args, "s", &tag ))
		return NULL;

	SoundManager::Event *pEvent = SoundManager::instance().play( tag );
	if (pEvent != NULL)
		return new PySound( pEvent, tag );
	else
		return SoundManager::error();
}
PY_MODULE_FUNCTION( playSound, BigWorld );

// Alias provided for backwards compatibility
PY_MODULE_FUNCTION_ALIAS( playSound, playSimple, BigWorld );

// This is an alias to hook in BigWorld.playUISound() from
// bigworld/src/client/bwsound.cpp
PY_MODULE_FUNCTION_ALIAS( playSound, playUISound, BigWorld );


/*~ function BigWorld.getSound
 *
 *  Returns a sound handle.
 *
 *  @param	path	The path to the sound event you want to trigger (see
 *  				BigWorld.playSound() for syntax)
 *
 *  @return			The PySound for the sound event.
 */
static PyObject* py_getSound( PyObject * args )
{
	BW_GUARD;
	char* tag;

	if (!PyArg_ParseTuple( args, "s", &tag ))
		return NULL;

	SoundManager::Event *pEvent = SoundManager::instance().get( tag );
	if (pEvent != NULL)
		return new PySound( pEvent, tag );
	else
		return SoundManager::error();
}
PY_MODULE_FUNCTION( getSound, BigWorld )

// Alias provided for backwards compatibility
PY_MODULE_FUNCTION_ALIAS( getSound, getSimple, BigWorld );


/*~ function BigWorld.loadEventProject
 *
 *  Load the specified event project file.  Note that the argument to this 
 *	function is the name of the project, *not* its filename.  So basically you 
 *	are passing in the filename minus the .fev extension.
 *
 *  @param	filename	The name of the soundbank.
 */
static PyObject* py_loadEventProject( PyObject *args )
{
	BW_GUARD;
	char *filename;

	if (!PyArg_ParseTuple( args, "s", &filename ))
		return NULL;

	if (SoundManager::instance().loadEventProject( filename ))
		Py_RETURN_NONE;
	else
		return SoundManager::error();
}

PY_MODULE_FUNCTION( loadEventProject, BigWorld );
PY_MODULE_FUNCTION_ALIAS( loadEventProject, loadSoundBank, BigWorld );


/*~ function BigWorld.unloadEventProject
 *
 *  Unload the named event project.
 *
 *  @param	name	The name of the project
 */
static PyObject* py_unloadEventProject( PyObject *args )
{
	BW_GUARD;
	char *name;

	if (!PyArg_ParseTuple( args, "s", &name ))
		return NULL;

	if (SoundManager::instance().unloadEventProject( name ))
		Py_RETURN_NONE;
	else
		return SoundManager::error();
}

PY_MODULE_FUNCTION( unloadEventProject, BigWorld );
PY_MODULE_FUNCTION_ALIAS( unloadEventProject, unloadSoundBank, BigWorld );


/*~ function BigWorld.reloadEventProject
 *
 *  Reload the named event project.  Use this if you have created or deleted 
 *	events in a project using the FMOD Designer tool and you want to get the
 *  changes in your app.  If you are just tweaking properties on existing sound
 *  events you can audition them using the network audition mode of the FMOD
 *  Designer tool without needing to call this function.
 *
 *  @param	name	The name of the project
 */
static PyObject* py_reloadEventProject( PyObject *args )
{
	BW_GUARD;
	char *name;

	if (!PyArg_ParseTuple( args, "s", &name ))
		return NULL;

	if (!SoundManager::instance().unloadEventProject( name ))
		return SoundManager::error();

	if (!SoundManager::instance().loadEventProject( name ))
		return SoundManager::error();

	Py_RETURN_NONE;
}

PY_MODULE_FUNCTION( reloadEventProject, BigWorld );
PY_MODULE_FUNCTION_ALIAS( reloadEventProject, reloadSoundBank, BigWorld );


/*~ function BigWorld.loadSoundGroup
 *
 *  Loads the wave data for the specified event group (and all descendant
 *  groups) into memory, so that the sounds will play instantly when triggered.
 *
 *  If called with just a project name as the argument, all sound groups in that
 *  project will be loaded.
 *
 *  @param	path	The path to the event group
 */
static PyObject *py_loadSoundGroup( PyObject * args )
{
	BW_GUARD;
	char *group;

	if (!PyArg_ParseTuple( args, "s", &group ))
		return NULL;

	if (SoundManager::instance().loadWaveData( group ))
		Py_RETURN_NONE;
	else
		return SoundManager::error();
}

PY_MODULE_FUNCTION( loadSoundGroup, BigWorld );


/*~ function BigWorld.unloadSoundGroup
 *
 *  Unloads the wave data for the specified event group (and all descendant
 *  groups) from memory.  Note that sounds can still be played from this group
 *  later on, however the first playback for each event will block whilst the
 *  wave data is loaded from disk.
 *
 *  @param	path	The path to the event group
 */
static PyObject *py_unloadSoundGroup( PyObject * args )
{
	BW_GUARD;
	char *group;

	if (!PyArg_ParseTuple( args, "s", &group ))
		return NULL;

	if (SoundManager::instance().unloadWaveData( group ))
		Py_RETURN_NONE;
	else
		return SoundManager::error();
}

PY_MODULE_FUNCTION( unloadSoundGroup, BigWorld );


/*~ function BigWorld.setDefaultSoundProject
 *
 *  Sets the default sound project that relatively-named events will be read
 *  from.
 *
 *  See BigWorld.playSound() for more information about sound event naming
 *  conventions.
 *
 *  See the 'soundMgr' section of engine_config.xml for how to load sound
 *  projects into your game.
 *
 *  @param	name	The name of the project to be set as the default.
 */
static PyObject *py_setDefaultSoundProject( PyObject * args )
{
	BW_GUARD;
	char *project;

	if (!PyArg_ParseTuple( args, "s", &project ))
		return NULL;

	if (SoundManager::instance().setDefaultProject( project ))
	{
		Py_RETURN_NONE;
	}
	else
	{
		PyErr_Format( PyExc_RuntimeError,
			"setDefaultProject( '%s' ) failed, see debug output for details",
			project );

		return SoundManager::error();
	}
}

PY_MODULE_FUNCTION( setDefaultSoundProject, BigWorld );


/*~ function BigWorld.setMasterVolume
 *
 *  Sets the master volume applied to all sounds.
 *
 *  @param	vol		A float in the range [0.0, 1.0].
 */
static PyObject *py_setMasterVolume( PyObject *args )
{
	BW_GUARD;
	float vol;

	if (!PyArg_ParseTuple( args, "f", &vol ))
		return NULL;

	if (SoundManager::instance().setMasterVolume( vol ))
		Py_RETURN_NONE;
	else
		return SoundManager::error();
}

PY_MODULE_FUNCTION( setMasterVolume, BigWorld );


/**
 *  This method is only provided for backwards-compatibility with BW1.7 and
 *  earlier sound code.
 */
static PyObject* py_getFxSoundDuration( PyObject *args )
{
	BW_GUARD;
	PyObject *pHandle = py_getSound( args );
	if (pHandle == NULL || pHandle == SoundManager::error())
		return pHandle;

	// Now that we have a valid sound handle, just return its duration
	PyObject *ret = PyFloat_FromDouble( ((PySound*)pHandle)->duration() );
	Py_DECREF( pHandle );
	return ret;
}

PY_MODULE_FUNCTION( getFxSoundDuration, BigWorld );
PY_MODULE_FUNCTION_ALIAS( getFxSoundDuration, getSoundDuration, BigWorld );
