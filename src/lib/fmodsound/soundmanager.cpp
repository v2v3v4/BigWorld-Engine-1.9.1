/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "soundmanager.hpp"
#include "pysound.hpp"
#include "resmgr/bwresource.hpp"
#include "cstdmf/profiler.hpp"

DECLARE_DEBUG_COMPONENT2( "SoundManager", 0 );	// debugLevel for this file
PROFILER_DECLARE( SoundManager_update, "SoundManager_update" );

/// FMod SoundManager Singleton
BW_SINGLETON_STORAGE( SoundManager )

namespace
{
// TODO: Move this out of static initialisation if it causes problems
SoundManager s_soundManagerInstance;
}

#if FMOD_SUPPORT

#include "cstdmf/bgtask_manager.hpp"

class SoundBankLoader
{
public:
	SoundBankLoader( const char* name, const char* file );

	static void onLoadCompleted( void* loader );
	static void createSoundBank( void* loader );

protected:
	std::string name_;
	std::string file_;
	DataSectionPtr ref_;
};


SoundBankLoader::SoundBankLoader( const char* name, const char* file )
{
	BW_GUARD;
	name_ = name;
	file_ = file;

	BgTaskManager::instance().addBackgroundTask( new CStyleBackgroundTask( &SoundBankLoader::createSoundBank, this, &SoundBankLoader::onLoadCompleted, this ) );
}

void SoundBankLoader::onLoadCompleted( void* loader )
{
	BW_GUARD;
	SoundBankLoader* l = (SoundBankLoader*)loader;

	if( l->ref_.exists() )
		SoundManager::instance().registerSoundBank( l->name_, l->ref_ );
	else
		ERROR_MSG( "SoundBankLoader::onLoadCompleted: loading sound bank '%s' failed.\n", l->file_.c_str() );

	// clean up SoundBankLoader
	delete l;
}

void SoundBankLoader::createSoundBank( void* loader )
{
	BW_GUARD;
	SoundBankLoader* l = (SoundBankLoader*)loader;

	l->ref_ = BWResource::openSection( l->file_ );
}

SoundManager::SoundManager() :
	errorLevel_( SoundManager::WARNING ),
    lastSet_( false ),
	eventSystem_( NULL ),
	defaultProject_( NULL ),
	listening_( false ),
	allowUnload_( true )
{}

SoundManager::~SoundManager()
{
	BW_GUARD;
}


/**
 *  Initialises the sound manager and devices.
 */
bool SoundManager::initialise( DataSectionPtr config )
{
	BW_GUARD;
	// check what we're gonna do when play()/get() calls fail
	if (config != NULL)
	{
		std::string errorLevel = config->readString( "errorLevel", "warning" );
		if (errorLevel == "silent")
		{
			this->errorLevel( SoundManager::SILENT );
		}
		else if (errorLevel == "warning")
		{
			this->errorLevel( SoundManager::WARNING );
		}
		else if (errorLevel == "exception")
		{
			this->errorLevel( SoundManager::EXCEPTION );
		}
		else
		{
			ERROR_MSG( "SoundManager::initialise: "
				"Unrecognised value for soundMgr/errorLevel: %s\n",
				errorLevel.c_str() );

			this->errorLevel( SoundManager::WARNING );
		}
	}

	int channels = config->readInt( "channels", 64 );

	FMOD_RESULT result;

	result = FMOD::EventSystem_Create( &eventSystem_ );

	// Warning, enabling this will make FMOD crawl as it outputs MB's of data
	// to 'FMOD.TXT' in the CWD. Also, to take advantage of the logging, you MUST
	// link to fmod_event_netL.lib instead of fmod_event_net.lib (note the 'L')
	//result = FMOD::Debug_SetLevel( FMOD_DEBUG_LEVEL_ALL | FMOD_DEBUG_TYPE_FILE | FMOD_DEBUG_TYPE_EVENT );
	//result = FMOD::Debug_SetLevel( FMOD_DEBUG_ALL );

	// TODO: Follow the guidelines regarding the recommended startup 
	// sequence. We may want more specific speaker setup.

	if (result == FMOD_OK)
	{
		result = eventSystem_->init( channels, FMOD_INIT_NORMAL, NULL );

		if (result != FMOD_OK)
		{
			ERROR_MSG( "SoundManager::initialise: "
				"Couldn't initialise event system: %s\n",
				FMOD_ErrorString( result ) );

			eventSystem_ = NULL;
		}
	}
	else
	{
		ERROR_MSG( "SoundManager::initialise: "
			"Couldn't create event system: %s\n",
			FMOD_ErrorString( result ) );

		eventSystem_ = NULL;
	}

	if (eventSystem_ == NULL)
	{
		NOTICE_MSG( "SoundManager::initialise: "
			"Sound init has failed, suppressing all sound error messages\n" );

		errorLevel_ = SoundManager::SILENT;
		return false;
	}

	// Break out now if XML config wasn't passed in
	if (config == NULL)
		return true;

	this->setPath( config->readString( "mediaPath", "" ) );

	DataSectionPtr banks = config->openSection( "soundbanks" );

	if (banks)
	{
		std::list< std::string > preloaded;
		std::list< std::string > streamed;
		std::list< std::string >::iterator it;

		//First of all build up 2 seperate lists for preloaded and streamed projects
		for (int i=0; i < banks->countChildren(); ++i)
		{
			DataSectionPtr file = banks->openChild( i );
			if (file)
			{
				std::string name = file->readString( "name", file->asString() );
				projectFiles_.push_back( name );
				if (file->readBool( "preload", false ))
				{
					preloaded.push_back( name );
				}
				else
				{
					streamed.push_back( name );
				}
			}
		}

		//Now load all the preloaded projects
		it = preloaded.begin();
		while (it != preloaded.end())
		{
			if (this->loadEventProject( *it ))
			{
				INFO_MSG( "SoundManager::initialise: "
					"Loaded sound project %s\n",
					it->c_str() );
			}
			else
			{
				ERROR_MSG( "SoundManager::initialise: "
					"Failed to load sound project %s\n",
					it->c_str() );
			}
			it++;
		}

		//Re-use the preloaded list for the sound banks to preload
		preloaded.clear();
		getSoundBanks( preloaded );

		//Preload all the sound banks
		it = preloaded.begin();
		while (it != preloaded.end())
		{
			INFO_MSG( "SoundManager::initialise: "
				"Starting preload of sound bank %s\n",
				it->c_str() );
			DataSectionPtr data = NULL;
			registerSoundBank( *it, data );
			it++;
		}

		//Now load all the streamed projects
		it = streamed.begin();
		while (it != streamed.end())
		{
			if (this->loadEventProject( *it ))
			{
				INFO_MSG( "SoundManager::initialise: "
					"Loaded sound project %s\n",
					it->c_str() );
			}
			else
			{
				ERROR_MSG( "SoundManager::initialise: "
					"Failed to load sound project %s\n",
					it->c_str() );
			}
			it++;
		}
	}
	else
	{
		WARNING_MSG( "SoundManager::initialise: "
			"No <soundMgr/soundbanks> config section found, "
			"no sounds have been loaded\n" );
	}

	// Net event system stuff
	if (config->readBool( "networkUpdates", true ))
	{
		result = FMOD::NetEventSystem_Init( eventSystem_, 0 );

		if (result == FMOD_OK)
		{
			listening_ = true;
		}
		else
		{
			ERROR_MSG( "SoundManager::initialise: "
				"Couldn't initialise net layer: %s\n",
				FMOD_ErrorString( result ) );
		}
	}

	// Is unloading allowed?
	this->allowUnload( config->readBool( "allowUnload", this->allowUnload() ) );

	return true;
}

