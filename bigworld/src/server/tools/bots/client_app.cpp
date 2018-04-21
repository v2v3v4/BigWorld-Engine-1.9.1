/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "client_app.hpp"
#include "main_app.hpp"
#include "movement_controller.hpp"
#include "py_entities.hpp"
#include "space_data_manager.hpp"

DECLARE_DEBUG_COMPONENT2( "Bots", 0 )

PY_TYPEOBJECT( ClientApp )

PY_BEGIN_METHODS( ClientApp )
	PY_METHOD( logOn )
	PY_METHOD( logOff )
	PY_METHOD( dropConnection )
	PY_METHOD( setConnectionLossRatio )
	PY_METHOD( setConnectionLatency )
	PY_METHOD( setMovementController )
	PY_METHOD( moveTo )
	PY_METHOD( faceTowards )
	PY_METHOD( snapTo )
	PY_METHOD( stop )
	PY_METHOD( addTimer )
	PY_METHOD( delTimer )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( ClientApp )
	PY_ATTRIBUTE( id )
	PY_ATTRIBUTE( spaceID )
	PY_ATTRIBUTE( loginName )
	PY_ATTRIBUTE( loginPassword )
	PY_ATTRIBUTE( tag )
	PY_ATTRIBUTE( speed )
	PY_ATTRIBUTE( position )
	PY_ATTRIBUTE( yaw )
	PY_ATTRIBUTE( pitch )
	PY_ATTRIBUTE( roll )
	PY_ATTRIBUTE( entities )
	PY_ATTRIBUTE( autoMove )
	PY_ATTRIBUTE( isOnline )
	PY_ATTRIBUTE( isMoving )
	PY_ATTRIBUTE( isDestroyed )
PY_END_ATTRIBUTES()

// -----------------------------------------------------------------------------
// Section: Construction/Destruction
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
 ClientApp::ClientApp( Mercury::Nub & mainNub, const std::string & name,
	const std::string & password, const std::string & tag, PyTypeObject * pType ) :
	PyObjectPlus( pType ),
	serverConnection_(),
	spaceID_( 0 ),
	playerID_( 0 ),
	pLoginInProgress_( NULL ),
	isDestroyed_( false ),
	isDormant_( true ),
	mainNub_( mainNub ),
	useScripts_( false ),
	userName_( name ),
	userPasswd_( password ),
	tag_( tag ),
	speed_( 6.f + float(rand())*2.f/float(RAND_MAX) ),
	pMovementController_( NULL ),
	autoMove_( true ),
	pDest_( NULL )
{
	useScripts_ = MainApp::instance().useScripts();

	// Register this bot's nub as a slave to the app's main nub.
	mainNub_.registerChildNub( &serverConnection_.nub(), this );

	pEntities_ = new PyEntities( this );

	this->logOn();
}

void ClientApp::logOn()
{
	if (isDestroyed_)
		return;

	isDormant_ = false;

	if ((pLoginInProgress_ != NULL) || serverConnection_.online())
		return;

	MainApp & app = app.instance();

	serverConnection_.pTime( &app.localTime() );

	// check digest
	MD5::Digest digest = app.digest();
	bool emptyDigest = true;
	for (uint32 i = 0; i < sizeof( digest ); i++)
	{
		if (digest.bytes[i] != '0')
		{
			emptyDigest = false;
			break;
		}
	}

	if (!emptyDigest)
		serverConnection_.digest( digest );

	TRACE_MSG( "Connecting to server at %s\n", app.serverName().c_str() );

	pLoginInProgress_ = serverConnection_.logOnBegin(
		app.serverName().c_str(),
		userName_.c_str(),
		userPasswd_.c_str(),
		app.publicKeyPath().c_str() );
}

void ClientApp::logOff()
{
	if (serverConnection_.online())
	{
		serverConnection_.send(); // make sure last bundle is pushed to server
		serverConnection_.disconnect();
		mainNub_.deregisterFileDescriptor( serverConnection_.nub().socket() );
	}
}

