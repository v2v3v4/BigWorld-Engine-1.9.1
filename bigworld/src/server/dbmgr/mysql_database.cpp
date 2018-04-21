/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "db_entitydefs.hpp"
#include "mysql_database.hpp"
#include "mysql_thread.hpp"
#include "entity_recoverer.hpp"
#include "mysql_typemapping.hpp"
#include "mysql_wrapper.hpp"
#include "mysql_notprepared.hpp"
#include "mysql_named_lock.hpp"
#include "database.hpp"
#include "db_interface_utils.hpp"
#include "db_config.hpp"

#include "baseappmgr/baseappmgr_interface.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/watcher.hpp"
#include "cstdmf/md5.hpp"
#include "server/bwconfig.hpp"

DECLARE_DEBUG_COMPONENT(0)

// -----------------------------------------------------------------------------
// Section: Utility classes
// -----------------------------------------------------------------------------

uint32 initInfoTable( MySql& connection )
{
	// Need additional detection of new database because pre-1.7 doesn't
	// have bigworldInfo table.
	std::vector<std::string> tableNames;
	connection.getTableNames( tableNames, "bigworldEntityTypes" );
	bool brandNewDb = (tableNames.size() == 0);

	MySqlTransaction transaction( connection );
	bool hasInfoTable = !brandNewDb;
	if (hasInfoTable)
	{
		// Double check that it really has bigworldInfo
		MySqlTableMetadata infoTableMetadata( connection, "bigworldInfo" );
		hasInfoTable = infoTableMetadata.isValid();
	}

	if (!hasInfoTable)
	{
#ifdef ENABLE_TABLE_SCHEMA_ALTERATIONS
		transaction.execute( "CREATE TABLE IF NOT EXISTS bigworldInfo "
				"(version INT UNSIGNED NOT NULL,snapshotTime TIMESTAMP NULL) "
				"ENGINE="MYSQL_ENGINE_TYPE );
#else
		throw std::runtime_error( "Cannot create bigworldInfo table because "
				"ENABLE_TABLE_SCHEMA_ALTERATIONS is not enabled" );
#endif
	}

	MySqlStatement	stmtGetVersion( connection,
									"SELECT version FROM bigworldInfo" );
	uint32			version = 0;
	MySqlBindings	b;
	b << version;
	stmtGetVersion.bindResult( b );

	transaction.execute( stmtGetVersion );
	if (stmtGetVersion.resultRows() > 0)
	{
		stmtGetVersion.fetch();
	}
	else
	{
		// If not new DB then must be old version of DB.
		version = brandNewDb ? DBMGR_CURRENT_VERSION : 0;
		std::stringstream ss;
		ss << "INSERT INTO bigworldInfo (version) VALUES (" <<
				version << ")";
		transaction.execute( ss.str() );
	}

	transaction.commit();

	return version;
}


// -----------------------------------------------------------------------------
// Section: class MySqlDatabase
// -----------------------------------------------------------------------------
MySqlDatabase::MySqlDatabase() :
	pThreadResPool_( 0 ),
	maxSpaceDataSize_( 2048 ),
	numConnections_( 5 ),
	numWriteSpaceOpsInProgress_( 0 ),
	reconnectTimerID_( Mercury::TIMER_ID_NONE ),
	reconnectCount_( 0 )
{
	MF_WATCH( "performance/numBusyThreads", *this,
				&MySqlDatabase::watcherGetNumBusyThreads );
	MF_WATCH( "performance/busyThreadsMaxElapsed", *this,
				&MySqlDatabase::watcherGetBusyThreadsMaxElapsedSecs );
	MF_WATCH( "performance/allOperations/rate", *this,
				&MySqlDatabase::watcherGetAllOpsCountPerSec );
	MF_WATCH( "performance/allOperations/duration", *this,
				&MySqlDatabase::watcherGetAllOpsAvgDurationSecs );
}

MySqlDatabase * MySqlDatabase::create()
{
	try
	{
		MySqlDatabase * pDatabase = new MySqlDatabase();
		return pDatabase;
	}
	catch (std::exception & e)
	{
		return NULL;
	}
}

MySqlDatabase::~MySqlDatabase()
{
}

bool MySqlDatabase::startup( const EntityDefs& entityDefs,
		bool isFaultRecovery, bool isUpgrade, bool isSyncTablesToDefsCmd )
{
	MF_ASSERT( !(isFaultRecovery && isUpgrade) );	// Can't do both at once.

	#ifdef USE_MYSQL_PREPARED_STATEMENTS
		INFO_MSG( "\tMySql: Compiled for prepared statements = True.\n" );
	#else
		INFO_MSG( "\tMySql: Compiled for prepared statements = False.\n" );
	#endif

	bool isSyncTablesToDefsCfg =
							BWConfig::get( "dbMgr/syncTablesToDefs", false );

	// Print out list of configured servers
	this->printConfigStatus();

	try
	{
		const DBConfig::Connection& connectionInfo =
				Database::instance().getServerConfig().getCurServer().connectionInfo;
		MySql connection( connectionInfo );

		// Lock the DB so another BigWorld process (like DBMgr or
		// consolidate_dbs) won't use it while we're using it.
		MySQL::NamedLock dbLock( connection,
				connectionInfo.generateLockName(), false );
		if (!dbLock.lock())
		{
			ERROR_MSG( "MySqlDatabase::startup: Database %s on %s:%d is being "
					"used by another BigWorld process\n",
					connectionInfo.database.c_str(),
					connectionInfo.host.c_str(),
					connectionInfo.port );
			return false;
		}

#ifndef ENABLE_TABLE_SCHEMA_ALTERATIONS
		if (isUpgrade)
		{
			ERROR_MSG( "MySqlDatabase::init: "
						"This build of DBMgr does not support the --upgrade "
						"option.\n"
						"Please rebuild DBMgr with "
						"ENABLE_TABLE_SCHEMA_ALTERATIONS enabled\n");
			return false;
		}

		if (isSyncTablesToDefsCmd)
		{
			ERROR_MSG( "MySqlDatabase::init: "
						"This build of DBMgr does not support the "
						"--sync-tables-to-defs option.\n"
						"Please rebuild DBMgr with "
						"ENABLE_TABLE_SCHEMA_ALTERATIONS enabled\n");
			return false;
		}

		if (isSyncTablesToDefsCfg)
		{
			ERROR_MSG( "MySqlDatabase::init: "
						"This build of DBMgr does not support the "
						"syncTablesToDefs option.\n"
						"Please disable the dbMgr/syncTablesToDefs "
						"configuration or rebuild DBMgr with "
						"ENABLE_TABLE_SCHEMA_ALTERATIONS enabled.\n");
			return false;
		}
#endif

		uint32 version = initInfoTable( connection );
		if (version < DBMGR_OLDEST_SUPPORTED_VERSION)
		{
			ERROR_MSG( "Cannot use database created by an ancient version "
					"of BigWorld\n" );
			return false;
		}
		else if ( (version < DBMGR_CURRENT_VERSION) && !isUpgrade )
		{
			ERROR_MSG( "Cannot use database from previous versions of BigWorld "
						"without upgrade\n" );
			INFO_MSG( "Database can be upgraded by running dbmgr --upgrade\n" );
			return false;
		}
		else if (version > DBMGR_CURRENT_VERSION)
		{
			ERROR_MSG( "Cannot use database from newer version of BigWorld\n" );
			return false;
		}
		else if ( (version == DBMGR_CURRENT_VERSION) && isUpgrade )
		{
			WARNING_MSG( "Database version is current, ignoring --upgrade option\n" );
		}

		maxSpaceDataSize_ = std::max( BWConfig::get( "dbMgr/maxSpaceDataSize",
										maxSpaceDataSize_ ), 1 );

		if (!isFaultRecovery)
		{
			if (!this->checkSpecialBigWorldTables( connection ))
			{
#ifdef ENABLE_TABLE_SCHEMA_ALTERATIONS
				this->createSpecialBigWorldTables( connection );
#else
				throw std::runtime_error( "BigWorld internal tables do not "
						"meet requirements. Please re-initialise tables with "
						"a DBMgr built with ENABLE_TABLE_SCHEMA_ALTERATIONS "
						"enabled" );
#endif
			}

			// Make sure we don't still have unconsolidated secondary
			// databases
			bool hasUnconsolidatedDBs =
					(this->getNumSecondaryDBs( connection ) > 0);
			if (hasUnconsolidatedDBs)
			{
				if (isSyncTablesToDefsCmd)
				{
					ERROR_MSG( "MySqlDatabase::startup: Cannot "
								"syncTablesToDefs when there are "
								"unconsolidated secondary databases\n" );
					return false;
				}

				isSyncTablesToDefsCfg = false;

				// __kyl__(14/8/2008) Ideally we should complete data
				// consolidation before trying to do syncTablesToDefs.
				// Unfortunately, we cannot initialise this object
				// without syncing tables to defs. And we cannot initialise
				// this object later on because of all the blocking
				// functions. Currently, it means that if people want to do
				// data consolidation then followed by syncTablesToDefs,
				// then start the system, they need to run DBMgr twice.
				// The first run will fail due to inability to
				// syncTablesToDefs.
			}

			bool shouldSyncTablesToDefs =
					isSyncTablesToDefsCmd || isSyncTablesToDefsCfg || isUpgrade;
			bool isEntityTablesInSync =
					initEntityTables( connection, entityDefs, version,
							shouldSyncTablesToDefs );

			if (!isEntityTablesInSync)
			{
				if (hasUnconsolidatedDBs)
				{
					ERROR_MSG( "MySqlDatabase::startup: "
								"Entity definitions were changed while there "
								"are unconsolidated secondary databases.\n"
								"Please revert changes to entity definitions "
								"and run the data consolidation tool.\n"
								"Alternatively, run \"consolidate_dbs --clear\""
								" to allow the server to run without doing "
								"data consolidation. Unconsolidated data "
								"will be lost.\n" );
				}
				else
				{
					MF_ASSERT( !isSyncTablesToDefsCmd );
					ERROR_MSG( "MySqlDatabase::startup: "
								"Tables not in sync with entity definitions.\n"
								"Please run dbmgr with --sync-tables-to-defs "
								"option to update tables\n" );
				}
				return false;
			}

			this->initSpecialBigWorldTables( connection, entityDefs );

			if (Database::instance().clearRecoveryDataOnStartUp())
			{
				MySqlTransaction t( connection );

				t.execute( "DELETE FROM bigworldLogOns" );

				t.execute( "DELETE FROM bigworldSpaces" );
				t.execute( "DELETE FROM bigworldSpaceData" );

				t.execute( "UPDATE bigworldGameTime SET time=0" );

				t.commit();
			}
		}

		numConnections_ = std::max( BWConfig::get( "dbMgr/numConnections",
													numConnections_ ), 1 );

		INFO_MSG( "\tMySql: Number of connections = %d.\n", numConnections_ );

		// Release lock on DB because MySqlThreadResPool tries to acquire it.
		MF_VERIFY( dbLock.unlock() );

		// Create threads and thread resources.
		pThreadResPool_ =
			new MySqlThreadResPool( Database::instance().getWorkerThreadMgr(),
				Database::instance().nub(),
				numConnections_, maxSpaceDataSize_, connectionInfo, entityDefs );
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "MySqlDatabase::startup: %s\n", e.what() );
		return false;
	}

	return true;
}