void SoundManager::fini()
{
	BW_GUARD;

	// clear out cached groups, they may no longer be defined, we can 
	// re-cache them later as needed. don't need to clear out individual
	// groups, since that's done by EventProject::release()
	eventGroups_.clear();

	for (EventProjects::iterator it = eventProjects_.begin();
		it != eventProjects_.end(); ++it)
	{
		FMOD_RESULT result = it->second->release();

		if (result != FMOD_OK)
		{
			ERROR_MSG( "SoundManager::fini(): "
				"Failed to release project '%s': %s\n",
				it->first.c_str(), FMOD_ErrorString( result ) );
		}
	}

	// free all sound banks
	SoundBankMap::iterator it = soundBankMap_.begin();
	SoundBankMap::iterator end = soundBankMap_.end();
	for ( ; it != end; ++it )
	{
		FMOD_RESULT result = eventSystem_->unregisterMemoryFSB( (it->first + ".fsb").c_str() );
		if (result != FMOD_OK)
		{
			ERROR_MSG( "SoundManager::fini(): unable to unregisterMemory: %s\n", FMOD_ErrorString( result ) );
		}
		it->second = NULL;	// release the binary data
	}
	soundBankMap_.clear();

	if ( eventSystem_ )
	{
		// removes all references and memory
		eventSystem_->unload();
		eventSystem_->release();
	}
	eventSystem_ = NULL;
}


/**
 *  Call the FMOD update() function which must be called once per main game
 *  loop.
 */
bool SoundManager::update()
{
	BW_GUARD;
	if (eventSystem_ == NULL)
		return false;

	bool ok = true;

	FMOD_RESULT result = eventSystem_->update();

	if (result != FMOD_OK)
	{
		ERROR_MSG( "SoundManager::update: %s\n", FMOD_ErrorString( result ) );
		ok = false;
	}

	if (listening_)
	{
		result = FMOD::NetEventSystem_Update();

		if (result != FMOD_OK)
		{
			ERROR_MSG( "SoundManager::update( net ): %s\n",
				FMOD_ErrorString( result ) );
			ok = false;
		}
	}

	PROFILER_BEGIN( SoundManager_update );
	
	for ( Events::iterator eit = events_.begin() ; eit != events_.end() ; )
	{
		Event * pEvent = (*eit).first;

		if ( pEvent == NULL )
		{
			eit = events_.erase( eit );
			continue;
		}

		FMOD_EVENT_STATE eventState;
		result = pEvent->getState( &eventState );

		if ( result == FMOD_ERR_INVALID_HANDLE )
		{
			// this handle is no longer valid, it should be removed from 
			// the map.
			eit = events_.erase( eit );
			continue;
		}

		if ( result != FMOD_OK  || ( eventState & FMOD_EVENT_STATE_ERROR ) )
		{
			ERROR_MSG( "Event::getState() failed or not ready: 0x%08x; %i; %s\n", pEvent, eventState, FMOD_ErrorString( result ) );
			eit = events_.erase( eit );
			continue;
		}

		void * ud = NULL;
		result = pEvent->getUserData( &ud );
		if ( result != FMOD_OK )
		{
			ERROR_MSG( "Event::getUserData() failed: 0x%08x; %s\n", pEvent, FMOD_ErrorString( result ) );
			eit = events_.erase( eit );
			continue;
		}

		++eit;
	}

	PROFILER_END();

	return ok;
}


/**
 *  Sets the path for the sound system to use when locating sounds.  This is
 *  just an interface to FMOD::EventSystem::setMediaPath().
 */
bool SoundManager::setPath( const std::string &path )
{
	BW_GUARD;
	if (eventSystem_ == NULL)
		return false;

	if (path.length() == 0)
	{
		ERROR_MSG( "SoundManager::setPath: "
			"Called with an empty path\n" );
		return false;
	}

	// Resolve the res-relative path into a real filesystem path.  Be aware that
	// using this mechanism means that none of this will work with zip/packed
	// filesystems.  A possible way to make this work would be to leverage
	// FMOD::EventSystem::registerMemoryFSB() which can be used to reference
	// samples from memory, but that doesn't really address the issue of large
	// stream files.
	// Add a trailing slash as per FMOD 4.11.02
	std::string realPath = BWResolver::resolveFilename( path ) + "\\";
	INFO_MSG( "Real path is %s\n", realPath.c_str() );

	FMOD_RESULT result;

	if ((result = eventSystem_->setMediaPath( realPath.c_str() )) != FMOD_OK)
	{
		ERROR_MSG( "SoundManager::setPath: "
			"Couldn't set media path to '%s': %s\n",
			realPath.c_str(), FMOD_ErrorString( result ) );
	}

	mediaPath_ = path;

	return result == FMOD_OK;
}


void SoundManager::allowUnload( bool b )
{
	allowUnload_ = b;
}


bool SoundManager::allowUnload() const
{
	return allowUnload_;
}