void ClientApp::dropConnection()
{
	// TODO: calling this function will hit an assertion
	// MF_ASSERT_DEV FAILED: !p->shouldCreateAnonymous() in nub.cpp
	// This is expected since we abruptly drop the connection (and deleting
	// corresponding channel). It will trigger dropped packet routine check as
	// data still get pumped from the server.
	// I believe the easiest way to fix this is removing the dev assert.
	// However, I don't want change any other code other than bots at this
	// moment.
	if (serverConnection_.online())
	{
		serverConnection_.disconnect( false );
		mainNub_.deregisterFileDescriptor( serverConnection_.nub().socket() );
	}
}

void ClientApp::setConnectionLossRatio( float lossRatio )
{
	if (lossRatio < 0.0 || lossRatio > 1.0)
	{
		PyErr_Format( PyExc_ValueError, "Loss ratio for connection "
			"should be within [0.0 - 1.0]" );
		return;
	}
	serverConnection_.nub().setLossRatio( lossRatio );
}

void ClientApp::setConnectionLatency( float latencyMin, float latencyMax )
{
	// TODO: add more checking and make it more sophisticated.
	if (latencyMin >= latencyMax)
	{
		PyErr_Format( PyExc_ValueError,
			"latency max should be larger than latency min" );
		return;
	}
	serverConnection_.nub().setLatency( latencyMin, latencyMax );
}

/**
 *	Destructor.
 */
ClientApp::~ClientApp()
{
	if (!isDestroyed_)
		this->destroy();
}

// -----------------------------------------------------------------------------
// Section: Python related
// -----------------------------------------------------------------------------

/**
 *	This method overrides the PyObjectPlus method to get a Python attribute of
 *	this object.
 */
PyObject * ClientApp::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();
	return PyObjectPlus::pyGetAttribute( attr );
}

/**
 *	This method overrides the PyObjectPlus method to set a Python attribute of
 *	this object.
 */
int ClientApp::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();
	return PyObjectPlus::pySetAttribute( attr, value );
}


// -----------------------------------------------------------------------------
// Section: InputNotificationHandler overrides
// -----------------------------------------------------------------------------

/*
 * Override from InputNotificationHandler.
 */
int ClientApp::handleInputNotification( int )
{
	this->tick( 0.f );

	return 0;
}


/**
 *	This method is called when the base part of the player has been created.
 */
void ClientApp::onBasePlayerCreate( EntityID id, EntityTypeID type,
	BinaryIStream & data )
{
	//TRACE_MSG( "ClientApp::onBasePlayerCreate(%08x): id = %d\n", (int)this, id );
	playerID_ = id;
	spaceID_ = 0;

	// Create the entity no matter it is scriptable or not,
	// so we can logoff the entity from server on bot deletion
	EntityType * pType = EntityType::find( type );
	MF_ASSERT( pType );

	MF_ASSERT( entities_.find( id ) == entities_.end() );

	this->entities_[ id ] = pType->newEntity( *this,
		id, Vector3::zero(), 0, 0, 0, data, /*isBasePlayer:*/true );
}


/**
 *	This method is called when the cell part of the player has been created.
 */
void ClientApp::onCellPlayerCreate( EntityID id,
	SpaceID spaceID, EntityID vehicleID, const Position3D & pos,
	float yaw, float pitch,	float roll,
	BinaryIStream & data )
{
	//TRACE_MSG( "ClientApp::onCellPlayerCreate: id = %d\n", id );
	if (useScripts_)
	{
		MF_ASSERT( id == playerID_ );

		Entity * pPlayer = this->entities_[ id ];
		MF_ASSERT( pPlayer );
		pPlayer->readCellPlayerData( data );
	}

	spaceID_ = spaceID;
	position_ = pos;
	direction_.yaw = yaw;
	direction_.pitch = pitch;
	direction_.roll = roll;

	pMovementController_ =
		MainApp::instance().createDefaultMovementController( speed_, position_);

	if (PyErr_Occurred())
	{
		PyErr_Print();
	}

	data.finish(); // Avoid message about data still being on the stream
}


/**
 *	This method is called when an entity enters the client's AOI.
 */