bool MySqlDatabase::shutDown()
{
	try
	{
		delete pThreadResPool_;
		pThreadResPool_ = NULL;

		if ( reconnectTimerID_ != Mercury::TIMER_ID_NONE )
		{
			Database::instance().nub().cancelTimer( reconnectTimerID_ );
			reconnectTimerID_ = Mercury::TIMER_ID_NONE;
		}

		return true;
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "MySqlDatabase::shutDown: %s\n", e.what() );
		return false;
	}
}

/**
 *	Tests the database connection and prints out the status.
 */
void MySqlDatabase::printConfigStatus()
{
	INFO_MSG( "\tMySql: Configured MySQL servers:\n" );
	DBConfig::Server& config = this->getServerConfig();
	// Test connection to database and all its backups.
	do
	{
		const DBConfig::Server::ServerInfo& serverInfo = config.getCurServer();
		// Test connection to server
		const char * failedString;
		try
		{
			MySql connection( serverInfo.connectionInfo );
			failedString = "";

			if (serverInfo.connectionInfo.password.length() == 0)
			{
				WARNING_MSG( "Connection to MySQL database '%s:%d/%s' has no "
						"password specified. This is a potential security "
						"risk.\n",
						serverInfo.connectionInfo.host.c_str(),
						serverInfo.connectionInfo.port,
						serverInfo.connectionInfo.database.c_str() );
			}
		}
		catch (std::exception& e)
		{
			failedString = " - FAILED!";
		}
		INFO_MSG( "\t\t%s: %s:%d (%s)%s\n",
					serverInfo.configName.c_str(),
					serverInfo.connectionInfo.host.c_str(),
					serverInfo.connectionInfo.port,
//					(serverInfo.connectionInfo.host + ";" +
//					serverInfo.connectionInfo.username + ";" +
//					serverInfo.connectionInfo.password).c_str(),
					serverInfo.connectionInfo.database.c_str(),
					failedString );
	} while (config.gotoNextServer());
}

void printSection( const char * msg, DataSectionPtr pSection )
{
	BinaryPtr pBinary = pSection->asBinary();
	DEBUG_MSG( "printSection: %s\n", msg );
	size_t len = pBinary->len();
	const char * data = static_cast<const char *>( pBinary->data() );
	for ( size_t i=0; i<len; ++i )
	{
		putchar( data[i] );
	}
	puts("");
}

inline MySqlThreadData& MySqlDatabase::getMainThreadData()
{
	return pThreadResPool_->getMainThreadData();
}


inline DBConfig::Server& MySqlDatabase::getServerConfig()
{
	return Database::instance().getServerConfig();
}

/**
 *	This method is called when one of our connections to the database drops out.
 * 	We assume that if one drops out, then all are in trouble.
 */
void MySqlDatabase::onConnectionFatalError()
{
	if (!this->hasFatalConnectionError())
	{
		if (this->getServerConfig().getNumServers() == 1)
		{
			// Poll every second.
			reconnectTimerID_ =
				Database::instance().nub().registerTimer( 1000000, this );
		}
		else
		{
			// Switch servers straight away.
			reconnectTimerID_ =
				Database::instance().nub().registerTimer( 1, this );
		}
		reconnectCount_ = 0;
	}
}

/**
 *	This method is attempts to restore all our connections to the database.
 */
bool MySqlDatabase::restoreConnectionToDb()
{
	MF_ASSERT( this->hasFatalConnectionError() );

	++reconnectCount_;

	DBConfig::Server& config = this->getServerConfig();
	bool isSuccessful;
	if (config.getNumServers() == 1)
	{
		// Try to reconnect to the same server. Quickly test if it's worthwhile
		// reconnecting.
		isSuccessful = this->getMainThreadData().connection.ping();
	}
	else
	{
		config.gotoNextServer();
		// Assume it's OK to connect to it. If it's down then we'd have done
		// a whole lot of useless work but hopefully it's a rare case.
		isSuccessful = true;

		if (reconnectCount_ == config.getNumServers())
		{
			// Go back to polling every second.
			Database::instance().nub().cancelTimer( reconnectTimerID_ );
			reconnectTimerID_ =
				Database::instance().nub().registerTimer( 1000000, this );
		}
	}

	if (isSuccessful)
	{
		MySqlThreadResPool* pOldThreadResPool = pThreadResPool_;
		// Wait for all tasks to finish because we are about to swap the global
		// thread resource pool. Tasks generally assume that this doesn't
		// change while they are executing.
		pOldThreadResPool->threadPool().waitForAllTasks();
		const DBConfig::Server::ServerInfo& curServer =
				this->getServerConfig().getCurServer();
		try
		{
			pThreadResPool_ =
				new MySqlThreadResPool( Database::instance().getWorkerThreadMgr(),
					Database::instance().nub(),
					numConnections_, maxSpaceDataSize_,
					curServer.connectionInfo,
					Database::instance().getEntityDefs(),
					pOldThreadResPool->isDBLocked() );

			if (isSuccessful)
			{
				Database::instance().nub().cancelTimer( reconnectTimerID_ );
				reconnectTimerID_ = Mercury::TIMER_ID_NONE;

				delete pOldThreadResPool;

				INFO_MSG( "MySqlDatabase: %s - Reconnected to database\n",
						 curServer.configName.c_str() );
			}
			else
			{
				delete pThreadResPool_;
				pThreadResPool_ = pOldThreadResPool;
			}
		}
		catch (std::exception& e)
		{
			ERROR_MSG( "MySqlDatabase::restoreConnectionToDb: %s - %s\n",
						curServer.configName.c_str(),
						e.what() );

			if (pThreadResPool_ != pOldThreadResPool)
				delete pThreadResPool_;
			pThreadResPool_ = pOldThreadResPool;

			isSuccessful = false;
		}
	}

	return isSuccessful;
}

/**
 *	Timer callback.
 */
int MySqlDatabase::handleTimeout( Mercury::TimerID id, void * arg )
{
	this->restoreConnectionToDb();

	return 0;
}

/**
 *	Watcher interface. Gets the number of threads currently busy.
 */
uint MySqlDatabase::watcherGetNumBusyThreads() const
{
	return (pThreadResPool_) ?
				(uint) pThreadResPool_->threadPool().getNumBusyThreads() : 0;
}

/**
 *	Watcher interface. For busy threads, get the duration of the thread that has
 *  been running the longest (in seconds).
 */
double MySqlDatabase::watcherGetBusyThreadsMaxElapsedSecs() const
{
	return (pThreadResPool_) ?
				pThreadResPool_->getBusyThreadsMaxElapsedSecs() : 0;
}

/**
 *	Watcher interface. Get the number of operations per second.
 */
double MySqlDatabase::watcherGetAllOpsCountPerSec() const
{
	return (pThreadResPool_) ?
				pThreadResPool_->getOpCountPerSec() : 0;
}

/**
 *	Watcher interface. Get the average duration of operations (in seconds).
 */
double MySqlDatabase::watcherGetAllOpsAvgDurationSecs() const
{
	return (pThreadResPool_) ? pThreadResPool_->getAvgOpDuration() : 0;
}


// -----------------------------------------------------------------------------
// Section: class MapLoginToEntityDBKeyTask
// -----------------------------------------------------------------------------
/**
 *	This class encapsulates a mapLoginToEntityDBKey() operation so that it can
 *	be executed in a separate thread.
 */
