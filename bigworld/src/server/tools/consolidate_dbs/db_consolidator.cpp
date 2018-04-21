/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "Python.h"		// See http://docs.python.org/api/includes.html

#include "db_consolidator.hpp"

#include "db_file_transfer.hpp"
#include "db_progress.hpp"
#include "tcp_listener.hpp"

#include "../../dbmgr/db_config.hpp"
#include "../../dbmgr/db_status.hpp"
#include "../../dbmgr/mysql_notprepared.hpp"
#include "../../dbmgr/mysql_named_lock.hpp"

#include "server/bwconfig.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/multi_file_system.hpp"
#include "entitydef/constants.hpp"
#include "network/machine_guard.hpp"
#include "network/watcher_nub.hpp"
#include "pyscript/script.hpp"

#include "third_party/sqlite/sqlite3.h"

#include <iostream>
#include <string>

DECLARE_DEBUG_COMPONENT( 0 )

// -----------------------------------------------------------------------------
// Section: Link tokens
// -----------------------------------------------------------------------------
extern int ResMgr_token;
static int s_moduleTokens = ResMgr_token;
extern int PyPatrolPath_token;
static int s_patrolToken = PyPatrolPath_token;

// -----------------------------------------------------------------------------
// Section: Constants
// -----------------------------------------------------------------------------
#define SELECT_DATA_FROM_SECDB "SELECT sm_dbID,sm_typeID,sm_time,sm_blob FROM "
enum SelectStmtColumn	// Order of columns in select statment.
{
	SelectStmtDbID, SelectStmtTypeID, SelectStmtTime, SelectStmtBlob
};

#define CHECKSUM_TABLE_NAME	"tbl_checksum"
#define CHECKSUM_COLUMN_NAME "sm_checksum"

// -----------------------------------------------------------------------------
// Section: Signal handlers
// -----------------------------------------------------------------------------
// Handles SIGINT and SIGHUP.
static void signalHandler( int /*sigNum*/ )
{
	DBConsolidator::pInstance()->abort();
}

// -----------------------------------------------------------------------------
// Section: Utility classes
// -----------------------------------------------------------------------------
/**
 * 	Wrapper for a sqlite3 connection.
 */
class SqliteConnection
{
	sqlite3* pConnection_;
public:
	SqliteConnection( const char * filePath, int& result ) :
		pConnection_( NULL )
	{
		result = sqlite3_open( filePath, &pConnection_ );
	}
	~SqliteConnection()
	{
		if (pConnection_)
		{
			// __kyl__(1/5/2008) Hmmm... Isn't there an OK return code?
			MF_VERIFY( sqlite3_close( pConnection_ ) != SQLITE_BUSY );
		}
	}

	sqlite3* get()	{ return pConnection_; }

	const char* lastError() {  return sqlite3_errmsg( pConnection_ ); }
};

/**
 * 	Wrapper for a sqlite3_stmt.
 */
class SqliteStatement
{
	sqlite3_stmt* pStmt_;
public:
	SqliteStatement( SqliteConnection& connection,
			const char * statement, int& result ) :
		pStmt_( NULL )
	{
		result = sqlite3_prepare_v2( connection.get(), statement, -1, &pStmt_,
					NULL );
	}
	~SqliteStatement()
	{
		MF_VERIFY( sqlite3_finalize( pStmt_ ) == SQLITE_OK );
	}

	sqlite3_stmt* get()	{ return pStmt_; }

	int step()			{ return sqlite3_step( pStmt_ ); }

	const unsigned char* textColumn( int column )
	{	return sqlite3_column_text( pStmt_, column ); }
	int intColumn( int column )
	{	return sqlite3_column_int( pStmt_, column );	}
};

// -----------------------------------------------------------------------------
// Section: DBFileTransferErrorMonitor
// -----------------------------------------------------------------------------
/**
 * 	This class is checks on FileReceiverMgr periodically to see whether there
 * 	are any file transfers that are hung or failed to start.
 */
class DBFileTransferErrorMonitor : public Mercury::TimerExpiryHandler
{
	enum
	{
		POLL_INTERVAL_SECS = 5,
		CONNECT_TIMEOUT_SECS = 30,
		INACTIVITY_TIMEOUT_SECS = 20
	};

public:

	DBFileTransferErrorMonitor( FileReceiverMgr& fileReceiverMgr ) :
		fileReceiverMgr_( fileReceiverMgr ),
		timerID_( fileReceiverMgr_.nub().registerTimer(
				POLL_INTERVAL_SECS * 1000000, this ) ),
		startTime_( timestamp() )
	{}

	virtual ~DBFileTransferErrorMonitor()
	{
		fileReceiverMgr_.nub().cancelTimer( timerID_ );
	}