void ClientApp::onEntityEnter( EntityID id, SpaceID spaceID, EntityID )
{
	//TRACE_MSG( "ClientApp::onEntityEnter(%d): entityID = %d\n", playerID_, id );

	if (useScripts_)
	{
		if (id != playerID_)
		{
			EntityMap::iterator iter = this->entities_.find( id );
			if (iter != this->entities_.end())
			{
// 				TRACE_MSG( "ClientApp::onEntityEnter(%d): entityID %d is in cache\n",
// 					playerID_, id);

				serverConnection_.requestEntityUpdate( id, iter->second->cacheStamps() );
			}
			else
			{
// 				TRACE_MSG( "ClientApp::onEntityEnter(%d): entityID %d is new\n",
// 					playerID_, id );

				serverConnection_.requestEntityUpdate( id );
			}
		}
	}
	else
	{
		serverConnection_.requestEntityUpdate( id );
	}
}


/**
 *	This method is called when an entity leaves the client's AOI.
 */
void ClientApp::onEntityLeave( EntityID id, const CacheStamps & stamps )
{
	//TRACE_MSG( "ClientApp::onEntityLeave(%d): entityID = %d\n", playerID_, id );
	EntityMap::iterator iter = this->entities_.find( id );
	if (iter != this->entities_.end())
	{
		if (iter->second)
		{
			iter->second->destroy();
			Py_DECREF( iter->second );
		}
		this->entities_.erase( iter );
	}
	else
	{
		// __glenc__: Disabling this warning because this can happen if a
		// leave message is recvd before the reply to requestEntityUpdate.
		// Since the client-side caching system is disabled, we should just
		// remove 'enterAoI' and have the server send 'createEntity' the
		// first time.
// 			WARNING_MSG("ClientApp::onEntityLeave(%d): "
// 				"cannot find entityID %d\n", playerID_, id);
	}
}

/**
 *	This method is called by the server in response to a
 *	requestEntityUpdate.
 */
void ClientApp::onEntityCreate( EntityID id, EntityTypeID type,
	SpaceID spaceID, EntityID vehicleID, const Position3D & pos,
	float yaw, float pitch,	float roll,
	BinaryIStream & data )
{
	//TRACE_MSG( "ClientApp::onEntityCreate(%d): entityID = %d\n", playerID_, id );
	// Make sure it doesn't already exist.
	if (this->entities_.find( id ) != this->entities_.end())
	{
		ERROR_MSG( "ClientApp::onEntityCreate(%d): "
				"entity(id = %d) already exists\n", playerID_, id );

		data.finish();
	}
	else
	{
		EntityType * pEntityType = EntityType::find( type );
		if (pEntityType == NULL)
		{
			ERROR_MSG( "ClientApp::onEntityCreate(%d): "
					"entity type %d doesn't exist for bots\n", playerID_, (int)type );
			return;
	    }
		Vector3 vectPos (pos);
		if (useScripts_)
		{
			// now, entity is only created when it is
			// required for running corresponding script.
			this->entities_[ id ] = pEntityType->newEntity( *this,
				id, vectPos, yaw, pitch, roll, data, /*isBasePlayer:*/false );
		}
		else
		{
			this->entities_[ id ] = NULL;
			data.finish();
		}
	}
}

/**
 *	This method is called by the server to update some properties of
 *	the given entity, while it is in our AoI.
 */
void ClientApp::onEntityProperties( EntityID id, BinaryIStream & data )
{
	//TRACE_MSG( "ClientApp::onEntityProperties(%d): entityID = %d\n", playerID_, id );
	if (useScripts_)
	{
		// unimplemented since this client does not support detail levels
		// (currently the only cause of this message)
		EntityMap::iterator iter = this->entities_.find( id );
		if (iter != this->entities_.end())
		{
			iter->second->updateProperties( data );
		}
		else
		{
			ERROR_MSG( "ClientApp::onEntityProperties(Bot %d): entity(id = %d) not found\n",
						playerID_, id );
		}
	}
	else
	{
		data.finish(); // Avoid message about data still being on the stream
	}
}


/**
 *	This method is called when the server sets a property on an entity.
 */
void ClientApp::onEntityProperty( EntityID entityID, int propertyID,
	BinaryIStream & data )
{
	//TRACE_MSG( "ClientApp::onEntityProperty(%d:%08x): entityID = %d\n", playerID_, (int)this, entityID );
	if (useScripts_)
	{
		EntityMap::iterator iter = this->entities_.find( entityID );
		if ( iter != this->entities_.end() )
		{
			iter->second->handlePropertyChange( propertyID , data );
		}
		else
		{
			// this could be a message for an entity that has not yet been
			// loaded, or has already been unloaded.
		}
	}
	else
	{
		data.finish(); // Avoid message about data still being on the stream
	}
}