class MapLoginToEntityDBKeyTask : public MySqlThreadTask
{
	std::string			logOnName_;
	std::string			password_;
	DatabaseLoginStatus	loginStatus_;
	IDatabase::IMapLoginToEntityDBKeyHandler& handler_;

public:
	MapLoginToEntityDBKeyTask( MySqlDatabase& owner,
		const std::string& logOnName, const std::string& password,
		IDatabase::IMapLoginToEntityDBKeyHandler& handler );

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	Constructor. Stores new entity information in MySQL bindings.
 */
MapLoginToEntityDBKeyTask::MapLoginToEntityDBKeyTask( MySqlDatabase& owner,
	const std::string& logOnName, const std::string& password,
	IDatabase::IMapLoginToEntityDBKeyHandler& handler )
	: MySqlThreadTask(owner), logOnName_(logOnName), password_(password),
	loginStatus_( DatabaseLoginStatus::LOGGED_ON ), handler_(handler)
{
	this->startThreadTaskTiming();

	MySqlThreadData& threadData = this->getThreadData();
	threadData.ekey.typeID = 0;
	threadData.ekey.dbID = 0;
	threadData.ekey.name.clear();
	threadData.exceptionStr.clear();
}

/**
 *	This method writes the new default entity into the database.
 *	May be run in another thread.
 */
void MapLoginToEntityDBKeyTask::run()
{
	bool retry;
	do
	{
		retry = false;
		MySqlThreadData& threadData = this->getThreadData();
		try
		{
			MySqlTransaction transaction( threadData.connection );
			std::string actualPassword;
			bool entryExists = threadData.typeMapping.getLogOnMapping( transaction,
				logOnName_, actualPassword, threadData.ekey.typeID,
				threadData.ekey.name );
			if (entryExists)
			{
				if (!actualPassword.empty() && password_ != actualPassword)
					loginStatus_ = DatabaseLoginStatus::LOGIN_REJECTED_INVALID_PASSWORD;
			}
			else
			{
				loginStatus_ = DatabaseLoginStatus::LOGIN_REJECTED_NO_SUCH_USER;
			}
			transaction.commit();
		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
		}
		catch (std::exception& e)
		{
			threadData.exceptionStr = e.what();
			loginStatus_ = DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE;
		}
	} while (retry);
}

/**
 *	This method is called in the main thread after run() is complete.
 */
void MapLoginToEntityDBKeyTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
	{
		ERROR_MSG( "MySqlDatabase::mapLoginToEntityDBKey: %s\n",
		           threadData.exceptionStr.c_str() );
	}
	else if (threadData.connection.hasFatalError())
	{
		loginStatus_ = DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE;
	}

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "MapLoginToEntityDBKeyTask for '%s' took %f seconds\n",
					logOnName_.c_str(), double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	DatabaseLoginStatus	loginStatus = loginStatus_;
	EntityDBKey			ekey = threadData.ekey;
	IDatabase::IMapLoginToEntityDBKeyHandler& handler = handler_;
	delete this;

	handler.onMapLoginToEntityDBKeyComplete( loginStatus, ekey );
}


/**
 *	IDatabase override
 */
void MySqlDatabase::mapLoginToEntityDBKey(
		const std::string & logOnName,
		const std::string & password,
		IDatabase::IMapLoginToEntityDBKeyHandler& handler )
{
	MapLoginToEntityDBKeyTask* pTask =
		new MapLoginToEntityDBKeyTask( *this, logOnName, password, handler );
	pTask->doTask();
}

/**
 *	This class encapsulates the setLoginMapping() operation so that it can be
 *	executed in a separate thread.
 */
class SetLoginMappingTask : public MySqlThreadTask
{
	IDatabase::ISetLoginMappingHandler& handler_;

public:
	SetLoginMappingTask( MySqlDatabase& owner, const std::string& username,
		const std::string & password, const EntityDBKey& ekey,
		IDatabase::ISetLoginMappingHandler& handler )
		: MySqlThreadTask(owner), handler_(handler)
	{
		this->startThreadTaskTiming();

		MySqlThreadData& threadData = this->getThreadData();

		threadData.typeMapping.logOnMappingToBound( username, password,
			ekey.typeID, ekey.name );

		threadData.exceptionStr.clear();
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method writes the log on mapping into the table.
 *	May be run in another thread.
 */
void SetLoginMappingTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();
	bool retry;
	do
	{
		retry = false;
		try
		{
			MySqlTransaction transaction( threadData.connection );
			threadData.typeMapping.setLogOnMapping( transaction );

			transaction.commit();
		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
		}
		catch (std::exception& e)
		{
			threadData.exceptionStr = e.what();
		}
	} while (retry);
}

/**
 *	This method is called in the main thread after run() is complete.
 */
void SetLoginMappingTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
	{
		// __kyl__ (13/7/2005) At the moment there is no good reason
		// why this operation should fail. Possibly something disasterous
		// like MySQL going away.
		ERROR_MSG( "MySqlDatabase::setLoginMapping: %s\n",
		           threadData.exceptionStr.c_str() );
	}

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "SetLoginMappingTask for '%s' took %f seconds\n",
					threadData.typeMapping.getBoundLogOnName().c_str(),
					double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	IDatabase::ISetLoginMappingHandler& handler = handler_;
	delete this;

	handler.onSetLoginMappingComplete();
}

/**
 *	IDatabase override
 */
void MySqlDatabase::setLoginMapping( const std::string& username,
	const std::string& password, const EntityDBKey& ekey,
	IDatabase::ISetLoginMappingHandler& handler )
{
	SetLoginMappingTask* pTask =
		new SetLoginMappingTask( *this, username, password, ekey, handler );
	pTask->doTask();
}

/**
 *	This class encapsulates the MySqlDatabase::getEntity() operation so that
 *	it can be executed in a separate thread.
 */
class GetEntityTask : public MySqlThreadTask
{
	IDatabase::IGetEntityHandler& handler_;

public:
	GetEntityTask( MySqlDatabase& owner,
		IDatabase::IGetEntityHandler& handler );

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();

private:
	bool fillKey( MySqlTransaction& transaction, EntityDBKey& ekey );
};

/**
 *	Constructor.
 */
GetEntityTask::GetEntityTask( MySqlDatabase& owner,
		IDatabase::IGetEntityHandler& handler )
	: MySqlThreadTask( owner ), handler_( handler )
{
	this->startThreadTaskTiming();
}

/**
 *	This method reads the entity data into the MySQL bindings. May be executed
 *	in a separate thread.
 */
void GetEntityTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();
	bool isOK = true;
	threadData.exceptionStr.clear();
	try
	{
		MySqlTransaction	transaction( threadData.connection );
		MySqlTypeMapping&	typeMapping = threadData.typeMapping;
		EntityDBKey&		ekey = handler_.key();
		EntityDBRecordOut&	erec = handler_.outrec();
		bool				definitelyExists = false;
		if (erec.isStrmProvided())
		{	// Get entity props
			definitelyExists = typeMapping.getEntityToBound( transaction, ekey );
			isOK = definitelyExists;
		}

		if (isOK && erec.isBaseMBProvided() && erec.getBaseMB())
		{	// Need to get base mail box
			if (!definitelyExists)
				isOK = this->fillKey( transaction, ekey );

			if (isOK)
			{	// Try to get base mailbox
				definitelyExists = true;
				if (!typeMapping.getLogOnRecord( transaction, ekey.typeID,
					ekey.dbID, *erec.getBaseMB() ) )
					erec.setBaseMB( 0 );

			}
		}

		if (isOK && !definitelyExists)
		{	// Caller hasn't asked for anything except the missing member of
			// ekey.
			isOK = this->fillKey( transaction, ekey );
		}
		transaction.commit();
	}
	catch (std::exception& e)
	{
		threadData.exceptionStr = e.what();
		isOK = false;
	}

	threadData.isOK = isOK;
}

/**
 *	This method is called in the main thread after run() is complete.
 */
void GetEntityTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length() > 0)
		ERROR_MSG( "MySqlDatabase::getEntity: %s\n",
			threadData.exceptionStr.c_str() );
	else if (threadData.connection.hasFatalError())
		threadData.isOK = false;

	if ( threadData.isOK )
	{
		EntityDBRecordOut&	erec = handler_.outrec();
		if (erec.isStrmProvided())
		{
			EntityDBKey& ekey = handler_.key();
			// NOTE: boundToStream() shouldn't be run in a separate thread
			// because is uses Python do to some operations.
			threadData.typeMapping.boundToStream( ekey.typeID, erec.getStrm(),
				handler_.getPasswordOverride() );
		}
	}

	const EntityDBKey& ekey = handler_.key();
	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "GetEntityTask for entity %"FMT_DBID" of type %d "
					"named '%s' took %f seconds\n",
					ekey.dbID, ekey.typeID, ekey.name.c_str(),
					double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	IDatabase::IGetEntityHandler& handler = handler_;
	bool isOK = threadData.isOK;
	delete this;

	handler.onGetEntityComplete( isOK );
}

/**
 *	This method set the missing member of the EntityDBKey. If entity doesn't
 *	have a name property then ekey.name is set to empty.
 *
 *	This method may be called from another thread.
 *
 *	@return	True if successful. False if entity doesn't exist.
 */
bool GetEntityTask::fillKey( MySqlTransaction& transaction,
	EntityDBKey& ekey )
{
	MySqlTypeMapping& typeMapping = this->getThreadData().typeMapping;
	bool isOK;
	if (typeMapping.hasNameProp( ekey.typeID ))
	{
		if (ekey.dbID)
		{
			isOK = typeMapping.getEntityName( transaction, ekey.typeID,
				ekey.dbID, ekey.name );
		}
		else
		{
			ekey.dbID = typeMapping.getEntityDbID( transaction,
						ekey.typeID, ekey.name );
			isOK = ekey.dbID != 0;
		}
	}
	else
	{	// Entity doesn't have a name property. Check for entity existence if
		// DBID is provided
		if (ekey.dbID)
		{
			isOK = typeMapping.checkEntityExists( transaction, ekey.typeID,
				ekey.dbID );
			ekey.name.clear();
		}
		else
		{
			isOK = false;
		}
	}

	return isOK;
}