void SoundManager::registerSoundBank( const std::string &filename, DataSectionPtr data )
{
	BW_GUARD;
	if( data.exists() == false )
	{
		new SoundBankLoader( filename.c_str(), (mediaPath_ + "/" + filename + ".fsb").c_str() );
		return;
	}

	if( eventSystem_ != NULL )
	{
		if ( soundBankMap_.find(filename) != soundBankMap_.end() )
		{
			ERROR_MSG("Trying to load a soundbank that is already loaded '%s'\n", filename.c_str());
			return;	// already loaded
		}

		BinaryPtr pBinary = data->asBinary();
		// TODO: make sure this const_cast is safe
		FMOD_RESULT result =
			eventSystem_->registerMemoryFSB( (filename + ".fsb").c_str(), const_cast<void *>(pBinary->data()), pBinary->len() );
		if ( result == FMOD_OK )
		{
			INFO_MSG( "SoundManager::registerSoundBank: Sound bank '%s' registered successfully\n",
				filename.c_str(), FMOD_ErrorString( result ) );
		}
		else
		{
			ERROR_MSG( "SoundManager::registerSoundBank: Couldn't register sound bank '%s': %s\n",
				filename.c_str(), FMOD_ErrorString( result ) );
		}
		// keep reference to the sound bank
		soundBankMap_[filename] = pBinary;
	}
}

bool SoundManager::unregisterSoundBank( const std::string &filename )
{
	BW_GUARD;

	if (!this->allowUnload())
	{
		ERROR_MSG( "Unloading sound banks is disabled\n" );
		return false;
	}

	if( eventSystem_ == NULL )
	{
		return false;
	}

	SoundBankMap::iterator it = soundBankMap_.find(filename);

	if( it != soundBankMap_.end() )
	{
		// invalidate any PySound referencing any Event that needed this
		// sound bank.
		for ( Events::iterator eit = events_.begin() ; eit != events_.end() ; ++eit )
		{
			Event * pEvent = (*eit).first;
			if ( pEvent == NULL )
				continue;

			// take the reference, and modify the original in the map if needed.
			bool & active = (*eit).second;
			if ( !active )
				continue;

			FMOD_EVENT_STATE eventState;
			FMOD_RESULT result = pEvent->getState( &eventState );
			if ( result != FMOD_OK  || eventState & FMOD_EVENT_STATE_ERROR )
			{
				ERROR_MSG( "Event::getState() failed or not error: #%i; %i; %s\n", pEvent, eventState, FMOD_ErrorString( result ) );
				active = false;
				continue;
			}

			FMOD_EVENT_INFO eventInfo;
			memset( &eventInfo, 0, sizeof( eventInfo ) );
			result = pEvent->getInfo( NULL, NULL, &eventInfo );
			if ( result != FMOD_OK )
			{
				ERROR_MSG( "Event::getInfo() failed: #%i; %s\n", pEvent, FMOD_ErrorString( result ) );
				active = false;
				continue;
			}

			bool usesThisSoundBank = false;
			for( char ** sbName = eventInfo.wavebanknames ; *sbName ; ++sbName )
			{
				if ( filename == (*sbName) )
				{
					usesThisSoundBank = true;
					break;
				}
			}

			if ( !usesThisSoundBank )
				continue;

			void * ud = NULL;
			result = pEvent->getUserData( &ud );
			if ( result != FMOD_OK )
			{
				ERROR_MSG( "Event::getUserData() failed: #%i; %s\n", pEvent, FMOD_ErrorString( result ) );
				active = false;
				continue;
			}

			if ( ud == NULL )
			{
				ERROR_MSG( "Event::getUserData() was NULL: #%i\n", pEvent );
				active = false;
				continue;
			}

			PySound * pySound = static_cast< PySound * >( ud );

			pySound->reset();
		}

		FMOD_RESULT result =
			eventSystem_->unregisterMemoryFSB( (filename + ".fsb").c_str() );

		it->second = NULL;	// release the binary data
		soundBankMap_.erase(it);

		for ( EventGroups::iterator it = eventGroups_.begin() ; it != eventGroups_.end() ; ++it )
		{
			EventGroup* eventGroup = (*it).second;
			FMOD_RESULT result = eventGroup->freeEventData( NULL, true );
			if ( result != FMOD_OK )
			{
				ERROR_MSG( "SoundManager::unregisterSoundBank: Couldn't freeEventData for Group '%s': %s\n",
					((*it).first).second.c_str(), FMOD_ErrorString( result ) );
			}
		}
		// clear out cached groups, they may no longer be defined, we can 
		// re-cache them later as needed.
		eventGroups_.clear();

		if ( result != FMOD_OK )
		{
			ERROR_MSG( "SoundManager::unregisterSoundBank: Couldn't unregister sound bank '%s': %s\n",
				filename.c_str(), FMOD_ErrorString( result ) );
			return false;
		}


	}
	else
	{
		ERROR_MSG( "SoundManager::unregisterSoundBank: sound bank '%s' not in sound bank mapping.",
			filename.c_str() );
		return false;
	}

	return true;
}

/**
 *	Deprecated API!
 *	Please use loadEventProject instead. 
 */
bool SoundManager::loadSoundBank( const std::string &project )
{
	BW_GUARD;
	WARNING_MSG( "This method has been deprecated.\n"
		"Please use loadEventProject instead.\n" );

	return this->loadEventProject( project );
}


/**
 *	Deprecated API!
 *	Please use unloadEventProject instead.
 */
bool SoundManager::unloadSoundBank( const std::string &project )
{
	BW_GUARD;
	WARNING_MSG( "This method has been deprecated.\n"
		"Please use unloadEventProject instead.\n" );

	return this->unloadEventProject( project );
}


/**
 *  Loads an event project from an FMOD .fev project file.  Note that the string
 *	Returns a list of soundbanks that are in use by the event system.
 */
void SoundManager::getSoundBanks( std::list< std::string > & soundBanks )
{
	BW_GUARD;

	FMOD_EVENT_SYSTEMINFO sysInfo;
	ZeroMemory( &sysInfo, sizeof(sysInfo) );

	if (!eventSystem_) return;

	eventSystem_->getInfo( &sysInfo );

	for( int i = 0; i < sysInfo.numwavebanks; ++i )
		soundBanks.push_back( sysInfo.wavebankinfo[i].name );
}


/**
 * This method returns true if a sound bank matching the name has been loaded.
 */