/**
 *	This method is called when the server calls a method on an entity.
 */
void ClientApp::onEntityMethod( EntityID entityID, int methodID,
	BinaryIStream & data )
{
	//TRACE_MSG( "ClientApp::onEntityMethod(%d): entityID = %d\n", playerID_, entityID );
	if (useScripts_)
	{
		EntityMap::iterator iter = this->entities_.find( entityID );
		if ( iter != this->entities_.end() )
		{
			iter->second->handleMethodCall( methodID , data );
		}
		else
		{
			// this could be a message for an entity that has not yet been
			// loaded, or has already been unloaded.
		}
	}
	else
	{
		data.finish(); // Avoid message about data still being on the stream
	}
}


/**
 *	This method is called when the position of an entity changes.
 */
void ClientApp::onEntityMove( EntityID entityID, SpaceID spaceID, EntityID vehicleID,
	const Position3D & pos,	float yaw, float pitch, float roll,
	bool isVolatile )
{
	//TRACE_MSG( "ClientApp::onEntityMove(%d): entityID = %d\n", playerID_, entityID );
	if (entityID == playerID_)
	{
		spaceID_ = spaceID;
		position_ = pos;
		direction_.yaw = yaw;direction_.pitch = pitch; direction_.roll = roll;
		vehicleID_ = vehicleID;
		serverConnection_.addMove( entityID, spaceID, vehicleID,
				pos, yaw, pitch, roll, false, pos );
	}

	if (useScripts_ &&
		this->entities_.find( entityID ) != this->entities_.end())
	{
		this->entities_[ entityID ]->position( pos );
	}
}


/**
 *	This method is called to set the current time of day.
 */
void ClientApp::setTime( TimeStamp gameTime,
		float initialTimeOfDay, float gameSecondsPerSecond )
{
	//TRACE_MSG( "ClientApp::setTime(%d)\n", playerID_ );
}


/**
 *	This method is called when data associated with a space is received.
 */
void ClientApp::spaceData( SpaceID spaceID, SpaceEntryID entryID, uint16 key,
	const std::string & data )
{
	//TRACE_MSG( "ClientApp::spaceData(%d): spaceID = %d\n", playerID_, spaceID );
	SpaceData * pSpace = SpaceDataManager::instance().findOrAddSpaceData( spaceID );

	if (!pSpace)
		return;

	SpaceData::EntryStatus entryStat = pSpace->dataEntry( entryID, key, data );

	// Actually, for client friendliness, we'll parse the data ourselves here.
	if (key == SPACE_DATA_TOD_KEY)
	{
		// TODO:: expose current game time for the space, if it is required.
		// todData.initialTimeOfDay, todData.gameSecondsPerSecond;
		/*
		const std::string * pData =
			pSpace->dataRetrieveFirst( SPACE_DATA_TOD_KEY );
		if (pData != NULL && pData->size() >= sizeof(SpaceData_ToDData))
		{
			SpaceData_ToDData & todData = *(SpaceData_ToDData *)pData->data();
		}
		*/
	}
/*
	else if (key == SPACE_DATA_WEATHER)
	{
		Script::call(
			PyObject_GetAttrString( Personality::instance(), "onWeatherChange" ),
			Py_BuildValue( "(is)", spaceID, data.c_str() ), "EntityManager::spaceData weather notifier: ", true );
	}
*/
	else if (key == SPACE_DATA_MAPPING_KEY_CLIENT_SERVER ||
			 key == SPACE_DATA_MAPPING_KEY_CLIENT_ONLY)
	{
		// We are not very interested in space geometry data
		// at this moment.
	}
	else // give script a chance to handle space data
	{
		PyObjectPtr pModule = MainApp::instance().getPersonalityModule();
		if (entryStat == SpaceData::DATA_ADDED)
		{
			Script::call(
				PyObject_GetAttrString( pModule.getObject(),
					"onSpaceDataCreated" ),
				Py_BuildValue( "(iis)", spaceID, key, data.c_str() ),
				"onSpaceDataCreated", true );
		}
		else if (entryStat == SpaceData::DATA_DELETED)
		{
			Script::call(
				PyObject_GetAttrString( pModule.getObject(),
					"onSpaceDataDeleted" ),
				Py_BuildValue( "(iO)", spaceID, Script::getData( entryID ) ),
				"onSpaceDataDeleted", true );
		}
		else if (entryStat == SpaceData::DATA_MODIFIED)
		{
			Script::call(
				PyObject_GetAttrString( pModule.getObject(),
					"onSpaceDataModified" ),
				Py_BuildValue( "(iis)", spaceID, key, data.c_str() ),
				"onSpaceDataModified", true );
		}
	}
}