	// Mercury::TimerExpiryHandler override.
	virtual int handleTimeout( Mercury::TimerID id, void * arg );

private:
	FileReceiverMgr& 	fileReceiverMgr_;
	Mercury::TimerID	timerID_;
	uint64				startTime_;
};

/**
 *	This method checks that the file transfer operation is going smoothly.
 *	Otherwise it flags it as an error.
 */
int DBFileTransferErrorMonitor::handleTimeout( Mercury::TimerID id, void * arg )
{
	uint64 now = timestamp();
	bool isTimedOut = false;

	// Check connection timeouts
	if (fileReceiverMgr_.hasUnstartedDBs() &&
			((now - startTime_) >= CONNECT_TIMEOUT_SECS * stampsPerSecond()))
	{
		FileReceiverMgr::SourceDBs unstartedDBs =
				fileReceiverMgr_.getUnstartedDBs();
		for (FileReceiverMgr::SourceDBs::const_iterator i =
				unstartedDBs.begin(); i != unstartedDBs.end(); ++i)
		{
			Mercury::Address addr( i->second, 0 );
			ERROR_MSG( "DBFileTransferErrorMonitor::handleTimeout: Timed out "
				"waiting for transfer of %s from %s to start.\n"
				"Please check transfer_db logs for any errors - they appear "
				"under the Tool process.\n",
				i->first.c_str(), addr.ipAsString() );
		}

		isTimedOut = true;
	}

	// Check inactivity timeouts
	const FileReceiverMgr::ReceiverSet& inProgReceivers =
			fileReceiverMgr_.startedReceivers();
	for ( FileReceiverMgr::ReceiverSet::const_iterator ppReceiver =
			inProgReceivers.begin(); ppReceiver != inProgReceivers.end();
			++ppReceiver )
	{
		if ((now - (*ppReceiver)->lastActivityTime()) >=
				INACTIVITY_TIMEOUT_SECS * stampsPerSecond())
		{
			if ((*ppReceiver)->srcPath().empty())
			{
				ERROR_MSG( "DBFileTransferErrorMonitor::handleTimeout: "
						"File transfer from %s is hung\n",
						(*ppReceiver)->srcAddr().ipAsString() );
			}
			else
			{
				ERROR_MSG( "DBFileTransferErrorMonitor::handleTimeout: "
						"Transfer of file %s from %s is hung\n",
						(*ppReceiver)->srcPath().c_str(),
						(*ppReceiver)->srcAddr().ipAsString() );
			}
			isTimedOut = true;
		}
	}

	if (isTimedOut)
	{
		fileReceiverMgr_.onFileReceiveError();
	}

	return 0;
}


// -----------------------------------------------------------------------------
// Section: DBConsolidator
// -----------------------------------------------------------------------------
BW_SINGLETON_STORAGE( DBConsolidator )

/**
 *	Constructor.
 */
DBConsolidator::DBConsolidator( Mercury::Nub& nub, WatcherNub& watcherNub,
		bool shouldReportToDBMgr, bool shouldStopOnError ) :
	nub_( nub ),
	watcherNub_( watcherNub ),
	dbMgrAddr_( 0, 0 ),
	pPrimaryDBConnection_( NULL ),
	consolidationDir_( "/tmp/" ),
	shouldStopOnError_( shouldStopOnError ),
	shouldAbort_( false )
{
	signal( SIGINT, ::signalHandler );
	signal( SIGHUP, ::signalHandler );

	// Find DBMgr watcher address
	if (shouldReportToDBMgr)
	{
		this->initDBMgrAddr();
	}
}

/**
 *	Destructor
 */
DBConsolidator::~DBConsolidator()
{
	for ( MySqlEntityTypeMappings::iterator i = entityTypeMappings_.begin();
			i < entityTypeMappings_.end(); ++i )
	{
		delete *i;
	}
	entityTypeMappings_.clear();
}

/**
 * 	This function should be called after constructor to initialise the object.
 * 	Returns true if initialisation succeeded, false if it failed.
 */