/**
 *	Override from IDatabase
 */
void MySqlDatabase::getEntity( IDatabase::IGetEntityHandler& handler )
{
	GetEntityTask*	pGetEntityTask = new GetEntityTask( *this, handler );
	pGetEntityTask->doTask();
}

/**
 *	This class encapsulates the MySqlDatabase::putEntity() operation so that
 *	it can be executed in a separate thread.
 */
class PutEntityTask : public MySqlThreadTask
{
	enum BaseRefAction
	{
		BaseRefActionNone,
		BaseRefActionWrite,
		BaseRefActionRemove
	};

	bool							writeEntityData_;
	BaseRefAction					baseRefAction_;
	IDatabase::IPutEntityHandler&	handler_;

public:
	PutEntityTask( MySqlDatabase& owner,
				   const EntityDBKey& ekey, EntityDBRecordIn& erec,
				   IDatabase::IPutEntityHandler& handler );

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	Constructor. Stores all required information from ekey and erec so that
 *	operation can be executed in a separate thread.
 */
PutEntityTask::PutEntityTask( MySqlDatabase& owner,
							  const EntityDBKey& ekey,
							  EntityDBRecordIn& erec,
							  IDatabase::IPutEntityHandler& handler )
	: MySqlThreadTask( owner ), writeEntityData_( false ),
	baseRefAction_( BaseRefActionNone ), handler_( handler )
{
	this->startThreadTaskTiming();

	MySqlThreadData& threadData = this->getThreadData();

	threadData.ekey = ekey;
	threadData.isOK = true;
	threadData.exceptionStr.clear();

	// Store entity data inside bindings, ready to be put into the database.
	if (erec.isStrmProvided())
	{
		threadData.typeMapping.streamToBound( ekey.typeID, ekey.dbID,
												erec.getStrm() );
		writeEntityData_ = true;
	}

	if (erec.isBaseMBProvided())
	{
		EntityMailBoxRef* pBaseMB = erec.getBaseMB();
		if (pBaseMB)
		{
			threadData.typeMapping.baseRefToBound( *pBaseMB );
			baseRefAction_ =  BaseRefActionWrite;
		}
		else
		{
			baseRefAction_ = BaseRefActionRemove;
		}
	}
}

/**
 *	This method writes the entity data into the database. May be executed in
 *	a separate thread.
 */
void PutEntityTask::run()
{
	MySqlThreadData& 	threadData = this->getThreadData();
	bool 				retry;
	do
	{
		retry = false;
		try
		{
			DatabaseID			dbID = threadData.ekey.dbID;
			EntityTypeID		typeID = threadData.ekey.typeID;
			bool				isOK = threadData.isOK;
			MySqlTransaction	transaction( threadData.connection );
			bool				definitelyExists = false;

			if (writeEntityData_)
			{
				if (dbID)
				{
					isOK = threadData.typeMapping.updateEntity( transaction,
																typeID );
				}
				else
				{
					dbID = threadData.typeMapping.newEntity( transaction, typeID );
					isOK = (dbID != 0);
				}

				definitelyExists = isOK;
			}

			if (isOK && baseRefAction_ != BaseRefActionNone)
			{
				if (!definitelyExists)
				{	// Check for existence to prevent adding crap LogOn records
					isOK = threadData.typeMapping.checkEntityExists( transaction,
								typeID, dbID );
				}

				if (isOK)
				{
					if (baseRefAction_ == BaseRefActionWrite)
					{
						// Add or update the log on record.
						threadData.typeMapping.addLogOnRecord( transaction,
								typeID, dbID );
					}
					else
					{	// Try to set BaseRef to "NULL" by removing the record
						threadData.typeMapping.removeLogOnRecord( transaction,
								typeID, dbID );
						if (transaction.affectedRows() == 0)
						{
							// Not really an error. If it doesn't exist then
							// it is effectively "NULL" already. Want to print
							// out a warning but no easy way to to that.
							// So doing something a little naughty and setting
							// exception string but leaving isOK as true.
							threadData.exceptionStr = "Failed to remove logon record";
						}
					}
				}
			}

			transaction.commit();

			threadData.ekey.dbID = dbID;
			threadData.isOK = isOK;

		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
		}
		catch (std::exception& e)
		{
			threadData.exceptionStr = e.what();
			threadData.isOK = false;
		}
	} while (retry);
}

/**
 *	This method is called in the main thread after run() is complete.
 */
void PutEntityTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		ERROR_MSG( "MySqlDatabase::putEntity: %s\n", threadData.exceptionStr.c_str() );
	else if (threadData.connection.hasFatalError())
		threadData.isOK = false;
	else if (!threadData.isOK)
		WARNING_MSG( "MySqlDatabase::putEntity: Failed to write entity %"FMT_DBID
					" of type %d into MySQL database.\n",
					threadData.ekey.dbID, threadData.ekey.typeID  );

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "PutEntityTask for entity %"FMT_DBID" of type %d "
					"took %f seconds\n",
					threadData.ekey.dbID, threadData.ekey.typeID,
					double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	bool isOK = threadData.isOK;
	DatabaseID dbID = threadData.ekey.dbID;
	IDatabase::IPutEntityHandler& handler = handler_;
	delete this;

	handler.onPutEntityComplete( isOK, dbID );
}

/**
 *	Override from IDatabase
 */
void MySqlDatabase::putEntity( const EntityDBKey& ekey, EntityDBRecordIn& erec,
							   IPutEntityHandler& handler )
{
	MF_ASSERT( erec.isStrmProvided() || erec.isBaseMBProvided() );

	PutEntityTask* pTask =
		new PutEntityTask( *this, ekey, erec, handler );
	pTask->doTask();
}

/**
 *	This class encapsulates the MySqlDatabase::delEntity() operation so that
 *	it can be executed in a separate thread.
 */
class DelEntityTask : public MySqlThreadTask
{
	IDatabase::IDelEntityHandler&	handler_;

public:
	DelEntityTask( MySqlDatabase& owner, const EntityDBKey& ekey,
		           IDatabase::IDelEntityHandler& handler );

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	Constructor.
 */
DelEntityTask::DelEntityTask( MySqlDatabase& owner, const EntityDBKey& ekey,
		IDatabase::IDelEntityHandler& handler )
	: MySqlThreadTask(owner), handler_(handler)
{
	this->startThreadTaskTiming();

	MySqlThreadData& threadData = this->getThreadData();

	threadData.ekey = ekey;
	threadData.isOK = true;
	threadData.exceptionStr.clear();
}

/**
 *	This method deletes an entity from the database. May be executed in a
 *	separate thread.
 */
void DelEntityTask::run()
{
	MySqlThreadData& 	threadData = this->getThreadData();
	bool 				retry;
	do
	{
		retry = false;
		EntityDBKey&		ekey = threadData.ekey;
		MySqlTypeMapping&	typeMapping = threadData.typeMapping;
		try
		{
			MySqlTransaction transaction( threadData.connection );

			if (ekey.dbID == 0)
			{
				ekey.dbID = threadData.typeMapping.getEntityDbID( transaction,
					ekey.typeID, ekey.name );
			}

			if (ekey.dbID)
			{
				if (typeMapping.deleteEntityWithID( transaction, ekey.typeID,
					ekey.dbID ))
				{
					typeMapping.removeLogOnRecord( transaction, ekey.typeID, ekey.dbID );
				}
				else
				{
					threadData.isOK = false;
				}
				transaction.commit();
			}
			else
			{
				threadData.isOK = false;
			}
		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
		}
		catch (std::exception& e)
		{
			threadData.exceptionStr = e.what();
			threadData.isOK = false;
		}
	} while (retry);
}

/**
 *	This method is called in the main thread after run() completes.
 */
void DelEntityTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();

	if (threadData.exceptionStr.length())
		ERROR_MSG( "MySqlDatabase::delEntity: %s\n", threadData.exceptionStr.c_str() );
	else if (threadData.connection.hasFatalError())
		threadData.isOK = false;

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "DelEntityTask for entity %"FMT_DBID" of type %d "
					"took %f seconds\n",
					threadData.ekey.dbID, threadData.ekey.typeID,
					double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	bool isOK = threadData.isOK;
	IDatabase::IDelEntityHandler& handler = handler_;
	delete this;

	handler.onDelEntityComplete(isOK);
}

/**
 *	IDatabase override
 */
void MySqlDatabase::delEntity( const EntityDBKey & ekey,
	IDatabase::IDelEntityHandler& handler )
{
	DelEntityTask* pTask =
			new DelEntityTask( *this, ekey, handler );
	pTask->doTask();
}

/**
 *	This class encapsulates the MySqlDatabase::executeRawCommand() operation
 *	so that it can be executed in a separate thread.
 */