/**
 *	This method is called when the given space is no longer visible to the
 *	client.
 */
void ClientApp::spaceGone( SpaceID spaceID )
{
	//TRACE_MSG( "ClientApp::spaceGone(%d): spaceID = %d\n", playerID_, spaceID );
}

/**
 *	This method is called when proxy data is received.
 */
void ClientApp::onProxyData( uint16 proxyDataID, BinaryIStream & data )
{
	TRACE_MSG( "ClientApp::onProxyData: id %04X, data '%.*s'\n",
		proxyDataID, data.remainingLength(),
		(char *)data.retrieve( data.remainingLength() ) );
}

/*
 *	Override from ServerMessageHandler.
 *  This method is called when the server tells us to reset all our
 *  entities. The player entity may optionally be saved (but still should
 *  not be considered to be in the world).
 *
 *  This can occur when the entity that the client is associated with
 *  changes or when the current client destroys its cell entity.
 */
void ClientApp::onEntitiesReset( bool keepPlayerOnBase )
{
	TRACE_MSG( "ClientApp::onEntitiesReset(%d): keepPlayerOnBase = %s\n",
				playerID_, keepPlayerOnBase ? "TRUE" : "FALSE" );

	spaceID_ = 0;

	EntityMap::iterator iterToDel;
	EntityMap::iterator iter = this->entities_.begin();
	while (iter != this->entities_.end())
	{
		Entity * pCurr = iter->second;
		iterToDel = iter;
		++iter;

		if (pCurr) {
			if (pCurr->id() == playerID_ &&
				keepPlayerOnBase)
			{
				continue;
			}
			pCurr->destroy();
			Py_DECREF( pCurr );
		}
		this->entities_.erase(iterToDel);
	}

	if (!keepPlayerOnBase)
	{
		playerID_ = 0;
	}
}


/**
 *  Override from ServerMessageHandler, done for testing streaming downloads to
 *  multiple clients.
 */
void ClientApp::onStreamComplete( uint16 id, const std::string &desc,
	BinaryIStream &data )
{
	INFO_MSG( "Streaming download #%d complete: %s (%d bytes)\n",
		id, desc.c_str(), data.remainingLength() );

	data.finish();
}


// -----------------------------------------------------------------------------
// Section: General interface
// -----------------------------------------------------------------------------

/**
 *	This method is called every tick (probably 100 milliseconds).
 */