bool DBConsolidator::init()
{
	bool isOK;

	// Check that we're using MySQL as our database type.
	std::string databaseType = BWConfig::get( "dbMgr/type", "xml" );
	if (databaseType != "mysql")
	{
		ERROR_MSG( "DBConsolidator::init: Cannot consolidate database of "
				"type '%s'\n", databaseType.c_str() );
		return false;
	}

	// Test connection
	DBConfig::Server 	primaryDBConfig;
	bool				isConnected = false;
	do
	{
		const DBConfig::Server::ServerInfo& serverInfo =
				primaryDBConfig.getCurServer();
		try
		{
			MySql connection( serverInfo.connectionInfo );
			isConnected = true;
		}
		catch (std::exception& e)
		{
			ERROR_MSG( "DBConsolidator::init: Failed to connect to "
					"%s: %s:%d (%s): %s\n",
					serverInfo.configName.c_str(),
					serverInfo.connectionInfo.host.c_str(),
					serverInfo.connectionInfo.port,
					serverInfo.connectionInfo.database.c_str(),
					e.what() );
		}
	} while (!isConnected && primaryDBConfig.gotoNextServer());

	if (isConnected)
	{
		isOK = this->init( primaryDBConfig.getCurServer().connectionInfo );
	}
	else
	{
		isOK = false;
	}

	return isOK;
}

/**
 * 	This function should be called after constructor to initialise the object.
 * 	Uses the provided primary database connection information instead of
 * 	reading it from bw.xml.
 * 	Returns true if initialisation succeeded, false if it failed.
 */
bool DBConsolidator::init( const DBConfig::Connection& primaryDBConnectionInfo )
{
	// Connect to primary database.
	if (!DBConsolidator::connect( primaryDBConnectionInfo,
			pPrimaryDBConnection_, primaryDBLock_ ))
	{
		return false;
	}

	TRACE_MSG( "DBConsolidator: Connected to primary database: "
			"host=%s:%d, username=%s, database=%s\n",
			primaryDBConnectionInfo.host.c_str(),
			primaryDBConnectionInfo.port,
			primaryDBConnectionInfo.username.c_str(),
			primaryDBConnectionInfo.database.c_str() );

	if (!Script::init( EntityDef::Constants::databasePath(), "database" ))
	{
		return false;
	}

	// Init entity definitions
	std::string defaultTypeName;
	std::string defaultNameProperty;
	BWConfig::update( "dbMgr/entityType", defaultTypeName );
	BWConfig::update( "dbMgr/nameProperty", defaultNameProperty );

	DataSectionPtr pSection =
			BWResource::openSection( EntityDef::Constants::entitiesFile() );

	if (!pSection)
	{
		ERROR_MSG( "DBConsolidator::init: Failed to open "
				"<res>/%s\n", EntityDef::Constants::entitiesFile() );
		return false;
	}

	if (!entityDefs_.init( pSection, defaultTypeName, defaultNameProperty ))
	{
		return false;
	}

	if (!this->checkEntityDefsMatch( *pPrimaryDBConnection_ ))
	{
		ERROR_MSG( "DBConsolidator::init: Our entity definitions do not match "
				"the ones used by the primary database\n"
				"Database consolidation should be run before making changes to "
				"entity definitions. Changing entity definitions potentially "
				"invalidates unconsolidated data.\n"
				"Run \"consolidate_dbs --clear\" to allow the server to "
				"run without doing data consolidation. Unconsolidated data "
				"will be lost.\n" );
		return false;
	}

	// Init entity type mappings
	try
	{
		createEntityMappings( entityTypeMappings_, entityDefs_,
				TABLE_NAME_PREFIX, *pPrimaryDBConnection_ );
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "DBConsolidator::init: Failed to create entity to "
				"database mapping objects: %s\n", e.what() );
		return false;
	}

	BWConfig::update( "dbMgr/consolidation/directory", consolidationDir_ );
	IFileSystem::FileType consolidationDirType =
			BWResource::resolveToAbsolutePath( consolidationDir_ );
	if (consolidationDirType != IFileSystem::FT_DIRECTORY)
	{
		ERROR_MSG( "DBConsolidator::init: Configuration setting "
				"dbMgr/consolidation/directory specifies a non-existent "
				"directory: %s\n", consolidationDir_.c_str() );
		return false;
	}

	return true;
}

/**
 * 	After initialisation, this method starts the data consolidation process
 */