bool SoundManager::hasSoundBank( const std::string & sbname ) const
{
	BW_GUARD;
	return soundBankMap_.find( sbname ) != soundBankMap_.end();
}


/**
 *  Returns a list of sound projects that are used.
 */
void SoundManager::getSoundProjects( std::list< std::string > & soundProjects )
{
	BW_GUARD;

	for( unsigned i = 0; i < projectFiles_.size(); ++i )
	{
		soundProjects.push_back( projectFiles_[i] );
	}
}

/**
 *	Returns a list of event groups that are use by the project.
 */
void SoundManager::getSoundGroups( const std::string& project, std::list< std::string > & soundGroups )
{
	BW_GUARD;
	
	if (project == "")
	{
		return;
	}

	FMOD::EventProject* pProject = NULL;
	if ((!this->parsePath( "/" + project, &pProject, NULL, NULL ))|| (pProject == NULL))
	{
		return;
	}
		
	int ignore = 0;
	char* name;

	int numGroups = 0;
	pProject->getNumGroups( &numGroups );
	for (int i=0; i<numGroups; i++)
	{
		FMOD::EventGroup* pEventGroup = NULL;
		pProject->getGroupByIndex( i, false, &pEventGroup);
		if (pEventGroup == NULL) continue;
		pEventGroup->getInfo( &ignore, &name);
		if (name)
			{
			std::string groupName( name );
			soundGroups.push_back( groupName );
		}
	}
}

/**
 *	Returns a list of events that are use by the project.
 */
void SoundManager::getSoundNames( const std::string& project, const std::string& group, std::list< std::string > & soundNames )
{
	BW_GUARD;
	
	if ((project == "") || (group == ""))
	{
		return;
	}
	
	FMOD::EventProject* pProject = NULL;
	FMOD::EventGroup* pGroup = NULL;
	if ((!this->parsePath( "/" + project + "/" + group, &pProject, &pGroup, NULL )) || (pProject == NULL) || (pGroup == NULL))
	{
		return;
	}
	
	int ignore = 0;
	char* name;

	int numEvents = 0;
	pGroup->getNumEvents( &numEvents );
	for (int i=0; i<numEvents; i++)
	{
		FMOD::Event *pEvent = NULL;
		pGroup->getEventByIndex( i, 0, &pEvent );
		if (pEvent == NULL) continue;
		pEvent->getInfo( &ignore, &name, NULL );
		if (name)
		{
			std::string soundName( name );
			soundNames.push_back( soundName );
		}
	}
}


/**
 *  Loads a sound bank from an FMOD .fev project file.  Note that the string
 *  that is passed in should be the prefix of the filename (i.e. everything but
 *  the .fev).
 */
bool SoundManager::loadEventProject( const std::string &project )
{
	BW_GUARD;
	// Prepend leading slash and drop extension to conform to standard syntax
	std::string path = "/";
	path += project;

	FMOD::EventProject *pProject;
	return this->parsePath( path, &pProject, NULL, NULL );
}


/**
 *  Unloads an event project.
 */
bool SoundManager::unloadEventProject( const std::string &project )
{
	BW_GUARD;
	if (!this->allowUnload())
	{
		PyErr_Format( PyExc_RuntimeError,
			"Unloading soundbanks is disabled" );
		return false;
	}

	// Prepend leading slash to conform to parsePath() syntax
	std::string path = "/";
	path += project;

	FMOD::EventProject *pProject;
	if (!this->parsePath( path, &pProject, NULL, NULL, false ))
	{
		PyErr_Format( PyExc_LookupError,
			"Soundbank '%s' is not currently loaded!", project.c_str() );
		return false;
	}

	FMOD_RESULT result = pProject->release();

	if (result != FMOD_OK)
	{
		PyErr_Format( PyExc_RuntimeError,
			"Couldn't release soundbank %s: %s",
			project.c_str(), FMOD_ErrorString( result ) );
		return false;
	}

	// Clear internal mappings related to this soundbank
	EventGroups::iterator git = eventGroups_.begin();
	while (git != eventGroups_.end())
	{
		if (git->first.first == pProject)
		{
			EventGroups::iterator del = git++;
			eventGroups_.erase( del );
		}
		else
			++git;
	}

	EventProjects::iterator pit = eventProjects_.begin();
	while (pit != eventProjects_.end())
	{
		if (pit->second == pProject)
		{
			EventProjects::iterator del = pit++;
			eventProjects_.erase( del );
		}
		else
			++pit;
	}

	if (defaultProject_ == pProject)
		defaultProject_ = NULL;

	return true;
}


/**
 *  Helper method for loadWaveData() and unloadWaveData().
 */
bool SoundManager::loadUnload( const std::string &group, bool load )
{
	BW_GUARD;
	FMOD::EventProject *pProject;
	FMOD::EventGroup *pGroup;
	FMOD_RESULT result;

	if (!this->parsePath( group, &pProject, &pGroup, NULL ))
	{
		PyErr_Format( PyExc_RuntimeError, "SoundManager::loadUnload: "
			"parsePath() failed for %s, see debug output for more info",
			group.c_str() );
		return false;
	}

	// Assemble a list of the sound groups we're working with
	std::vector< FMOD::EventGroup* > groups;
	if (pGroup)
	{
		groups.push_back( pGroup );
	}
	else
	{
		int nGroups;
		pProject->getNumGroups( &nGroups );

		for (int i=0; i < nGroups; i++)
		{
			result = pProject->getGroupByIndex( i, false, &pGroup );
			if (result != FMOD_OK)
			{
				PyErr_Format( PyExc_RuntimeError, "SoundManager::loadUnload: "
					"Couldn't get project group #%d: %s\n",
					i, FMOD_ErrorString( result ) );
				return false;
			}

			groups.push_back( pGroup );
		}
	}

	bool ok = true;

	// Iterate across groups and perform load/unload
	for (unsigned i=0; i < groups.size(); i++)
	{
		if (load)
		{
			result = groups[i]->loadEventData(
				FMOD_EVENT_RESOURCE_SAMPLES, FMOD_EVENT_DEFAULT );
		}
		else
		{
			result = groups[i]->freeEventData( NULL );
		}

		if (result != FMOD_OK)
		{
			PyErr_Format( PyExc_RuntimeError, "SoundManager::loadUnload: "
				"%sEventData() failed: %s",
				load ? "load" : "free", FMOD_ErrorString( result ) );
			ok = false;
		}
	}

	return ok;
}