class ExecuteRawCommandTask : public MySqlThreadTask
{
	std::string									command_;
	IDatabase::IExecuteRawCommandHandler&		handler_;

public:
	ExecuteRawCommandTask( MySqlDatabase& owner, const std::string& command,
		IDatabase::IExecuteRawCommandHandler& handler )
		: MySqlThreadTask(owner), command_(command), handler_(handler)
	{
		this->startThreadTaskTiming();

		MySqlThreadData& threadData = this->getThreadData();

		threadData.isOK = true;
		threadData.exceptionStr.clear();
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method executes a raw database command. May be executed in a
 *	separate thread.
 */
void ExecuteRawCommandTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();

	bool retry;
	do
	{
		retry = false;

		int errorNum;
		MySqlTransaction transaction( threadData.connection, errorNum );
		if (errorNum == 0)
		{
			errorNum = transaction.query( command_ );
			if (errorNum == 0)
			{
				MYSQL_RES* pResult = transaction.storeResult();
				if (pResult)
				{
					MySqlResult result( *pResult );
					BinaryOStream& stream = handler_.response();
					stream << std::string();	// no error.
					stream << uint32( result.getNumFields() );
					stream << uint32( result.getNumRows() );
					while ( result.getNextRow() )
					{
						for ( uint i = 0; i < result.getNumFields(); ++i )
						{
							DBInterfaceUtils::addPotentialNullBlobToStream(
									stream, DBInterfaceUtils::Blob(
										result.getField( i ),
										result.getFieldLen( i ) ) );
						}
					}
				}
				else
				{
					errorNum = transaction.getLastErrorNum();
					if (errorNum == 0)
					{
						// Result is empty. Return affected rows instead.
						BinaryOStream& stream = handler_.response();
						stream << std::string();	// no error.
						stream << int32( 0 ); 		// no fields.
						stream << uint64( transaction.affectedRows() );
					}
				}
			}
		}

		if (errorNum != 0)
		{
			if (transaction.shouldRetry())
			{
				retry = true;
			}
			else
			{
				threadData.exceptionStr = transaction.getLastError();
				threadData.isOK = false;

				handler_.response() << threadData.exceptionStr;
			}
		}
		else
		{
			transaction.commit();
		}
	} while (retry);
}

/**
 *	This method is called in the main thread after run() completes.
 */
void ExecuteRawCommandTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		ERROR_MSG( "MySqlDatabase::executeRawCommand: %s\n",
			threadData.exceptionStr.c_str() );
	// __kyl__ (2/8/2006) Following 2 lines not necessary but nice for sanity.
//	else if (threadData.connection.hasFatalError())
//		threadData.isOK = false;

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "ExecuteRawCommandTask for '%s' took %f seconds\n",
					command_.c_str(), double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	IDatabase::IExecuteRawCommandHandler& handler = handler_;
	delete this;

	handler.onExecuteRawCommandComplete();
}

void MySqlDatabase::executeRawCommand( const std::string & command,
	IDatabase::IExecuteRawCommandHandler& handler )
{
	ExecuteRawCommandTask* pTask =
		new ExecuteRawCommandTask( *this, command, handler );
	pTask->doTask();
}

/**
 *	This class encapsulates the MySqlDatabase::putIDs() operation
 *	so that it can be executed in a separate thread.
 */
class PutIDsTask : public MySqlThreadTask
{
	int			numIDs_;
	EntityID*	ids_;

public:
	PutIDsTask( MySqlDatabase& owner, int numIDs, const EntityID * ids )
		: MySqlThreadTask(owner), numIDs_(numIDs), ids_(new EntityID[numIDs])
	{
		this->startThreadTaskTiming();

		memcpy( ids_, ids, sizeof(EntityID)*numIDs );

		this->getThreadData().exceptionStr.clear();
	}
	virtual ~PutIDsTask()
	{
		delete [] ids_;
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method puts unused IDs into the database. May be executed in a
 *	separate thread.
 */
void PutIDsTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();
	bool retry;
	do
	{
		retry = false;
		try
		{
			MySqlTransaction transaction( threadData.connection );

			const EntityID * ids = ids_;
			const EntityID * end = ids_ + numIDs_;
			// TODO: ugh... make this not a loop somehow!
			while (ids != end)
			{
				threadData.boundID_ = *ids++;
				threadData.connection.execute( *threadData.putIDStatement_ );
			}

			transaction.commit();
		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
		}
		catch (std::exception& e)
		{
			threadData.exceptionStr = e.what();
		}
	} while (retry);
}

/**
 *	This method is called in the main thread after run() completes.
 */
void PutIDsTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		// Oh crap! We just lost some IDs.
		// TODO: Store these IDs somewhere and retry later?
		ERROR_MSG( "MySqlDatabase::putIDs: %s\n",
			threadData.exceptionStr.c_str() );

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "PutIDsTask for %d IDs took %f seconds\n",
					numIDs_, double(duration)/stampsPerSecondD() );

	delete this;
}

void MySqlDatabase::putIDs( int numIDs, const EntityID * ids )
{
	PutIDsTask* pTask = new PutIDsTask( *this, numIDs, ids );
	pTask->doTask();
}

/**
 *	This class encapsulates the MySqlDatabase::getIDs() operation
 *	so that it can be executed in a separate thread.
 */
class GetIDsTask : public MySqlThreadTask
{
	int							numIDs_;
	IDatabase::IGetIDsHandler&	handler_;

public:
	GetIDsTask( MySqlDatabase& owner, int numIDs,
		IDatabase::IGetIDsHandler& handler )
		: MySqlThreadTask(owner), numIDs_(numIDs), handler_(handler)
	{
		this->startThreadTaskTiming();
		this->getThreadData().exceptionStr.clear();
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method gets some unused IDs from the database. May be executed in a
 *	separate thread.
 */
void GetIDsTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();
	bool retry;
	do
	{
		retry = false;
		try
		{
			MySqlTransaction transaction( threadData.connection );

			BinaryOStream& strm = handler_.idStrm();

			// Reuse any id's we can get our hands on
			threadData.boundLimit_ = numIDs_;
			threadData.connection.execute( *threadData.getIDsStatement_ );
			int numIDsRetrieved = threadData.getIDsStatement_->resultRows();
			while (threadData.getIDsStatement_->fetch())
			{
				strm << threadData.boundID_;
			}
			if (numIDsRetrieved > 0)
				threadData.connection.execute( *threadData.delIDsStatement_ );
			// Grab new IDs and increment bigworldNewID.id.
			threadData.boundLimit_ = numIDs_ - numIDsRetrieved;
			if (threadData.boundLimit_)
			{
				threadData.connection.execute( *threadData.incIDStatement_ );
				threadData.connection.execute( *threadData.getIDStatement_ );
				threadData.getIDStatement_->fetch();
				while (numIDsRetrieved++ < numIDs_)
				{
					strm << --threadData.boundID_;
				}
			}

			transaction.commit();
		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
			handler_.resetStrm();
		}
		catch (std::exception& e)
		{
			threadData.exceptionStr = e.what();
		}
	} while (retry);
}

/**
 *	This method is called in the main thread after run() completes.
 */
void GetIDsTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		ERROR_MSG( "MySqlDatabase::getIDs: %s\n",
			threadData.exceptionStr.c_str() );

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "GetIDsTask for %d IDs took %f seconds\n",
					numIDs_, double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	IDatabase::IGetIDsHandler& handler = handler_;
	delete this;

	handler.onGetIDsComplete();
}

void MySqlDatabase::getIDs( int numIDs, IDatabase::IGetIDsHandler& handler )
{
	GetIDsTask* pTask = new GetIDsTask( *this, numIDs, handler );
	pTask->doTask();
}


// -----------------------------------------------------------------------------
// Section: Space related
// -----------------------------------------------------------------------------

/**
 *	This class encapsulates the MySqlDatabase::writeSpaceData() operation
 *	so that it can be executed in a separate thread.
 */
class WriteSpaceDataTask : public MySqlThreadTask
{
	MemoryOStream	data_;
	uint32			numSpaces_;

public:
	WriteSpaceDataTask( MySqlDatabase& owner, BinaryIStream& spaceData )
		: MySqlThreadTask( owner ), numSpaces_( 0 )
	{
		this->startThreadTaskTiming();

		MySqlThreadData& threadData = this->getThreadData();

		threadData.exceptionStr.clear();
		data_.transfer( spaceData, spaceData.remainingLength() );

		owner.onWriteSpaceOpStarted();
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method writes the space data into the database.
 *	May be executed in a separate thread.
 */
void WriteSpaceDataTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();
	bool 				retry;
	do
	{
		retry = false;
		try
		{
			MySqlTransaction transaction( threadData.connection );

			transaction.execute( *threadData.delSpaceIDsStatement_ );
			transaction.execute( *threadData.delSpaceDataStatement_ );

			numSpaces_ = writeSpaceDataStreamToDB( threadData.connection,
					threadData.boundSpaceID_,
					*threadData.writeSpaceStatement_,
					threadData.boundSpaceData_,
					*threadData.writeSpaceDataStatement_,
					data_ );

			transaction.commit();
		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
			data_.rewind();
		}
		catch (std::exception & e)
		{
			threadData.exceptionStr = e.what();
		}
	} while (retry);

}

/**
 *	This method is called in the main thread after run() completes.
 */
void WriteSpaceDataTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		ERROR_MSG( "MySqlDatabase::writeSpaceData: execute failed (%s)\n",
				   threadData.exceptionStr.c_str() );

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "WriteSpaceDataTask for %u spaces took %f seconds\n",
					this->numSpaces_, double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	MySqlDatabase& owner = this->getOwner();
	delete this;

	owner.onWriteSpaceOpCompleted();
}

/**
 *	This method writes data associated with a space to the database.
 */
void MySqlDatabase::writeSpaceData( BinaryIStream& spaceData )
{
	WriteSpaceDataTask* pTask = new WriteSpaceDataTask( *this, spaceData );
	pTask->doTask();
}