bool DBConsolidator::run()
{
	bool isOK;
	// Get it secondary DB info from primary database.
	SecondaryDBInfos secondaryDBs;
	isOK = this->getSecondaryDBInfos( secondaryDBs );
	if (!isOK)
	{
		return false;
	}

	if (secondaryDBs.empty())
	{
		ERROR_MSG( "DBConsolidator::run: No secondary databases to "
				"consolidate\n" );
		return false;
	}

	// Start listening for incoming connections
	FileReceiverMgr	fileReceiverMgr( nub_, secondaryDBs, consolidationDir_ );
	TcpListener< FileReceiverMgr > connectionsListener( fileReceiverMgr );
	if (!connectionsListener.init( 0, nub_.address().ip, secondaryDBs.size() ))
	{
		return false;
	}

	// Make our address:port into a string to pass to child processes
	Mercury::Address ourAddr;
	connectionsListener.getBoundAddr( ourAddr );
	char	ourAddrStr[15+1+5+1];	// nnn.nnn.nnn.nnn:nnnnn
	ourAddr.writeToString( ourAddrStr, sizeof( ourAddrStr ) );

	// Start remote file transfer service
	for ( SecondaryDBInfos::const_iterator i = secondaryDBs.begin();
			i != secondaryDBs.end(); ++i )
	{
		const ssize_t argc = 3;
		const char * argv[ argc ];
		argv[0] = "consolidate";
		argv[1] = i->location.c_str();
		argv[2] = ourAddrStr;
		if (!this->startRemoteProcess( i->hostIP, "commands/transfer_db",
				argc, argv ))
		{
			return false;
		}
	}

	{
		DBFileTransferErrorMonitor errorMonitor( fileReceiverMgr );

		// Wait for file transfer to complete
		nub_.processUntilBreak();
	}

	isOK = fileReceiverMgr.finished();
	if (isOK)
	{
		// Consolidate databases
		const FileNames& dbFilePaths = fileReceiverMgr.receivedFilePaths();
		isOK = this->consolidateSecondaryDBs( dbFilePaths );

		if (isOK)
		{
			fileReceiverMgr.cleanUpRemoteFiles( consolidationErrors_ );
			this->cleanUp();
			TRACE_MSG( "DBConsolidator::run: Completed successfully\n" );
		}
	}

	fileReceiverMgr.cleanUpLocalFiles();

	return isOK;
}

/**
 *	Consolidates the secondary databases pointed to by filePath into the
 * 	primary database.
 */
bool DBConsolidator::consolidateSecondaryDBs( const FileNames & filePaths )
{
	ProgressReporter progressReporter( *this, filePaths.size() );

	for ( FileNames::const_iterator i = filePaths.begin();
			i != filePaths.end(); ++i )
	{
		// TODO: Use wrapInTransaction()
		MySqlTransaction transaction( *pPrimaryDBConnection_ );

		if (!this->consolidateSecondaryDB( transaction, *i, progressReporter ))
		{
			if (shouldAbort_)
			{
				TRACE_MSG( "DBConsolidator::consolidateSecondaryDBs: Data "
						"consolidation was aborted\n" );
			}
			else
			{
				WARNING_MSG( "DBConsolidator::consolidateSecondaryDBs: Some "
						"entities were not consolidated. Data consolidation "
						"must be re-run after errors have been corrected.\n" );
			}
			return false;
		}

		transaction.commit();
	}

	return true;
}

/**
 * 	Get list of secondary DBs from the primary DB.
 */
bool DBConsolidator::getSecondaryDBInfos( SecondaryDBInfos& secondaryDBInfos )
{
	bool isOK = true;
	try
	{
		MySqlUnPrep::Statement	getStmt( *pPrimaryDBConnection_,
				"SELECT ip,location FROM bigworldSecondaryDatabases" );
		uint32					ip;
		MySqlBuffer				location( MAX_SECONDARY_DB_LOCATION_LENGTH );
		MySqlUnPrep::Bindings	bindings;
		bindings << ip << location;
		getStmt.bindResult( bindings );

		pPrimaryDBConnection_->execute( getStmt );

		while (getStmt.fetch())
		{
			secondaryDBInfos.push_back( SecondaryDBInfo( htonl( ip ),
					location.getString() ) );
		}
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "DBConsolidator::getSecondaryDBInfos: Failed to get "
				"secondary DB information from primary database: %s\n",
				e.what() );
		isOK = false;
	}

	return isOK;
}

/**
 *	Starts a process on the specified machine (IP address)
 */
bool DBConsolidator::startRemoteProcess( uint32 remoteIP, const char * command,
		int argc, const char **argv )
{
	CreateWithArgsMessage cm;
	cm.uid_ = getUserId();
#if defined _DEBUG
	cm.config_ = "Debug";
#elif defined _HYBRID
	cm.config_ = "Hybrid";
#endif
	cm.recover_ = 0;
	cm.name_ = command;
	cm.fwdIp_ = 0;
	cm.fwdPort_ = 0;

	for (int i = 0; i < argc; ++i)
	{
		cm.args_.push_back( argv[i] );
	}

	shouldAbort_ = false;
	if (cm.sendAndRecv( 0, remoteIP, this ) != Mercury::REASON_SUCCESS)
	{
		ERROR_MSG( "DBConsolidator::startRemoteProcess: "
			"Could not send CreateWithArgs request to %s.\n",
			inet_ntoa( (in_addr&)remoteIP ) );
		return false;
	}

	// shouldAbort_ magically set by onPidMessage() callback.
	return !shouldAbort_;
}