/**
 *  Trigger a sound event, returning a handle to the event if successful, or
 *  NULL on failure.  For details on the semantics of event naming, please see
 *  the Python API documentation for BigWorld.playSound().
 */
SoundManager::Event* SoundManager::play( const std::string &name )
{
	BW_GUARD;
	FMOD_RESULT result;

	Event *pEvent = this->get( name );

	if (pEvent == NULL)
		return NULL;

	result = pEvent->start();
	if (result != FMOD_OK)
	{
		PyErr_Format( PyExc_RuntimeError, "SoundManager::play: "
			"Failed to play '%s': %s",
			name.c_str(), FMOD_ErrorString( result ) );

		return NULL;
	}

	return pEvent;
}

/**
 *  Trigger a sound event, returning a handle to the event if successful, or
 *  NULL on failure.  For details on the semantics of event naming, please see
 *  the Python API documentation for BigWorld.playSound().
 */
SoundManager::Event* SoundManager::play( const std::string &name, const Vector3 &pos )
{
	BW_GUARD;
	FMOD_RESULT result;

	Event *pEvent = this->get( name );

	if (pEvent == NULL)
		return NULL;

	result = pEvent->set3DAttributes( (FMOD_VECTOR*)&pos, NULL, NULL );

	if (result != FMOD_OK)
	{
		ERROR_MSG( "SoundManager::play: "
			"Failed to set 3D attributes for %s: %s\n",
			name.c_str(), FMOD_ErrorString( result ) );
	}

	result = pEvent->start();
	if (result != FMOD_OK)
	{
		PyErr_Format( PyExc_RuntimeError, "SoundManager::play: "
			"Failed to play '%s': %s",
			name.c_str(), FMOD_ErrorString( result ) );

		return NULL;
	}

	return pEvent;
}


/**
 *  Fetch a handle to a sound event.  For details on the semantics of event
 *  naming, please see the Python API documentation for BigWorld.playSound().
 */
SoundManager::Event* SoundManager::get( const std::string &name )
{
	BW_GUARD;
	FMOD::EventProject *pProject;
	FMOD::EventGroup *pGroup;
	FMOD::Event *pEvent;

	if (this->parsePath( name, &pProject, &pGroup, &pEvent ))
		return pEvent;
	else
		return NULL;
}


/**
 * This is a helper method to get an Event by index from an EventGroup.
 * It's here so SoundManager can track when new Event's are 'instanced'
 */
SoundManager::Event* SoundManager::get( FMOD::EventGroup * pGroup, int index )
{
	BW_GUARD;
	MF_ASSERT( pGroup );

	Event * pEvent = NULL;
	FMOD_RESULT result = pGroup->getEventByIndex( index, FMOD_EVENT_DEFAULT, &pEvent );

	if (result != FMOD_OK)
	{
		PyErr_Format( PyExc_LookupError, "SoundManager::get: "
			"Couldn't get event #i from group: %s",
			index, FMOD_ErrorString( result ) );
	}
	else
	{
		// insert it, or, if it exists, make it true anyway.
		events_[ pEvent ] = true;
	}

	return pEvent;
}


/**
 * This method removes the Event from the internal map, making sure that it's
 * not queried every frame.
 */
void SoundManager::release( SoundManager::Event * pEvent )
{
	BW_GUARD;
	Events::iterator eit = events_.find( pEvent );
	if ( eit == events_.end() )
		return;

	events_.erase( eit );
}


/**
 *  Set the 3D position of a sound event.
 */
bool SoundManager::set3D( Event *pEvent, const Vector3& pos, bool silent )
{
	BW_GUARD;
	FMOD_RESULT result;

	if (pEvent == NULL)
	{
		ERROR_MSG( "SoundManager::set3D: "
			"NULL event handle passed\n" );
		return false;
	}

	result = pEvent->set3DAttributes( (FMOD_VECTOR*)&pos, NULL, NULL );

	if (result != FMOD_OK && !silent)
	{
		ERROR_MSG( "SoundManager::set3D: "
			"Failed to set 3D attributes for %s: %s\n",
			SoundManager::name( pEvent ), FMOD_ErrorString( result ) );
	}

	return result == FMOD_OK;
}


/**
 *  Set the project that will be used to resolve relatively-named sound events.
 */
bool SoundManager::setDefaultProject( const std::string &name )
{
	BW_GUARD;
	FMOD::EventProject *pProject;
	std::string path = "/";
	path.append( name );

	if (!this->parsePath( path, &pProject, NULL, NULL ))
		return false;

	defaultProject_ = pProject;
	return true;
}


/**
 *  Sets the microphone position of the listener.
 *
 *  @param position		The listener's position
 *  @param forward		The forward vector
 *  @param up			The up vector
 *  @param deltaTime	Time since last call to this method
 */
bool SoundManager::setListenerPosition( const Vector3& position,
	const Vector3& forward, const Vector3& up, float deltaTime )
{
	BW_GUARD;
	if (eventSystem_ == NULL)
		return false;

    // if the listener position has been set before
    if( lastSet_ )
    {
        if( deltaTime > 0 )
        {
            lastVelocity_ = ( position - lastPosition_ ) / deltaTime;
        }
        else
        {
            lastVelocity_ = Vector3( 0, 0, 0 );
        }

        lastPosition_ = position;
    }
    else
    {
        lastSet_ = true;
        lastPosition_ = position;
        lastVelocity_ = Vector3( 0, 0, 0 );
    }

	eventSystem_->set3DListenerAttributes( 0,
		(FMOD_VECTOR*)&lastPosition_,
		(FMOD_VECTOR*)&lastVelocity_,
		(FMOD_VECTOR*)&forward,
		(FMOD_VECTOR*)&up );

    return this->update();
}


/**
 *  Get the most recent position of the listener.  Pass NULL for any of the
 *  parameters you aren't interested in.
 */
void SoundManager::getListenerPosition( Vector3 *pPosition, Vector3 *pVelocity )
{
	BW_GUARD;
	if (pPosition != NULL)
	{
		*pPosition = lastPosition_;
	}

	if (pVelocity != NULL)
	{
		*pVelocity = lastVelocity_;
	}
}


/**
 *  Set the master volume.  Returns true on success.  All errors are reported as
 *  Python errors, so if you are calling this from C++ you will need to extract
 *  error messages with PyErr_PrintEx(0).
 */