/**
 *	Override from IDatabase.
 */
bool MySqlDatabase::getSpacesData( BinaryOStream& strm )
{
	MySql& connection = this->getMainThreadData().connection;

	try
	{
		MySqlBindings paramBindings;
		MySqlBindings resultBindings;

		// TODO: Make this handle the case where we are halfway through
		// updating the space data i.e. there are multiple versions present.
		// In that case we should probably use the last complete version
		// instead of the latest incomplete version.
		MySqlStatement spacesStmt( connection,
								   "SELECT id from bigworldSpaces" );
		SpaceID spaceID;
		resultBindings << spaceID;
		spacesStmt.bindResult( resultBindings );

		MySqlStatement spaceDataStmt( connection,
			"SELECT spaceEntryID, entryKey, data "
					"FROM bigworldSpaceData where id = ?" );
		paramBindings << spaceID;
		spaceDataStmt.bindParams( paramBindings );

		uint64 boundSpaceEntryID;
		uint16 boundSpaceDataKey;
		MySqlBuffer boundSpaceData( maxSpaceDataSize_ );
		resultBindings.clear();
		resultBindings << boundSpaceEntryID;
		resultBindings << boundSpaceDataKey;
		resultBindings << boundSpaceData;
		spaceDataStmt.bindResult( resultBindings );

		connection.execute( spacesStmt );

		int numSpaces = spacesStmt.resultRows();
		std::vector< SpaceID > spaceIDs;
		spaceIDs.reserve( numSpaces );

		strm << numSpaces;

		INFO_MSG( "MySqlDatabase::getSpacesData: numSpaces = %d\n", numSpaces );

		for (int i = 0; i < numSpaces; ++i)
		{
			spacesStmt.fetch();
			spaceIDs.push_back( spaceID );
		}

		for (size_t i = 0; i < spaceIDs.size(); ++i)
		{
			spaceID = spaceIDs[i];
			strm << spaceID;
			connection.execute( spaceDataStmt );

			int numData = spaceDataStmt.resultRows();
			strm << numData;

			for (int dataIndex = 0; dataIndex < numData; ++dataIndex)
			{
				spaceDataStmt.fetch();
				strm << boundSpaceEntryID;
				strm << boundSpaceDataKey;
				strm << boundSpaceData.getString();
			}
		}
	}
	catch (std::exception & e)
	{
		ERROR_MSG( "MySqlDatabase::getSpacesData: Failed to get spaces data: "
				"%s\n", e.what() );
		return false;
	}

	return true;
}

/**
 *	Override from IDatabase.
 */
void MySqlDatabase::restoreEntities( EntityRecoverer& recoverer )
{
	MySql& connection = this->getMainThreadData().connection;

	try
	{
		std::map< int, EntityTypeID > typeTranslation;

		// TODO: Should be able to do this in SQL.
		{
			// I'm not too sure why we keep two different values for entity type
			// id. Best guess is for updating. If the entity types change
			// indexes, the values in bigworldLogOns do not need to be changed.
			// It may be easier to just modify these values if this occurs.
			MySqlStatement typeStmt( connection,
					"SELECT typeID, bigworldID FROM bigworldEntityTypes" );
			MySqlBindings resultBindings;
			int dbTypeID;
			EntityTypeID bigworldTypeID;
			resultBindings << dbTypeID << bigworldTypeID;
			typeStmt.bindResult( resultBindings );
			connection.execute( typeStmt );

			int numResults = typeStmt.resultRows();

			for (int i = 0; i < numResults; ++i)
			{
				typeStmt.fetch();
				typeTranslation[ dbTypeID ] = bigworldTypeID;
			}
		}

		{
			MySqlStatement logOnsStmt( connection,
					"SELECT databaseID, typeID from bigworldLogOns" );
			MySqlBindings resultBindings;
			DatabaseID dbID;
			int dbTypeID;
			resultBindings << dbID << dbTypeID;
			logOnsStmt.bindResult( resultBindings );

			connection.execute( logOnsStmt );

			int numResults = logOnsStmt.resultRows();

			if (numResults > 0)
			{
				recoverer.reserve( numResults );

				// Get the entities that have to be recovered.
				for (int i = 0; i < numResults; ++i)
				{
					logOnsStmt.fetch();
					EntityTypeID bwTypeID = typeTranslation[ dbTypeID ];
					recoverer.addEntity( bwTypeID, dbID );
				}

				connection.execute( "DELETE FROM bigworldLogOns" );
			}

			recoverer.start();
		}
	}
	catch (std::exception & e)
	{
		ERROR_MSG( "MySqlDatabase::restoreGameState: Restore entities failed (%s)\n",
				e.what() );
		recoverer.abort();
	}
}

class SetGameTimeTask : public MySqlThreadTask
{
public:
	SetGameTimeTask( MySqlDatabase& owner, TimeStamp gameTime )
		: MySqlThreadTask(owner)
	{
		this->startThreadTaskTiming();

		MySqlThreadData& threadData = this->getThreadData();
		threadData.gameTime_ = gameTime;
		threadData.exceptionStr.clear();
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method updates the game time in the database. May be executed in a
 *	separate thread.
 */
void SetGameTimeTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();
	try
	{
		threadData.connection.execute( *threadData.setGameTimeStatement_ );
	}
	catch (std::exception & e)
	{
		threadData.exceptionStr = e.what();
	}
}

void SetGameTimeTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		ERROR_MSG( "MySqlDatabase::setGameTime: "
					"execute failed for time %u (%s)\n",
					threadData.gameTime_, threadData.exceptionStr.c_str() );

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "SetGameTimeTask for game time %u took %f seconds\n",
					threadData.gameTime_, double(duration)/stampsPerSecondD() );

	delete this;
}

/**
 *	This method sets the game time stored in the database.
 */
void MySqlDatabase::setGameTime( TimeStamp gameTime )
{
	SetGameTimeTask* pTask = new SetGameTimeTask( *this, gameTime );
	pTask->doTask();
}

namespace MySQL
{

/**
 *	This method returns the game time stored in the database. Returns result
 *	via the gameTime parameter.
 */
bool getGameTime( MySql& connection, TimeStamp& gameTime )
{
	MySqlUnPrep::Statement stmt( connection,
			"SELECT * FROM bigworldGameTime" );
	MySqlUnPrep::Bindings bindings;
	bindings << gameTime;
	stmt.bindResult( bindings );

	connection.execute( stmt );

	MF_ASSERT_DEV( stmt.resultRows() == 1 );

	return stmt.fetch();
}

/**
 * 	This method returns the maximum app ID stored in the list of secondary
 * 	database entries.
 */
bool getMaxSecondaryDBAppID( MySql& connection, int32& maxAppID )
{
	MySqlUnPrep::Statement stmt( connection,
			"SELECT MAX( appID ) FROM bigworldSecondaryDatabases" );
	MySqlUnPrep::Bindings bindings;
	MySqlValueWithNull< int32 > maxAppIDBuf;
	bindings << maxAppIDBuf;
	stmt.bindResult( bindings );

	connection.execute( stmt );

	bool isOK = stmt.fetch();
	if (isOK && maxAppIDBuf.get())
	{
		maxAppID = *maxAppIDBuf.get();
	}

	return isOK;
}


/**
 *	This class implements the getBaseAppMgrInitData() function as a thread task.
 */
class GetBaseAppMgrInitDataTask : public MySqlThreadTask
{
	IDatabase::IGetBaseAppMgrInitDataHandler& 	handler_;
	TimeStamp	gameTime_;
	int32		maxAppID_;

public:
	GetBaseAppMgrInitDataTask( MySqlDatabase& owner,
			IDatabase::IGetBaseAppMgrInitDataHandler& handler ) :
		MySqlThreadTask( owner ), handler_( handler ),
		gameTime_( 0 ), maxAppID_( 0 )
	{
		this->MySqlThreadTask::standardInit();
	}

	// WorkerThread::ITask overrides
	virtual void run()
	{
		MySqlThreadData& threadData = this->getThreadData();
		// Calls execute()
		wrapInTransaction( threadData.connection, threadData, *this );
	}

	// Things to do inside wrapInTransaction()
	void execute( MySql& connection, MySqlThreadData& threadData )
	{
		getGameTime( connection, gameTime_ );
		getMaxSecondaryDBAppID( connection, maxAppID_ );
	}

	virtual void onRunComplete()
	{
		this->MySqlThreadTask::standardOnRunComplete<
				GetBaseAppMgrInitDataTask >();

		IDatabase::IGetBaseAppMgrInitDataHandler& 	handler = handler_;
		TimeStamp 	gameTime = gameTime_;
		int32 		maxAppID = maxAppID_;

		// Release thread resources before callback. If callback decides
		// to do another operation that requires thread resource, it is not
		// deadlocked.
		delete this;

		handler.onGetBaseAppMgrInitDataComplete( gameTime, maxAppID );
	}

	// Used by MySqlThreadTask::standardOnRunComplete()
	static const char * errorMethodName()	{ return "getBaseAppMgrInitData"; }
};

}	// end namespace MySQL

/**
 *	Override from IDatabase.
 */
void MySqlDatabase::getBaseAppMgrInitData(
		IGetBaseAppMgrInitDataHandler& handler )
{
	MySQL::GetBaseAppMgrInitDataTask* pTask =
			new MySQL::GetBaseAppMgrInitDataTask( *this, handler );
	pTask->doTask();
}

/**
 *	Override from IDatabase.
 */
