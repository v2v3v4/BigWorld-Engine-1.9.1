/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef DATABASE_HPP
#define DATABASE_HPP

#include "cstdmf/md5.hpp"
#include "cstdmf/singleton.hpp"
#include "network/channel.hpp"
#include "resmgr/datasection.hpp"
#include "common/doc_watcher.hpp"
#include "server/backup_hash.hpp"

#include "db_interface.hpp"
#include "db_status.hpp"
#include "idatabase.hpp"
#include "worker_thread.hpp"
#include "signal_set.hpp"

#ifdef USE_ORACLE
#include "oracle_database.hpp"
#endif

#include <map>
#include <set>

class RelogonAttemptHandler;
class BWResource;
class EntityDefs;

namespace DBConfig
{
	class Server;
}

typedef Mercury::ChannelOwner BaseAppMgr;

/**
 *	This class is used to implement the main singleton object that represents
 *	this application.
 */
class Database : public Mercury::TimerExpiryHandler,
	public IDatabase::IGetBaseAppMgrInitDataHandler,
	public IDatabase::IUpdateSecondaryDBshandler,
	public Singleton< Database >
{
public:
	enum InitResult
	{
		InitResultSuccess,
		InitResultFailure,
		InitResultAutoShutdown
	};

public:

	Database( Mercury::Nub & nub );
	virtual ~Database();
	InitResult init( bool isUpgrade, bool isSyncTablesToDefs );
	void run();
	void finalise();

	void onSignalled( int sigNum );
	void onChildProcessExit( pid_t pid, int status );

	BaseAppMgr & baseAppMgr() { return baseAppMgr_; }

	// Overrides
	int handleTimeout( Mercury::TimerID id, void * arg );

	void handleStatusCheck( BinaryIStream & data );

	// Lifetime messages
	void handleBaseAppMgrBirth( DBInterface::handleBaseAppMgrBirthArgs & args );
	void handleDatabaseBirth( DBInterface::handleDatabaseBirthArgs & args );
	void shutDown( DBInterface::shutDownArgs & /*args*/ );
	void startSystemControlledShutdown();
	void shutDownNicely();
	void shutDown();

	void controlledShutDown( DBInterface::controlledShutDownArgs & args );
	void cellAppOverloadStatus( DBInterface::cellAppOverloadStatusArgs & args );

	// Entity messages
	void logOn( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void logOn( const Mercury::Address & srcAddr,
		Mercury::ReplyID replyID,
		LogOnParamsPtr pParams,
		const Mercury::Address & addrForProxy,
		bool offChannel );

	void onLogOnLoggedOnUser( EntityTypeID typeID, DatabaseID dbID,
		LogOnParamsPtr pParams,
		const Mercury::Address & proxyAddr, const Mercury::Address& replyAddr,
		bool offChannel, Mercury::ReplyID replyID,
		const EntityMailBoxRef* pExistingBase );

	void onEntityLogOff( EntityTypeID typeID, DatabaseID dbID );

	bool calculateOverloaded( bool isOverloaded );

	void sendFailure( Mercury::ReplyID replyID,
		const Mercury::Address & dstAddr,
		bool offChannel,
		DatabaseLoginStatus reason, const char * pDescription = NULL );

	void loadEntity( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void writeEntity( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void deleteEntity( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		DBInterface::deleteEntityArgs & args );
	void deleteEntityByName( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void lookupEntity( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		DBInterface::lookupEntityArgs & args );
	void lookupEntityByName( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );
	void lookupDBIDByName( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );
	void lookupEntity( EntityDBKey & ekey, BinaryOStream & bos );

	// Misc messages
	void executeRawCommand( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void putIDs( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void getIDs( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void writeSpaces( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void writeGameTime( DBInterface::writeGameTimeArgs & args );

	void handleBaseAppDeath( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void checkStatus( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void secondaryDBRegistration( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void updateSecondaryDBs( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );
	virtual void onUpdateSecondaryDBsComplete(
			const IDatabase::SecondaryDBEntries& removedEntries );

	void getSecondaryDBDetails( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header, BinaryIStream & data );

	// Accessors
	const EntityDefs& getEntityDefs() const
		{ return *pEntityDefs_; }
	EntityDefs& swapEntityDefs( EntityDefs& entityDefs )
	{
		EntityDefs& curDefs = *pEntityDefs_;
		pEntityDefs_ = &entityDefs;
		return curDefs;
	}
	DBConfig::Server& 			getServerConfig()	{ return *pServerConfig_; }

	Mercury::Nub & nub()			{ return nub_; }
	static Mercury::Nub & getNub()	{ return Database::instance().nub(); }

	static Mercury::Channel & getChannel( const Mercury::Address & addr )
	{
		return Database::instance().nub().findOrCreateChannel( addr );
	}

	WorkerThreadMgr& getWorkerThreadMgr()	{	return workerThreadMgr_;	}
	IDatabase& getIDatabase()	{ MF_ASSERT(pDatabase_); return *pDatabase_; }

	// IDatabase pass-through methods. Call these instead of IDatabase methods
	// so that we can intercept and muck around with stuff.
	/**
	 *	This class is meant to replace IDatabase::IGetEntityHandler as the base
	 * 	class for IDatabase::getEntity() callbacks. It allows us to muck around
	 * 	with the results before passing them on to the actual handler.
	 */
	class GetEntityHandler : public IDatabase::IGetEntityHandler
	{
	public:
		// Intercepts the result callback. Derived classes should NOT implement
		// this method.
		virtual void onGetEntityComplete( bool isOK );
		// Derived classes should implement this method instead of
		// onGetEntityComplete() - note the extra 'd'.
		virtual void onGetEntityCompleted( bool isOK ) = 0;
	};
	void getEntity( GetEntityHandler& handler );
	void putEntity( const EntityDBKey& ekey, EntityDBRecordIn& erec,
			IDatabase::IPutEntityHandler& handler );
	void delEntity( const EntityDBKey & ekey,
			IDatabase::IDelEntityHandler& handler );
	void setLoginMapping( const std::string & username,
			const std::string & password, const EntityDBKey& ekey,
			IDatabase::ISetLoginMappingHandler& handler );

	bool shouldLoadUnknown() const		{ return shouldLoadUnknown_; }
	bool shouldCreateUnknown() const	{ return shouldCreateUnknown_; }
	bool shouldRememberUnknown() const	{ return shouldRememberUnknown_; }

	bool clearRecoveryDataOnStartUp() const
										{ return clearRecoveryDataOnStartUp_; }

	// Watcher
	void hasStartBegun( bool hasStartBegun );
	bool hasStartBegun() const
	{
		return status_.status() > DBStatus::WAITING_FOR_APPS;
	}
	bool isConsolidating() const		{ return consolidatePid_ != 0; }

	bool isReady() const
	{
		return status_.status() >= DBStatus::WAITING_FOR_APPS;
	}

	void startServerBegin( bool isRecover = false );
	void startServerEnd( bool isRecover = false );
	void startServerError();

	// Sets baseRef to "pending base creation" state.
	static void setBaseRefToLoggingOn( EntityMailBoxRef& baseRef,
		EntityTypeID entityTypeID )
	{
		baseRef.init( 0, Mercury::Address( 0, 0 ),
			EntityMailBoxRef::BASE, entityTypeID );
	}
	// Checks that pBaseRef is fully checked out i.e. not in "pending base
	// creation" state.
	static bool isValidMailBox( const EntityMailBoxRef* pBaseRef )
	{	return (pBaseRef && pBaseRef->id != 0);	}

	bool defaultEntityToStrm( EntityTypeID typeID, const std::string& name,
		BinaryOStream& strm, const std::string* pPassword = 0 ) const;

	static DatabaseID* prepareCreateEntityBundle( EntityTypeID typeID,
		DatabaseID dbID, const Mercury::Address& addrForProxy,
		Mercury::ReplyMessageHandler* pHandler, Mercury::Bundle& bundle,
		LogOnParamsPtr pParams = NULL );

	RelogonAttemptHandler* getInProgRelogonAttempt( EntityTypeID typeID,
			DatabaseID dbID )
	{
		PendingAttempts::iterator iter =
				pendingAttempts_.find( EntityKey( typeID, dbID ) );
		return (iter != pendingAttempts_.end()) ? iter->second : NULL;
	}
	void onStartRelogonAttempt( EntityTypeID typeID, DatabaseID dbID,
		 	RelogonAttemptHandler* pHandler )
	{
		MF_VERIFY( pendingAttempts_.insert(
				PendingAttempts::value_type( EntityKey( typeID, dbID ),
						pHandler ) ).second );
	}
	void onCompleteRelogonAttempt( EntityTypeID typeID, DatabaseID dbID )
	{
		MF_VERIFY( pendingAttempts_.erase( EntityKey( typeID, dbID ) ) > 0 );
	}

	bool onStartEntityCheckout( const EntityKey& entityID )
	{
		return inProgCheckouts_.insert( entityID ).second;
	}
	bool onCompleteEntityCheckout( const EntityKey& entityID,
			const EntityMailBoxRef* pBaseRef );

	/**
	 *	This interface is used to receive the event that an entity has completed
	 *	checking out.
	 */
	struct ICheckoutCompletionListener
	{
		// This method is called when the onCompleteEntityCheckout() is
		// called for the entity that you've registered for via
		// registerCheckoutCompletionListener(). After this call, this callback
		// will be automatically deregistered.
		virtual void onCheckoutCompleted( const EntityMailBoxRef* pBaseRef ) = 0;
	};
	bool registerCheckoutCompletionListener( EntityTypeID typeID,
			DatabaseID dbID, ICheckoutCompletionListener& listener );

	bool hasMailboxRemapping() const	{ return !remappingDestAddrs_.empty(); }
	void remapMailbox( EntityMailBoxRef& mailbox ) const;

private:
	void endMailboxRemapping();

	void sendInitData();
	virtual void onGetBaseAppMgrInitDataComplete( TimeStamp gameTime,
			int32 appID );

	bool processSignals();

	// Data consolidation methods
	void consolidateData();
	bool startConsolidationProcess();
	void onConsolidateProcessEnd( bool isOK );
	bool sendRemoveDBCmd( uint32 destIP, const std::string& dbLocation );

	#ifdef DBMGR_SELFTEST
		void runSelfTest();
	#endif

	Mercury::Nub &		nub_;
	WorkerThreadMgr		workerThreadMgr_;
	EntityDefs*			pEntityDefs_;
	IDatabase*			pDatabase_;

	Signal::Set			signals_;

	DBStatus			status_;

	BaseAppMgr			baseAppMgr_;

	bool				shouldLoadUnknown_;
	bool 				shouldCreateUnknown_;
	bool				shouldRememberUnknown_;
	std::auto_ptr<DBConfig::Server> pServerConfig_;

	bool				allowEmptyDigest_;

	bool				shouldSendInitData_;

	bool				shouldConsolidate_;

	uint32				desiredBaseApps_;
	uint32				desiredCellApps_;

	Mercury::TimerID	statusCheckTimerId_;

	bool				clearRecoveryDataOnStartUp_;

	Mercury::Nub::TransientMiniTimer	writeEntityTimer_;

	friend class RelogonAttemptHandler;
	typedef std::map< EntityKey, RelogonAttemptHandler * > PendingAttempts;
	PendingAttempts pendingAttempts_;
	typedef std::set< EntityKey > EntityKeySet;
	EntityKeySet	inProgCheckouts_;

	typedef std::multimap< EntityKey, ICheckoutCompletionListener* >
			CheckoutCompletionListeners;
	CheckoutCompletionListeners checkoutCompletionListeners_;

	float				curLoad_;
	float				maxLoad_;
	bool				anyCellAppOverloaded_;
	uint64				allowOverloadPeriod_;
	uint64				overloadStartTime_;

	Mercury::Address	remappingSrcAddr_;
	BackupHash			remappingDestAddrs_;
	int					mailboxRemapCheckCount_;

	std::string			secondaryDBPrefix_;
	uint				secondaryDBIndex_;

	pid_t				consolidatePid_;

	bool				isProduction_;
};

#ifdef CODE_INLINE
#include "database.ipp"
#endif

#endif // DATABASE_HPP

// database.hpp