bool SoundManager::setMasterVolume( float vol )
{
	BW_GUARD;
	FMOD_RESULT result;
	FMOD::EventCategory *pCategory;

	if (eventSystem_ == NULL)
	{
		PyErr_Format( PyExc_RuntimeError, "SoundManager::setMasterVolume: ",
			"No sound subsystem, can't set master volume" );
		return false;
	}

	result = eventSystem_->getCategory( "master", &pCategory );
	if (result != FMOD_OK)
	{
		PyErr_Format( PyExc_RuntimeError, "SoundManager::setMasterVolume: ",
			"Couldn't get master EventCategory: %s\n",
			FMOD_ErrorString( result ) );

		return false;
	}

	result = pCategory->setVolume( vol );
	if (result != FMOD_OK)
	{
		PyErr_Format( PyExc_RuntimeError, "SoundManager::setMasterVolume: ",
			"Couldn't set master channel group volume: %s\n",
			FMOD_ErrorString( result ) );

		return false;
	}

	return true;
}


float SoundManager::dbToLinearLevel( float db )
{
	BW_GUARD;
	if (db > 0)
	{
		WARNING_MSG( "SoundManager::dbToLinearLevel: "
			"Level > 0dB passed in (%f) - capping to 0dB\n", db );
		db = 0;
	}

	return 1.f / (db / -3.f);
}


/**
 *  This is the catch-all method for parsing soundbank paths.  The general
 *  semantics are similar to those for filesystem paths, which gives two general
 *  forms of event name:
 *
 *  absolute: /project/group1/group2/event
 *  relative: group1/group2/event
 *
 *  The default project is used to look up relative paths.
 *
 *  The caller must pass in FMOD pointer pointers which the return values are
 *  written to, or NULL if the caller isn't interested in a particular pointer.
 *
 *  You cannot pass a non-NULL pointer after you have passed a NULL one,
 *  i.e. you can't pass NULL for ppProject and then pass a non-NULL pointer for
 *  ppGroup.
 *
 *  If you pass NULL for ppEvent, then the entire path is considered to be the
 *  name of the event group, rather than the usual 'path/to/group/eventname'
 *  semantics.
 */
bool SoundManager::parsePath( const std::string &path,
	FMOD::EventProject **ppProject, FMOD::EventGroup **ppGroup,
	Event **ppEvent, bool allowLoadProject )
{
	BW_GUARD;
	FMOD_RESULT result;
	ssize_t groupStart = 0;
	std::string groupName, eventName;

	if (eventSystem_ == NULL)
		return false;

	// Sanity check for the path
	if (path.size() == 0)
	{
		ERROR_MSG( "SoundManager::parsePath: Invalid path '%s'\n",
			path.c_str() );

		return false;
	}

	// If the project isn't wanted, bail now
	if (!ppProject)
		return true;

	// If the leading character is a '/', then the project has been manually
	// specified
	if (path[0] == '/')
	{
		ssize_t firstSlash = path.find( '/', 1 );
		bool gotFirstSlash = (firstSlash != std::string::npos);

		groupStart = gotFirstSlash ? firstSlash + 1 : path.size();

		std::string projectName( path, 1,
			gotFirstSlash ? firstSlash - 1 : path.size() - 1 );

		EventProjects::iterator it = eventProjects_.find( projectName );

		// If the project isn't loaded, do it now
		if (it == eventProjects_.end())
		{
			if (!allowLoadProject)
				return false;

			DataSectionPtr data = BWResource::openSection(mediaPath_ + "/" + projectName + ".fev");

			if( data.exists() == false )
			{
				ERROR_MSG( "SoundManager::parsePath: "
					"Failed to load '%s'\n", (mediaPath_ + "/" + projectName + ".fev").c_str() );
				return false;
			}

			BinaryPtr pBinary = data->asBinary();

			FMOD_EVENT_LOADINFO loadInfo;
			loadInfo.size = sizeof(loadInfo);
			loadInfo.encryptionkey = 0;
			loadInfo.sounddefentrylimit = 0;
			loadInfo.loadfrommemory_length = pBinary->len();

#if 0 // not going to load from disk
			// todo load from zip and pass data
			result = eventSystem_->load(
				(projectName + ".fev").c_str(), NULL, ppProject );
#else // load from memory
			result = eventSystem_->load(
				(const char*)pBinary->data(), &loadInfo, ppProject );
#endif			

			if (result == FMOD_OK)
			{
				eventProjects_[ projectName ] = *ppProject;

				// Set the default project if there isn't one already
				if (defaultProject_ == NULL)
					defaultProject_ = *ppProject;
			}
			else
			{
				ERROR_MSG( "SoundManager::parsePath: "
					"Failed to load project %s: %s\n",
					projectName.c_str(), FMOD_ErrorString( result ) );

				return false;
			}
		}

		// Otherwise just pass back handle to already loaded project
		else
		{
			*ppProject = it->second;
		}
	}

	// If no leading slash, then we're talking about the default project
	else
	{
		groupStart = 0;

		if (defaultProject_ == NULL)
		{
			PyErr_Format( PyExc_LookupError, "SoundManager::parsePath: "
				"No project specified and no default project loaded: %s",
				path.c_str() );

			return false;
		}
		else
		{
			*ppProject = defaultProject_;
		}
	}

	// If the group isn't wanted, bail now
	if (!ppGroup)
		return true;

	// If ppEvent isn't provided, then the group name is the rest of the path
	if (!ppEvent)
	{
		groupName = path.substr( groupStart );
	}

	// Otherwise, we gotta split on the final slash
	else
	{
		ssize_t lastSlash = path.rfind( '/' );

		if (lastSlash == std::string::npos || lastSlash < groupStart)
		{
			PyErr_Format( PyExc_SyntaxError, "SoundManager::parsePath: "
				"Asked for illegal top-level event '%s'", path.c_str() );
			return false;
		}

		ssize_t eventStart = lastSlash + 1;
		groupName = path.substr( groupStart, lastSlash - groupStart );
		eventName = path.substr( eventStart );
	}

	// If the group name is empty, set ppGroup to NULL and we're done.
	if (groupName.empty())
	{
		*ppGroup = NULL;
		return true;
	}

	// If the event group hasn't been loaded yet, do it now.
	Group g( *ppProject, groupName );
	EventGroups::iterator it = eventGroups_.find( g );

	if (it != eventGroups_.end())
	{
		*ppGroup = it->second;
	}
	else
	{
		// We pass 'cacheevents' as false here because there is no Python API
		// exposure for FMOD::Group and precaching is all handled by
		// BigWorld.loadSoundGroup().
		result = (*ppProject)->getGroup(
			groupName.c_str(), false, ppGroup );

		if (result == FMOD_OK)
		{
			eventGroups_[ g ] = *ppGroup;
		}
		else
		{
			PyErr_Format( PyExc_LookupError, "SoundManager::get: "
				"Couldn't get event group '%s': %s",
				groupName.c_str(), FMOD_ErrorString( result ) );

			return false;
		}
	}

	// If the event isn't wanted, bail now
	if (!ppEvent)
		return true;

	// Get event handle
	result = (*ppGroup)->getEvent( eventName.c_str(), FMOD_EVENT_DEFAULT, 
									ppEvent );

	if (result != FMOD_OK)
	{
		PyErr_Format( PyExc_LookupError, "SoundManager::get: "
			"Couldn't get event %s from group %s: %s",
			eventName.c_str(), groupName.c_str(), FMOD_ErrorString( result ) );

		return false;
	}
	else
	{
		// insert it, or, if it exists, make it true anyway.
		events_[ *ppEvent ] = true;
	}

	return true;
}