void MySqlDatabase::remapEntityMailboxes( const Mercury::Address& srcAddr,
		const BackupHash & destAddrs )
{
	try
	{
		MySql& connection = this->getMainThreadData().connection;

		std::stringstream updateStmtStrm;
		updateStmtStrm << "UPDATE bigworldLogOns SET ip=?, port=? WHERE ip="
				<< ntohl( srcAddr.ip ) << " AND port=" << ntohs( srcAddr.port )
				<< " AND ((((objectID * " << destAddrs.prime()
				<< ") % 0x100000000) >> 8) % " << destAddrs.virtualSize()
				<< ")=?";

//		DEBUG_MSG( "MySqlDatabase::remapEntityMailboxes: %s\n",
//				updateStmtStrm.str().c_str() );

		MySqlStatement updateStmt( connection, updateStmtStrm.str() );
		uint32 	boundAddress;
		uint16	boundPort;
		int		i;

		MySqlBindings params;
		params << boundAddress << boundPort << i;

		updateStmt.bindParams( params );

		// Wait for all tasks to complete just in case some of them updates
		// bigworldLogOns.
		MySqlThreadResPool& threadResPool = this->getThreadResPool();
		threadResPool.threadPool().waitForAllTasks();

		for (i = 0; i < int(destAddrs.size()); ++i)
		{
//			DEBUG_MSG( "MySqlDatabase::remapEntityMailboxes: %s\n",
//					(char *) destAddrs[i] );
			boundAddress = ntohl( destAddrs[i].ip );
			boundPort = ntohs( destAddrs[i].port );

			connection.execute( updateStmt );
		}
		for (i = int(destAddrs.size()); i < int(destAddrs.virtualSize()); ++i)
		{
			int realIdx = i/2;
//			DEBUG_MSG( "MySqlDatabase::remapEntityMailboxes (round 2): %s\n",
//					(char *) destAddrs[realIdx] );
			boundAddress = ntohl( destAddrs[realIdx].ip );
			boundPort = ntohs( destAddrs[realIdx].port );

			connection.execute( updateStmt );
		}
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "MySqlDatabase::remapEntityMailboxes: Remap entity "
				"mailboxes failed (%s)\n", e.what() );
	}
}

namespace MySQL
{

/**
 * 	This thread task does one query on the bigworldSecondaryDatabases table
 * 	using the MySqlThreadData::secondaryDBEntry_ binding.
 *
 * 	METHOD should be a functor that returns a MySqlStatement to execute.
 */
template < class STATEMENT >
class SimpleSecondaryDBTask : public MySqlThreadTask
{
public:
	SimpleSecondaryDBTask( MySqlDatabase& owner,
			const IDatabase::SecondaryDBEntry& entry ) :
		MySqlThreadTask( owner )
	{
		this->MySqlThreadTask::standardInit();

		this->getThreadData().secondaryDBOps_.entryBuf().set(
				ntohl( entry.addr.ip ), ntohs( entry.addr.port ),
				entry.appID, entry.location );
	}

	// WorkerThread::ITask overrides
	virtual void run()
	{
		MySqlThreadData& threadData = this->getThreadData();
		wrapInTransaction( threadData.connection, threadData,
				STATEMENT::getStmt( threadData ) );
	}
	virtual void onRunComplete()
	{
		this->MySqlThreadTask::standardOnRunComplete< SimpleSecondaryDBTask >();
		delete this;
	}

	// MySqlThreadTask overrides
	virtual std::string getTaskInfo() const
	{
		return this->getThreadData().secondaryDBOps_.entryBuf().getAsString();
	}

	// Used by MySqlThreadTask::standardOnRunComplete()
	static const char * errorMethodName()
	{
		return STATEMENT::errorMethodName();
	}
};

/**
 *	Template parameter for SimpleSecondaryDBTask to implement the thread task
 * 	for addSecondaryDB().
 */
struct AddSecondaryDBEntry
{
	// Used by SimpleSecondaryDBTask
	static MySqlStatement& getStmt( MySqlThreadData& threadData )
	{
		return threadData.secondaryDBOps_.addStmt( threadData.connection );
	}
	// Used by MySqlThreadTask::standardOnRunComplete()
	static const char * errorMethodName()	{ return "addSecondaryDB"; }
};

/**
 * 	This method executes SELECTs ip,port,appID,location FROM
 * 	bigworldSecondaryDatabases with the WHERE clause and returns the results
 * 	in entries.
 */
void getSecondaryDBEntries( MySql& connection,
		IDatabase::SecondaryDBEntries& entries,
		const std::string& condition = std::string() )
{
	std::stringstream getStmtStrm;
	getStmtStrm << "SELECT ip,port,appID,location FROM "
			"bigworldSecondaryDatabases" << condition;

	MySqlUnPrep::Statement	getStmt( connection, getStmtStrm.str() );
	SecondaryDBOps::DbEntryBuffer buffer;
	MySqlUnPrep::Bindings	bindings;
	buffer.addToBindings( bindings );
	getStmt.bindResult( bindings );

	connection.execute( getStmt );

	while (getStmt.fetch())
	{
		entries.push_back( IDatabase::SecondaryDBEntry(
				htonl( buffer.ip ),
				htons( buffer.port ),
				buffer.appID,
				buffer.location.getString() ) );
	}
}

/**
 *	This class implements the updateSecondaryDBs() function as a thread task.
 */
class UpdateSecondaryDBsTask : public MySqlThreadTask
{
	IDatabase::IUpdateSecondaryDBshandler&			handler_;
	std::string 									condition_;
	std::auto_ptr< IDatabase::SecondaryDBEntries > 	pEntries_;

public:
	UpdateSecondaryDBsTask( MySqlDatabase& owner, const BaseAppIDs& ids,
			IDatabase::IUpdateSecondaryDBshandler& handler ) :
		MySqlThreadTask( owner ), handler_( handler ),
		pEntries_( new IDatabase::SecondaryDBEntries )
	{
		this->MySqlThreadTask::standardInit();

		// Create condition from ids.
		if (!ids.empty())
		{
			std::stringstream conditionStrm;
			conditionStrm << " WHERE appID NOT IN (";
			BaseAppIDs::const_iterator iter = ids.begin();
			conditionStrm << *iter;
			for (; iter != ids.end(); ++iter)
			{
				conditionStrm << "," << *iter;
			}
			conditionStrm << ')';
			condition_ = conditionStrm.str();
		}
	}

	// WorkerThread::ITask overrides
	virtual void run()
	{
		MySqlThreadData& threadData = this->getThreadData();
		// Calls execute()
		wrapInTransaction( threadData.connection, threadData, *this );
	}

	// Things to do inside wrapInTransaction()
	void execute( MySql& connection, MySqlThreadData& threadData )
	{
		// Get the entries that we're going to delete.
		getSecondaryDBEntries( connection, *pEntries_, condition_ );

		// Delete the entries
		std::stringstream delStmtStrm_;
		delStmtStrm_ << "DELETE FROM bigworldSecondaryDatabases" << condition_;
		connection.execute( delStmtStrm_.str() );
	}

	virtual void onRunComplete()
	{
		this->MySqlThreadTask::standardOnRunComplete< UpdateSecondaryDBsTask >();

		IDatabase::IUpdateSecondaryDBshandler&			handler( handler_ );
		std::auto_ptr< IDatabase::SecondaryDBEntries >	pEntries( pEntries_ );

		delete this;

		handler.onUpdateSecondaryDBsComplete( *pEntries );
	}

	// Used by MySqlThreadTask::standardOnRunComplete()
	static const char * errorMethodName()	{ return "updateSecondaryDBs"; }
};

/**
 *	This class implements the getSecondaryDBs() function as a thread task.
 */
class GetSecondaryDBsTask : public MySqlThreadTask
{
	typedef IDatabase::IGetSecondaryDBsHandler Handler;

	Handler& 										handler_;
	std::auto_ptr< IDatabase::SecondaryDBEntries >	pEntries_;

public:
	GetSecondaryDBsTask( MySqlDatabase& owner, Handler& handler ) :
		MySqlThreadTask( owner ), handler_( handler ),
		pEntries_( new IDatabase::SecondaryDBEntries )
	{
		this->MySqlThreadTask::standardInit();
	}

	// WorkerThread::ITask overrides
	virtual void run()
	{
		MySqlThreadData& threadData = this->getThreadData();
		// Calls execute()
		wrapInTransaction( threadData.connection, threadData, *this );
	}

	// Things to do inside wrapInTransaction()
	void execute( MySql& connection, MySqlThreadData& threadData )
	{
		getSecondaryDBEntries( connection, *pEntries_ );
	}

	virtual void onRunComplete()
	{
		this->MySqlThreadTask::standardOnRunComplete< GetSecondaryDBsTask >();

		// Release thread resources before callback. If callback decides
		// to do another operation that requires thread resource, it is not
		// deadlocked.
		Handler&										handler( handler_ );
		std::auto_ptr< IDatabase::SecondaryDBEntries > 	pEntries( pEntries_ );
		delete this;

		handler.onGetSecondaryDBsComplete( *pEntries );
	}

	// Used by MySqlThreadTask::standardOnRunComplete()
	static const char * errorMethodName()	{ return "getSecondaryDBs"; }
};

/**
 *	Template parameter to wrapInTransaction() to implement clearSecondaryDBs()
 */
struct ClearSecondaryDBs
{
	int	numCleared;

	ClearSecondaryDBs(): numCleared( 0 ) {}