/**
 *	This method is called when a remote process to transfer the secondary
 * 	DB file is started.
 */
bool DBConsolidator::onPidMessage( PidMessage &pm, uint32 addr )
{
	in_addr	address;
	address.s_addr = addr;

	if (pm.running_)
	{
		TRACE_MSG( "DBConsolidator::onPidMessage: Started remote file transfer "
				"process %hd on %s\n", pm.pid_, inet_ntoa( address ) );
	}
	else
	{
		ERROR_MSG( "DBConsolidator::onPidMessage: Failed to start remote file "
				"transfer process on %s\n", inet_ntoa( address ) );
		shouldAbort_ = true;
	}

	return false;	// Stop waiting for more responses. We only expect one.
}

/**
 * 	This function initialises the dbMgrAddr_ member with the address of the
 * 	DBMgr.
 */
void DBConsolidator::initDBMgrAddr()
{
	ProcessStatsMessage	psm;
	psm.param_ = ProcessMessage::PARAM_USE_CATEGORY |
				ProcessMessage::PARAM_USE_UID |
				ProcessMessage::PARAM_USE_NAME;
	psm.category_ = psm.WATCHER_NUB;
	psm.uid_ = getUserId();
	psm.name_ = "dbmgr";

	// onProcessStatsMessage() will be called inside sendAndRecv().
	if (psm.sendAndRecv( 0, BROADCAST, this ) != Mercury::REASON_SUCCESS)
	{
		ERROR_MSG( "initDBMgrAddr: Could not send request.\n" );
	}
	else if (dbMgrAddr_.isNone())
	{
		INFO_MSG( "DBConsolidator::initDBMgrAddr: No DBMgrs running\n" );
	}
}

/**
 *	This method is called to provide us with information about the DBMgr
 * 	running on our cluster.
 */
bool DBConsolidator::onProcessStatsMessage( ProcessStatsMessage &psm,
		uint32 addr )
{
	if (psm.pid_ == 0)	// DBMgr not found on the machine
	{
		return true;
	}

	if (dbMgrAddr_.isNone())
	{
		dbMgrAddr_.ip = addr;
		dbMgrAddr_.port = psm.port_;

		char	addrStr[15+1+5+1];	// nnn.nnn.nnn.nnn:nnnnn
		dbMgrAddr_.writeToString( addrStr, sizeof( addrStr ) );
		TRACE_MSG( "DBConsolidator::onProcessStatsMessage: Found DBMgr at %s\n",
				addrStr );
	}
	else
	{
		Mercury::Address dbMgrAddr( addr, psm.port_ );
		char	addrStr[15+1+5+1];	// nnn.nnn.nnn.nnn:nnnnn
		dbMgrAddr.writeToString( addrStr, sizeof( addrStr ) );
		WARNING_MSG( "DBConsolidator::onProcessStatsMessage: Already found a "
				"DBMgr. Ignoring DBMgr at %s\n", addrStr );
	}

	return true;
}

/**
 *	Consolidates the secondary database pointed to by filePath into the
 * 	primary database.
 */