bool ClientApp::tick( float dTime )
{
	// if it is dormant skip call tick.
	if (isDormant_)
		return true;


	serverConnection_.processInput();

	// if there is a login in progress see if we can complete it
	if (pLoginInProgress_ != NULL)
	{
		if (pLoginInProgress_->done())
		{
			LogOnStatus status =
				serverConnection_.logOnComplete( pLoginInProgress_, this );

			pLoginInProgress_ = NULL;

			if (!status.succeeded())
			{
				ERROR_MSG( "LogOn failed (%s)\n",
						serverConnection_.errorMsg().c_str() );
				return false;
			}
			else
			{
				if (serverConnection_.online())
				{
					serverConnection_.enableEntities();
				}
			}
		}
		return true;
	}
	else // pLoginInProgress_ == NULL
	{
		if (!serverConnection_.online())
		{
			//clear entity maps first
			for (EntityMap::iterator iter = entities_.begin();
				iter != entities_.end(); iter++)
			{
				if (iter->second)
				{
					iter->second->destroy();
					Py_DECREF( iter->second );
				}	
			}
			entities_.clear();

			if (pMovementController_)
			{
				delete pMovementController_;
				pMovementController_= NULL;
			}

			// allow script to decide we shall self-destruct or
			// still be alive so that we may reattempt log in.

			PyObjectPtr pModule = MainApp::instance().getPersonalityModule();

			if (pModule && (playerID_ != 0))
			{
				PyObject * pResult = Script::ask(
					PyObject_GetAttrString( pModule.getObject(),
					"onLoseConnection" ),
					Py_BuildValue( "(i)", playerID_ ),
					"onLoseConnection", true, true );

				spaceID_ = 0;
				playerID_ = 0;
				vehicleID_ = 0;

				if (pResult != NULL)
				{
					// if the script return true, it shall be destroyed.
					bool toBeDestroyed = (PyObject_IsTrue( pResult ) == 0);
					Py_DECREF( pResult );
					isDormant_ = toBeDestroyed;
					return toBeDestroyed;
				}
				else
				{
					PyErr_Print();
				}
			}
			spaceID_ = 0;
			playerID_ = 0;
			vehicleID_ = 0;
			return false;
		}


		if (dTime > 0.f)
		{
			if (spaceID_ != 0)
			{
				if (useScripts_)
				{
					Entity * pPlayer = this->entities_[ playerID_ ];
					MF_ASSERT( pPlayer );

					PyObject * pFn = PyObject_GetAttrString( pPlayer, "onTick" );

					if (pFn)
					{
						PyObject * pResult = PyObject_CallFunction( pFn, "d",
							serverConnection_.serverTime(
							MainApp::instance().localTime() ) );

						if (pResult != NULL)
						{
							Py_DECREF( pResult );
						}
						else
						{
							PyErr_Print();
						}
						Py_DECREF( pFn );
					}
					else
					{
						PyErr_Clear();
					}
					// Handle any user timeouts
					this->processTimers();
				}


				// Movement ordered by moveTo() takes precedence over
				// movement by movement controller
				if (pDest_ != NULL)
				{
					const float closeEnough = 1.0;
					Vector3 displacement = *pDest_ - position_;
					float length = displacement.length();

					if (length < closeEnough)
					{
						delete pDest_;
						pDest_ = NULL;
					}
					else
					{
						displacement *= speed_ * dTime / length;
						position_ += displacement;
						direction_.yaw = displacement.yaw();
						serverConnection_.addMove(
							playerID_, spaceID_, 0,	position_,
							direction_.yaw,	0, 0, true, position_ );
					}

				}
				else if (autoMove_)
					this->addMove( dTime );
			}
			serverConnection_.send();
		}
	}
	return true;
}


/**
 *	This method sends a movement message to the server.
 */
void ClientApp::addMove( double dTime )
{
	if (isDestroyed_)
		return;

	if (pMovementController_)
	{
		pMovementController_->nextStep( speed_, dTime, position_, direction_ );
		serverConnection_.addMove( playerID_, spaceID_, 0, position_,
				direction_.yaw, direction_.pitch, direction_.roll,
				true, position_ );
	}
	else
	{
		double time = MainApp::instance().localTime();
		const float period = 10.f * speed_ / 7.f;
		const float radius = 10.f;
		const float angle = time * 2 * MATH_PI / period;

		Vector3 position( position_.x + radius * sinf( angle ),
						  0.f,
						  position_.z + radius * cosf( angle ) );

		serverConnection_.addMove( playerID_, spaceID_, 0, position,
				angle + MATH_PI/2.f, 0.f, 0.f, true, position );
	}
}


/**
 *	This method sets a new movement controller for this bot. On failure, the
 *	controller is left unchanged.
 *
 *	@return True on success, otherwise false.
 */
bool ClientApp::setMovementController( const std::string & type,
		const std::string & data )
{
	if (isDestroyed_)
		return false;

	MovementController * pNewController =
		MainApp::instance().createMovementController(
			speed_, position_, type, data );

	if (PyErr_Occurred())
	{
		return false;
	}

	if (pMovementController_ != NULL)
		delete pMovementController_;

	pMovementController_ = pNewController;
	return true;
}

void ClientApp::moveTo( const Vector3 &pos )
{
	if (isDestroyed_)
		return;

	// make sure no memory leak if called repeatedly from script.
	if (pDest_ != NULL)
	{
		pDest_->set( pos.x, pos.y, pos.z );
	}
	else
	{
		pDest_ = new Vector3( pos );
	}

	autoMove_ = false;
}

