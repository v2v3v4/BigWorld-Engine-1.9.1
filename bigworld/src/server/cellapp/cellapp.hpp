/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/*
 *	Class: CellApp - represents the Cell application.
 *
 *	This class is used as a singleton. This object represents the entire Cell
 *	application. It's main functionality is to handle the interface to this
 *	application and redirect the calls.
 */

#ifndef CELLAPP_HPP
#define CELLAPP_HPP

#include <Python.h>

#include "cstdmf/memory_stream.hpp"
#include "cstdmf/singleton.hpp"
#include "cstdmf/time_queue.hpp"

#include "entitydef/entity_description_map.hpp"
#include "network/mercury.hpp"
#include "pyscript/pickler.hpp"

#include "server/common.hpp"
#include "server/id_client.hpp"
#include "server/python_server.hpp"

#include "cellapp_interface.hpp"
#include "cellappmgr.hpp"
#include "profile.hpp"
#include "common/doc_watcher.hpp"
#include "updatable.hpp"

#include "cell_viewer_server.hpp"
#include "cellapp_death_listener.hpp"

class Cell;
class Entity;
class SharedData;
class Space;
class TimeKeeper;
struct CellAppInitData;

typedef Mercury::ChannelOwner DBMgr;


/**
 *	This singleton class represents the entire application.
 */