bool DBConsolidator::consolidateSecondaryDB( MySqlTransaction& transaction,
		const std::string& filePath, ProgressReporter& progressReporter )
{
	// Open db file
	int result;
	SqliteConnection secondaryDBConnection( filePath.c_str(), result );
	if (result != SQLITE_OK)
	{
		ERROR_MSG( "DBConsolidator::consolidateSecondaryDB: Could not open "
				"'%s'\n", filePath.c_str() );
		return false;
	}

	TRACE_MSG( "DBConsolidator::consolidateSecondaryDB: Consolidating '%s'\n",
			filePath.c_str() );

	if (!checkEntityDefsMatch( secondaryDBConnection ))
	{
		ERROR_MSG( "DBConsolidator::consolidateSecondaryDB: %s failed entity "
				"digest check\n", filePath.c_str() );
		return false;
	}

	int numEntities = 0;

	// Make select statements
	typedef std::vector< sqlite3_stmt* > SQLiteStatements;
	SQLiteStatements selectStatements;
	const char * tableNames[] = { "tbl_flip", "tbl_flop" };
	for (size_t i = 0; i < sizeof(tableNames)/sizeof(tableNames[0]); ++i)
	{
		// Check table exists
		std::stringstream ss;
		ss << "SELECT * FROM " << tableNames[i] << " WHERE 0";
		if (sqlite3_exec( secondaryDBConnection.get(), ss.str().c_str(), NULL,
				NULL, NULL ) == SQLITE_OK)
		{
			// Make select all statement
			std::stringstream ss;
			ss << SELECT_DATA_FROM_SECDB << tableNames[i];
			sqlite3_stmt* selectAllStmt;
			MF_VERIFY( sqlite3_prepare_v2( secondaryDBConnection.get(),
					ss.str().c_str(), -1, &selectAllStmt,
					NULL ) == SQLITE_OK );
			selectStatements.push_back( selectAllStmt );

			int numRows =
					this->getNumRows( secondaryDBConnection, tableNames[i] );
			if (numRows > 0)
			{
				numEntities += numRows;
			}
		}
	}

	progressReporter.onStartConsolidateDB( BWResource::getFilename( filePath ),
			numEntities );

	// Have a good guess about which table is older and do the younger one
	// first.
	this->orderTableByAge( selectStatements );

	// Consolidate!
	bool isOK = true;
	bool hasIgnoredErrors = false;
	for ( SQLiteStatements::iterator i = selectStatements.begin();
			i != selectStatements.end() && isOK; ++i )
	{
		isOK = this->consolidateSecondaryDBTable( secondaryDBConnection,
				*i, transaction, progressReporter );
		if (!isOK)
		{
			consolidationErrors_.addSecondaryDB( filePath );
			if (!shouldStopOnError_)
			{
				isOK = true;
				hasIgnoredErrors = true;
			}
		}
	}

	if (isOK)
	{
		TRACE_MSG( "DBConsolidator::consolidateSecondaryDB: Consolidated '%s'\n",
				filePath.c_str() );
	}
	else if (!shouldAbort_)
	{
		ERROR_MSG( "DBConsolidator::consolidateSecondaryDB: Error while "
				"consolidating %s\n", filePath.c_str() );
	}

	// Clean-up
	for ( SQLiteStatements::iterator i = selectStatements.begin();
			i != selectStatements.end(); ++i )
	{
		// If there were errors, there's a chance that sqlite3_finalize()
		// won't be successful.
		MF_VERIFY( (sqlite3_finalize( *i ) == SQLITE_OK) || !isOK ||
				hasIgnoredErrors );
	}

	return isOK;
}

/**
 *	Returns true if the given quoted MD5 digest matches the entity definition
 * 	digest that we've currently loaded
 */
bool DBConsolidator::checkEntityDefsDigestMatch(
		const std::string& quotedDigest )
{
	MD5::Digest	digest;
	if (!digest.unquote( quotedDigest ))
	{
		ERROR_MSG( "DBConsolidator::checkEntityDefsDigestMatch: "
				"Not a valid MD5 digest\n" );
		return false;
	}

	return entityDefs_.getPersistentPropertiesDigest() == digest;
}

/**
 * 	Returns true if our entity definitions matches the ones used by the
 *  primary database when the system was last started.
 */
bool DBConsolidator::checkEntityDefsMatch( MySql& connection )
{
	try
	{
		MySqlUnPrep::Statement getChecksumStmt( connection,
				"SELECT checksum FROM bigworldEntityDefsChecksum" );

		MySqlBuffer	checkSumBuf( 255 );
		MySqlUnPrep::Bindings bindings;
		bindings << checkSumBuf;

		getChecksumStmt.bindResult( bindings );

		connection.execute( getChecksumStmt );

		if (getChecksumStmt.fetch())
		{
			return this->checkEntityDefsDigestMatch( checkSumBuf.getString() );
		}
		else
		{
			ERROR_MSG( "DBConsolidator::checkEntityDefsMatch: "
					"Checksum table is empty\n" );
		}
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "DBConsolidator::checkEntityDefsMatch: Failed to retrieve "
				"the primary database entity definition checksum: %s\n",
				e.what() );
	}
	return false;
}

/**
 * 	Returns true if our entity definitions matches the ones used when
 * 	the secondary database was created.
 */
bool DBConsolidator::checkEntityDefsMatch( SqliteConnection& connection )
{
	int result;
	SqliteStatement getChecksumStmt( connection,
			"SELECT "CHECKSUM_COLUMN_NAME" FROM "CHECKSUM_TABLE_NAME, result );
	if( result != SQLITE_OK )
	{
		ERROR_MSG( "DBConsolidator::checkEntityDefsMatch: "
				"Failed to open checksum table\n" );
		return false;
	}

	if (getChecksumStmt.step() != SQLITE_ROW)
	{
		ERROR_MSG( "DBConsolidator::checkEntityDefsMatch: "
				"Checksum table is empty\n" );
		return false;
	}

	std::string quotedDigest(
			reinterpret_cast<const char*>( getChecksumStmt.textColumn( 0 ) ) );
	return this->checkEntityDefsDigestMatch( quotedDigest );
}