/**
 *  Converts the provided sound path into an absolute path.
 */
bool SoundManager::absPath( const std::string &path, std::string &ret )
{
	BW_GUARD;
	// If the path is already absolute just copy it
	if (path.size() && path[0] == '/')
	{
		ret = path;
		return true;
	}

	// Otherwise, prepend the default project
	else if (defaultProject_)
	{
		char *pname;
		defaultProject_->getInfo( NULL, &pname );
		ret = "/";
		ret += pname;
		ret.push_back( '/' );
		ret += path;
		return true;
	}

	else
	{
		PyErr_Format( PyExc_RuntimeError,
			"Can't resolve absolute path with no default project" );
		return false;
	}
}

// -----------------------------------------------------------------------------
// Section: EventList
// -----------------------------------------------------------------------------

SoundManager::EventList::EventList() :
	std::list< Event* >(),
	stopOnDestroy_( true )
{
	BW_GUARD;	
}

/**
 *  If required, the destructor cleans up any sounds still remaining in the
 *  list.
 */
SoundManager::EventList::~EventList()
{
	BW_GUARD;
	if (stopOnDestroy_)
	{
		this->stopAll();
	}

	// If we're allowing sounds to play after this list is destroyed, then we
	// need to make sure their callbacks are disabled so they don't try to
	// delete themselves from this list after it has been destroyed
	else
	{
		for (iterator it = this->begin(); it != this->end(); ++it)
		{
			(*it)->setCallback( NULL, NULL );
		}
	}
}


/**
 *  FMOD callback called for sounds attached to models.  Just removes this
 *  Event* from the model's EventList.
 */
FMOD_RESULT F_CALLBACK attachedEventCallback( FMOD::Event * pEvent,
	FMOD_EVENT_CALLBACKTYPE type, void *p1, void *p2, void *userData )
{
	BW_GUARD;
	if (type == FMOD_EVENT_CALLBACKTYPE_EVENTFINISHED ||
		type == FMOD_EVENT_CALLBACKTYPE_STOLEN)
	{
		SoundManager::EventList *pList = (SoundManager::EventList*)userData;
		SoundManager::EventList::iterator iter = std::find(
			pList->begin(), pList->end(), pEvent );

		// It's fine for an event to not be in the list ... that just means it
		// was erased during an update(), probably because its Event* had been
		// stolen by a newer Event.
		if (iter != pList->end())
		{
			pList->erase( iter );
		}

		// Avoid needless callback on EVENTFINISHED for this event.
		if (type == FMOD_EVENT_CALLBACKTYPE_STOLEN)
		{
			pEvent->setCallback( NULL, NULL );
		}
	}

	return FMOD_OK;
}


/**
 *  Appends an Event to this list, and sets the FMOD callback for the Event so
 *  it will automatically remove itself from the list once it has finished
 *  playing.
 */
void SoundManager::EventList::push_back( Event *pEvent )
{
	BW_GUARD;
	pEvent->setCallback( (FMOD_EVENT_CALLBACK)attachedEventCallback, this );
	std::list< Event* >::push_back( pEvent );
}


/**
 *  Update positions for any sounds that are still playing.
 */
bool SoundManager::EventList::update( const Vector3 &pos )
{
	BW_GUARD;
	FMOD_RESULT result;
	FMOD_EVENT_STATE state;
	bool ok = true;

	iterator it = this->begin();
	while (it != this->end())
	{
		Event *pEvent = *it;
		result = pEvent->getState( &state );

		if (SoundManager::instance().set3D( pEvent, pos, true ))
		{
			++it;
		}
		else
		{
			// If we get to here, the event must have had it's channel stolen
			// and so you'd think that it's EVENTFINISHED callback would never
			// be called, but sometimes it seems to be.  Strange.  Make sure the
			// callback won't be called so we don't segfault in the callback if
			// this EventList has been deleted in the meantime.
			pEvent->setCallback( NULL, NULL );

			it = this->erase( it );
		}
	}

	return ok;
}

/**
 *  Stop and clear all sound events.
 */
bool SoundManager::EventList::stopAll()
{
	BW_GUARD;
	bool ok = true;

	for (iterator it = this->begin(); it != this->end(); ++it)
	{
		Event *pEvent = *it;

		// Nullify the callback now, since we don't want the callback to be
		// called when we stop() the event below.
		pEvent->setCallback( NULL, NULL );

		FMOD_RESULT result = pEvent->stop();

		if (result != FMOD_OK)
		{
			ERROR_MSG( "SoundManager::EventList::stopAll: "
				"Couldn't stop %s: %s\n",
				SoundManager::name( pEvent ), FMOD_ErrorString( result ) );

			ok = false;
		}
	}

	if (!ok)
	{
		ERROR_MSG( "SoundManager::EventList::stopAll: "
			"Some events failed to stop\n" );
	}

	this->clear();

	return ok;
}


/**
 *  Returns the name of the provided Event.  Uses memory managed by FMOD so
 *  don't expect the pointer to be valid for long.
 */