void ClientApp::faceTowards( const Vector3 &pos )
{
	if (isDestroyed_)
		return;

	direction_.yaw = (pos - position_).yaw();
	serverConnection_.addMove(
		playerID_, spaceID_, 0,	position_,
		direction_.yaw,	0, 0, true, position_ );
}

void ClientApp::stop()
{
	if (isDestroyed_)
		return;

	if (pDest_ != NULL)
	{
		delete pDest_;
		pDest_ = NULL;
	}
	autoMove_ = false;
}

/**
 *  This method adds a timer for this bot.  The callback will be executed during
 *  the next tick after the specified number of seconds has elapsed.  The id of
 *  this timer is returned so it can be canceled later on with delTimer() if
 *  desired.  A negative return value indicates failure.
 */
int ClientApp::addTimer( float interval, PyObjectPtr pFunc, bool repeat )
{
	if (isDestroyed_)
		return -1;

	PyObjectPtr pFuncPyStr( PyObject_Str( pFunc.getObject() ),
		PyObjectPtr::STEAL_REFERENCE );
	char *pFuncStr = PyString_AsString( pFuncPyStr.getObject() );

	// Make sure a function or method was passed
	if (!PyCallable_Check( pFunc.getObject() ))
	{
		ERROR_MSG( "ClientApp::addTimer(): %s is not callable; "
			"timer not added\n", pFuncStr );
		return -1;
	}

	// Make new timeRec and insert into the heap of timers
	TimerRec tr( interval, pFunc, repeat );
	timerRecs_.push( tr );
	return tr.id();
}

/**
 *  This method deletes a timer for this bot.  It actually just adds the timer
 *  ID to a list of timers to be ignored so when the timer finally expires its
 *  callback is not executed.
 */
void ClientApp::delTimer( int id )
{
	if (isDestroyed_)
		return;
	deletedTimerRecs_.push_back( id );
}

void ClientApp::processTimers()
{
	// Process any timers that have elapsed
	while (!timerRecs_.empty() && timerRecs_.top().elapsed())
	{
		TimerRec tr = timerRecs_.top();
		timerRecs_.pop();

		// Check if it has been deleted, if so just ignore it
		std::list< int >::iterator iter = std::find( deletedTimerRecs_.begin(),
			deletedTimerRecs_.end(), tr.id() );
		if (iter !=	deletedTimerRecs_.end())
		{
			deletedTimerRecs_.erase( iter );
			continue;
		}

		PyObject *pResult = PyObject_CallFunction( tr.func(), "" );

		if (pResult != NULL)
		{
			Py_DECREF( pResult );
		}
		else
		{
			PyErr_Print();
		}

		// Re-insert the timer into the queue if it's on repeat
		if (tr.repeat())
		{
			tr.restart();
			timerRecs_.push( tr );
		}
	}
}

/**
 *	This method destroys this ClientApp.
 */
void ClientApp::destroy()
{
	if (!isDestroyed_)
	{
		isDestroyed_ = true;

		PyObjectPtr pModule = MainApp::instance().getPersonalityModule();

		if (pModule && (playerID_ != 0))
		{
			Script::call(
				PyObject_GetAttrString( pModule.getObject(),
						"onClientAppDestroy" ),
					Py_BuildValue( "(i)", playerID_ ),
					"onClientAppDestroy", true );
		}

		if (serverConnection_.online())
		{
			this->logOff();
		}

		//clear entity maps
		for (EntityMap::iterator iter = entities_.begin();
				iter != entities_.end(); iter++)
		{
			if (iter->second)
			{
				iter->second->destroy();
				Py_DECREF( iter->second );
			}
		}
		entities_.clear();
		Py_XDECREF( pEntities_ );

		spaceID_ = 0;
		playerID_ = 0;
		vehicleID_ = 0;

		if (pMovementController_)
		{
			delete pMovementController_;
			pMovementController_= NULL;
		}
		mainNub_.deregisterChildNub( &serverConnection_.nub() );
	}
}

// -----------------------------------------------------------------------------
// Section: TimerRec stuff
// -----------------------------------------------------------------------------
int ClientApp::TimerRec::ID_TICKER = 0;

// client_app.cpp