/**
 * 	Returns the number of rows in the given table in the given SQLite database.
 */
int DBConsolidator::getNumRows( SqliteConnection& connection,
		const std::string& tblName )
{
	std::stringstream ss;
	ss << "SELECT COUNT(*) FROM " << tblName;

	int result;
	SqliteStatement getNumRowsStmt( connection, ss.str().c_str(), result );
	if( result != SQLITE_OK )
	{
		ERROR_MSG( "DBConsolidator::getNumRows: "
				"Failed to get the number of rows from %s\n", tblName.c_str() );
		return -1;
	}

	MF_VERIFY(getNumRowsStmt.step() == SQLITE_ROW);
	return getNumRowsStmt.intColumn( 0 );
}

/**
 *	Returns table1 and table2 in with the younger table in result.first and
 * 	the older (less up to date) table in result.second.
 */
void DBConsolidator::orderTableByAge( SQLiteStatements& tables )
{
	if (tables.size() <= 1)
	{
		return;	// nothing to do
	}

	// Base guess on the game time in the first row in each table.
	// Greater game time means more recently written to i.e. younger.
	typedef std::multimap< TimeStamp, sqlite3_stmt* > OrderedTables;
	OrderedTables orderedTables;
	for (SQLiteStatements::iterator i = tables.begin(); i != tables.end(); ++i)
	{
		TimeStamp time = 0;
		if (sqlite3_step( *i ) == SQLITE_ROW)
		{
			time = sqlite3_column_int( *i, SelectStmtTime );
		}
		// Reset statement so that it starts from the first row again.
		MF_VERIFY( sqlite3_reset( *i ) == SQLITE_OK );
		orderedTables.insert( std::make_pair( time, *i ) );
	}

	MF_ASSERT( orderedTables.size() == tables.size() );

	// Now put it back into tables variable in order of greatest timestamp
	// first
	SQLiteStatements::iterator i = tables.begin();
	for (OrderedTables::reverse_iterator j = orderedTables.rbegin();
			j != orderedTables.rend(); ++i, ++j)
	{
		*i = j->second;
	}
}

/**
 * 	Consolidates all data from a table in the secondary database. The select
 * 	statement will return data from that table.
 */
bool DBConsolidator::consolidateSecondaryDBTable( SqliteConnection& connection,
		sqlite3_stmt* selectStmt,
		MySqlTransaction& transaction, ProgressReporter& progressReporter )
{
	// For each row...
	int stepRes;
	while ( !shouldAbort_ &&
			((stepRes = sqlite3_step( selectStmt )) == SQLITE_ROW) )
	{
		// Do this at the start because of various "continue" statements.
		progressReporter.onConsolidatedRow();

		// Read row data
		DatabaseID		dbID 	=
				sqlite3_column_int64( selectStmt, SelectStmtDbID );
		EntityTypeID	typeID 	=
				EntityTypeID( sqlite3_column_int( selectStmt, SelectStmtTypeID ) );
		TimeStamp		time 	=
				sqlite3_column_int( selectStmt, SelectStmtTime );

		// Check if we've already written a newer version of this entity.
		EntityKey	entityKey( typeID, dbID );
		ConsolidatedTimes::const_iterator pConsolidateTime =
				consolidatedTimes_.find( entityKey );
		if ((pConsolidateTime != consolidatedTimes_.end()) &&
				(time <= pConsolidateTime->second))
		{
			continue;
		}
		consolidatedTimes_[ entityKey ] = time;

		MemoryIStream	data( sqlite3_column_blob( selectStmt, SelectStmtBlob ),
				sqlite3_column_bytes( selectStmt, SelectStmtBlob ) );
		MemoryIStream	metaData( &time, sizeof(time) );

		// Write entity into primary database
		MySqlEntityTypeMapping& entityTypeMapping = *entityTypeMappings_[typeID];
		entityTypeMapping.setDBID( dbID );
		entityTypeMapping.streamEntityPropsToBound( data );
		entityTypeMapping.streamMetaPropsToBound( metaData );

		try
		{
			if (!entityTypeMapping.update( transaction ))
			{
				ERROR_MSG( "DBConsolidator::consolidateSecondaryDBTable: "
						"Failed to update %s entity %"FMT_DBID": "
						"Entity does not exist?\n",
						entityTypeMapping.getEntityDescription().name().c_str(),
						dbID );

				return false;
			}
		}
		catch (std::exception& e)
		{
			ERROR_MSG( "DBConsolidator::consolidateSecondaryDBTable: "
					"Failed to update %s entity %"FMT_DBID": %s\n",
					entityTypeMapping.getEntityDescription().name().c_str(),
					dbID, e.what() );
			return false;
		}
	}

	bool isOK = (stepRes == SQLITE_DONE);
	if (!isOK && !shouldAbort_)
	{
		ERROR_MSG( "DBConsolidator::consolidateSecondaryDBTable: "
				"SQLite error: %s\n", connection.lastError() );
	}

	return isOK;
}

