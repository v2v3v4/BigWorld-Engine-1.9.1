/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BASEAPP_HPP
#define BASEAPP_HPP

#include "Python.h"

#include <string>
#include <map>


#include "server/common.hpp"
#include "cstdmf/singleton.hpp"
#include "cstdmf/time_queue.hpp"

#include "baseappmgr/baseappmgr_interface.hpp"
#include "server/backup_hash.hpp"
#include "server/id_client.hpp"
#include "common/doc_watcher.hpp"

#include "baseapp_int_interface.hpp"
#include "common/baseapp_ext_interface.hpp"
#include "base.hpp"
#include "proxy.hpp"
#include "bwtracer.hpp"
#include "loading_thread.hpp"
#include "rate_limit_message_filter.hpp"

class GlobalBases;
class Pickler;
class Proxy;
class PythonServer;
class SharedData;
class TimeKeeper;
class WorkerThread;
class SqliteDatabase;
struct BaseAppInitData;
namespace DBInterface
{
	class RunConfig;
}

typedef SmartPointer<Proxy> ProxyPtr;
typedef Mercury::ChannelOwner BaseAppMgr;
typedef Mercury::ChannelOwner DBMgr;

/**
 *	This class implement the main singleton object in the base application.
 *	Its main responsibility is to manage all of the bases on the local process.
 */