class CellApp : public Mercury::TimerExpiryHandler,
	public Singleton< CellApp >
{
private:
	enum TimeOutType
	{
		TIMEOUT_GAME_TICK,
		TIMEOUT_TRIM_HISTORIES,
		TIMEOUT_LOADING_TICK
	};

public:
	/// @name Construction/Initialisation
	//@{
	CellApp( Mercury::Nub & nub );
	virtual ~CellApp();

	bool init( int argc, char *argv[] );
	bool finishInit( const CellAppInitData & initData );

	bool run( int argc, char *argv[] );

	void onGetFirstCell( bool isFromDB );

	//@}

	/// @name Message handlers
	//@{
	void addCell( BinaryIStream & data );
	void startup( const CellAppInterface::startupArgs & args );
	void setGameTime( const CellAppInterface::setGameTimeArgs & args );

	void handleCellAppMgrBirth(
		const CellAppInterface::handleCellAppMgrBirthArgs & args );

	void handleCellAppDeath(
		const CellAppInterface::handleCellAppDeathArgs & args );

	void handleBaseAppDeath( BinaryIStream & data );

	void shutDown( const CellAppInterface::shutDownArgs & args );
	void controlledShutDown(
			const CellAppInterface::controlledShutDownArgs & args );
	void requestShutDown();

	void createSpaceIfNecessary( BinaryIStream & data );

	void runScript( BinaryIStream & data );
	void setSharedData( BinaryIStream & data );
	void delSharedData( BinaryIStream & data );

	void setBaseApp( const CellAppInterface::setBaseAppArgs & args );

	void onloadTeleportedEntity( const Mercury::Address & srcAddr,
		const Mercury::UnpackedMessageHeader & header, BinaryIStream & data );

	//@}

	/// @name Utility methods
	//@{
	Entity * findEntity( EntityID id ) const;

	void entityKeys( PyObject * pList ) const;
	void entityValues( PyObject * pList ) const;
	void entityItems( PyObject * pList ) const;

	std::string pickle( PyObject * args );
	PyObject * unpickle( const std::string & str );
	PyObject * newClassInstance( PyObject * pyClass,
			PyObject * pDictionary );

	bool reloadScript( bool isFullReload );
	//@}

	/// @name Accessors
	//@{
	Cell *	findCell( SpaceID id ) const;
	Space *	findSpace( SpaceID id ) const;
	Space * findOrCreateSpace( SpaceID id );

	static Mercury::Channel & getChannel( const Mercury::Address & addr )
	{
		return CellApp::instance().nub_.findOrCreateChannel( addr );
	}

	Mercury::Nub & nub()						{ return nub_; }

	CellAppMgr & cellAppMgr()				{ return cellAppMgr_; }
	DBMgr & dbMgr()							{ return *dbMgr_.pChannelOwner(); }

	TimeQueue & timeQueue()						{ return timeQueue_; }
	const std::string & exeName()				{ return exeName_; }

	TimeStamp time() const						{ return time_; }
	double gameTimeInSeconds() const
									{ return double(time_)/updateHertz_; }
	float getLoad() const						{ return load_; }
	float spareTime() const						{ return spareTime_; }

	int updateHertz() const						{ return updateHertz_; }

	uint64 lastGameTickTime() const				{ return lastGameTickTime_; }

	typedef std::vector< Cell * > Cells;
	Cells & cells()								{ return cells_; }
	const Cells & cells() const					{ return cells_; }

	typedef std::map< SpaceID, Space * > Spaces;
	Spaces & spaces()							{ return spaces_; }
	const Spaces & spaces() const				{ return spaces_; }

	bool hasStarted() const						{ return gameTimerID_ != 0; }
	bool isShuttingDown() const					{ return shutDownTime_ != 0; }

	bool isProduction() const					{ return isProduction_; }

	int numRealEntities() const;

	int numStartupRetries() const				{ return numStartupRetries_; }

	float noiseStandardRange() const			{ return noiseStandardRange_; }
	float noiseVerticalSpeed() const			{ return noiseVerticalSpeed_; }
	float noiseHorizontalSpeedSqr() const
											{ return noiseHorizontalSpeedSqr_; }

	int maxGhostsToDelete() const				{ return maxGhostsToDelete_; }
	int minGhostLifespanInTicks() const		{ return minGhostLifespanInTicks_; }

	/// Amount to scale back CPU usage: 256 = none, 0 = fully
	// int scaleBack() const						{ return scaleBack_; }
	float emergencyThrottle() const			{ return emergencyThrottle_; }

	const Mercury::InterfaceElement & entityMessage( int index ) const;

	Entity * pTeleportingEntity() const		{ return pTeleportingEntity_; }
	void pTeleportingEntity( Entity * pEntity )	{ pTeleportingEntity_ = pEntity; }

	const Mercury::Address & baseAppAddr() const	{ return baseAppAddr_; }

	bool shouldLoadAllChunks() const	{ return shouldLoadAllChunks_; }
	bool shouldUnloadChunks() const		{ return shouldUnloadChunks_; }

	bool shouldResolveMailBoxes() const	{ return shouldResolveMailBoxes_; }

	bool useDefaultSpace() const		{ return useDefaultSpace_; }

	int entitySpamSize() const			{ return entitySpamSize_; }

	bool extrapolateLoadFromPendingRealTransfers() const
					{ return extrapolateLoadFromPendingRealTransfers_; }

	IDClient & idClient()				{ return idClient_; }
	//@}

	/// @name Update methods
	//@{
	bool registerForUpdate( Updatable * pObject, int level = 0 );
	bool deregisterForUpdate( Updatable * pObject );

	bool nextTickPending() const;	// are we running out of time?
	//@}

	/// @name Misc
	//@{
	void killCell( Cell * pCell );

	static CellApp * findMessageHandler( BinaryIStream & /*data*/ )
											{ return CellApp::pInstance(); }
	void detectDeadCellApps(
		const std::vector< Mercury::Address > & addrs );
	//@}

	void bufferGhostMessage( const Mercury::Address & srcAddr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data,
			EntityID id,
			Mercury::InputMessageHandler * pHandler );

	void playBufferedGhostMessages( Entity * pEntity );
	void eraseBufferedGhostMessages( EntityID entityID );

	void addReplacedGhost( Entity * pEntity );

	bool handleReplacedGhostMessage( const Mercury::Address & srcAddr,
		const Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data,
		EntityID id );

	void callWatcher( const Mercury::Address& srcAddr,
		const Mercury::UnpackedMessageHeader& header,
		BinaryIStream & data );

	bool shouldOffload() const { return shouldOffload_; }
	void shouldOffload( bool b ) { shouldOffload_ = b; }

	int id() const				{ return id_; }

private:
	// Methods
	void initExtensions();
	bool initScript();

	void addWatchers();

	void callUpdates();
	void adjustUpdatables();

	void checkSendWindowOverflows();

	void checkPython();

	void bindNewlyLoadedChunks();

	int secondsToTicks( float seconds, int lowerBound );

	void startGameTime();

	void runScript( std::string script );

	void sendShutdownAck( ShutDownStage stage );

	bool inShutDownPause() const
	{
		return (shutDownTime_ != 0) && (time_ == shutDownTime_);
	}

	/// @name Overrides
	//@{
	int handleTimeout( Mercury::TimerID id, void * arg );
	//@}
	int handleGameTickTimeSlice();
	int handleTrimHistoriesTimeSlice();

	// Data
	Cells			cells_;
	Spaces			spaces_;

	Mercury::Nub &	nub_;
	CellAppMgr		cellAppMgr_;
	AnonymousChannelClient dbMgr_;

	TimeStamp		time_;
	TimeStamp		shutDownTime_;
	TimeQueue		timeQueue_;
	TimeKeeper * 	pTimeKeeper_;

	Pickler * pPickler_;

	Entity * pTeleportingEntity_;

	typedef std::vector< Updatable * > UpdatableObjects;

	UpdatableObjects updatableObjects_;
	std::vector< int > updatablesLevelSize_;
	bool		inUpdate_;
	int			deletedUpdates_;

	// Used for throttling back
	float		emergencyThrottle_;
	float		spareTime_;

	// Throttling configuration
	float					throttleSmoothingBias_;
	float					throttleBackTrigger_;
	float					throttleForwardTrigger_;
	float					throttleForwardStep_;
	float					minThrottle_;
	float					throttleEstimatedEffect_;

	bool					extrapolateLoadFromPendingRealTransfers_;

	uint64					lastGameTickTime_;

	int						updateHertz_;

	PythonServer *			pPythonServer_;

	SharedData *			pCellAppData_;
	SharedData *			pGlobalData_;

	bool					isShuttingDown_;
	bool					shouldRequestShutDown_;
	std::string				exeName_;
	IDClient				idClient_;

	Mercury::Address		baseAppAddr_;

	int						backupIndex_;
	int						backupPeriod_;
	int						checkOffloadsPeriod_;

	Mercury::TimerID		gameTimerID_;
	uint64					reservedTickTime_;

	CellViewerServer *		pViewerServer_;
	CellAppID				id_;

	// TODO: We could put all "global" configuration options in their own object
	// or namespace.
	float					demoNumEntitiesPerCell_;
	float					load_;
	float					loadSmoothingBias_;

	bool					demoLoadBalancing_;
	bool					shouldLoadAllChunks_;
	bool					shouldUnloadChunks_;
	bool					shouldOffload_;
	bool					fastShutdown_;
	bool					isFromMachined_;
	bool					isProduction_;

	bool					shouldResolveMailBoxes_;

	bool					useDefaultSpace_;

	int						entitySpamSize_;

	int						maxGhostsToDelete_;
	int						minGhostLifespanInTicks_;

	float					maxCPUOffload_;
	int						minEntityOffload_;

	int						numStartupRetries_;

	float					noiseStandardRange_;
	float					noiseVerticalSpeed_;
	float					noiseHorizontalSpeedSqr_;

	bool 					hasAckedCellAppMgrShutDown_;

  	/**
 	 *	This class is used to store a real->ghost message that has arrived too
 	 *	early.  This could mean the ghost doesn't exist yet, or that it is a
 	 *	message that has been reordered as a side-effect of offloading (since we
 	 *	cannot strictly guarantee the ordering between two channels).
  	 */
 	class BufferedGhostMessage
	{
	public:
		BufferedGhostMessage( const Mercury::Address & srcAddr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data,
			EntityID entityID,
			Mercury::InputMessageHandler * pHandler );

		void play();
		bool isValidFirstMessage() const;

	private:
		EntityID entityID_;
		Mercury::Address srcAddr_;
		Mercury::UnpackedMessageHeader header_;
		MemoryOStream data_;
		Mercury::InputMessageHandler * pHandler_;
	};


 	/**
 	 *  This class represents a stream of buffered real->ghost messages from a
 	 *  single CellApp.  It owns the BufferedGhostMessages that it contains.
 	 */
 	class BufferedGhostMessageQueue: public ReferenceCount
 	{
 	public:
 		typedef std::list< BufferedGhostMessage* > Messages;
		typedef Messages::iterator iterator;

 		BufferedGhostMessageQueue( const Mercury::Address & srcAddr ) :
 			srcAddr_( srcAddr )
 		{}

		~BufferedGhostMessageQueue();

 		void add( BufferedGhostMessage * pMessage )
 		{
 			messages_.push_back( pMessage );
 		}

		const Mercury::Address & srcAddr() const { return srcAddr_; }
		unsigned size() const { return messages_.size(); }

		iterator begin() 				{ return messages_.begin(); }
		iterator end() 					{ return messages_.end(); }

		// Unlink the front message from the queue and return it. The caller is
		// responsible for deleting the BufferedGhostMessage instance.
		BufferedGhostMessage * pop_front()
		{
			BufferedGhostMessage * pMsg = messages_.front();
			messages_.pop_front();
			return pMsg;
		}

		bool empty() const				{ return messages_.empty(); }


 	private:
 		Messages messages_;
 		Mercury::Address srcAddr_;
 	};

	typedef SmartPointer<BufferedGhostMessageQueue>
		BufferedGhostMessageQueuePtr;

 	/**
 	 *  This class is a list of BufferedGhostMessageQueues.  It currently
 	 *  assumes that the lists were created in the correct order.  This should
 	 *  eventually use the entity's channel version to guarantee correct
 	 *  ordering.
 	 */
 	class BufferedGhostMessagesForEntity
 	{
 	public:
		BufferedGhostMessagesForEntity( EntityID entityID ) :
			entityID_( entityID )
		{}

 		void add( const Mercury::Address & addr,
			BufferedGhostMessage * pMessage );

		void play( Entity * pEntity );
		EntityID entityID() const { return entityID_; }

 	private:
		EntityID entityID_;

 		typedef std::list< BufferedGhostMessageQueuePtr > Queues;
 		Queues queues_;
 	};

 	typedef std::map< EntityID, BufferedGhostMessagesForEntity* >
 		BufferedGhostMessageMap;

 	BufferedGhostMessageMap bufferedGhostMessageMap_;

	/**
	 *  This is a ghost that was prematurely replaced by another ghost (see the
	 *  WARNING_MSG in Space::createGhost()).  We need to track these because we
	 *  need to discard all messages on this "ghost channel" until we either get
	 *  a ghostSetNextReal or a delGhost, i.e. until we're sure that we have
	 *  flushed all messages coming from the old ghost.
	 */
	class ReplacedGhost
	{
	public:
		ReplacedGhost( EntityID id, const Mercury::Address & realAddr ) :
			id_( id ),
			realAddr_( realAddr )
		{}

		EntityID id() const { return id_; }
		const Mercury::Address & realAddr() const { return realAddr_; }

		// Operators to support using std::find and std::remove
		bool operator==( const ReplacedGhost & other )
		{
			return (id_ == other.id_) && (realAddr_ == other.realAddr_);
		}

		bool operator==( const Mercury::Address & addr )
		{
			return realAddr_ == addr;
		}

	private:
		EntityID id_;
		Mercury::Address realAddr_;
	};


	/**
	 *  This class is a collection of ReplacedGhosts that knows how to cope with
	 *  a CellApp crashing.
	 */
	class ReplacedGhosts :
		public std::vector< ReplacedGhost >,
		public CellAppDeathListener
	{
	private:
		virtual void handleCellAppDeath( const Mercury::Address & addr );
	};

	ReplacedGhosts replacedGhosts_;

	friend class CellAppResourceReloader;
};


#ifdef CODE_INLINE
#include "cellapp.ipp"
#endif


#endif // CELLAPP_HPP