const char* SoundManager::name( Event * pEvent )
{
	BW_GUARD;
	MF_ASSERT( pEvent );
	char *name = NULL;

	FMOD_RESULT result = pEvent->getInfo( NULL, &name, NULL );

	if (result == FMOD_OK)
	{
		return name;
	}
	else
	{
		static const char *err = "<error>";
		ERROR_MSG( "SoundManager::name: %s\n", FMOD_ErrorString( result ) );
		return err;
	}
}


// -----------------------------------------------------------------------------
// Section: Stubs for disabled sound support
// -----------------------------------------------------------------------------

#else // FMOD_SUPPORT

/**
 *  All the operations here are no-ops and will fail.
 */

bool SoundManager::initialise( DataSectionPtr config )
{
	return false;
}

void SoundManager::fini()
{
}

bool SoundManager::update()
{
	return false;
}

bool SoundManager::setPath( const std::string &path )
{
	return false;
}

void SoundManager::allowUnload( bool b )
{
}

bool SoundManager::allowUnload() const
{
	return false;
}

void SoundManager::registerSoundBank( const std::string &filename, DataSectionPtr data )
{
}

bool SoundManager::unregisterSoundBank( const std::string &filename )
{
	return false;
}

bool SoundManager::loadSoundBank( const std::string &soundbank )
{
	return false;
}

bool SoundManager::unloadSoundBank( const std::string &soundbank )
{
	return false;
}

void SoundManager::getSoundBanks( std::list< std::string > & soundBanks )
{
}


bool SoundManager::loadEventProject( const std::string &soundbank )
{
	return false;
}

bool SoundManager::unloadEventProject( const std::string &soundbank )
{
	return false;
}

SoundManager::Event* SoundManager::play( const std::string &name )
{
	PyErr_SetString( PyExc_NotImplementedError,
		"FMOD support disabled, all sound calls will fail" );
	return NULL;
}

SoundManager::Event* SoundManager::play( const std::string &name, const Vector3 &pos )
{
	PyErr_SetString( PyExc_NotImplementedError,
		"FMOD support disabled, all sound calls will fail" );
	return NULL;
}

SoundManager::Event* SoundManager::get( const std::string &name )
{
	PyErr_SetString( PyExc_NotImplementedError,
		"FMOD support disabled, all sound calls will fail" );
	return NULL;
}

void SoundManager::release( SoundManager::Event * pEvent )
{
}

bool SoundManager::set3D( Event *pEvent, const Vector3& position, bool silent )
{
	return false;
}

bool SoundManager::setListenerPosition( const Vector3& position,
	const Vector3& forward, const Vector3& up, float deltaTime )
{
	return false;
}

bool SoundManager::setDefaultProject( const std::string &name )
{
	return false;
}

bool SoundManager::loadUnload( const std::string &group, bool load )
{
	return false;
}

const char *SoundManager::name( Event *pEvent )
{
	static const char * err =
		"<FMOD support disabled, all sounds calls will fail>";

	return err;
}

bool SoundManager::setMasterVolume( float vol )
{
	return false;
}

#endif // FMOD_SUPPORT


/**
 *  Precache the wavedata for a particular event group (and all groups and
 *  events below it).  See the documentation for
 *  FMOD::EventGroup::loadEventData() for more information.
 */
bool SoundManager::loadWaveData( const std::string &group )
{
	BW_GUARD;
	return this->loadUnload( group, true );
}


/**
 *  Unload the wavedata and free the Event* handles for an event group.  See the
 *  documentation for FMOD::EventGroup::freeEventData() for more info.
 */
bool SoundManager::unloadWaveData( const std::string &group )
{
	BW_GUARD;
	return this->loadUnload( group, false );
}


/**
 *  Return this if you are supposed to return an Event* from a function that is
 *  exposed to script and something goes wrong.
 */
PyObject* SoundManager::error()
{
	BW_GUARD;
	switch (instance().errorLevel())
	{
		case EXCEPTION:
			return NULL;

		case WARNING:
			PyErr_PrintEx(0);
			Py_RETURN_NONE;

		case SILENT:
		default:
			PyErr_Clear();
			Py_RETURN_NONE;
	}
}

static PyObject* py_getSoundBanks( PyObject *args )
{
	BW_GUARD;
	std::list< std::string > soundbanks;
	
	SoundManager::instance().getSoundBanks( soundbanks );

	PyObject* result = PyList_New(0);

	if( result == NULL )
		ERROR_MSG( "py_getSoundbanks: PyList_New failed\n" );

	for( std::list< std::string >::iterator it = soundbanks.begin(); it != soundbanks.end(); ++it )
	{
		PyObject* str = PyString_FromString((*it).c_str());
		PyList_Append( result, str );
		Py_XDECREF(str);
	}

	return result;
}

/*~ function BigWorld.getSoundBanks
 *
 *  Returns a list of sound banks that are referenced by FMod.
 *
 *  @return	list of sound bank names
 */
PY_MODULE_FUNCTION( getSoundBanks, BigWorld );

static PyObject* py_loadSoundBankIntoMemory( PyObject *args )
{
	BW_GUARD;
	const char *soundbank;

	if( PyArg_ParseTuple( args, "s", &soundbank ) == false )
		return NULL;

	SoundManager::instance().registerSoundBank( soundbank, NULL );

	Py_RETURN_NONE;
}

/*~ function BigWorld.loadSoundBankIntoMemory
 *
 *  Loads a sound bank into memory.
 *
 *  @param	soundbank	name of soundbank
 */
PY_MODULE_FUNCTION( loadSoundBankIntoMemory, BigWorld );

static PyObject* py_unloadSoundBankFromMemory( PyObject *args )
{
	BW_GUARD;
	const char *soundbank;

	if( PyArg_ParseTuple( args, "s", &soundbank ) == false )
		return NULL;

	if ( !SoundManager::instance().unregisterSoundBank( soundbank ) )
	{
		PyErr_Format( PyExc_RuntimeError, 
			"Error unregistering soundbank '%s'", soundbank );
		return NULL;
	}

	Py_RETURN_NONE;
}

/*~ function BigWorld.unloadSoundBankFromMemory
 *
 *  Unloads a sound bank from memory.
 *
 *  @param	soundbank	name of soundbank
 */
PY_MODULE_FUNCTION( unloadSoundBankFromMemory, BigWorld );

// soundmanager.cpp