class BaseApp : public Mercury::TimerExpiryHandler,
	public Singleton< BaseApp >
{
public:
	BaseApp( Mercury::Nub & nub );
	virtual ~BaseApp();
	bool init( int argc, char *argv[] );

	bool finishInit( const BaseAppInitData & initData );

	void shutDown();

	/* Internal Interface */

	// create this client's proxy
	void createBaseWithCellData( const Mercury::Address& srcAddr,
			const Mercury::UnpackedMessageHeader& header,
			BinaryIStream & data );

	// Handles request create a base from DB
	void createBaseFromDB( const Mercury::Address& srcAddr,
			const Mercury::UnpackedMessageHeader& header,
			BinaryIStream & data );

	void logOnAttempt( const Mercury::Address& srcAddr,
			const Mercury::UnpackedMessageHeader& header,
			BinaryIStream & data );

	void addGlobalBase( BinaryIStream & data );
	void delGlobalBase( BinaryIStream & data );

	void runScript( const Mercury::Address& srcAddr,
			const Mercury::UnpackedMessageHeader& header,
			BinaryIStream & data );

	void callWatcher( const Mercury::Address& srcAddr,
		const Mercury::UnpackedMessageHeader& header,
		BinaryIStream & data );

	void handleCellAppMgrBirth(
		const BaseAppIntInterface::handleCellAppMgrBirthArgs & args );
	void handleBaseAppMgrBirth(
		const BaseAppIntInterface::handleBaseAppMgrBirthArgs & args );

	void handleCellAppDeath( BinaryIStream & data );

	void emergencySetCurrentCell( const Mercury::Address & srcAddr,
		const Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void startup( const BaseAppIntInterface::startupArgs & args );

	// shut down this app nicely
	void shutDown( const BaseAppIntInterface::shutDownArgs & args );
	void controlledShutDown( const Mercury::Address& srcAddr,
		const Mercury::UnpackedMessageHeader& header,
		BinaryIStream & data );

	void setCreateBaseInfo( BinaryIStream & data );

	// Old-style BaseApp backup
	void old_setBackupBaseApp(
		const BaseAppIntInterface::old_setBackupBaseAppArgs & args );

	void old_startBaseAppBackup(
			const BaseAppIntInterface::old_startBaseAppBackupArgs & args );

	void old_stopBaseAppBackup(
			const BaseAppIntInterface::old_stopBaseAppBackupArgs & args );

	void old_backupBaseEntities( const Mercury::Address& srcAddr,
			const Mercury::UnpackedMessageHeader& header,
			BinaryIStream & data );

	void old_backupHeartbeat(
			const BaseAppIntInterface::old_backupHeartbeatArgs & args );

	void old_restoreBaseApp(
			const BaseAppIntInterface::old_restoreBaseAppArgs & args );

	// New-style BaseApp backup
	void startBaseEntitiesBackup(
			const BaseAppIntInterface::startBaseEntitiesBackupArgs & args );

	void stopBaseEntitiesBackup(
			const BaseAppIntInterface::stopBaseEntitiesBackupArgs & args );

	void backupBaseEntity( const Mercury::Address & srcAddr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void stopBaseEntityBackup( const Mercury::Address & srcAddr,
			const BaseAppIntInterface::stopBaseEntityBackupArgs & args );

	void handleBaseAppDeath( const Mercury::Address & srcAddr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void setBackupBaseApps( const Mercury::Address & srcAddr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	Mercury::Address backupAddrFor( EntityID entityID ) const
	{
		return backupHash_.addressFor( entityID );
	}

	// Shared Data message handlers
	void setSharedData( BinaryIStream & data );
	void delSharedData( BinaryIStream & data );

	// set the proxy to receive future messages
	void setClient( const BaseAppIntInterface::setClientArgs & args );

	/* External Interface */
	// let the proxy know who we really are
	void baseAppLogin( const Mercury::Address& srcAddr,
			const Mercury::UnpackedMessageHeader& header,
			BinaryIStream & data );

	// let the proxy know who we really are
	void authenticate( const Mercury::Address & srcAddr,
		const Mercury::UnpackedMessageHeader & header,
		const BaseAppExtInterface::authenticateArgs & args );

	/* C++ stuff */

	bool initScript();

	PyObject * createBaseRemotely( PyObject * args, PyObject * kwargs );
	PyObject * createBaseAnywhere( PyObject * args, PyObject * kwargs );
	PyObject * createBaseLocally( PyObject * args, PyObject * kwargs );
	PyObject * createBase( EntityType * pType, PyObject * pDict,
							PyObject * pCellData = NULL ) const;

	bool createBaseFromDB( const std::string& entityType,
					const std::string& name,
					PyObjectPtr pResultHandler );
	bool createBaseFromDB( const std::string& entityType, DatabaseID id,
					PyObjectPtr pResultHandler );
	PyObject* createRemoteBaseFromDB( const char * entityType,
					DatabaseID dbID,
					const char * name,
					const Mercury::Address* pDestAddr,
					PyObjectPtr pCallback,
					const char* origAPIFuncName );

	float getLoad() const						{ return load_; }

	Mercury::Nub & intNub()						{ return intNub_; }
	Mercury::Nub & extNub()						{ return extNub_; }

	static Mercury::Channel & getChannel( const Mercury::Address & addr )
	{
		return BaseApp::instance().intNub().findOrCreateChannel( addr );
	}

	BaseAppMgr & baseAppMgr()						{ return baseAppMgr_; }
	const BaseAppMgr & baseAppMgr() const			{ return baseAppMgr_; }

	const Mercury::Address & cellAppMgrAddr() const	{ return cellAppMgr_; }

	IDClient & idClient()							{ return idClient_; }

	DBMgr & dbMgr()						{ return *dbMgr_.pChannelOwner(); }

	SqliteDatabase* pSqliteDB() const			{ return pSqliteDB_; }
	void commitSecondaryDB( bool commit )		{ commitSecondaryDB_ = commit; }

	void addressDead( const Mercury::Address & address,	Mercury::Reason reason );

	void addBase( Base * pNewBase );
	void addProxy( Proxy * pNewProxy );

	void removeBase( Base * pToGo );
	void removeProxy( Proxy * pToGo );

	void impendingDataPresentLocally( uint32 version );
	bool allImpendingDataSent( int urgency );
	bool allReloadedByClients( int urgency );

	static void reloadResources( void * arg );
	void reloadResources();

	void setBaseForCall( Base * pBase );
	Base * getBaseForCall( bool okayIfNull = false );
	ProxyPtr getProxyForCall( bool okayIfNull = false );
	ProxyPtr clearProxyForCall();
	ProxyPtr getAndCheckProxyForCall( Mercury::UnpackedMessageHeader & header,
									  const Mercury::Address & srcAddr );
	bool versForCallIsOld() const	{ return versForCallIsOld_; }

	std::string pickle( PyObject * pObj ) const;
	PyObject * unpickle( const std::string & str ) const;

	Base * findBase( EntityID id ) const;

	typedef std::map<EntityID, Base *> Bases;
	const Bases & bases() const		{ return bases_; }

	CellEntityMailBoxPtr findMailBoxInSpace( SpaceID spaceID );

	TimeStamp time() const			{ return time_; }
	TimeQueue & timeQueue()			{ return timeQueue_; }
	int updateHertz() const			{ return updateHertz_; }
	float inactivityTimeout() const	{ return inactivityTimeout_; }
	int clientOverflowLimit() const	{ return clientOverflowLimit_; }
	int noSuchPortThreshold() const	{ return noSuchPortThreshold_; }
	int bytesToClientPerPacket() const	{ return bytesToClientPerPacket_; }

	bool old_isBackup() const			{ return old_pBackupHandler_ != NULL; }

	double gameTimeInSeconds() const { return double( time_ )/updateHertz_; }

	bool nextTickPending() const;

	// Overrides from TimerExpiryHandler
	virtual int handleTimeout( Mercury::TimerID id, void * arg );

	WorkerThread & workerThread()	{ return *pWorkerThread_; }

	GlobalBases * pGlobalBases() const	{ return pGlobalBases_; }

	bool shouldBackUpUndefinedProperties() const
				{ return shouldBackUpUndefinedProperties_; }

	bool shouldResolveMailBoxes() const
				{ return shouldResolveMailBoxes_; }

	bool useDefaultSpace() const		{ return useDefaultSpace_; }
	bool oldStyleBaseDestroy() const	{ return oldStyleBaseDestroy_; }
	bool warnOnNoDef() const			{ return warnOnNoDef_; }

	bool inShutDownPause() const
				{ return (shutDownTime_ != 0) && (time_ >= shutDownTime_); }

	bool hasStarted() const				{ return waitingFor_ == 0; }
	bool isShuttingDown() const			{ return shutDownTime_ != 0; }

	void startGameTickTimer();
	void ready( int component );
	void registerSecondaryDB();

	enum
	{
		READY_BASE_APP_MGR	= 0x1,
	};

	bool isRecentlyDeadCellApp( const Mercury::Address & addr ) const;

	const RateLimitConfig & extMsgFilterConfig() const
	{ return extMsgFilterConfig_; }


private:
	bool initSecondaryDB();

	int serveInterfaces();
	void addWatchers();
	void old_backup();
	void backup();
	void restartArchiveCycle();

	void checkSendWindowOverflows();

	void runScript( std::string script );

	PyObject * createBaseCommon( const Mercury::Address * pAddr,
		PyObject * args, PyObject * kwargs, bool hasCallback = true );

	BasePtr createBaseFromStream( BinaryIStream & data,
			Mercury::Address * pClientAddr = NULL,
			std::string * pEncryptionKey = NULL );

	bool addCreateBaseData( BinaryOStream & stream,
					EntityTypePtr pType, PyObjectPtr pDict,
					PyObject * pCallback, EntityID * pNewID = NULL );
	EntityTypePtr consolidateCreateBaseArgs( PyObjectPtr pDestDict,
			PyObject * args, int headOffset, int tailOffset ) const;

	bool changeIP( const char * ifname, const Mercury::Address & addr );

	void tickRateLimitFilters();
	void sendIdleProxyChannels();

	Mercury::Nub &		intNub_;
	Mercury::Nub		extNub_;

	// Note: Must be after intNub_ so that they're destroyed before the nub.
	BaseAppMgr			baseAppMgr_;
	Mercury::Address	cellAppMgr_;
	AnonymousChannelClient	dbMgr_;

	SqliteDatabase *	pSqliteDB_;
	bool				commitSecondaryDB_;

	BWTracer*			bwTracer_;

	typedef std::map<Mercury::Address, Proxy *> Proxies;
	Proxies proxies_;

	Bases bases_;

	BaseAppID	id_;

	BasePtr			baseForCall_;
	bool			baseForCallIsProxy_;
	bool			versForCallIsOld_;

	IDClient		idClient_;

	PythonServer *	pPythonServer_;

	SharedData *	pBaseAppData_;
	SharedData *	pGlobalData_;

	TimeStamp		time_;
	TimeStamp		commitTime_;
	TimeStamp		shutDownTime_;
	Mercury::ReplyID	shutDownReplyID_;
	TimeQueue		timeQueue_;
	int				updateHertz_;
	TimeKeeper *	pTimeKeeper_;
	Mercury::TimerID		gameTimerID_;
	uint64			reservedTickTime_;

	enum TimeOutType
	{
		TIMEOUT_GAME_TICK
	};

	WorkerThread *	pWorkerThread_;

	GlobalBases *	pGlobalBases_;

	// Old-style backup members
	class OldBackupHandler *	old_pBackupHandler_;
	Mercury::Address			old_backupAddr_;

	// New-style backup members
	BackupHash			backupHash_;	// The up-to-date hash
	BackupHash			newBackupHash_; // The hash we are trying to switch to.
	bool				isUsingNewBackup_;

	class BackedUpBaseApp
	{
	public:
		BackedUpBaseApp() : usingNew_( false ) {}

		void startNewBackup( uint32 index, const MiniBackupHash & hash );

		std::string & getDataFor( EntityID entityID )
		{
			if (usingNew_)
				return newBackup_.getDataFor( entityID );
			else
				return currentBackup_.getDataFor( entityID );
		}

		bool erase( EntityID entityID )
		{
			if (usingNew_)
				return newBackup_.erase( entityID );
			else
				return currentBackup_.erase( entityID );
		}

		void switchToNewBackup();
		void discardNewBackup();

		void restore();

	private:
		class BackedUpEntities
		{
		public:
			void init( uint32 index, const MiniBackupHash & hash,
					const BackedUpEntities & current );

			void swap( BackedUpEntities & other )
			{
				uint32 tempInt = index_;
				index_ = other.index_;
				other.index_ = tempInt;

				MiniBackupHash tempHash = hash_;
				hash_ = other.hash_;
				other.hash_ = tempHash;

				data_.swap( other.data_ );
			}

			std::string & getDataFor( EntityID entityID )
			{
				return data_[ entityID ];
			}

			bool erase( EntityID entityID )
			{
				return data_.erase( entityID );
			}

			void clear()
			{
				data_.clear();
			}

			void restore();

			bool empty() const	{ return data_.empty(); }

		private:
			uint32 index_;
			MiniBackupHash hash_;
			std::map< EntityID, std::string > data_;
		};

		BackedUpEntities currentBackup_; // Up-to-date backup.
		BackedUpEntities newBackup_;	// Backup we're trying to switch to.
		bool usingNew_;
	};

	// Misc
	Pickler *			pPickler_;

	Mercury::Address	deadBaseAppAddr_;

	typedef std::map< Mercury::Address, BackedUpBaseApp > BackedUpBaseApps;
	BackedUpBaseApps	backedUpBaseApps_;
	float				backupRemainder_;
	std::vector<EntityID> 	basesToBackUp_;

	// Used for which tick in archivePeriodInTicks_ we are up to.
	int					archiveIndex_;
	std::vector<EntityID> 	basesToArchive_;

	int					archivePeriodInTicks_;
	float				archiveEmergencyThreshold_;
	int					backupPeriodInTicks_;
	unsigned int		maxCommitPeriodInTicks_;
	bool                shouldBackUpUndefinedProperties_;
	bool                shouldResolveMailBoxes_;

	bool				useDefaultSpace_;
	bool				oldStyleBaseDestroy_;
	bool				warnOnNoDef_;

	bool				isBootstrap_;
	bool				isFromMachined_;
	int					waitingFor_;
	bool				isProduction_;

	float				load_;
	float				loadSmoothingBias_;

	int					numStartupRetries_;
	float				inactivityTimeout_;
	int					clientOverflowLimit_;
	int					noSuchPortThreshold_;
	int					bytesToClientPerPacket_;
	Mercury::Address	createBaseAnywhereAddr_;
	float				createBaseElsewhereThreshold_;

	/**
	 *  A class for keeping track of CellApps that have died recently.
	 */
	class DeadCellApp
	{
	public:
		DeadCellApp( const Mercury::Address & addr ) :
			addr_( addr ),
			timestamp_( timestamp() )
		{}

		/// How long we keep these things around for.  Basically the purpose of
		/// them is to ensure that emergencySetCurrentCell messages that are
		/// received after the handleCellAppDeath message cause an immediate
		/// restore.
		static const int MAX_AGE = 10; // seconds

		/// The address of the dead app
		Mercury::Address addr_;

		/// The time at which we found out about the app death
		uint64 timestamp_;
	};

	/// A collection of recently deceased CellApps.
	typedef std::vector< DeadCellApp > DeadCellApps;
	DeadCellApps deadCellApps_;

	// Statistics
	uint32	numLogins_;
	uint32	numLoginsAddrNAT_;
	uint32	numLoginsPortNAT_;
	uint32	numLoginsMultiAttempts_;
	uint32	maxLoginAttempts_;
	uint32	numLoginCollisions_;

	// Maximum downstream bitrate for stream*ToClient() stuff, in bytes/tick
	int					maxDownloadRate_;

	// Actual downstream bandwidth in use by all proxies, in bytes/tick
	int					curDownloadRate_;

	// Maximum downstream bitrate per client, in bytes/tick
	int				    maxClientDownloadRate_;

	// Maximum increase in download rate to a single client per tick, in bytes
	int					downloadRampUpRate_;

	// Unacked packet age at which we throttle downloads
	int					downloadBacklogLimit_;

	// Rate limiter message filter
	RateLimitConfig 	extMsgFilterConfig_;

public:

	int maxDownloadRate() const;
	int curDownloadRate() const;
	int maxClientDownloadRate() const;
	int downloadRampUpRate() const;
	int downloadBacklogLimit() const;
	float downloadScaleBack() const;
	void modifyDownloadRate( int delta );

	/**
	 *	This class is used to store information about entities that have logged
	 *	in via the LoginApp but still need to confirm this via the BaseApp.
	 */
	class PendingLogins
	{
	public:
		/**
		 *	This class stores information about an entity that has just logged
		 *	in via the LoginApp but still needs to confirm this via the BaseApp.
		 */
		class PendingLogin
		{
		public:
			// Should never be called but needed by std::map.
			PendingLogin() : pProxy_( NULL ), addrFromLoginApp_( 0, 0 )
			{
			}

			PendingLogin( Proxy * pProxy,
					const Mercury::Address & loginAppAddr ) :
				pProxy_( pProxy ),
				addrFromLoginApp_( loginAppAddr ) {}

			SmartPointer< Proxy > pProxy_;
			// The address the client used to connect to the loginApp.
			Mercury::Address addrFromLoginApp_;
		};
		typedef std::map< SessionKey, PendingLogin > Container;
		typedef Container::iterator iterator;

		iterator find( SessionKey key );
		iterator end();
		void erase( iterator iter );

		SessionKey add( Proxy * pProxy, const Mercury::Address & loginAppAddr );

		void tick();

	private:
		Container container_;

		/**
		 *	This class is used to keep information in a time-sorted queue. It
		 *	is used to identify proxies that have logged in via LoginApp but
		 *	were never confirmed directly with the BaseApp.
		 */
		class QueueElement
		{
		public:
			QueueElement( TimeStamp expiryTime,
					EntityID proxyID, SessionKey loginKey ) :
				expiryTime_( expiryTime ),
				proxyID_( proxyID ),
				loginKey_( loginKey )
			{
			}

			bool hasExpired( TimeStamp time ) const
			{
				return (time == expiryTime_);
			}

			EntityID proxyID() const	{ return proxyID_; }
			SessionKey loginKey() const	{ return loginKey_; }

		private:
			TimeStamp expiryTime_;
			EntityID proxyID_;
			SessionKey loginKey_;
		};

		typedef std::list< QueueElement > Queue;
		Queue queue_;
	};

private:
	PendingLogins pendingLogins_;
};


#ifdef CODE_INLINE
#include "baseapp.ipp"
#endif


#endif // BASEAPP_HPP