	void execute( MySql& connection )
	{
		connection.query( "DELETE FROM bigworldSecondaryDatabases");
		numCleared = int( connection.affectedRows() );
	}

	void setExceptionStr( const char * errorStr )
	{
		ERROR_MSG( "MySqlDatabase::clearSecondaryDBs: %s\n", errorStr );
		numCleared = -1;
	}

} clearSecondaryDBs;

}	// namespace MySQL

/**
 *	Overrides IDatabase method.
 */
void MySqlDatabase::addSecondaryDB( const SecondaryDBEntry& entry )
{
	typedef MySQL::SimpleSecondaryDBTask< MySQL::AddSecondaryDBEntry >
			AddSecondaryDBTask;
	AddSecondaryDBTask* pTask = new AddSecondaryDBTask( *this, entry );
	pTask->doTask();
}

/**
 *	Overrides IDatabase method.
 */
void MySqlDatabase::updateSecondaryDBs( const BaseAppIDs& ids,
		IUpdateSecondaryDBshandler& handler )
{
	MySQL::UpdateSecondaryDBsTask* pTask =
			new MySQL::UpdateSecondaryDBsTask( *this, ids, handler );
	pTask->doTask();
}

/**
 *	Overrides IDatabase method
 */
void MySqlDatabase::getSecondaryDBs( IGetSecondaryDBsHandler& handler )
{
	MySQL::GetSecondaryDBsTask* pTask =
			new MySQL::GetSecondaryDBsTask( *this, handler );
	pTask->doTask();
}

/**
 *	Overrides IDatabase method
 */
uint32 MySqlDatabase::getNumSecondaryDBs()
{
	MySql& connection = this->getMainThreadData().connection;

	return MySqlDatabase::getNumSecondaryDBs( connection );
}

/**
 * 	This static method returns the number of rows in the
 * 	bigworldSecondaryDatabases table.
 */
uint32 MySqlDatabase::getNumSecondaryDBs( MySql& connection )
{
	MySqlUnPrep::Statement getCountStmt( connection,
			"SELECT COUNT(*) FROM bigworldSecondaryDatabases" );

	uint32 count;
	MySqlUnPrep::Bindings bindings;
	bindings << count;
	getCountStmt.bindResult( bindings );

	connection.execute( getCountStmt );
	getCountStmt.fetch();

	return count;
}

/**
 *	Overrides IDatabase method
 */
int MySqlDatabase::clearSecondaryDBs()
{
	MySQL::ClearSecondaryDBs clearSecondaryDBs;

	MySqlThreadData& mainThreadData = this->getMainThreadData();
	wrapInTransaction( mainThreadData.connection, clearSecondaryDBs );

	return clearSecondaryDBs.numCleared;
}

/**
 *	Overrides IDatabase method
 */
bool MySqlDatabase::lockDB()
{
	return pThreadResPool_->lockDB();
}

/**
 *	Overrides IDatabase method
 */
bool MySqlDatabase::unlockDB()
{
	return pThreadResPool_->unlockDB();
}


/**
 * 	This function creates all the tables that DbMgr uses to store non-entity
 * 	data e.g. logons, meta data etc.
 */
void MySqlDatabase::createSpecialBigWorldTables( MySql& connection )
{
	char buffer[512];

	// Metadata tables.
	connection.execute( "CREATE TABLE IF NOT EXISTS bigworldEntityTypes "
			 "(typeID INT NOT NULL AUTO_INCREMENT, bigworldID INT, "
			 "name CHAR(255) NOT NULL UNIQUE, PRIMARY KEY(typeID), "
			 "KEY(bigworldID)) ENGINE="MYSQL_ENGINE_TYPE );

	// Logon/checkout tables
	connection.execute( "CREATE TABLE IF NOT EXISTS bigworldLogOns "
			 "(databaseID BIGINT NOT NULL, typeID INT NOT NULL, "
			 "objectID INT, ip INT UNSIGNED, port SMALLINT UNSIGNED, "
			 "salt SMALLINT UNSIGNED, PRIMARY KEY(typeID, databaseID)) "
			 "ENGINE="MYSQL_ENGINE_TYPE );
	bw_snprintf( buffer, sizeof(buffer),
			"CREATE TABLE IF NOT EXISTS bigworldLogOnMapping "
			 "(logOnName VARCHAR(%d) NOT NULL, password VARCHAR(%d),"
			 " typeID INT NOT NULL, recordName VARCHAR(%d),"
			 " PRIMARY KEY(logOnName)) ENGINE="MYSQL_ENGINE_TYPE,
			 BWMySQLMaxLogOnNameLen, BWMySQLMaxLogOnPasswordLen,
			 BWMySQLMaxNamePropertyLen );
	connection.execute( buffer );

	// Entity ID tables.
	connection.execute( "CREATE TABLE IF NOT EXISTS bigworldNewID "
					 "(id INT NOT NULL) ENGINE="MYSQL_ENGINE_TYPE );
	connection.execute( "CREATE TABLE IF NOT EXISTS bigworldUsedIDs "
					 "(id INT NOT NULL) ENGINE="MYSQL_ENGINE_TYPE );

	// Game time
	connection.execute( "CREATE TABLE IF NOT EXISTS bigworldGameTime "
					"(time INT NOT NULL) ENGINE="MYSQL_ENGINE_TYPE );

	// Space data tables.
	const std::string& blobTypeName =
		MySqlTypeTraits<std::string>::colTypeStr( maxSpaceDataSize_ );
	connection.execute( "CREATE TABLE IF NOT EXISTS bigworldSpaces "
			"(id INT NOT NULL UNIQUE) ENGINE="MYSQL_ENGINE_TYPE );
	bw_snprintf( buffer, sizeof(buffer),
			"CREATE TABLE IF NOT EXISTS bigworldSpaceData "
			"(id INT NOT NULL, INDEX (id), "
			"spaceEntryID BIGINT NOT NULL, "
			"entryKey SMALLINT UNSIGNED NOT NULL, "
			"data %s NOT NULL ) ENGINE="MYSQL_ENGINE_TYPE,
			blobTypeName.c_str() );
	connection.execute( buffer );
	// Just in case the table already exists and have a different BLOB
	// type for the data column. maxSpaceDataSize_ is configurable.
	bw_snprintf( buffer, sizeof(buffer),
			"ALTER TABLE bigworldSpaceData MODIFY data %s",
			 blobTypeName.c_str() );
	connection.execute( buffer );

	// Secondary database information tables.
	MySQL::SecondaryDBOps::createTable( connection );

	// SQLite checksum table.
	connection.execute( "CREATE TABLE IF NOT EXISTS bigworldEntityDefsChecksum "
			 "(checksum CHAR(255)) ENGINE="MYSQL_ENGINE_TYPE );}

/**
 *	This function resets the state of some bigworld internal tables to their
 * 	initial state.
 */
void MySqlDatabase::initSpecialBigWorldTables( MySql& connection,
		const EntityDefs& entityDefs )
{
	MySqlTransaction transaction( connection );

	// Reset entity ID tables.
	transaction.execute( "DELETE FROM bigworldUsedIDs" );
	transaction.execute( "DELETE FROM bigworldNewID" );
	transaction.execute( "INSERT INTO bigworldNewID (id) VALUES (1)" );
	transaction.execute( "DELETE FROM bigworldEntityDefsChecksum" );

	// Set game time to 0 if it doesn't exist.
	// NOTE: This statement assumes bigworldNewID exists and has 1 row in it.
	transaction.execute( "INSERT INTO bigworldGameTime "
			"SELECT 0 FROM bigworldNewID "
			"WHERE NOT EXISTS(SELECT * FROM bigworldGameTime)" );

	// Set the checksum of all persistent properties
	const MD5::Digest& digest = entityDefs.getPersistentPropertiesDigest();
	std::string stmt;

	stmt = "INSERT INTO bigworldEntityDefsChecksum VALUES ('";
	stmt += digest.quote();
	stmt += "')";
	transaction.execute( stmt );

	transaction.commit();
}

/**
 *	This funcion checks that all the non-entity tables required by DBMgr exists
 * 	and have the correct column.
 */
bool MySqlDatabase::checkSpecialBigWorldTables( MySql& connection )
{
	// We cheat a bit. We only check that they number of columns match
	// requirements.
	struct CheckList
	{
		std::string		tableName;
		unsigned int	numColumns;
	};
	static const CheckList checkList[] =
	{	{ "bigworldEntityTypes", 3 }
	, 	{ "bigworldLogOns", 6 }
	, 	{ "bigworldLogOnMapping", 4 }
	, 	{ "bigworldNewID", 1 }
	, 	{ "bigworldUsedIDs", 1 }
	, 	{ "bigworldGameTime", 1 }
	, 	{ "bigworldSpaces", 1 }
	, 	{ "bigworldSpaceData", 4 }
	, 	{ "bigworldSecondaryDatabases", 4 }
	, 	{ "bigworldEntityDefsChecksum", 1 }
	};

	bool isOK = true;
	for (size_t i = 0; i < sizeof(checkList)/sizeof(CheckList); ++i)
	{
		MySqlTableMetadata	tableMetadata( connection, checkList[i].tableName );
		if (!tableMetadata.isValid() ||
				(tableMetadata.getNumFields() != checkList[i].numColumns))
		{
#ifndef ENABLE_TABLE_SCHEMA_ALTERATIONS
			INFO_MSG( "\tTable %s should have %u columns\n",
					checkList[i].tableName.c_str(),
					checkList[i].numColumns );
#endif
			isOK = false;
		}
	}

	return isOK;
}


// mysql_database.cpp