/**
 * 	Sets DBMgr detailed status watcher.
 */
void DBConsolidator::setDBMgrStatusWatcher( const std::string& status )
{
	if (!dbMgrAddr_.isNone())
	{
		MemoryOStream	strm( status.size() + 32 );
		// Stream on WatcherDataMsg
		strm << int( WATCHER_MSG_SET2 ) << int(1); // message type and count
		strm << uint32(0);		// Sequence number. We don't care about it.
		// Add watcher path
		strm.addBlob( DBSTATUS_WATCHER_STATUS_DETAIL_PATH,
				strlen( DBSTATUS_WATCHER_STATUS_DETAIL_PATH ) + 1 );
		// Add data
		strm << uchar(WATCHER_TYPE_STRING);
		strm << status;

		watcherNub_.socket().sendto( strm.data(), strm.size(),
				dbMgrAddr_.port, dbMgrAddr_.ip );
	}
}

/**
 *	Perform clean-up operations after secondary databases have been
 * 	consolidated successfully.
 */
void DBConsolidator::cleanUp()
{
	uint numEntries;
	DBConsolidator::clearSecondaryDBEntries( *pPrimaryDBConnection_,
			numEntries );
}

/**
 *	Aborts the consolidation process.
 */
void DBConsolidator::abort()
{
	nub_.breakProcessing();
	shouldAbort_ = true;
}

/**
 * 	This static method connects to the database and obtains BigWorld's
 * 	lock on the database. It returns the connection in pConnection and the
 * 	lock in pLock. Returns true if successful, false if either the connection
 * 	or obtaining the lock failed.
 */
bool DBConsolidator::connect( const DBConfig::Connection& connectionInfo,
		std::auto_ptr<MySql>& pConnection,
		std::auto_ptr<MySQL::NamedLock>& pLock )
{
	try
	{
		std::auto_ptr< MySql > pTempConnection( new MySql( connectionInfo ) );
		pLock.reset( new MySQL::NamedLock( *pTempConnection,
				connectionInfo.generateLockName() ) );
		pConnection = pTempConnection;
	}
	catch (MySQL::NamedLock::Exception& e)
	{
		ERROR_MSG( "DBConsolidator::connect: Database %s on %s:%d is being "
				"used by another BigWorld process\n",
				connectionInfo.database.c_str(),
				connectionInfo.host.c_str(),
				connectionInfo.port );
		return false;
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "DBConsolidator::connect: Failed to connect to "
				"%s:%d (%s): %s\n",
				connectionInfo.host.c_str(),
				connectionInfo.port,
				connectionInfo.database.c_str(),
				e.what() );
		return false;
	}

	return true;
}


/**
 * 	This static method clears the secondary DB entries from the primary
 * 	database. Returns the number of entries deleted in numEntriesCleared.
 */
bool DBConsolidator::clearSecondaryDBEntries( MySql& connection,
		uint& numEntriesCleared )
{
	try
	{
		MySqlTransaction transaction( connection );

		connection.execute( "DELETE FROM bigworldSecondaryDatabases" );

		numEntriesCleared = uint32( connection.affectedRows() );

		transaction.commit();
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "DBConsolidator::clearSecondaryDBEntries: %s", e.what() );
		return false;
	}

	return true;
}

/**
 * 	This static method clears the secondary DB entries from the primary
 * 	database.
 */
bool DBConsolidator::connectAndClearSecondaryDBEntries()
{
	DBConfig::Server 	primaryDBConfig;
	const DBConfig::Server::ServerInfo& serverInfo =
			primaryDBConfig.getCurServer();

	std::auto_ptr<MySql> 			pConnection;
	std::auto_ptr<MySQL::NamedLock> pLock;
	if (!DBConsolidator::connect( serverInfo.connectionInfo, pConnection,
			pLock ))
	{
		return false;
	}

	uint numEntries = 0;
	if (!DBConsolidator::clearSecondaryDBEntries( *pConnection, numEntries ))
	{
		return false;
	}

	TRACE_MSG( "DBConsolidator::connectAndClearSecondaryDBEntries: "
			"Cleared %u entries from %s:%d (%s)\n",
			numEntries,
			serverInfo.connectionInfo.host.c_str(),
			serverInfo.connectionInfo.port,
			serverInfo.connectionInfo.database.c_str() );

	return true;
}
