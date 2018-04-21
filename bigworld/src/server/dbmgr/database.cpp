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

#include "database.hpp"
#include "db_config.hpp"
#include "db_entitydefs.hpp"
#include "db_interface.hpp"
#include "db_interface_utils.hpp"
#include "entity_recoverer.hpp"

#include "baseappmgr/baseappmgr_interface.hpp"
#include "baseapp/baseapp_int_interface.hpp"

#include "cstdmf/memory_stream.hpp"

#include "resmgr/bwresource.hpp"
#include "resmgr/dataresource.hpp"
#include "resmgr/bwresource.hpp"

#include "entitydef/constants.hpp"

#include "server/bwconfig.hpp"
#include "server/reviver_subject.hpp"
#include "server/writedb.hpp"

#include "network/watcher_glue.hpp"
#include "network/portmap.hpp"
#include "network/blocking_reply_handler.hpp"

#include "pyscript/py_output_writer.hpp"

#ifdef USE_ORACLE
#include "oracle_database.hpp"
#endif

#ifdef USE_MYSQL
#include "mysql_database.hpp"
#endif

#ifdef USE_XML
#include "xml_database.hpp"
#endif

#include "resmgr/xml_section.hpp"

#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <pwd.h>

#ifndef CODE_INLINE
#include "database.ipp"
#endif

#include "custom.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

/// DBMgr Singleton.
BW_SINGLETON_STORAGE( Database )

// -----------------------------------------------------------------------------
// Section: Constants
// -----------------------------------------------------------------------------
namespace
{

const char * DEFAULT_ENTITY_TYPE_STR = "Avatar";
const char * DEFAULT_NAME_PROPERTY_STR = "playerName";
const char * UNSPECIFIED_ERROR_STR = "Unspecified error";

#define CONSOLIDATE_DBS_FILENAME_STR 	"consolidate_dbs"
#define CONSOLIDATE_DBS_RELPATH_STR 	"commands/"CONSOLIDATE_DBS_FILENAME_STR
const int CONSOLIDATE_DBS_EXEC_FAILED_EXIT_CODE = 100;

} // end anonymous namespace

// -----------------------------------------------------------------------------
// Section: Utility Functions
// -----------------------------------------------------------------------------
namespace
{

/**
 * 	Gets our full executable path.
 */
std::string getOurExePath()
{
	// NOTE: This function only works on Linux. On other flavours of Unix,
	// the structure of the /proc filesystem is a bit different.
	pid_t pid = getpid();

	char linkPath[64];
	snprintf( linkPath, sizeof(linkPath), "/proc/%i/exe", pid );

	// Read the symbolic link
	char fullExePath[1024];
	int ret = readlink( linkPath, fullExePath, sizeof(fullExePath) );

	if ((ret == -1) || (ret >= int(sizeof(fullExePath))))
	{
		return std::string();
	}

	return std::string( fullExePath, ret );
}

/**
 * 	Gets our executable directory.
 */
std::string getOurExeDir()
{
	std::string ourExePath = getOurExePath();
	std::string::size_type slashPos = ourExePath.find_last_of( '/' );
	return std::string( ourExePath, 0, slashPos );
}

}	// end anonymous namespace

// -----------------------------------------------------------------------------
// Section: Functions
// -----------------------------------------------------------------------------
namespace
{

static void signalHandler( int sigNum )
{
	Database * pDB = Database::pInstance();

	if (pDB != NULL)
	{
		pDB->onSignalled( sigNum );
	}
}

bool commandShutDown( std::string & output, std::string & value )
{
	Database * pDB = Database::pInstance();
	if (pDB != NULL)
	{
		pDB->shutDown();
	}

	return true;
}

} // end anonymous namespace

// extern int Math_token;
extern int ResMgr_token;
static int s_moduleTokens =
	/* Math_token |*/ ResMgr_token;
extern int PyPatrolPath_token;
static int s_patrolToken = PyPatrolPath_token;


// -----------------------------------------------------------------------------
// Section: Database
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
Database::Database( Mercury::Nub & nub ) :
	nub_( nub ),
	workerThreadMgr_( nub_ ),
	pEntityDefs_( NULL ),
	pDatabase_( NULL ),
	signals_(),
	status_(),
	baseAppMgr_( nub_ ),
	shouldLoadUnknown_( true ),
	shouldCreateUnknown_( true ),
	shouldRememberUnknown_( true ),
	pServerConfig_(),
	allowEmptyDigest_( true ), // Should probably default to false.
	shouldSendInitData_( false ),
	shouldConsolidate_( true ),
	desiredBaseApps_( 1 ),
	desiredCellApps_( 1 ),
	statusCheckTimerId_( Mercury::TIMER_ID_NONE),
	clearRecoveryDataOnStartUp_( true ),
	writeEntityTimer_( 5 ),
	curLoad_( 1.f ),
	maxLoad_( 1.f ),
	anyCellAppOverloaded_( true ),
	overloadStartTime_( 0 ),
	mailboxRemapCheckCount_( 0 ),
	secondaryDBPrefix_(),
	secondaryDBIndex_( 0 ),
	consolidatePid_( 0 ),
	isProduction_( BWConfig::get( "production", false ) )
{
	// The channel to the BaseAppMgr is irregular
	baseAppMgr_.channel().isIrregular( true );
}


/**
 *	Destructor.
 */
Database::~Database()
{
	delete pDatabase_;
	// Destroy entity descriptions before calling Script::fini() so that it
	// can clean up any PyObjects that it may have.
	delete pEntityDefs_;
	DataType::clearStaticsForReload();
	Script::fini();
	pDatabase_ = NULL;
}


/**
 *	This method initialises this object. It should be called before any other
 *	method.
 */
Database::InitResult Database::init( bool isUpgrade, bool isSyncTablesToDefs )
{
	if (nub_.socket() == -1)
	{
		ERROR_MSG( "Database::init: Failed to create Nub on internal "
			"interface.\n");
		return InitResultFailure;
	}

	if (isProduction_)
	{
		INFO_MSG( "Database::init: Production mode enabled.\n" );
	}

	ReviverSubject::instance().init( &nub_, "dbMgr" );

	if (!Script::init( EntityDef::Constants::databasePath(), "database" ))
	{
		return InitResultFailure;
	}

	std::string defaultTypeName = DEFAULT_ENTITY_TYPE_STR;
	std::string nameProperty;

	int dumpLevel = 0;

	BWConfig::update( "dbMgr/allowEmptyDigest", allowEmptyDigest_ );
	if ( (allowEmptyDigest_) && (isProduction_) )
	{
		ERROR_MSG( "Database::init: Production Mode: Allowing client "
			"connections with empty entity definition digests! This is "
			"a potential security risk.\n" );
	}
	BWConfig::update( "dbMgr/loadUnknown", shouldLoadUnknown_ );
	BWConfig::update( "dbMgr/createUnknown", shouldCreateUnknown_ );
	BWConfig::update( "dbMgr/rememberUnknown", shouldRememberUnknown_ );

	BWConfig::update( "dbMgr/entityType", defaultTypeName );
	BWConfig::update( "dbMgr/nameProperty", nameProperty );

	pServerConfig_.reset( new DBConfig::Server() );

	if (nameProperty.empty())
	{
		nameProperty = DEFAULT_NAME_PROPERTY_STR;
	}
	else
	{
		INFO_MSG( "dbMgr/nameProperty has been deprecated. Please add the "
					"attribute <Identifier> true </Identifier> to the name "
					"property of the entity\n" );
	}

	BWConfig::update( "dbMgr/dumpEntityDescription", dumpLevel );

	BWConfig::update( "desiredBaseApps", desiredBaseApps_ );
	BWConfig::update( "desiredCellApps", desiredCellApps_ );

	BWConfig::update( "dbMgr/clearRecoveryData", clearRecoveryDataOnStartUp_ );

	BWConfig::update( "dbMgr/overloadLevel", maxLoad_ );
	allowOverloadPeriod_ = uint64( stampsPerSecondD() *
			BWConfig::get( "dbMgr/overloadTolerancePeriod", 5.f ) );

	PyOutputWriter::overrideSysMembers(
		BWConfig::get( "dbMgr/writePythonLog", false ) );

	// Generate the run ID.
	// __kyl__(24/4/2008) Theoretically, using local time will not generate
	// a unique run ID across daylight savings transitions. But what are the
	// chances that that the server is restarted in the same second an hour
	// later?
	time_t epochTime = time( NULL );
	tm timeAndDate;
	localtime_r( &epochTime, &timeAndDate );

	// Get username for run ID
	uid_t 		uid = getuid();
	passwd* 	pUserDetail = getpwuid( uid );
	std::string username;
	if (pUserDetail)
	{
		username = pUserDetail->pw_name;
	}
	else
	{
		WARNING_MSG( "Database::init: Using '%hd' as the username due to uid "
				"to name lookup failure\n", uid );
		std::stringstream ss;
		ss << uid;
		username = ss.str();
	}

	BW_INIT_WATCHER_DOC( "dbmgr" );

	MF_WATCH( "allowEmptyDigest",	allowEmptyDigest_ );
	MF_WATCH( "createUnknown",		shouldCreateUnknown_ );
	MF_WATCH( "rememberUnknown",	shouldRememberUnknown_ );
	MF_WATCH( "loadUnknown",		shouldLoadUnknown_ );
	MF_WATCH( "isReady", *this, &Database::isReady );
	status_.registerWatchers();

	MF_WATCH( "hasStartBegun",
			*this, MF_ACCESSORS( bool, Database, hasStartBegun ) );

	MF_WATCH( "desiredBaseApps", desiredBaseApps_ );
	MF_WATCH( "desiredCellApps", desiredCellApps_ );

	MF_WATCH( "clearRecoveryDataOnStartUp", clearRecoveryDataOnStartUp_ );

	MF_WATCH( "performance/writeEntity/performance", writeEntityTimer_,
		Watcher::WT_READ_ONLY );
	MF_WATCH( "performance/writeEntity/rate", writeEntityTimer_,
		&Mercury::Nub::TransientMiniTimer::getCountPerSec );
	MF_WATCH( "performance/writeEntity/duration", (Mercury::Nub::MiniTimer&) writeEntityTimer_,
		&Mercury::Nub::MiniTimer::getAvgDurationSecs );

	MF_WATCH( "load", curLoad_, Watcher::WT_READ_ONLY );
	MF_WATCH( "overloadLevel", maxLoad_ );

	MF_WATCH( "anyCellAppOverloaded", anyCellAppOverloaded_ );

	// Command watcher to shutdown DBMgr
	Watcher::rootWatcher().addChild( "command/shutDown",
			new NoArgCallableWatcher( commandShutDown,
				CallableWatcher::LOCAL_ONLY,
				"Shuts down DBMgr" ) );

	DataSectionPtr pSection =
		BWResource::openSection( EntityDef::Constants::entitiesFile() );

	if (!pSection)
	{
		ERROR_MSG( "Database::init: Failed to open "
				"<res>/%s\n", EntityDef::Constants::entitiesFile() );
		return InitResultFailure;
	}

	status_.set( DBStatus::STARTING, "Loading entity definitions" );

	pEntityDefs_ = new EntityDefs();
	if ( !pEntityDefs_->init( pSection, defaultTypeName, nameProperty ) )
	{
		return InitResultFailure;
	}

	// Check that dbMgr/entityType is valid. Unless dbMgr/shouldLoadUnknown and
	// dbMgr/shouldCreateUnknown is false, in which case we don't use
	// dbMgr/entityType anyway so don't care if it's invalid.
	if ( !pEntityDefs_->isValidEntityType( pEntityDefs_->getDefaultType() ) &&
	     (shouldLoadUnknown_ || shouldCreateUnknown_) )
	{
		ERROR_MSG( "Database::init: Invalid dbMgr/entityType '%s'. "
				"Consider changing dbMgr/entityType in bw.xml\n",
				defaultTypeName.c_str() );
		return InitResultFailure;
	}

	pEntityDefs_->debugDump( dumpLevel );

	// Initialise the watcher
	BW_REGISTER_WATCHER( 0, "dbmgr", "DBMgr", "dbMgr", nub_ );

	Watcher::rootWatcher().addChild( "nub", Mercury::Nub::pWatcher(),
		&this->nub_ );

	std::string databaseType = BWConfig::get( "dbMgr/type", "xml" );

#ifdef USE_XML
	if (databaseType == "xml")
	{
		pDatabase_ = new XMLDatabase();

		shouldConsolidate_ = false;
	} else
#endif
#ifdef USE_ORACLE
	if (databaseType == "oracle")
	{
		char * oracle_home = getenv( "ORACLE_HOME" );

		if (oracle_home == NULL)
		{
			INFO_MSG( "ORACLE_HOME not set. Setting to /home/local/oracle\n" );
			putenv( "ORACLE_HOME=/home/local/oracle" );
		}

		pDatabase_ = new OracleDatabase();
	}
	else
#endif
#ifdef USE_MYSQL
	if (databaseType == "mysql")
	{
		// pDatabase_ = new MySqlDatabase();
		pDatabase_ = MySqlDatabase::create();

		if (pDatabase_ == NULL)
		{
			return InitResultFailure;
		}
	}
	else
#endif
	{
		ERROR_MSG("Unknown database type: %s\n", databaseType.c_str());
#ifndef USE_MYSQL
		if (databaseType == "mysql")
		{
			INFO_MSG( "DBMgr needs to be rebuilt with MySQL support. See "
					"the Server Installation Guide for more information\n" );
		}
#endif
		return InitResultFailure;
	}

	INFO_MSG( "\tDatabase layer      = %s\n", databaseType.c_str() );
	if ((databaseType == "xml") && (isProduction_))
	{
		ERROR_MSG(
			"The XML database is suitable for demonstrations and "
			"evaluations only.\n"
			"Please use the MySQL database for serious development and "
			"production systems.\n"
			"See the Server Operations Guide for instructions on how to switch "
		   	"to the MySQL database.\n" );
	}

	status_.set( DBStatus::STARTING, "Initialising database layer" );

	bool isRecover = false;

	if (isUpgrade || isSyncTablesToDefs)
	{
		if (!pDatabase_->startup( this->getEntityDefs(),
									isRecover, isUpgrade, isSyncTablesToDefs ))
		{
			return InitResultFailure;
		}

		return InitResultAutoShutdown;
	}

	signal( SIGCHLD, ::signalHandler );
	signal( SIGINT, ::signalHandler );
#ifndef _WIN32  // WIN32PORT
	signal( SIGHUP, ::signalHandler );
#endif //ndef _WIN32  // WIN32PORT

	{
		nub_.registerBirthListener( DBInterface::handleBaseAppMgrBirth,
									"BaseAppMgrInterface" );

		// find the BaseAppMgr interface
		Mercury::Address baseAppMgrAddr;
		Mercury::Reason reason =
			nub_.findInterface( "BaseAppMgrInterface", 0, baseAppMgrAddr );

		if (reason == Mercury::REASON_SUCCESS)
		{
			baseAppMgr_.addr( baseAppMgrAddr );

			INFO_MSG( "Database::init: BaseAppMgr at %s\n",
				baseAppMgr_.c_str() );
		}
		else if (reason == Mercury::REASON_TIMER_EXPIRED)
		{
			INFO_MSG( "Database::init: BaseAppMgr is not ready yet.\n" );
		}
		else
		{
			CRITICAL_MSG("Database::init: "
				"findInterface( BaseAppMgrInterface ) failed! (%s)\n",
				Mercury::reasonToString( (Mercury::Reason)reason ) );

			return InitResultFailure;
		}
	}

	DBInterface::registerWithNub( nub_ );

	Mercury::Reason reason =
		DBInterface::registerWithMachined( nub_, 0 );

	if (reason != Mercury::REASON_SUCCESS)
	{
		ERROR_MSG( "Database::init: Unable to register with nub. "
				"Is machined running?\n" );
		return InitResultFailure;
	}

	nub_.registerBirthListener( DBInterface::handleDatabaseBirth,
								"DBInterface" );

	// We are in recovery mode if BaseAppMgr has already started
	if (baseAppMgr_.addr() != Mercury::Address::NONE)
	{
		Mercury::BlockingReplyHandlerWithResult <bool> handler( nub_ );
		Mercury::Bundle & bundle = baseAppMgr_.bundle();

		bundle.startRequest( BaseAppMgrInterface::requestHasStarted, &handler );
		baseAppMgr_.send();

		if ( handler.waitForReply( & baseAppMgr_.channel() ) ==
				Mercury::REASON_SUCCESS )
		{
			isRecover = handler.get();
		}

		shouldSendInitData_ = !isRecover;
	}

	if (!pDatabase_->startup( this->getEntityDefs(),
								isRecover, isUpgrade, isSyncTablesToDefs ))
	{
		return InitResultFailure;
	}

	if (shouldConsolidate_)
	{
		// Really generate run ID
		char runIDBuf[ BUFSIZ ];
		snprintf( runIDBuf, sizeof(runIDBuf),
					"%s_%04d-%02d-%02d_%02d:%02d:%02d",
					username.c_str(),
					timeAndDate.tm_year + 1900, timeAndDate.tm_mon + 1,
					timeAndDate.tm_mday,
					timeAndDate.tm_hour, timeAndDate.tm_min, timeAndDate.tm_sec );
		secondaryDBPrefix_ = runIDBuf;
	}

	INFO_MSG( "Database::init: secondaryDBPrefix_ = \"%s\"\n",
			secondaryDBPrefix_.c_str() );


	if (isRecover)
	{
		this->startServerBegin( true );
	}
	else
	{
		// Do data consolidation stuff
		if (shouldConsolidate_)
		{
			this->consolidateData();
		}
		else
		{
			status_.set( DBStatus::WAITING_FOR_APPS, "Waiting for other "
					"components to become ready" );
		}
	}

	// A one second timer to check all sorts of things.
	statusCheckTimerId_ = nub_.registerTimer( 1000000, this );

	#ifdef DBMGR_SELFTEST
		this->runSelfTest();
	#endif

	INFO_MSG( "\tNub address         = %s\n", (char *)nub_.address() );
	INFO_MSG( "\tAllow empty digest  = %s\n",
		allowEmptyDigest_ ? "True" : "False" );
	INFO_MSG( "\tLoad unknown user = %s\n",
		shouldLoadUnknown_ ? "True" : "False" );
	INFO_MSG( "\tCreate unknown user = %s\n",
		shouldCreateUnknown_ ? "True" : "False" );
	INFO_MSG( "\tRemember unknown user = %s\n",
		shouldRememberUnknown_ ? "True" : "False" );
	INFO_MSG( "\tRecover database = %s\n",
		isRecover ? "True" : "False" );
	INFO_MSG( "\tClear recovery data = %s\n",
		clearRecoveryDataOnStartUp_ ? "True" : "False" );

	return InitResultSuccess;
}


/**
 *	This method runs the database.
 */
void Database::run()
{
	INFO_MSG( "---- DBMgr is running ----\n" );
	do
	{
		nub_.processUntilBreak();
	} while (!signals_.isClear() && this->processSignals());

	this->finalise();
}

/**
 *	This method performs some clean-up at the end of the shut down process.
 */
void Database::finalise()
{
	if (pDatabase_)
	{
		pDatabase_->shutDown();
	}
}

/**
 *	Called when this process receives a signal
 */
void Database::onSignalled( int sigNum )
{
	signals_.set( sigNum );

	// Defer further processing to processSignal()
	nub_.breakProcessing();
}

/**
 * 	Processes a signal. Returns true if we should continue running this
 * 	process, false if we should terminate this process as soon as possible.
 */
bool Database::processSignals()
{
	static Signal::Set allSignals( Signal::Set::FULL );

	bool shouldContinueProcess = true;
	do
	{
		{
			// Block all signals
			Signal::Blocker signalBlocker( allSignals );

			// Un-breakProcessing so that if anything breaks processing, it
			// means we should shut down this process.
			nub_.breakProcessing( false );

			if (signals_.isSet( SIGCHLD ))
			{
				int		status;
				pid_t 	childPid = waitpid( -1, &status, WNOHANG );
				if (childPid != 0)
				{
					Database::instance().onChildProcessExit( childPid, status );
				}
			}

			if (signals_.isSet( SIGINT ) || signals_.isSet( SIGHUP ))
			{
				this->shutDown();
			}

			signals_.clear();

			// Check if we should shutdown this process.
			shouldContinueProcess = !nub_.processingBroken();
		}
		// Once we unblock signals, we could release a flood of signals, which
		// means nub_.processingBroken() will be true again.
	} while (shouldContinueProcess && nub_.processingBroken());

	return shouldContinueProcess;
}

/**
 * 	Notification that a child process has exited.
 */
void Database::onChildProcessExit( pid_t pid, int status )
{
	// Should be the consolidation process since we only launch one child
	// process at a time.
	MF_ASSERT( pid == this->consolidatePid_ );

	bool isOK = true;
	if (WIFEXITED( status ))
	{
		int exitCode = WEXITSTATUS( status );
		if (exitCode != 0)
		{
			if (exitCode == CONSOLIDATE_DBS_EXEC_FAILED_EXIT_CODE)
			{
				std::string fullPath = getOurExeDir() +
										"/"CONSOLIDATE_DBS_RELPATH_STR;
				ERROR_MSG( "Database::onChildProcessExit: "
							"Failed to execute %s.\n"
							"Please ensure that the "
							CONSOLIDATE_DBS_FILENAME_STR " executable exists "
							"and is runnable. You may need to build it "
							"manually as it not part of the standard "
							"package.\n",
						fullPath.c_str() );
			}
			else
			{
				ERROR_MSG( "Database::onChildProcessExit: "
						"Consolidate process exited with code %d\n"
						"Please examine the logs for ConsolidateDBs or run "
						CONSOLIDATE_DBS_FILENAME_STR " manually to determine "
						"the cause of the error\n",
						exitCode );
			}
			isOK = false;
		}
	}
	else if (WIFSIGNALED( status ))
	{
		ERROR_MSG( "Database::onChildProcessExit: "
				"Consolidate process was terminated by signal %d\n",
				int( WTERMSIG( status ) ) );
		isOK = false;
	}

	if (isOK)
	{
		TRACE_MSG( "Finished data consolidation\n" );
	}

	this->consolidatePid_ = 0;

	// Re-acquire lock to DB
	while (!pDatabase_->lockDB())
	{
		WARNING_MSG( "Database::onChildProcessExit: "
				"Failed to re-lock database. Retrying...\n" );
		sleep(1);
	}

	this->onConsolidateProcessEnd( isOK );
}

/**
 *	This method starts the data consolidation process.
 */
void Database::consolidateData()
{
	if (status_.status() <= DBStatus::STARTING)
	{
		status_.set( DBStatus::STARTUP_CONSOLIDATING, "Consolidating data" );
	}
	else if ( status_.status() >= DBStatus::SHUTTING_DOWN)
	{
		status_.set( DBStatus::SHUTDOWN_CONSOLIDATING, "Consolidating data" );
	}
	else
	{
		CRITICAL_MSG( "Database::consolidateData: Not a valid state to be "
				"running data consolidation!" );
		return;
	}

	uint32 numSecondaryDBs = pDatabase_->getNumSecondaryDBs();
	if (numSecondaryDBs > 0)
	{
		TRACE_MSG( "Starting data consolidation\n" );
		this->startConsolidationProcess();
	}
	else
	{
		this->onConsolidateProcessEnd( true );
	}
}

/**
 *	This method runs an external command to consolidate data from secondary
 * 	databases.
 */
bool Database::startConsolidationProcess()
{
	if (this->isConsolidating())
	{
		TRACE_MSG( "Database::startConsolidationProcess: Ignoring second "
				"attempt to consolidate data while data consolidation is "
				"already in progress\n" );
		return false;
	}

	pDatabase_->unlockDB();		// So consolidate process can access it.

	std::vector< std::string > cmdArgs;

	// Add resource paths.
	// NOTE: BWResource::getPathAsCommandLine() has some weird code which
	// made it unsuitable for us.
	{
		int numPaths = BWResource::getPathNum();
		if (numPaths > 0)
		{
			cmdArgs.push_back( "--res" );

			std::stringstream ss;
			ss << BWResource::getPath( 0 );
			for (int i = 1; i < numPaths; ++i)
			{
				ss << BW_RES_PATH_SEPARATOR << BWResource::getPath( i );
			}
			cmdArgs.push_back( ss.str() );
		}
	}

	// We rely on consolidate_dbs to read the primary database settings from
	// bw.xml and the secondary database entries from the primary database.
//	// Add primary database config into command line
//	{
//		const DBConfig::Connection& config =
//			this->getServerConfig().getCurServer().connectionInfo;
//		std::stringstream ss;
//		ss << config.host << ';' << config.username << ';' << config.password
//				<< ';' << config.database;
//		cmdArgs.push_back( ss.str() );
//	}
//
//	// Add secondary database locations into the command line
//	for ( IDatabase::IGetSecondaryDBsHandler::Entries::const_iterator i =
//			entries.begin(); i != entries.end(); ++i )
//	{
//		std::stringstream ss;
//		ss << i->addr.ip << ';' << i->location;
//		cmdArgs.push_back( ss.str() );
//	}

	if ((consolidatePid_ = fork()) == 0)
	{
		// Find path
		std::string path = getOurExeDir();

		// Change to it
		if (chdir( path.c_str() ) == -1)
		{
			printf( "Failed to change directory to %s\n", path.c_str() );
			exit(1);
		}

		// Add the exe name
		path += "/"CONSOLIDATE_DBS_RELPATH_STR;

		// Close parent sockets
		close( this->nub_.socket() );

		// Make arguments into const char * array.
		const char ** argv = new const char *[cmdArgs.size() + 2];
		int i = 0;
		argv[i++] = path.c_str();
		for (std::vector< std::string >::iterator pCurArg = cmdArgs.begin();
				pCurArg != cmdArgs.end(); ++pCurArg, ++i)
		{
			argv[i] = pCurArg->c_str();
		}
		argv[i] = NULL;

		// __kyl__ (21/4/2008) This is a rather worrying const cast. If execv()
		// modifies any of the arguments, we're screwed since we're pointing
		// to constant strings.
		int result = execv( path.c_str(),
				const_cast< char * const * >( argv ) );

		if (result == -1)
		{
			exit( CONSOLIDATE_DBS_EXEC_FAILED_EXIT_CODE );
		}

		exit(1);
	}

	return true;
}

/**
 * 	This method is called when the consolidation process exits.
 */
void Database::onConsolidateProcessEnd( bool isOK )
{
	if (status_.status() == DBStatus::STARTUP_CONSOLIDATING)
	{
		if (isOK)
		{
			status_.set( DBStatus::WAITING_FOR_APPS, "Waiting for other "
					"components to become ready" );
		}
		else
		{
			// Prevent trying to consolidate again during controlled shutdown.
			shouldConsolidate_ = false;

			this->startSystemControlledShutdown();
		}
	}
	else if (status_.status() == DBStatus::SHUTDOWN_CONSOLIDATING)
	{
		this->shutDown();
	}
	else
	{
		CRITICAL_MSG( "Database::onChildProcessExit: Invalid state %d at "
				"the end of data consolidation\n", status_.status() );
	}
}

/**
 *	This method handles the checkStatus request's reply.
 */
class LoginAppCheckStatusReplyHandler : public Mercury::ReplyMessageHandler
{
public:
	LoginAppCheckStatusReplyHandler( const Mercury::Address & srcAddr,
			Mercury::ReplyID replyID ) :
		srcAddr_( srcAddr ),
		replyID_ ( replyID )
	{
	}

private:
	virtual void handleMessage( const Mercury::Address & /*srcAddr*/,
			Mercury::UnpackedMessageHeader & /*header*/,
			BinaryIStream & data, void * /*arg*/ )
	{
		Mercury::ChannelSender sender( Database::getChannel( srcAddr_ ) );
		Mercury::Bundle & bundle = sender.bundle();

		bundle.startReply( replyID_ );

		bool isOkay;
		int32 numBaseApps;
		int32 numCellApps;

		data >> isOkay >> numBaseApps >> numCellApps;

		bundle << uint8( isOkay && (numBaseApps > 0) && (numCellApps > 0));

		bundle.transfer( data, data.remainingLength() );

		if (numBaseApps <= 0)
		{
			bundle << "No BaseApps";
		}

		if (numBaseApps <= 0)
		{
			bundle << "No CellApps";
		}

		delete this;
	}

	virtual void handleException( const Mercury::NubException & /*ne*/,
		void * /*arg*/ )
	{
		Mercury::ChannelSender sender( Database::getChannel( srcAddr_ ) );
		Mercury::Bundle & bundle = sender.bundle();

		bundle.startReply( replyID_ );
		bundle << uint8( false );
		bundle << "No reply from BaseAppMgr";

		delete this;
	}

	Mercury::Address srcAddr_;
	Mercury::ReplyID replyID_;
};


/**
 *	This method handles the checkStatus messsages request from the LoginApp.
 */
void Database::checkStatus( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data )
{
	Mercury::ChannelSender sender(
		Database::instance().baseAppMgr().channel() );

	sender.bundle().startRequest( BaseAppMgrInterface::checkStatus,
		   new LoginAppCheckStatusReplyHandler( srcAddr, header.replyID ) );
}


/**
 *	This method handles the replies from the checkStatus requests.
 */
void Database::handleStatusCheck( BinaryIStream & data )
{
	bool isOkay;
	uint32 numBaseApps = 0;
	uint32 numCellApps = 0;
	data >> isOkay >> numBaseApps >> numCellApps;
	INFO_MSG( "Database::handleStatusCheck: "
				"baseApps = %u/%u. cellApps = %u/%u\n",
			  std::max( uint32(0), numBaseApps ), desiredBaseApps_,
			  std::max( uint32(0), numCellApps ), desiredCellApps_ );

	// Ignore other status information
	data.finish();

	if ((status_.status() <= DBStatus::WAITING_FOR_APPS) &&
			!data.error() &&
			(numBaseApps >= desiredBaseApps_) &&
			(numCellApps >= desiredCellApps_))
	{
		this->startServerBegin();
	}
}


/**
 *	This method handles the checkStatus request's reply.
 */
class CheckStatusReplyHandler : public Mercury::ReplyMessageHandler
{
private:
	virtual void handleMessage( const Mercury::Address & /*srcAddr*/,
			Mercury::UnpackedMessageHeader & /*header*/,
			BinaryIStream & data, void * /*arg*/ )
	{
		Database::instance().handleStatusCheck( data );
		delete this;
	}

	virtual void handleException( const Mercury::NubException & /*ne*/,
		void * /*arg*/ )
	{
		delete this;
	}
};

/**
 * 	This method handles a secondary database registration message.
 */
void Database::secondaryDBRegistration( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data )
{
	IDatabase::SecondaryDBEntry	secondaryDBEntry;

	data >> secondaryDBEntry.addr >> secondaryDBEntry.appID;

	data >> secondaryDBEntry.location;
	pDatabase_->addSecondaryDB( secondaryDBEntry );
}

/**
 * 	This method handles an update secondary database registrations
 * 	message. Secondary databases registered by a BaseApp not in the
 * 	provided list are deleted.
 */
void Database::updateSecondaryDBs( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data )
{
	uint32 size;
	data >> size;

	BaseAppIDs ids;

	for (uint32 i = 0; i < size; ++i)
	{
		int id;
		data >> id;
		ids.push_back( id );
	}

	pDatabase_->updateSecondaryDBs( ids, *this );
	// updateSecondaryDBs() calls onUpdateSecondaryDBsComplete() it completes.
}


/**
 *	This method handles the request to get information for creating a new
 *	secondary database. It replies with the name of the new database.
 */
void Database::getSecondaryDBDetails( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data )
{
	Mercury::ChannelSender sender( Database::getChannel( srcAddr ) );
	Mercury::Bundle & bundle = sender.bundle();
	bundle.startReply( header.replyID );

	if (!secondaryDBPrefix_.empty())
	{
		char buf[ BUFSIZ ];
		++secondaryDBIndex_;
		snprintf( buf, sizeof( buf ), "%s-%d.db",
				secondaryDBPrefix_.c_str(),
				secondaryDBIndex_ );

		bundle << buf;
	}
	else
	{
		// An empty string indicates that secondary
		bundle << "";
	}
}


/**
 * 	Deletes secondary databases whose registration have just been removed.
 */
void Database::onUpdateSecondaryDBsComplete(
		const IDatabase::SecondaryDBEntries& removedEntries )
{
	for (IDatabase::SecondaryDBEntries::const_iterator pEntry =
			removedEntries.begin(); pEntry != removedEntries.end(); ++pEntry )
	{
		if (this->sendRemoveDBCmd( pEntry->addr.ip, pEntry->location ))
		{
			TRACE_MSG( "Database::onUpdateSecondaryDBsComplete: "
					"Deleting secondary database file %s on %s\n",
					pEntry->location.c_str(), pEntry->addr.ipAsString() );
		}
		else
		{
			ERROR_MSG( "Database::onUpdateSecondaryDBsComplete: Failed to "
					"delete secondary database file %s on %s. It should be "
					"manually deleted to prevent disk space exhaustion.\n",
					pEntry->location.c_str(), pEntry->addr.ipAsString() );
		}
	}
}

/**
 *	Sends a message to the destination BWMachined that will cause the
 *	database at dbLocation to be removed.
 */
bool Database::sendRemoveDBCmd( uint32 destIP, const std::string& dbLocation )
{
	CreateWithArgsMessage cm;
	cm.uid_ = getUserId();
#if defined _DEBUG
	cm.config_ = "Debug";
#elif defined _HYBRID
	cm.config_ = "Hybrid";
#endif
	cm.recover_ = 0;
	cm.name_ = "commands/remove_db";
	cm.fwdIp_ = 0;
	cm.fwdPort_ = 0;

	cm.args_.push_back( dbLocation );

	Endpoint ep;
	ep.socket( SOCK_DGRAM );

	return (ep.good() && (ep.bind() == 0) &&
			cm.sendto( ep, htons( PORT_MACHINED ), destIP ));
}

/**
 *	This method handles timer events. It is called every second.
 */
int Database::handleTimeout( Mercury::TimerID id, void * arg )
{
	// See if we should send initialisation data to BaseAppMgr
	if (shouldSendInitData_)
	{
		shouldSendInitData_ = false;
		this->sendInitData();
	}

	// See if we are ready to start.
	if (baseAppMgr_.channel().isEstablished() &&
			(status_.status() == DBStatus::WAITING_FOR_APPS))
	{
		Mercury::Bundle & bundle = baseAppMgr_.bundle();

		bundle.startRequest( BaseAppMgrInterface::checkStatus,
			   new CheckStatusReplyHandler() );

		baseAppMgr_.send();

		nub_.clearSpareTime();
	}

	// Update our current load so we can know whether or not we are overloaded.
	if (status_.status() > DBStatus::WAITING_FOR_APPS)
	{
		uint64 spareTime = nub_.getSpareTime();
		nub_.clearSpareTime();

		curLoad_ = 1.f - float( double(spareTime) / stampsPerSecondD() );

		// TODO: Consider asking DB implementation if it is overloaded too...
	}

	// Check whether we should end our remapping of mailboxes for a dead BaseApp
	if (--mailboxRemapCheckCount_ == 0)
		this->endMailboxRemapping();

	return 0;
}


// -----------------------------------------------------------------------------
// Section: Database lifetime
// -----------------------------------------------------------------------------

/**
 *	This method is called when a new BaseAppMgr is started.
 */
void Database::handleBaseAppMgrBirth( DBInterface::handleBaseAppMgrBirthArgs & args )
{
	baseAppMgr_.addr( args.addr );

	INFO_MSG( "Database::handleBaseAppMgrBirth: BaseAppMgr is at %s\n",
		baseAppMgr_.c_str() );

	if (status_.status() < DBStatus::SHUTTING_DOWN)
	{
		shouldSendInitData_ = true;
	}
}

/**
 *	This method is called when a new DbMgr is started.
 */
void Database::handleDatabaseBirth( DBInterface::handleDatabaseBirthArgs & args )
{
	if (args.addr != nub_.address())
	{
		WARNING_MSG( "Database::handleDatabaseBirth: %s\n", (char *)args.addr );
		this->shutDown();	// Don't consolidate
	}
}

/**
 *	This method handles the shutDown message.
 */
void Database::shutDown( DBInterface::shutDownArgs & /*args*/ )
{
	this->shutDown();
}

/**
 * 	This method starts a controlled shutdown for the entire system.
 */
void Database::startSystemControlledShutdown()
{
	if (baseAppMgr_.channel().isEstablished())
	{
		BaseAppMgrInterface::controlledShutDownArgs args;
		args.stage = SHUTDOWN_TRIGGER;
		args.shutDownTime = 0;
		baseAppMgr_.bundle() << args;
		baseAppMgr_.send();
	}
	else
	{
		WARNING_MSG( "Database::startSystemControlledShutdown: "
				"No known BaseAppMgr, only shutting down self\n" );
		this->shutDownNicely();
	}
}

/**
 * 	This method starts shutting down DBMgr.
 */
void Database::shutDownNicely()
{
	if (status_.status() >= DBStatus::SHUTTING_DOWN)
	{
		WARNING_MSG( "Database::shutDownNicely: Ignoring second shutdown\n" );
		return;
	}

	TRACE_MSG( "Database::shutDownNicely: Shutting down\n" );

	status_.set( DBStatus::SHUTTING_DOWN, "Shutting down" );

	nub_.processUntilChannelsEmpty();

	if (shouldConsolidate_)
	{
		this->consolidateData();
	}
	else
	{
		this->shutDown();
	}
}

/**
 *	This method shuts this process down.
 */
void Database::shutDown()
{
	TRACE_MSG( "Database::shutDown\n" );

	if (consolidatePid_ != 0)
	{
		WARNING_MSG( "Database::shutDown: Stopping ongoing consolidation "
				"process %d\n", consolidatePid_ );
		::kill( consolidatePid_, SIGINT );
	}

	nub_.breakProcessing();
}

/**
 *	This method handles telling us to shut down in a controlled manner.
 */
void Database::controlledShutDown( DBInterface::controlledShutDownArgs & args )
{
	DEBUG_MSG( "Database::controlledShutDown: stage = %d\n", args.stage );

	switch (args.stage)
	{
	case SHUTDOWN_REQUEST:
	{
		// Make sure we no longer send to anonymous channels etc.
		nub_.prepareToShutDown();

		if (baseAppMgr_.channel().isEstablished())
		{
			BaseAppMgrInterface::controlledShutDownArgs args;
			args.stage = SHUTDOWN_REQUEST;
			args.shutDownTime = 0;
			baseAppMgr_.bundle() << args;
			baseAppMgr_.send();
		}
		else
		{
			WARNING_MSG( "Database::controlledShutDown: "
					"No BaseAppMgr. Proceeding to shutdown immediately\n" );
			this->shutDownNicely();
		}
	}
	break;

	case SHUTDOWN_PERFORM:
		this->shutDownNicely();
		break;

	default:
		ERROR_MSG( "Database::controlledShutDown: Stage %d not handled.\n",
				args.stage );
		break;
	}
}


/**
 *	This method handles telling us that a CellApp is or isn't overloaded.
 */
void Database::cellAppOverloadStatus(
	DBInterface::cellAppOverloadStatusArgs & args )
{
	anyCellAppOverloaded_ = args.anyOverloaded;
}


// -----------------------------------------------------------------------------
// Section: IDatabase intercept methods
// -----------------------------------------------------------------------------
/**
 *	This method intercepts the result of IDatabase::getEntity() operations and
 *	mucks around with it before passing it to onGetEntityCompleted().
 */
void Database::GetEntityHandler::onGetEntityComplete( bool isOK )
{
	// Update mailbox for dead BaseApps.
	if (Database::instance().hasMailboxRemapping() &&
			this->outrec().isBaseMBProvided() &&
			this->outrec().getBaseMB())
	{
		Database::instance().remapMailbox( *this->outrec().getBaseMB() );
	}

	// Give results to real handler.
	this->onGetEntityCompleted( isOK );
}

/**
 *	This method is meant to be called instead of IDatabase::getEntity() so that
 * 	we can muck around with stuff before passing it to IDatabase.
 */
void Database::getEntity( GetEntityHandler& handler )
{
	pDatabase_->getEntity( handler );
}

/**
 *	This method is meant to be called instead of IDatabase::putEntity() so that
 * 	we can muck around with stuff before passing it to IDatabase.
 */
void Database::putEntity( const EntityDBKey& ekey, EntityDBRecordIn& erec,
		IDatabase::IPutEntityHandler& handler )
{
	// Update mailbox for dead BaseApps.
	if (this->hasMailboxRemapping() && erec.isBaseMBProvided() &&
			erec.getBaseMB())
		this->remapMailbox( *erec.getBaseMB() );

	pDatabase_->putEntity( ekey, erec, handler );
}

/**
 *	This method is meant to be called instead of IDatabase::delEntity() so that
 * 	we can muck around with stuff before passing it to IDatabase.
 */
inline void Database::delEntity( const EntityDBKey & ekey,
		IDatabase::IDelEntityHandler& handler )
{
	pDatabase_->delEntity( ekey, handler );
}

/**
 *	This method is meant to be called instead of IDatabase::setLoginMapping()
 * 	so that we can muck around with stuff before passing it to IDatabase.
 */
inline void Database::setLoginMapping( const std::string & username,
		const std::string & password, const EntityDBKey& ekey,
		IDatabase::ISetLoginMappingHandler& handler )
{
	pDatabase_->setLoginMapping( username, password, ekey, handler );
}

// -----------------------------------------------------------------------------
// Section: LoginHandler
// -----------------------------------------------------------------------------

/**
 *	This class is used to receive the reply from a createEntity call to
 *	BaseAppMgr.
 */
class LoginHandler : public Mercury::ReplyMessageHandler,
                     public IDatabase::IMapLoginToEntityDBKeyHandler,
					 public IDatabase::ISetLoginMappingHandler,
                     public Database::GetEntityHandler,
                     public IDatabase::IPutEntityHandler
{
	enum State
	{
		StateInit,
		StateWaitingForLoadUnknown,
		StateWaitingForLoad,
		StateWaitingForPutNewEntity,
		StateWaitingForSetLoginMappingForLoadUnknown,
		StateWaitingForSetLoginMappingForCreateUnknown,
		StateWaitingForSetBaseToLoggingOn,
		StateWaitingForSetBaseToFinal,
	};

public:
	LoginHandler(
			LogOnParamsPtr pParams,
			const Mercury::Address& clientAddr,
			const Mercury::Address & replyAddr,
			bool offChannel,
			Mercury::ReplyID replyID ) :
		state_( StateInit ),
		ekey_( 0, 0 ),
		pParams_( pParams ),
		clientAddr_( clientAddr ),
		replyAddr_( replyAddr ),
		offChannel_( offChannel ),
		replyID_( replyID ),
		pStrmDbID_( 0 )
	{
	}
	virtual ~LoginHandler() {}

	void login();

	// Mercury::ReplyMessageHandler overrides
	virtual void handleMessage( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data, void * arg );

	virtual void handleException( const Mercury::NubException & ne,
		void * arg );

	// IDatabase::IMapLoginToEntityDBKeyHandler override
	virtual void onMapLoginToEntityDBKeyComplete( DatabaseLoginStatus status,
		const EntityDBKey& ekey );

	// IDatabase::ISetLoginMappingHandler override
	virtual void onSetLoginMappingComplete();

	// IDatabase::IGetEntityHandler/Database::GetEntityHandler overrides
	virtual EntityDBKey& key()					{	return ekey_;	}
	virtual EntityDBRecordOut& outrec()			{	return outRec_;	}

	virtual const std::string * getPasswordOverride() const
	{
		return Database::instance().getEntityDefs().
			entityTypeHasPassword( ekey_.typeID ) ? &pParams_->password() : 0;
	}

	virtual void onGetEntityCompleted( bool isOK );

	// IDatabase::IPutEntityHandler override
	virtual void onPutEntityComplete( bool isOK, DatabaseID dbID );

private:
	void handleFailure( BinaryIStream * pData, DatabaseLoginStatus reason );
	void checkOutEntity();
	void createNewEntity( bool isBundlePrepared = false );
	void sendCreateEntityMsg();
	void sendReply();
	void sendFailureReply( DatabaseLoginStatus status, const char * msg = NULL );

	State				state_;
	EntityDBKey			ekey_;
	LogOnParamsPtr		pParams_;
	Mercury::Address	clientAddr_;
	Mercury::Address	replyAddr_;
	bool				offChannel_;
	Mercury::ReplyID	replyID_;
	Mercury::Bundle		bundle_;
	EntityMailBoxRef	baseRef_;
	EntityMailBoxRef*	pBaseRef_;
	EntityDBRecordOut	outRec_;
	DatabaseID*			pStrmDbID_;
};

/**
 *	Start the login process
 */
void LoginHandler::login()
{
	// __glenc__ TODO: See if this needs to be converted to use params
	Database::instance().getIDatabase().mapLoginToEntityDBKey(
		pParams_->username(), pParams_->password(), *this );

	// When mapLoginToEntityDBKey() completes, onMapLoginToEntityDBKeyComplete()
	// will be called.
}

/**
 *	IDatabase::IMapLoginToEntityDBKeyHandler override
 */
void LoginHandler::onMapLoginToEntityDBKeyComplete( DatabaseLoginStatus status,
												   const EntityDBKey& ekey )
{
	bool shouldLoadEntity = false;
	bool shouldCreateEntity = false;

	if (status == DatabaseLoginStatus::LOGGED_ON)
	{
		ekey_ = ekey;
		shouldLoadEntity = true;
		state_ = StateWaitingForLoad;
	}
	else if (status == DatabaseLoginStatus::LOGIN_REJECTED_NO_SUCH_USER)
	{
		if (Database::instance().shouldLoadUnknown())
		{
			ekey_.typeID = Database::instance().getEntityDefs().getDefaultType();
			ekey_.name = pParams_->username();
			shouldLoadEntity = true;
			state_ = StateWaitingForLoadUnknown;
		}
		else if (Database::instance().shouldCreateUnknown())
		{
			shouldCreateEntity = true;
		}
	}

	if (shouldLoadEntity)
	{
		// Start "create new base" message even though we're not sure entity
		// exists. This is to take advantage of getEntity() streaming properties
		// into the bundle directly.
		pStrmDbID_ = Database::prepareCreateEntityBundle( ekey_.typeID,
			ekey_.dbID, clientAddr_, this, bundle_, pParams_ );

		// Get entity data
		pBaseRef_ = &baseRef_;
		outRec_.provideBaseMB( pBaseRef_ );		// Get entity mailbox
		outRec_.provideStrm( bundle_ );			// Get entity data into bundle

		Database::instance().getEntity( *this );
		// When getEntity() completes, onGetEntityCompleted() is called.
	}
	else if (shouldCreateEntity)
	{
		this->createNewEntity();
	}
	else
	{
		const char * msg;
		bool isError = false;
		switch (status)
		{
			case DatabaseLoginStatus::LOGIN_REJECTED_NO_SUCH_USER:
				msg = "Unknown user."; break;
			case DatabaseLoginStatus::LOGIN_REJECTED_INVALID_PASSWORD:
				msg = "Invalid password."; break;
			case DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE:
				msg = "Unexpected database failure.";
				isError = true;
				break;
			default:
				msg = UNSPECIFIED_ERROR_STR;
				isError = true;
				break;
		}
		if (isError)
		{
			ERROR_MSG( "Database::logOn: mapLoginToEntityDBKey for %s failed: "
				"(%d) %s\n", pParams_->username().c_str(), int(status), msg );
		}
		else
		{
			NOTICE_MSG( "Database::logOn: mapLoginToEntityDBKey for %s failed: "
				"(%d) %s\n", pParams_->username().c_str(), int(status), msg );
		}
		Database::instance().sendFailure( replyID_, replyAddr_, offChannel_,
			status, msg );
		delete this;
	}

}

/**
 *	Database::GetEntityHandler override
 */
void LoginHandler::onGetEntityCompleted( bool isOK )
{
	if (!isOK)
	{	// Entity doesn't exist.
		if ( (state_ == StateWaitingForLoadUnknown) &&
			 Database::instance().shouldCreateUnknown() )
		{
			this->createNewEntity( true );
		}
		else
		{
			ERROR_MSG( "Database::logOn: Entity %s does not exist\n",
				ekey_.name.c_str() );

			this->sendFailureReply(
				DatabaseLoginStatus::LOGIN_REJECTED_NO_SUCH_USER,
				"Failed to load entity." );
		}
	}
	else
	{
		if (pStrmDbID_)
		{	// Means ekey.dbID was 0 when we called prepareCreateEntityBundle()
			// Now fix up everything.
			*pStrmDbID_ = ekey_.dbID;
		}

		if ( (state_ == StateWaitingForLoadUnknown) &&
			 (Database::instance().shouldRememberUnknown()) )
		{	// Need to remember this login mapping.
			state_ = StateWaitingForSetLoginMappingForLoadUnknown;

			Database::instance().setLoginMapping(
				pParams_->username(), pParams_->password(), ekey_, *this );

			// When setLoginMapping() completes onSetLoginMappingComplete() is called
		}
		else
		{
			this->checkOutEntity();
		}
	}
}

/**
 *	This function checks out the login entity. Must be called after
 *	entity has been successfully retrieved from the database.
 */
void LoginHandler::checkOutEntity()
{
	if ( !outRec_.getBaseMB() &&
		Database::instance().onStartEntityCheckout( ekey_ ) )
	{	// Not checked out and not in the process of being checked out.
		state_ = StateWaitingForSetBaseToLoggingOn;
		Database::setBaseRefToLoggingOn( baseRef_, ekey_.typeID );
		pBaseRef_ = &baseRef_;
		EntityDBRecordIn	erec;
		erec.provideBaseMB( pBaseRef_ );
		Database::instance().putEntity( ekey_, erec, *this );
		// When putEntity() completes, onPutEntityComplete() is called.
	}
	else	// Checked out
	{
		Database::instance().onLogOnLoggedOnUser( ekey_.typeID, ekey_.dbID,
			pParams_, clientAddr_, replyAddr_, offChannel_, replyID_,
			outRec_.getBaseMB() );

		delete this;
	}
}

/**
 *	IDatabase::IPutEntityHandler override
 */
void LoginHandler::onPutEntityComplete( bool isOK, DatabaseID dbID )
{
	switch (state_)
	{
		case StateWaitingForPutNewEntity:
			MF_ASSERT( pStrmDbID_ );
			*pStrmDbID_ = dbID;
			if (isOK)
			{
				ekey_.dbID = dbID;

				state_ = StateWaitingForSetLoginMappingForCreateUnknown;
				Database::instance().setLoginMapping(
					pParams_->username(), pParams_->password(),	ekey_, *this );

				// When setLoginMapping() completes onSetLoginMappingComplete() is called
				break;
			}
			else
			{	// Failed "rememberEntity" function.
				ERROR_MSG( "Database::logOn: Failed to write default entity "
					"for %s\n", pParams_->username().c_str());

				// Let them log in anyway since this is meant to be a
				// convenience feature during product development.
				// Fallthrough
			}
		case StateWaitingForSetBaseToLoggingOn:
			if (isOK)
				this->sendCreateEntityMsg();
			else
			{
				Database::instance().onCompleteEntityCheckout( ekey_, NULL );
				// Something horrible like database disconnected or something.
				this->sendFailureReply(
						DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE,
						"Unexpected database failure." );
			}
			break;
		case StateWaitingForSetBaseToFinal:
			Database::instance().onCompleteEntityCheckout( ekey_,
					(isOK) ? &baseRef_ : NULL );
			if (isOK)
				this->sendReply();
			else
				// Something horrible like database disconnected or something.
				this->sendFailureReply(
						DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE,
						"Unexpected database failure." );
			break;
		default:
			MF_ASSERT(false);
			delete this;
	}
}

/**
 *	This method sends the BaseAppMgrInterface::createEntity message.
 *	Assumes bundle has the right data.
 */
inline void LoginHandler::sendCreateEntityMsg()
{
	INFO_MSG( "Database::logOn: %s\n", pParams_->username().c_str() );

	Database::instance().baseAppMgr().send( &bundle_ );
}

/**
 *	This method sends the reply to the LoginApp. Assumes bundle already
 *	has the right data.
 *
 *	This should also be the last thing this object is doing so this
 *	also destroys this object.
 */
inline void LoginHandler::sendReply()
{
	if (offChannel_)
	{
		Database::instance().nub().send( replyAddr_, bundle_ );
	}
	else
	{
		Database::getChannel( replyAddr_ ).send( &bundle_ );
	}
	delete this;
}

/**
 *	This method sends a failure reply to the LoginApp.
 *
 *	This should also be the last thing this object is doing so this
 *	also destroys this object.
 */
inline void LoginHandler::sendFailureReply( DatabaseLoginStatus status,
		const char * msg )
{
	Database::instance().sendFailure( replyID_, replyAddr_, offChannel_,
		status, msg );
	delete this;
}

/**
 *	IDatabase::ISetLoginMappingHandler override
 */
void LoginHandler::onSetLoginMappingComplete()
{
	if (state_ == StateWaitingForSetLoginMappingForLoadUnknown)
	{
		this->checkOutEntity();
	}
	else
	{
		MF_ASSERT( state_ == StateWaitingForSetLoginMappingForCreateUnknown );
		this->sendCreateEntityMsg();
	}
}

/**
 *	This function creates a new login entity for the user.
 */
void LoginHandler::createNewEntity( bool isBundlePrepared )
{
	ekey_.typeID = Database::instance().getEntityDefs().getDefaultType();
	ekey_.name = pParams_->username();

	if (!isBundlePrepared)
	{
		pStrmDbID_ = Database::prepareCreateEntityBundle( ekey_.typeID,
			0, clientAddr_, this, bundle_, pParams_ );
	}

	bool isDefaultEntityOK;

	if (Database::instance().shouldRememberUnknown())
	{
		// __kyl__ (13/7/2005) Need additional MemoryOStream because I
		// haven't figured out how to make a BinaryIStream out of a
		// Mercury::Bundle.
		MemoryOStream strm;
		isDefaultEntityOK = Database::instance().defaultEntityToStrm(
			ekey_.typeID, pParams_->username(), strm, &pParams_->password() );

		if (isDefaultEntityOK)
		{
			bundle_.transfer( strm, strm.size() );
			strm.rewind();

			// Put entity data into DB and set baseref to "logging on".
			state_ = StateWaitingForPutNewEntity;
			Database::setBaseRefToLoggingOn( baseRef_, ekey_.typeID );
			pBaseRef_ = &baseRef_;
			EntityDBRecordIn	erec;
			erec.provideBaseMB( pBaseRef_ );
			erec.provideStrm( strm );
			Database::instance().putEntity( ekey_, erec, *this );
			// When putEntity() completes, onPutEntityComplete() is called.
		}
	}
	else
	{
		*pStrmDbID_ = 0;

		// No need for additional MemoryOStream. Just stream into bundle.
		isDefaultEntityOK = Database::instance().defaultEntityToStrm(
			ekey_.typeID, pParams_->username(), bundle_,
			&pParams_->password() );

		if (isDefaultEntityOK)
			this->sendCreateEntityMsg();
	}

	if (!isDefaultEntityOK)
	{
		ERROR_MSG( "Database::logOn: Failed to create default entity for %s\n",
			pParams_->username().c_str());

		this->sendFailureReply( DatabaseLoginStatus::LOGIN_CUSTOM_DEFINED_ERROR,
				"Failed to create default entity" );
	}
}

/**
 *	Mercury::ReplyMessageHandler override.
 */
void LoginHandler::handleMessage( const Mercury::Address & source,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data,
	void * arg )
{
	Mercury::Address proxyAddr;

	data >> proxyAddr;

	if (proxyAddr.ip == 0)
	{
		DatabaseLoginStatus::Status status;
		switch (proxyAddr.port)
		{
			case BaseAppMgrInterface::CREATE_ENTITY_ERROR_NO_BASEAPPS:
				status = DatabaseLoginStatus::LOGIN_REJECTED_NO_BASEAPPS;
				break;
			case BaseAppMgrInterface::CREATE_ENTITY_ERROR_BASEAPPS_OVERLOADED:
				status = DatabaseLoginStatus::LOGIN_REJECTED_BASEAPP_OVERLOAD;
				break;
			default:
				status = DatabaseLoginStatus::LOGIN_CUSTOM_DEFINED_ERROR;
				break;
		}

		this->handleFailure( &data, status );
	}
	else
	{
		data >> baseRef_;

		bundle_.clear();
		bundle_.startReply( replyID_ );

		// Assume success.
		bundle_ << (uint8)DatabaseLoginStatus::LOGGED_ON;
		bundle_ << proxyAddr;
		// session key (if there is one)
		bundle_.transfer( data, data.remainingLength() );

		if ( ekey_.dbID )
		{
			state_ = StateWaitingForSetBaseToFinal;
			pBaseRef_ = &baseRef_;
			EntityDBRecordIn erec;
			erec.provideBaseMB( pBaseRef_ );
			Database::instance().putEntity( ekey_, erec, *this );
			// When putEntity() completes, onPutEntityComplete() is called.
		}
		else
		{	// Must be either "loadUnknown" or "createUnknown", but
			// "rememberUnknown" is false.
			this->sendReply();
		}
	}
}

/**
 *	Mercury::ReplyMessageHandler override.
 */
void LoginHandler::handleException( const Mercury::NubException & ne,
	void * arg )
{
	MemoryOStream mos;
	mos << "BaseAppMgr timed out creating entity.";
	this->handleFailure( &mos,
			DatabaseLoginStatus::LOGIN_REJECTED_BASEAPPMGR_TIMEOUT );
}

/**
 *	Handles a failure to create entity base.
 */
void LoginHandler::handleFailure( BinaryIStream * pData,
		DatabaseLoginStatus reason )
{
	bundle_.clear();
	bundle_.startReply( replyID_ );

	bundle_ << (uint8)reason;

	bundle_.transfer( *pData, pData->remainingLength() );

	if ( ekey_.dbID )
	{
		state_ = StateWaitingForSetBaseToFinal;
		pBaseRef_ = 0;
		EntityDBRecordIn erec;
		erec.provideBaseMB( pBaseRef_ );
		Database::instance().putEntity( ekey_, erec, *this );
		// When putEntity() completes, onPutEntityComplete() is called.
	}
	else
	{	// Must be either "loadUnknown" or "createUnknown", but
		// "rememberUnknown" is false.
		this->sendReply();
	}
}



// -----------------------------------------------------------------------------
// Section: RelogonAttemptHandler
// -----------------------------------------------------------------------------

/**
 *	This class is used to receive the reply from a createEntity call to
 *	BaseAppMgr during a re-logon operation.
 */
class RelogonAttemptHandler : public Mercury::ReplyMessageHandler,
                            public IDatabase::IPutEntityHandler
{
	enum State
	{
		StateWaitingForOnLogOnAttempt,
		StateWaitingForSetBaseToFinal,
		StateWaitingForSetBaseToNull,
		StateAborted
	};

public:
	RelogonAttemptHandler( EntityTypeID entityTypeID,
			DatabaseID dbID,
			const Mercury::Address & replyAddr,
			bool offChannel,
			Mercury::ReplyID replyID,
			LogOnParamsPtr pParams,
			const Mercury::Address & addrForProxy ) :
		state_( StateWaitingForOnLogOnAttempt ),
		ekey_( entityTypeID, dbID ),
		replyAddr_( replyAddr ),
		offChannel_( offChannel ),
		replyID_( replyID ),
		pParams_( pParams ),
		addrForProxy_( addrForProxy ),
		replyBundle_()
	{
		Database::instance().onStartRelogonAttempt( entityTypeID, dbID, this );
	}

	virtual ~RelogonAttemptHandler()
	{
		if (state_ != StateAborted)
		{
			Database::instance().onCompleteRelogonAttempt(
					ekey_.typeID, ekey_.dbID );
		}
	}

	virtual void handleMessage( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * arg )
	{
		uint8 result;
		data >> result;

		if (state_ != StateAborted)
		{
			switch (result)
			{
				case BaseAppIntInterface::LOG_ON_ATTEMPT_TOOK_CONTROL:
				{
					INFO_MSG( "RelogonAttemptHandler: It's taken over.\n" );
					Mercury::Address proxyAddr;
					data >> proxyAddr;

					EntityMailBoxRef baseRef;
					data >> baseRef;

					replyBundle_.startReply( replyID_ );

					// Assume success.
					replyBundle_ << (uint8)DatabaseLoginStatus::LOGGED_ON;
					replyBundle_ << proxyAddr;
					replyBundle_.transfer( data, data.remainingLength() );

					// Set base mailbox.
					// __kyl__(6/6/2006) Not sure why we need to do this. Can
					// they return a different one to the one we tried to
					// relogon to?
					state_ = StateWaitingForSetBaseToFinal;
					EntityMailBoxRef*	pBaseRef = &baseRef;
					EntityDBRecordIn	erec;
					erec.provideBaseMB( pBaseRef );
					Database::instance().putEntity( ekey_, erec, *this );
					// When putEntity() completes, onPutEntityComplete() is
					// called.

					return;	// Don't delete ourselves just yet.
				}
				case BaseAppIntInterface::LOG_ON_ATTEMPT_NOT_EXIST:
				{
					INFO_MSG( "RelogonAttemptHandler: Entity does not exist. "
						"Attempting to log on normally.\n" );
					// Log off entity from database since base no longer
					// exists.
					state_ = StateWaitingForSetBaseToNull;
					EntityDBRecordIn	erec;
					EntityMailBoxRef* 	pBaseRef = 0;
					erec.provideBaseMB( pBaseRef );
					Database::instance().putEntity( ekey_, erec, *this );
					// When putEntity() completes, onPutEntityComplete() is
					// called.

					return;	// Don't delete ourselves just yet.
				}
				case BaseAppIntInterface::LOG_ON_ATTEMPT_REJECTED:
				{
					INFO_MSG( "RelogonAttemptHandler: "
							"Re-login not allowed for %s.\n",
						pParams_->username().c_str() );

					Database::instance().sendFailure( replyID_, replyAddr_,
						false, /* offChannel */
						DatabaseLoginStatus::LOGIN_REJECTED_ALREADY_LOGGED_IN,
						"Relogin denied." );

					break;
				}
				default:
					CRITICAL_MSG( "RelogonAttemptHandler: Invalid result %d\n",
							int(result) );
					break;
			}
		}

		delete this;
	}

	virtual void handleException( const Mercury::NubException & exception,
		void * arg )
	{
		if (state_ != StateAborted)
		{
			const char * errorMsg = Mercury::reasonToString( exception.reason() );
			ERROR_MSG( "RelogonAttemptHandler: %s.\n", errorMsg );
			Database::instance().sendFailure( replyID_, replyAddr_,
					offChannel_,
					DatabaseLoginStatus::LOGIN_REJECTED_BASEAPP_TIMEOUT,
					errorMsg );
		}

		delete this;
	}

	// IDatabase::IPutEntityHandler override
	virtual void onPutEntityComplete( bool isOK, DatabaseID )
	{
		switch (state_)
		{
			case StateWaitingForSetBaseToFinal:
				if (isOK)
				{
					if (!offChannel_)
					{
						Database::getChannel( replyAddr_ ).send(
							&replyBundle_ );
					}
					else
					{
						Database::instance().nub().send( replyAddr_,
							replyBundle_ );
					}
				}
				else
				{
					this->sendEntityDeletedFailure();
				}
				break;
			case StateWaitingForSetBaseToNull:
				if (isOK)
				{
					this->onEntityLogOff();
				}
				else
				{
					this->sendEntityDeletedFailure();
				}
				break;
			case StateAborted:
				break;
			default:
				CRITICAL_MSG( "RelogonHandler::onPutEntityComplete: "
						"Invalid state %d\n",
					state_ );
				break;
		}

		delete this;
	}

	void sendEntityDeletedFailure()
	{
		// Someone deleted the entity while we were logging on.
		ERROR_MSG( "Database::logOn: Entity %s was deleted during logon.\n",
			ekey_.name.c_str() );

		Database::instance().sendFailure( replyID_, replyAddr_,
				offChannel_,
				DatabaseLoginStatus::LOGIN_REJECTED_NO_SUCH_USER,
				"Entity deleted during login." );
	}

	// This function is called when the entity that we're trying to re-logon
	// to suddenly logs off.
	void onEntityLogOff()
	{
		if (state_ != StateAborted)
		{
			// Abort our re-logon attempt... actually, just flag it as aborted.
			// Still need to wait for callbacks.
			state_ = StateAborted;
			Database::instance().onCompleteRelogonAttempt(
					ekey_.typeID, ekey_.dbID );

			// Log on normally
			Database::instance().logOn( replyAddr_, replyID_, pParams_,
				addrForProxy_, offChannel_ );
		}
	}

private:
	State 					state_;
	EntityDBKey				ekey_;
	Mercury::Address 		replyAddr_;
	bool 					offChannel_;
	Mercury::ReplyID 		replyID_;
	LogOnParamsPtr 			pParams_;
	Mercury::Address 		addrForProxy_;
	Mercury::Bundle			replyBundle_;
};


// -----------------------------------------------------------------------------
// Section: Entity entry database requests
// -----------------------------------------------------------------------------

/**
 *	This method handles a logOn request.
 */
void Database::logOn( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data )
{
	// __kyl__ (9/9/2005) There are no specific code in logOn to handle the
	// update process because it is assumed that logins are disabled during
	// update - or at least during the part where we need to consider multiple
	// entity versions.
	Mercury::Address addrForProxy;
	bool offChannel;
	LogOnParamsPtr pParams = new LogOnParams();

	data >> addrForProxy >> offChannel >> *pParams;

	const MD5::Digest & digest = pParams->digest();
	bool goodDigest = (digest == this->getEntityDefs().getDigest());

	if (!goodDigest && allowEmptyDigest_)
	{
		goodDigest = true;

		// Bots and egclient send an empty digest
		for (uint32 i = 0; i < sizeof( digest ); i++)
		{
			if (digest.bytes[i] != '\0')
			{
				goodDigest = false;
				break;
			}
		}

		if (goodDigest)
		{
			WARNING_MSG( "Database::logOn: %s logged on with empty digest.\n",
				pParams->username().c_str() );
		}
	}

	if (!goodDigest)
	{
		ERROR_MSG( "Database::logOn: Incorrect digest\n" );
		this->sendFailure( header.replyID, srcAddr, offChannel,
			DatabaseLoginStatus::LOGIN_REJECTED_BAD_DIGEST,
			"Defs digest mismatch." );
		return;
	}

	this->logOn( srcAddr, header.replyID, pParams, addrForProxy, offChannel );
}


/**
 *	This method attempts to log on a player.
 */
void Database::logOn( const Mercury::Address & srcAddr,
		Mercury::ReplyID replyID,
		LogOnParamsPtr pParams,
		const Mercury::Address & addrForProxy,
		bool offChannel )
{
	if (status_.status() != DBStatus::RUNNING)
	{
		INFO_MSG( "Database::logOn: "
			"Login failed for %s. Server not ready.\n",
			pParams->username().c_str() );

		this->sendFailure( replyID, srcAddr, offChannel,
			DatabaseLoginStatus::LOGIN_REJECTED_SERVER_NOT_READY,
			"Server not ready." );

		return;
	}

	bool isOverloaded = curLoad_ > maxLoad_;

	if (this->calculateOverloaded( isOverloaded ))
	{
		INFO_MSG( "Database::logOn: "
				"Login failed for %s. We are overloaded "
				"(load=%.02f > max=%.02f)\n",
			pParams->username().c_str(), curLoad_, maxLoad_ );

		this->sendFailure( replyID, srcAddr, offChannel,
			DatabaseLoginStatus::LOGIN_REJECTED_DBMGR_OVERLOAD,
			"DBMgr is overloaded." );

		return;
	}

	if (anyCellAppOverloaded_)
	{
		INFO_MSG( "Database::logOn: "
			"Login failed for %s. At least one CellApp is overloaded.\n",
			pParams->username().c_str() );

		this->sendFailure( replyID, srcAddr, offChannel,
			DatabaseLoginStatus::LOGIN_REJECTED_CELLAPP_OVERLOAD,
			"At least one CellApp is overloaded." );

		return;
	}

	LoginHandler * pHandler =
		new LoginHandler( pParams, addrForProxy, srcAddr, offChannel, replyID );

	pHandler->login();
}

/**
 * Performs checks to see whether we should see ourselves as being
 * overloaded.
 */
bool Database::calculateOverloaded( bool isOverloaded )
{
	if (isOverloaded)
	{
		uint64 overloadTime;

		// Start rate limiting logins
		if (overloadStartTime_ == 0)
			overloadStartTime_ = timestamp();

		overloadTime = timestamp() - overloadStartTime_;
		INFO_MSG( "DBMgr::Overloaded for "PRIu64"ms\n", overloadTime/(stampsPerSecond()/1000) );

		return (overloadTime >= allowOverloadPeriod_);
	}
	else
	{
		// We're not overloaded, stop the overload timer.
		overloadStartTime_ = 0;
		return false;
	}
}

/**
 *	This method is called when there is a log on request for an entity
 *	that is already logged on.
 */
void Database::onLogOnLoggedOnUser( EntityTypeID typeID, DatabaseID dbID,
	LogOnParamsPtr pParams,
	const Mercury::Address & clientAddr, const Mercury::Address & replyAddr,
	bool offChannel, Mercury::ReplyID replyID,
	const EntityMailBoxRef* pExistingBase )
{
	// TODO: Make this a member
	bool shouldAttemptRelogon_ = true;

	if (shouldAttemptRelogon_ &&
		(Database::instance().getInProgRelogonAttempt( typeID, dbID ) == NULL))
	{
		if (Database::isValidMailBox( pExistingBase ))
		{
			// Logon to existing base
			Mercury::ChannelSender sender(
				Database::getChannel( pExistingBase->addr ) );

			Mercury::Bundle & bundle = sender.bundle();
			bundle.startRequest( BaseAppIntInterface::logOnAttempt,
				new RelogonAttemptHandler( pExistingBase->type(), dbID,
					replyAddr, offChannel, replyID, pParams, clientAddr )
				);

			bundle << pExistingBase->id;
			bundle << clientAddr;
			bundle << pParams->encryptionKey();

			bool hasPassword =
				this->getEntityDefs().entityTypeHasPassword( typeID );

			bundle << hasPassword;

			if (hasPassword)
			{
				bundle << pParams->password();
			}
		}
		else
		{
			// Another logon still in progress.
			WARNING_MSG( "Database::logOn: %s already logging in\n",
				pParams->username().c_str() );

			this->sendFailure( replyID, replyAddr, offChannel,
				DatabaseLoginStatus::LOGIN_REJECTED_ALREADY_LOGGED_IN,
			   "Another login of same name still in progress." );
		}
	}
	else
	{
		// Another re-logon already in progress.
		INFO_MSG( "Database::logOn: %s already logged on\n",
			pParams->username().c_str() );

		this->sendFailure( replyID, replyAddr, offChannel,
				DatabaseLoginStatus::LOGIN_REJECTED_ALREADY_LOGGED_IN,
				"A relogin of same name still in progress." );
	}
}

/*
 *	This method creates a default entity (via createNewEntity() in
 *	custom.cpp) and serialises it into the stream.
 *
 *	@param	The type of entity to create.
 *	@param	The name of the entity (for entities with a name property)
 *	@param	The stream to serialise entity into.
 *	@param	If non-NULL, this will override the "password" property of
 *	the entity.
 *	@return	True if successful.
 */
bool Database::defaultEntityToStrm( EntityTypeID typeID,
	const std::string& name, BinaryOStream& strm,
	const std::string* pPassword ) const
{
	DataSectionPtr pSection = createNewEntity( typeID, name );
	bool isCreated = pSection.exists();
	if (isCreated)
	{
		if (pPassword)
		{
			if (this->getEntityDefs().getPropertyType( typeID, "password" ) == "BLOB")
				pSection->writeBlob( "password", *pPassword );
			else
				pSection->writeString( "password", *pPassword );
		}

		const EntityDescription& desc =
			this->getEntityDefs().getEntityDescription( typeID );
		desc.addSectionToStream( pSection, strm,
			EntityDescription::BASE_DATA | EntityDescription::CELL_DATA |
			EntityDescription::ONLY_PERSISTENT_DATA );
		if (desc.hasCellScript())
		{
			Vector3	defaultVec( 0, 0, 0 );

			strm << defaultVec;	// position
			strm << defaultVec;	// direction
			strm << SpaceID(0);	// space ID
		}

		strm << TimeStamp(0);	// game time
	}

	return isCreated;
}

/*
 *	This method inserts the "header" info into the bundle for a
 *	BaseAppMgrInterface::createEntity message, up till the point
 *	where entity properties should begin.
 *
 *	@return	If dbID is 0, then this function returns the position in the
 *	bundle where you should put the DatabaseID.
 */
DatabaseID* Database::prepareCreateEntityBundle( EntityTypeID typeID,
		DatabaseID dbID, const Mercury::Address& addrForProxy,
		Mercury::ReplyMessageHandler* pHandler, Mercury::Bundle& bundle,
		LogOnParamsPtr pParams )
{
	bundle.startRequest( BaseAppMgrInterface::createEntity, pHandler, 0,
		Mercury::DEFAULT_REQUEST_TIMEOUT + 1000000 ); // 1 second extra

	// This data needs to match BaseAppMgr::createBaseWithCellData.
	bundle	<< EntityID( 0 )
			<< typeID;

	DatabaseID*	pDbID = 0;
	if (dbID)
		bundle << dbID;
	else
		pDbID = reinterpret_cast<DatabaseID*>(bundle.reserve( sizeof(*pDbID) ));

	// This is the client address. It is used if we are making a proxy.
	bundle << addrForProxy;

	bundle << ((pParams != NULL) ? pParams->encryptionKey() : "");

	bundle << true;		// Has persistent data only

	return pDbID;
}

/**
 *	This helper method sends a failure reply.
 */
void Database::sendFailure( Mercury::ReplyID replyID,
		const Mercury::Address & dstAddr, bool offChannel,
		DatabaseLoginStatus reason, const char * pDescription )
{
	MF_ASSERT( reason != DatabaseLoginStatus::LOGGED_ON );

	Mercury::Bundle * pBundle;

	if (offChannel)
	{
		pBundle = new Mercury::Bundle();
	}
	else
	{
		Mercury::ChannelSender sender( Database::getChannel( dstAddr ) );
		pBundle = &sender.bundle();
	}
	Mercury::Bundle & bundle = *pBundle;

	bundle.startReply( replyID );
	bundle << uint8( reason );

	if (pDescription == NULL)
		pDescription = UNSPECIFIED_ERROR_STR;

	bundle << pDescription;

	if (offChannel)
	{
		Database::instance().nub().send( dstAddr, bundle );
		delete pBundle;
	}
}

/**
 *	This class is used by Database::writeEntity() to write entities into
 *	the database and wait for the result.
 */
class WriteEntityHandler : public IDatabase::IPutEntityHandler,
                           public IDatabase::IDelEntityHandler
{
	EntityDBKey				ekey_;
	int8					flags_;
	bool					shouldReply_;
	Mercury::ReplyID		replyID_;
	const Mercury::Address	srcAddr_;

public:
	WriteEntityHandler( const EntityDBKey ekey, int8 flags, bool shouldReply,
			Mercury::ReplyID replyID, const Mercury::Address & srcAddr )
		: ekey_(ekey), flags_(flags), shouldReply_(shouldReply),
		replyID_(replyID), srcAddr_(srcAddr)
	{}
	virtual ~WriteEntityHandler() {}

	void writeEntity( BinaryIStream & data, EntityID entityID );

	void deleteEntity();

	// IDatabase::IPutEntityHandler override
	virtual void onPutEntityComplete( bool isOK, DatabaseID );

	// IDatabase::IDelEntityHandler override
	virtual void onDelEntityComplete( bool isOK );

private:
	void putEntity( EntityDBRecordIn& erec )
	{
		Database::instance().putEntity( ekey_, erec, *this );
	}
	void finalise(bool isOK);
};

/**
 *	This method writes the entity data into the database.
 *
 *	@param	data	Stream should be currently at the start of the entity's
 *	data.
 *	@param	entityID	The entity's base mailbox object ID.
 */

void WriteEntityHandler::writeEntity( BinaryIStream & data, EntityID entityID )
{
	EntityDBRecordIn erec;
	if (flags_ & WRITE_ALL_DATA)
		erec.provideStrm( data );

	if (flags_ & WRITE_LOG_OFF)
	{
		EntityMailBoxRef* pBaseRef = 0;
		erec.provideBaseMB( pBaseRef );
		this->putEntity( erec );
	}
	else if (!ekey_.dbID)
	{	// New entity is checked out straight away
		EntityMailBoxRef	baseRef;
		baseRef.init( entityID, srcAddr_, EntityMailBoxRef::BASE,
			ekey_.typeID );
		EntityMailBoxRef* pBaseRef = &baseRef;
		erec.provideBaseMB( pBaseRef );
		this->putEntity( erec );
	}
	else
	{
		this->putEntity( erec );
	}
	// When putEntity() completes onPutEntityComplete() is called.
}

/**
 *	IDatabase::IPutEntityHandler override
 */
void WriteEntityHandler::onPutEntityComplete( bool isOK, DatabaseID dbID )
{
	ekey_.dbID = dbID;
	if (!isOK)
	{
		ERROR_MSG( "Database::writeEntity: Failed to update entity %"FMT_DBID
			" of type %d.\n", dbID, ekey_.typeID );
	}

	this->finalise(isOK);
}

/**
 *	Deletes the entity from the database.
 */
void WriteEntityHandler::deleteEntity()
{
	MF_ASSERT( flags_ & WRITE_DELETE_FROM_DB );
	Database::instance().delEntity( ekey_, *this );
	// When delEntity() completes, onDelEntityComplete() is called.
}

/**
 *	IDatabase::IDelEntityHandler override
 */
void WriteEntityHandler::onDelEntityComplete( bool isOK )
{
	if (!isOK)
	{
		ERROR_MSG( "Database::writeEntity: Failed to delete entity %"FMT_DBID" of type %d.\n",
			ekey_.dbID, ekey_.typeID );
	}

	this->finalise(isOK);
}

/**
 *	This function does some common stuff at the end of the operation.
 */
void WriteEntityHandler::finalise( bool isOK )
{
	if (shouldReply_)
	{
		Mercury::ChannelSender sender( Database::getChannel( srcAddr_ ) );
		sender.bundle().startReply( replyID_ );
		sender.bundle() << isOK << ekey_.dbID;
	}

	if (isOK && (flags_ & WRITE_LOG_OFF))
	{
		Database::instance().onEntityLogOff( ekey_.typeID, ekey_.dbID );
	}

	delete this;
}

/**
 *	This method handles the writeEntity mercury message.
 */
void Database::writeEntity( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data )
{
	Mercury::Nub::TransientMiniTimer::Op< Mercury::Nub::TransientMiniTimer >
		writeEntityTimerOp(writeEntityTimer_);

	int8 flags;
	data >> flags;
	// if this fails then the calling component had no need to call us
	MF_ASSERT( flags & (WRITE_ALL_DATA|WRITE_LOG_OFF) );

	EntityDBKey	ekey( 0, 0 );
	data >> ekey.typeID >> ekey.dbID;

	// TRACE_MSG( "Database::writeEntity: %lld flags=%i\n",
	//		   ekey.dbID, flags );

	bool isOkay = this->getEntityDefs().isValidEntityType( ekey.typeID );
	if (!isOkay)
	{
		ERROR_MSG( "Database::writeEntity: Invalid entity type %d\n",
			ekey.typeID );

		if (header.flags & Mercury::Packet::FLAG_HAS_REQUESTS)
		{
			Mercury::ChannelSender sender( Database::getChannel( srcAddr ) );
			sender.bundle().startReply( header.replyID );
			sender.bundle() << isOkay << ekey.dbID;
		}
	}
	else
	{
		WriteEntityHandler* pHandler =
			new WriteEntityHandler( ekey, flags,
				(header.flags & Mercury::Packet::FLAG_HAS_REQUESTS),
				header.replyID, srcAddr );
		if (flags & WRITE_DELETE_FROM_DB)
		{
			pHandler->deleteEntity();
		}
		else
		{
			EntityID entityID;
			data >> entityID;

			pHandler->writeEntity( data, entityID );
		}
	}
}

/**
 *	This method is called when we've just logged off an entity.
 *
 *	@param	typeID The type ID of the logged off entity.
 *	@param	dbID The database ID of the logged off entity.
 */
void Database::onEntityLogOff( EntityTypeID typeID, DatabaseID dbID )
{
	// Notify any re-logon handler waiting on this entity that it has gone.
	RelogonAttemptHandler* pHandler =
			this->getInProgRelogonAttempt( typeID, dbID );
	if (pHandler)
		pHandler->onEntityLogOff();
}

/**
 *	This class is used by Database::loadEntity() to load an entity from
 *	the database and wait for the results.
 */
class LoadEntityHandler : public Database::GetEntityHandler,
                          public IDatabase::IPutEntityHandler,
                          public Database::ICheckoutCompletionListener
{
	EntityDBKey			ekey_;
	EntityMailBoxRef	baseRef_;
	EntityMailBoxRef*	pBaseRef_;
	EntityDBRecordOut	outRec_;
	Mercury::Address	srcAddr_;
	EntityID			entityID_;
	Mercury::ReplyID	replyID_;
	Mercury::Bundle		replyBundle_;
	DatabaseID*			pStrmDbID_;

public:
	LoadEntityHandler( const EntityDBKey& ekey,
			const Mercury::Address& srcAddr, EntityID entityID,
			Mercury::ReplyID replyID ) :
		ekey_(ekey), baseRef_(), pBaseRef_(0), outRec_(), srcAddr_(srcAddr),
		entityID_(entityID), replyID_(replyID), pStrmDbID_(0)
	{}
	virtual ~LoadEntityHandler() {}

	void loadEntity();

	// IDatabase::IGetEntityHandler/Database::GetEntityHandler overrides
	virtual EntityDBKey& key()					{	return ekey_;	}
	virtual EntityDBRecordOut& outrec() 		{	return outRec_;	}
	virtual void onGetEntityCompleted( bool isOK );

	// IDatabase::IPutEntityHandler override
	virtual void onPutEntityComplete( bool isOK, DatabaseID );

	// Database::ICheckoutCompletionListener override
	virtual void onCheckoutCompleted( const EntityMailBoxRef* pBaseRef );

private:
	void sendAlreadyCheckedOutReply( const EntityMailBoxRef& baseRef );
};



void LoadEntityHandler::loadEntity()
{
	// Start reply bundle even though we're not sure the entity exists.
	// This is to take advantage of getEntity() streaming directly into bundle.
	replyBundle_.startReply( replyID_ );
	replyBundle_ << (uint8) DatabaseLoginStatus::LOGGED_ON;

	if (ekey_.dbID)
	{
		replyBundle_ << ekey_.dbID;
	}
	else
	{
		// Reserve space for a DBId since we don't know what it is yet.
		pStrmDbID_ = reinterpret_cast<DatabaseID*>(replyBundle_.reserve(sizeof(*pStrmDbID_)));
	}

	pBaseRef_ = &baseRef_;
	outRec_.provideBaseMB( pBaseRef_ );		// Get base mailbox into baseRef_
	outRec_.provideStrm( replyBundle_ );	// Get entity data into bundle
	Database::instance().getEntity( *this );
	// When getEntity() completes, onGetEntityCompleted() is called.
}

/**
 *	Database::GetEntityHandler override
 */
void LoadEntityHandler::onGetEntityCompleted( bool isOK )
{
	if (isOK)
	{
		if ( !pBaseRef_ &&	// same as outRec_.getBaseMB()
			 Database::instance().onStartEntityCheckout( ekey_ ) )
		{	// Not checked out and not in the process of being checked out.
			if (pStrmDbID_)
			{
				// Now patch the dbID in the stream.
				*pStrmDbID_ = ekey_.dbID;
			}

			// Check out entity.
			baseRef_.init( entityID_, srcAddr_, EntityMailBoxRef::BASE, ekey_.typeID );

			pBaseRef_ = &baseRef_;
			EntityDBRecordIn inrec;
			inrec.provideBaseMB( pBaseRef_ );
			Database::instance().putEntity( ekey_, inrec, *this );
			// When putEntity() completes, onPutEntityComplete() is called.
			// __kyl__ (24/6/2005) Race condition when multiple check-outs of the same entity
			// at the same time. More than one can succeed. Need to check for checking out entity?
			// Also, need to prevent deletion of entity which checking out entity.
			return;	// Don't delete ourselves just yet.
		}
		else if (pBaseRef_) // Already checked out
		{
			this->sendAlreadyCheckedOutReply( *pBaseRef_ );
		}
		else // In the process of being checked out.
		{
			MF_VERIFY( Database::instance().registerCheckoutCompletionListener(
					ekey_.typeID, ekey_.dbID, *this ) );
			// onCheckoutCompleted() will be called when the entity is fully
			// checked out.
			return;	// Don't delete ourselves just yet.
		}
	}
	else
	{
		if (ekey_.dbID)
			ERROR_MSG( "Database::loadEntity: No such entity %"FMT_DBID" of type %d.\n",
					ekey_.dbID, ekey_.typeID );
		else
			ERROR_MSG( "Database::loadEntity: No such entity %s of type %d.\n",
					ekey_.name.c_str(), ekey_.typeID );
		Database::instance().sendFailure( replyID_, srcAddr_, false,
			DatabaseLoginStatus::LOGIN_REJECTED_NO_SUCH_USER,
			"No such user." );
	}
	delete this;
}

/**
 *	IDatabase::IPutEntityHandler override
 */
void LoadEntityHandler::onPutEntityComplete( bool isOK, DatabaseID )
{
	if (isOK)
	{
		Database::getChannel( srcAddr_ ).send( &replyBundle_ );
	}
	else
	{
		// Something horrible like database disconnected or something.
		Database::instance().sendFailure( replyID_, srcAddr_, false,
			DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE,
			"Unexpected failure from database layer." );
	}
	// Need to call onCompleteEntityCheckout() after we send reply since
	// onCompleteEntityCheckout() can trigger other tasks to send their
	// own replies which assumes that the entity creation has already
	// succeeded or failed (depending on the parameters we pass here).
	// Obviously, if they sent their reply before us, then the BaseApp will
	// get pretty confused since the entity creation is not completed until
	// it gets our reply.
	Database::instance().onCompleteEntityCheckout( ekey_,
			(isOK) ? &baseRef_ : NULL );

	delete this;
}

/**
 *	Database::ICheckoutCompletionListener override
 */
void LoadEntityHandler::onCheckoutCompleted( const EntityMailBoxRef* pBaseRef )
{
	if (pBaseRef)
	{
		this->sendAlreadyCheckedOutReply( *pBaseRef );
	}
	else
	{
		// __kyl__ (11/8/2006) Currently there are no good reason that a
		// checkout would fail. This usually means something has gone horribly
		// wrong. We'll just return an error for now. We could retry the
		// operation from scratch but that's just too much work for now.
		Database::instance().sendFailure( replyID_, srcAddr_, false,
				DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE,
				"Unexpected failure from database layer." );
	}

	delete this;
}

/**
 *	This method sends back a reply that says that the entity is already
 * 	checked out.
 */
void LoadEntityHandler::sendAlreadyCheckedOutReply(
		const EntityMailBoxRef& baseRef )
{
	Mercury::ChannelSender sender( Database::getChannel( srcAddr_ ) );
	Mercury::Bundle & bundle = sender.bundle();

	bundle.startReply( replyID_ );
	bundle << uint8( DatabaseLoginStatus::LOGIN_REJECTED_ALREADY_LOGGED_IN );
	bundle << ekey_.dbID;
	bundle << baseRef;
}

/**
 *	This method handles a message to load an entity from the database.
 */
void Database::loadEntity( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream& input )
{
	EntityDBKey	ekey( 0, 0 );
	DataSectionPtr pSection;
	bool byName;
	EntityID entityID;
	input >> ekey.typeID >> entityID >> byName;

	if (!this->getEntityDefs().isValidEntityType( ekey.typeID ))
	{
		ERROR_MSG( "Database::loadEntity: Invalid entity type %d\n",
			ekey.typeID );
		this->sendFailure( header.replyID, srcAddr, false,
			DatabaseLoginStatus::LOGIN_CUSTOM_DEFINED_ERROR,
			"Invalid entity type" );
		return;
	}

	if (byName)
		input >> ekey.name;
	else
		input >> ekey.dbID;

	LoadEntityHandler* pHandler = new LoadEntityHandler( ekey, srcAddr, entityID,
		header.replyID );
	pHandler->loadEntity();
}

/**
 *	This class processes a request to delete an entity from the database.
 */
class DeleteEntityHandler : public Database::GetEntityHandler
                          , public IDatabase::IDelEntityHandler
{
	Mercury::Bundle		replyBundle_;
	Mercury::Address	srcAddr_;
	EntityDBKey			ekey_;
	EntityMailBoxRef	baseRef_;
	EntityMailBoxRef*	pBaseRef_;
	EntityDBRecordOut	outRec_;

public:
	DeleteEntityHandler( EntityTypeID typeID, DatabaseID dbID,
		const Mercury::Address& srcAddr, Mercury::ReplyID replyID );
	DeleteEntityHandler( EntityTypeID typeID, const std::string& name,
		const Mercury::Address& srcAddr, Mercury::ReplyID replyID );
	virtual ~DeleteEntityHandler() {}

	void deleteEntity();

	// IDatabase::IGetEntityHandler/Database::GetEntityHandler overrides
	virtual EntityDBKey& key()					{	return ekey_;	}
	virtual EntityDBRecordOut& outrec()			{	return outRec_;	}
	virtual void onGetEntityCompleted( bool isOK );

	// IDatabase::IDelEntityHandler override
	virtual void onDelEntityComplete( bool isOK );
};

/**
 *	Constructor. For deleting entity by database ID.
 */
DeleteEntityHandler::DeleteEntityHandler( EntityTypeID typeID,
	DatabaseID dbID, const Mercury::Address& srcAddr, Mercury::ReplyID replyID )
	: replyBundle_(), srcAddr_(srcAddr), ekey_(typeID, dbID),
	baseRef_(), pBaseRef_(0), outRec_()
{
	replyBundle_.startReply(replyID);
}

/**
 *	Constructor. For deleting entity by name.
 */
DeleteEntityHandler::DeleteEntityHandler( EntityTypeID typeID,
		const std::string& name, const Mercury::Address& srcAddr,
		Mercury::ReplyID replyID )
	: replyBundle_(), srcAddr_(srcAddr), ekey_(typeID, 0, name),
	baseRef_(), pBaseRef_(0), outRec_()
{
	replyBundle_.startReply(replyID);
}

/**
 *	Starts the process of deleting the entity.
 */
void DeleteEntityHandler::deleteEntity()
{
	if (Database::instance().getEntityDefs().isValidEntityType( ekey_.typeID ))
	{
		// See if it is checked out
		pBaseRef_ = &baseRef_;
		outRec_.provideBaseMB(pBaseRef_);
		Database::instance().getEntity( *this );
		// When getEntity() completes, onGetEntityCompleted() is called.
	}
	else
	{
		ERROR_MSG( "DeleteEntityHandler::deleteEntity: Invalid entity type "
				"%d\n", int(ekey_.typeID) );
		replyBundle_ << int32(-1);

		Database::getChannel( srcAddr_ ).send( &replyBundle_ );
		delete this;
	}
}

/**
 *	Database::GetEntityHandler overrides
 */
void DeleteEntityHandler::onGetEntityCompleted( bool isOK )
{
	if (isOK)
	{
		if (Database::isValidMailBox(outRec_.getBaseMB()))
		{
			TRACE_MSG( "Database::deleteEntity: entity checked out\n" );
			// tell the caller where to find it
			replyBundle_ << *outRec_.getBaseMB();
		}
		else
		{	// __kyl__ TODO: Is it a problem if we delete the entity when it's awaiting creation?
			Database::instance().delEntity( ekey_, *this );
			// When delEntity() completes, onDelEntityComplete() is called.
			return;	// Don't send reply just yet.
		}
	}
	else
	{	// Entity doesn't exist
		TRACE_MSG( "Database::deleteEntity: no such entity\n" );
		replyBundle_ << int32(-1);
	}

	Database::getChannel( srcAddr_ ).send( &replyBundle_ );

	delete this;
}

/**
 *	IDatabase::IDelEntityHandler overrides
 */
void DeleteEntityHandler::onDelEntityComplete( bool isOK )
{
	if ( isOK )
	{
		TRACE_MSG( "Database::deleteEntity: succeeded\n" );
	}
	else
	{
		ERROR_MSG( "Database::deleteEntity: Failed to delete entity '%s' "
					"(%"FMT_DBID") of type %d\n",
					ekey_.name.c_str(), ekey_.dbID, ekey_.typeID );
		replyBundle_ << int32(-1);
	}

	Database::getChannel( srcAddr_ ).send( &replyBundle_ );

	delete this;
}

/**
 *	This message deletes the specified entity if it exists and is not
 *	checked out. If it is checked out, it returns a mailbox to it instead.
 *	If it does not exist, it returns -1 as an int32.
 */
void Database::deleteEntity( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	DBInterface::deleteEntityArgs & args )
{
	DeleteEntityHandler* pHandler = new DeleteEntityHandler( args.entityTypeID,
			args.dbid, srcAddr, header.replyID );
	pHandler->deleteEntity();
}

/**
 *	This message deletes the specified entity if it exists and is not
 *	checked out, and returns an empty message. If it is checked out,
 *	it returns a mailbox to it instead. If it does not exist,
 *	it returns -1 as an int32.
 */
void Database::deleteEntityByName( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data )
{
	EntityTypeID	entityTypeID;
	std::string		name;
	data >> entityTypeID >> name;

	DeleteEntityHandler* pHandler = new DeleteEntityHandler( entityTypeID,
			name, srcAddr, header.replyID );
	pHandler->deleteEntity();
}

/**
 *	This class processes a request to retrieve the base mailbox of a
 *	checked-out entity from the database.
 */
class LookupEntityHandler : public Database::GetEntityHandler
{
	Mercury::Bundle		replyBundle_;
	Mercury::Address	srcAddr_;
	EntityDBKey			ekey_;
	EntityMailBoxRef	baseRef_;
	EntityMailBoxRef*	pBaseRef_;
	EntityDBRecordOut	outRec_;
	bool				offChannel_;

public:
	LookupEntityHandler( EntityTypeID typeID, DatabaseID dbID,
		const Mercury::Address& srcAddr, Mercury::ReplyID replyID,
		bool offChannel );
	LookupEntityHandler( EntityTypeID typeID, const std::string& name,
		const Mercury::Address& srcAddr, Mercury::ReplyID replyID,
		bool offChannel );
	virtual ~LookupEntityHandler() {}

	void lookupEntity();

	// IDatabase::IGetEntityHandler/Database::GetEntityHandler overrides
	virtual EntityDBKey& key()					{	return ekey_;	}
	virtual EntityDBRecordOut& outrec()			{	return outRec_;	}
	virtual void onGetEntityCompleted( bool isOK );

};

/**
 *	Constructor. For looking up entity by database ID.
 */
LookupEntityHandler::LookupEntityHandler( EntityTypeID typeID,
		DatabaseID dbID, const Mercury::Address& srcAddr,
		Mercury::ReplyID replyID, bool offChannel ) :
	replyBundle_(), srcAddr_(srcAddr), ekey_(typeID, dbID),
	baseRef_(), pBaseRef_(0), outRec_(), offChannel_( offChannel )
{
	replyBundle_.startReply(replyID);
}

/**
 *	Constructor. For looking up entity by name.
 */
LookupEntityHandler::LookupEntityHandler( EntityTypeID typeID,
		const std::string& name, const Mercury::Address& srcAddr,
		Mercury::ReplyID replyID, bool offChannel ) :
	replyBundle_(), srcAddr_(srcAddr), ekey_(typeID, 0, name),
	baseRef_(), pBaseRef_(0), outRec_(), offChannel_( offChannel )
{
	replyBundle_.startReply(replyID);
}

/**
 *	Starts the process of looking up the entity.
 */
void LookupEntityHandler::lookupEntity()
{
	if (Database::instance().getEntityDefs().isValidEntityType( ekey_.typeID ))
	{
		pBaseRef_ = &baseRef_;
		outRec_.provideBaseMB(pBaseRef_);
		Database::instance().getEntity( *this );
		// When getEntity() completes, onGetEntityCompleted() is called.
	}
	else
	{
		ERROR_MSG( "LookupEntityHandler::lookupEntity: Invalid entity type "
				"%d\n", ekey_.typeID );
		replyBundle_ << int32(-1);

		Database::getChannel( srcAddr_ ).send( &replyBundle_ );

		delete this;
	}
}

void LookupEntityHandler::onGetEntityCompleted( bool isOK )
{
	if (isOK)
	{
		if (Database::isValidMailBox(outRec_.getBaseMB()))
		{	// Entity is checked out.
			replyBundle_ << *outRec_.getBaseMB();
		}
		else
		{
			// not checked out so empty message
		}
	}
	else
	{	// Entity doesn't exist
		replyBundle_ << int32(-1);
	}

	if (offChannel_)
	{
		Database::instance().nub().send( srcAddr_, replyBundle_ );
	}
	else
	{
		Database::getChannel( srcAddr_ ).send( &replyBundle_ );
	}

	delete this;
}

/**
 *	This message looks up the specified entity if it exists and is checked
 *	out and returns a mailbox to it. If it is not checked out it returns
 *	an empty message. If it does not exist, it returns -1 as an int32.
 */
void Database::lookupEntity( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	DBInterface::lookupEntityArgs & args )
{
	LookupEntityHandler* pHandler = new LookupEntityHandler( args.entityTypeID,
		args.dbid, srcAddr, header.replyID, args.offChannel );
	pHandler->lookupEntity();
}

/**
 *	This message looks up the specified entity if it exists and is checked
 *	out and returns a mailbox to it. If it is not checked out it returns
 *	an empty message. If it does not exist, it returns -1 as an int32.
 */
void Database::lookupEntityByName( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data )
{
	EntityTypeID	entityTypeID;
	std::string		name;
	bool			offChannel;
	data >> entityTypeID >> name >> offChannel;
	LookupEntityHandler* pHandler = new LookupEntityHandler( entityTypeID,
		name, srcAddr, header.replyID, offChannel );
	pHandler->lookupEntity();
}


/**
 *	This class processes a request to retrieve the DBID of an entity from the
 * 	database.
 */
class LookupDBIDHandler : public Database::GetEntityHandler
{
	Mercury::Bundle		replyBundle_;
	Mercury::Address	srcAddr_;
	EntityDBKey			ekey_;
	EntityDBRecordOut	outRec_;

public:
	LookupDBIDHandler( EntityTypeID typeID, const std::string& name,
			const Mercury::Address& srcAddr, Mercury::ReplyID replyID ) :
		replyBundle_(), srcAddr_( srcAddr ), ekey_( typeID, 0, name ), outRec_()
	{
		replyBundle_.startReply( replyID );
	}
	virtual ~LookupDBIDHandler() {}

	void lookupDBID();

	// IDatabase::IGetEntityHandler/Database::GetEntityHandler overrides
	virtual EntityDBKey& key()					{	return ekey_;	}
	virtual EntityDBRecordOut& outrec()			{	return outRec_;	}
	virtual void onGetEntityCompleted( bool isOK );

};

/**
 *	Starts the process of looking up the DBID.
 */
void LookupDBIDHandler::lookupDBID()
{
	if (Database::instance().getEntityDefs().isValidEntityType( ekey_.typeID ))
	{
		Database::instance().getEntity( *this );
		// When getEntity() completes, onGetEntityCompleted() is called.
	}
	else
	{
		ERROR_MSG( "LookupDBIDHandler::lookupDBID: Invalid entity type "
				"%d\n", ekey_.typeID );
		replyBundle_ << DatabaseID( 0 );
		Database::getChannel( srcAddr_ ).send( &replyBundle_ );
		delete this;
	}
}

/**
 *	Database::GetEntityHandler override.
 */
void LookupDBIDHandler::onGetEntityCompleted( bool isOK )
{
	replyBundle_ << ekey_.dbID;
	Database::getChannel( srcAddr_ ).send( &replyBundle_ );

	delete this;
}


/**
 *	This message looks up the DBID of the entity. The DBID will be 0 if the
 * 	entity does not exist.
 */
void Database::lookupDBIDByName( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header, BinaryIStream & data )
{
	EntityTypeID	entityTypeID;
	std::string		name;
	data >> entityTypeID >> name;

	LookupDBIDHandler* pHandler = new LookupDBIDHandler( entityTypeID,
		name, srcAddr, header.replyID );
	pHandler->lookupDBID();
}


// -----------------------------------------------------------------------------
// Section: Miscellaneous database requests
// -----------------------------------------------------------------------------

/**
 *	This class represents a request to execute a raw database command.
 */
class ExecuteRawCommandHandler : public IDatabase::IExecuteRawCommandHandler
{
	Mercury::Bundle		replyBundle_;
	Mercury::Address	srcAddr_;

public:
	ExecuteRawCommandHandler( const Mercury::Address srcAddr,
			Mercury::ReplyID replyID ) :
		replyBundle_(), srcAddr_(srcAddr)
	{
		replyBundle_.startReply(replyID);
	}
	virtual ~ExecuteRawCommandHandler() {}

	void executeRawCommand( const std::string& command )
	{
		Database::instance().getIDatabase().executeRawCommand( command, *this );
	}

	// IDatabase::IExecuteRawCommandHandler overrides
	virtual BinaryOStream& response()	{	return replyBundle_;	}
	virtual void onExecuteRawCommandComplete()
	{
		Database::getChannel( srcAddr_ ).send( &replyBundle_ );

		delete this;
	}
};

/**
 *	This method executaes a raw database command specific to the present
 *	implementation of the database interface.
 */
void Database::executeRawCommand( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data )
{
	std::string command( (char*)data.retrieve( header.length ), header.length );
	ExecuteRawCommandHandler* pHandler =
		new ExecuteRawCommandHandler( srcAddr, header.replyID );
	pHandler->executeRawCommand( command );
}

/**
 *  This method stores some previously used ID's into the database
 */
void Database::putIDs( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream& input )
{
	int numIDs = input.remainingLength() / sizeof(EntityID);
	INFO_MSG( "Database::putIDs: storing %d id's\n", numIDs);
	pDatabase_->putIDs( numIDs,
			static_cast<const EntityID*>( input.retrieve( input.remainingLength() ) ) );
}

/**
 *	This class represents a request to get IDs from the database.
 */
class GetIDsHandler : public IDatabase::IGetIDsHandler
{
	Mercury::Address	srcAddr_;
	Mercury::ReplyID 	replyID_;
	Mercury::Bundle		replyBundle_;

public:
	GetIDsHandler( const Mercury::Address& srcAddr, Mercury::ReplyID replyID ) :
		srcAddr_(srcAddr), replyID_( replyID ), replyBundle_()
	{
		replyBundle_.startReply( replyID );
	}
	virtual ~GetIDsHandler() {}

	void getIDs( int numIDs )
	{
		Database::instance().getIDatabase().getIDs( numIDs, *this );
	}

	virtual BinaryOStream& idStrm()	{ return replyBundle_; }
	virtual void resetStrm()
	{
		replyBundle_.clear();
		replyBundle_.startReply( replyID_ );
	}
	virtual void onGetIDsComplete()
	{
		Database::getChannel( srcAddr_ ).send( &replyBundle_ );
		delete this;
	}
};

/**
 *  This methods grabs some more ID's from the database
 */
void Database::getIDs( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream& input )
{
	int numIDs;
	input >> numIDs;
	INFO_MSG( "Database::getIDs: fetching %d id's\n", numIDs);

	GetIDsHandler* pHandler = new GetIDsHandler( srcAddr, header.replyID );
	pHandler->getIDs( numIDs );
}


/**
 *	This method writes information about the spaces to the database.
 */
void Database::writeSpaces( const Mercury::Address & /*srcAddr*/,
		Mercury::UnpackedMessageHeader & /*header*/,
		BinaryIStream & data )
{
	pDatabase_->writeSpaceData( data );
}


/**
 *	This method handles a message from the BaseAppMgr informing us that a
 *	BaseApp has died.
 */
void Database::handleBaseAppDeath( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
{
	if (this->hasMailboxRemapping())
	{
		ERROR_MSG( "Database::handleBaseAppDeath: Multiple BaseApp deaths "
				"not supported. Some mailboxes may not be updated\n" );
		this->endMailboxRemapping();
	}

	data >> remappingSrcAddr_ >> remappingDestAddrs_;

	INFO_MSG( "Database::handleBaseAppDeath: %s\n", (char *)remappingSrcAddr_ );

	pDatabase_->remapEntityMailboxes( remappingSrcAddr_, remappingDestAddrs_ );

	mailboxRemapCheckCount_ = 5;	// do remapping for 5 seconds.
}


/**
 *	This method ends the mailbox remapping for a dead BaseApp.
 */
void Database::endMailboxRemapping()
{
//	DEBUG_MSG( "Database::endMailboxRemapping: End of handleBaseAppDeath\n" );
	remappingDestAddrs_.clear();
}


/**
 *	This method changes the address of input mailbox to cater for recent
 * 	BaseApp death.
 */
void Database::remapMailbox( EntityMailBoxRef& mailbox ) const
{
	if (mailbox.addr == remappingSrcAddr_)
	{
		const Mercury::Address& newAddr =
				remappingDestAddrs_.addressFor( mailbox.id );
		// Mercury::Address::salt must not be modified.
		mailbox.addr.ip = newAddr.ip;
		mailbox.addr.port = newAddr.port;
	}
}

/**
 *	This method writes the game time to the database.
 */
void Database::writeGameTime( DBInterface::writeGameTimeArgs & args )
{
	pDatabase_->setGameTime( args.gameTime );
}

/**
 * 	Gathers initialisation data to send to BaseAppMgr
 */
void Database::sendInitData()
{
	// NOTE: Due to the asynchronous call, if two BaseAppMgrs register in
	// quick succession then we'll end up sending the init data twice to the
	// second BaseAppMgr.
	// onGetBaseAppMgrInitDataComplete() will be called.
	pDatabase_->getBaseAppMgrInitData( *this );
}

/**
 * 	Sends initialisation data to BaseAppMgr
 */
void Database::onGetBaseAppMgrInitDataComplete( TimeStamp gameTime,
		int32 maxSecondaryDBAppID )
{
	// __kyl__(14/8/2008) Cater for case where DB consolidation is run during
	// start-up and has not yet completed. In that case, the
	// maxSecondaryDBAppID is 0 since that's what it would be if data
	// consolidation completed successfully. If it doesn't complete
	// successfully then we'll shutdown the system so sending the "wrong"
	// value isn't that bad.
	if (status_.status() < DBStatus::RUNNING)
	{
		maxSecondaryDBAppID = 0;
	}

	Mercury::Bundle& bundle = baseAppMgr_.bundle();
	bundle.startMessage( BaseAppMgrInterface::initData );
	bundle << gameTime;
	bundle << maxSecondaryDBAppID;

	baseAppMgr_.send();
}

/**
 *	This method sets whether we have started. It is used so that the server can
 *	be started from a watcher.
 */
void Database::hasStartBegun( bool hasStartBegun )
{
	if (hasStartBegun)
	{
		if (status_.status() >= DBStatus::WAITING_FOR_APPS)
		{
			this->startServerBegin();
		}
		else
		{
			NOTICE_MSG( "Database::hasStartBegun: Server is not ready "
					"to start yet\n" );
		}
	}
}

/**
 *	This method starts the process of starting the server.
 */
void Database::startServerBegin( bool isRecover )
{
	if (status_.status() > DBStatus::WAITING_FOR_APPS)
	{
		ERROR_MSG( "Database::startServerBegin: Server already started. Cannot "
				"start again.\n" );
		return;
	}

	if (isRecover)
	{
		// Skip restore from DB
		this->startServerEnd( isRecover );
	}
	else
	{
		status_.set( DBStatus::RESTORING_STATE, "Restoring game state" );

		// Restore game state from DB
		Mercury::Bundle& bundle = baseAppMgr_.bundle();
		bundle.startMessage( BaseAppMgrInterface::spaceDataRestore );
		if (pDatabase_->getSpacesData( bundle ))
		{
			baseAppMgr_.send();

			EntityRecoverer* pRecoverer = new EntityRecoverer;
			pDatabase_->restoreEntities( *pRecoverer );
			// When restoreEntities() finishes startServerEnd() or
			// startServerError() will be called.
		}
		else
		{
			// Something bad happened. baseAppMgr_.bundle() is probably
			// stuffed.
			// Can't do shutdown since we'll try to send stuff to BaseAppMgr.
			// this->shutdown();
			CRITICAL_MSG( "Database::startServerBegin: Failed to read game "
					"time and space data from database!" );
		}
	}
}

/**
 *	This method completes the starting process for the DBMgr and starts all of
 *	the other processes in the system.
 */
void Database::startServerEnd( bool isRecover )
{
	if (status_.status() < DBStatus::RUNNING)
	{
		status_.set( DBStatus::RUNNING, "Running" );

		if (!isRecover)
		{
			TRACE_MSG( "Database::startServerEnd: Sending startup message\n" );
			Mercury::ChannelSender sender(
				Database::instance().baseAppMgr().channel() );

			sender.bundle().startMessage( BaseAppMgrInterface::startup );
		}
	}
	else
	{
		ERROR_MSG( "Database::startServerEnd: Already started.\n" );
	}
}

/**
 *	This method is called instead of startServerEnd() to indicate that there
 * 	was an error during or after startServerBegin().
 */
void Database::startServerError()
{
	MF_ASSERT( status_.status() < DBStatus::RUNNING );
	this->startSystemControlledShutdown();
}

/**
 *	This method is the called when an entity that is being checked out has
 * 	completed the checkout process. onStartEntityCheckout() should've been
 * 	called to mark the start of the operation. pBaseRef is the base mailbox
 * 	of the now checked out entity. pBaseRef should be NULL if the checkout
 * 	operation failed.
 */
bool Database::onCompleteEntityCheckout( const EntityKey& entityID,
	const EntityMailBoxRef* pBaseRef )
{
	bool isErased = (inProgCheckouts_.erase( entityID ) > 0);
	if (isErased && (checkoutCompletionListeners_.size() > 0))
	{
		std::pair< CheckoutCompletionListeners::iterator,
				CheckoutCompletionListeners::iterator > listeners =
			checkoutCompletionListeners_.equal_range( entityID );
		for ( CheckoutCompletionListeners::const_iterator iter =
				listeners.first; iter != listeners.second; ++iter )
		{
			iter->second->onCheckoutCompleted( pBaseRef );
		}
		checkoutCompletionListeners_.erase( listeners.first,
				listeners.second );
	}

	return isErased;
}

/**
 *	This method registers listener to be called when the entity identified
 * 	by typeID and dbID completes its checkout process. This function will
 * 	false and not register the listener if the entity is not currently
 * 	in the process of being checked out.
 */
bool Database::registerCheckoutCompletionListener( EntityTypeID typeID,
		DatabaseID dbID, Database::ICheckoutCompletionListener& listener )
{
	EntityKeySet::key_type key( typeID, dbID );
	bool isFound = (inProgCheckouts_.find( key ) != inProgCheckouts_.end());
	if (isFound)
	{
		CheckoutCompletionListeners::value_type item( key, &listener );
		checkoutCompletionListeners_.insert( item );
	}
	return isFound;
}


// -----------------------------------------------------------------------------
// Section: Message handling glue templates
// -----------------------------------------------------------------------------

/**
 *	This class is used to handle a fixed length request made of the database.
 */
template <class ARGS_TYPE>
class SimpleDBMessageHandler : public Mercury::InputMessageHandler
{
	public:
		typedef void (Database::*Handler)( ARGS_TYPE & args );

		SimpleDBMessageHandler( Handler handler ) : handler_( handler ) {}

	private:
		virtual void handleMessage( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
		{
			ARGS_TYPE args;
			if (sizeof(args) != 0)
				data >> args;
			(Database::instance().*handler_)( args );
		}

		Handler handler_;
};


/**
 *	This class is used to handle a fixed length request made of the database.
 */
template <class ARGS_TYPE>
class ReturnDBMessageHandler : public Mercury::InputMessageHandler
{
	public:
		typedef void (Database::*Handler)( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			ARGS_TYPE & args );

		ReturnDBMessageHandler( Handler handler ) : handler_( handler ) {}

	private:
		virtual void handleMessage( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
		{
			ARGS_TYPE & args = *(ARGS_TYPE*)data.retrieve( sizeof(ARGS_TYPE) );
			(Database::instance().*handler_)( srcAddr, header, args );
		}

		Handler handler_;
};



/**
 *	This class is used to handle a variable length request made of the database.
 */
class DBVarLenMessageHandler : public Mercury::InputMessageHandler
{
	public:
		typedef void (Database::*Handler)( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

		DBVarLenMessageHandler( Handler handler ) : handler_( handler ) {}

	private:
		virtual void handleMessage( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
		{
			(Database::instance().*handler_)( srcAddr, header, data );
		}

		Handler handler_;
};

#ifdef DBMGR_SELFTEST
#include "cstdmf/memory_stream.hpp"

class SelfTest : public IDatabase::IGetEntityHandler,
	             public IDatabase::IPutEntityHandler,
				 public IDatabase::IDelEntityHandler
{
	int				stepNum_;
	IDatabase&		db_;

	std::string		entityName_;
	std::string		badEntityName_;
	EntityTypeID	entityTypeID_;
	DatabaseID		newEntityID_;
	DatabaseID		badEntityID_;
	MemoryOStream	entityData_;
	EntityMailBoxRef entityBaseMB_;

	EntityDBKey		ekey_;
	EntityDBRecordOut outRec_;
	MemoryOStream	tmpEntityData_;
	EntityMailBoxRef tmpEntityBaseMB_;
	EntityMailBoxRef* pTmpEntityBaseMB_;

public:
	SelfTest( IDatabase& db ) :
		stepNum_(0), db_(db),
		entityName_("test_entity"), badEntityName_("Ben"), entityTypeID_(0),
		newEntityID_(0), badEntityID_(0), entityData_(), entityBaseMB_(),
		ekey_( 0, 0 ), outRec_(), tmpEntityData_(), tmpEntityBaseMB_(),
		pTmpEntityBaseMB_(0)
	{
		entityBaseMB_.init( 123, Mercury::Address( 7654321, 1234 ), EntityMailBoxRef::CLIENT_VIA_CELL, 1 );
	}
	virtual ~SelfTest() {}

	void nextStep();

	// IDatabase::IGetEntityHandler overrides
	virtual EntityDBKey& key()					{	return ekey_;	}
	virtual EntityDBRecordOut& outrec()			{	return outRec_;	}
	virtual void onGetEntityComplete( bool isOK );

	// IDatabase::IPutEntityHandler override
	virtual void onPutEntityComplete( bool isOK, DatabaseID );

	// IDatabase::IDelEntityHandler override
	virtual void onDelEntityComplete( bool isOK );
};

void SelfTest::nextStep()
{
	TRACE_MSG( "SelfTest::nextStep - step %d\n", stepNum_ + 1);
	switch (++stepNum_)
	{
		case 1:
		{	// Create new entity
			MemoryOStream strm;
			bool isDefaultEntityOK =
				Database::instance().defaultEntityToStrm( entityTypeID_, entityName_, strm );
			MF_ASSERT( isDefaultEntityOK );

			EntityDBRecordIn erec;
			erec.provideStrm( strm );
			EntityDBKey	ekey( entityTypeID_, 0);
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 2:
		{	// Check entity exists by name
			outRec_.unprovideBaseMB();
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, 0, entityName_ );
			db_.getEntity( *this );
			break;
		}
		case 3:
		{	// Check entity not exists by name
			outRec_.unprovideBaseMB();
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, 0, badEntityName_ );
			db_.getEntity( *this );
			break;
		}
		case 4:
		{	// Check entity exists by ID
			outRec_.unprovideBaseMB();
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, newEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 5:
		{	// Check entity not exists by ID
			outRec_.unprovideBaseMB();
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, badEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 6:
		{	// Get entity data by ID.
			outRec_.unprovideBaseMB();
			outRec_.provideStrm( entityData_ );
			ekey_ = EntityDBKey( entityTypeID_, newEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 7:
		{	// Get entity data by non-existent ID.
			outRec_.unprovideBaseMB();
			tmpEntityData_.reset();
			outRec_.provideStrm( tmpEntityData_ );
			ekey_ = EntityDBKey( entityTypeID_, badEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 8:
		{	// Overwrite entity data
			EntityDBRecordIn erec;
			erec.provideStrm( entityData_ );
			EntityDBKey	ekey( entityTypeID_, newEntityID_ );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 9:
		{	// Get entity data by name.
			outRec_.unprovideBaseMB();
			tmpEntityData_.reset();
			outRec_.provideStrm( tmpEntityData_ );
			ekey_ = EntityDBKey( entityTypeID_, 0, entityName_ );
			db_.getEntity( *this );
			break;
		}
		case 10:
		{	// Overwrite non-existent entity with new data
			EntityDBRecordIn erec;
			erec.provideStrm( entityData_ );
			EntityDBKey	ekey( entityTypeID_, badEntityID_ );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 11:
		{	// Get entity data with bad name.
			outRec_.unprovideBaseMB();
			tmpEntityData_.reset();
			outRec_.provideStrm( tmpEntityData_ );
			ekey_ = EntityDBKey( entityTypeID_, 0, badEntityName_ );
			db_.getEntity( *this );
			break;
		}
		case 12:
		{	// Get NULL base MB by ID
			pTmpEntityBaseMB_ = &tmpEntityBaseMB_;
			outRec_.provideBaseMB( pTmpEntityBaseMB_ );
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, newEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 13:
		{	// Get NULL base MB by bad ID
			pTmpEntityBaseMB_ = &tmpEntityBaseMB_;
			outRec_.provideBaseMB( pTmpEntityBaseMB_ );
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, badEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 14:
		{	// Put base MB with ID
			EntityMailBoxRef* pBaseRef = &entityBaseMB_;
			EntityDBRecordIn erec;
			erec.provideBaseMB( pBaseRef );
			EntityDBKey	ekey( entityTypeID_, newEntityID_ );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 15:
		{	// Put base MB with bad ID
			EntityMailBoxRef baseMB;
			EntityMailBoxRef* pBaseRef = &baseMB;
			EntityDBRecordIn erec;
			erec.provideBaseMB( pBaseRef );
			EntityDBKey	ekey( entityTypeID_, badEntityID_ );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 16:
		{	// Get base MB by ID
			tmpEntityBaseMB_.init( 666, Mercury::Address( 66666666, 666 ), EntityMailBoxRef::CLIENT_VIA_BASE, 1 );
			pTmpEntityBaseMB_ = &tmpEntityBaseMB_;
			outRec_.provideBaseMB( pTmpEntityBaseMB_ );
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, newEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 17:
		{	// Update base MB by ID
			entityBaseMB_.id = 999;
			EntityMailBoxRef* pBaseRef = &entityBaseMB_;
			EntityDBRecordIn erec;
			erec.provideBaseMB( pBaseRef );
			EntityDBKey	ekey( entityTypeID_, newEntityID_ );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 18:
		{	// Get entity data and base MB by ID.
			tmpEntityBaseMB_.init( 666, Mercury::Address( 66666666, 666 ), EntityMailBoxRef::CLIENT_VIA_BASE, 1 );
			pTmpEntityBaseMB_ = &tmpEntityBaseMB_;
			outRec_.provideBaseMB( pTmpEntityBaseMB_ );
			tmpEntityData_.reset();
			outRec_.provideStrm( tmpEntityData_ );
			ekey_ = EntityDBKey( entityTypeID_, newEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 19:
		{	// Put NULL base MB with ID
			EntityMailBoxRef* pBaseRef = 0;
			EntityDBRecordIn erec;
			erec.provideBaseMB( pBaseRef );
			EntityDBKey	ekey( entityTypeID_, newEntityID_ );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 20:
		{	// Get NULL base MB by name
			pTmpEntityBaseMB_ = &tmpEntityBaseMB_;
			outRec_.provideBaseMB( pTmpEntityBaseMB_ );
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, 0, entityName_ );
			db_.getEntity( *this );
			break;
		}
		case 21:
		{	// Get NULL base MB by bad name
			pTmpEntityBaseMB_ = &tmpEntityBaseMB_;
			outRec_.provideBaseMB( pTmpEntityBaseMB_ );
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, 0, badEntityName_ );
			db_.getEntity( *this );
			break;
		}
		case 22:
		{	// Delete entity by name
			EntityDBKey	ekey( entityTypeID_, 0, entityName_ );
			db_.delEntity( ekey, *this );
			break;
		}
		case 23:
		{	// Create new entity by stream.
			EntityDBRecordIn erec;
			erec.provideStrm( entityData_ );
			EntityDBKey	ekey( entityTypeID_, 0 );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 24:
		{	// Get entity data by name (again)
			outRec_.unprovideBaseMB();
			tmpEntityData_.reset();
			outRec_.provideStrm( tmpEntityData_ );
			ekey_ = EntityDBKey( entityTypeID_, 0, entityName_ );
			db_.getEntity( *this );
			break;
		}
		case 25:
		{	// Delete entity by ID
			EntityDBKey	ekey( entityTypeID_, newEntityID_ );
			db_.delEntity( ekey, *this );
			break;
		}
		case 26:
		{	// Delete non-existent entity by name
			EntityDBKey	ekey( entityTypeID_, 0, entityName_ );
			db_.delEntity( ekey, *this );
			break;
		}
		case 27:
		{	// Delete non-existent entity by ID
			EntityDBKey	ekey( entityTypeID_, newEntityID_ );
			db_.delEntity( ekey, *this );
			break;
		}
		default:
			TRACE_MSG( "SelfTest::nextStep - completed\n" );
			delete this;
			break;
	}
}

void SelfTest::onGetEntityComplete( bool isOK )
{
	switch (stepNum_)
	{
		case 2:
		{
			bool checkEntityWithName = isOK;
			MF_ASSERT( checkEntityWithName && (ekey_.dbID == newEntityID_) );
			break;
		}
		case 3:
		{
			bool checkEntityWithBadName = isOK;
			MF_ASSERT( !checkEntityWithBadName );
			break;
		}
		case 4:
		{
			bool checkEntityWithID = isOK;
			MF_ASSERT( checkEntityWithID && (ekey_.name == entityName_) );
			break;
		}
		case 5:
		{
			bool checkEntityWithBadID = isOK;
			MF_ASSERT( !checkEntityWithBadID );
			break;
		}
		case 6:
		{
			bool getEntityDataWithID = isOK;
			MF_ASSERT( getEntityDataWithID );
			break;
		}
		case 7:
		{
			bool getEntityDataWithBadID = isOK;
			MF_ASSERT( !getEntityDataWithBadID );
			break;
		}
		case 9:
		{
			bool getEntityDataWithName = isOK;
			MF_ASSERT( getEntityDataWithName &&
					(entityData_.size() == tmpEntityData_.size()) &&
					(memcmp( entityData_.data(), tmpEntityData_.data(), entityData_.size()) == 0) );
			break;
		}
		case 11:
		{
			bool getEntityDataWithBadName = isOK;
			MF_ASSERT( !getEntityDataWithBadName );
			break;
		}
		case 12:
		{
			bool getNullBaseMBWithID = isOK;
			MF_ASSERT( getNullBaseMBWithID && outRec_.getBaseMB() == 0 );
			break;
		}
		case 13:
		{
			bool getNullBaseMBWithBadID = isOK;
			MF_ASSERT( !getNullBaseMBWithBadID );
			break;
		}
		case 16:
		{
			bool getBaseMBWithID = isOK;
			MF_ASSERT( getBaseMBWithID && outRec_.getBaseMB() &&
					(tmpEntityBaseMB_.id == entityBaseMB_.id) &&
					(tmpEntityBaseMB_.type() == entityBaseMB_.type()) &&
					(tmpEntityBaseMB_.component() == entityBaseMB_.component()) &&
					(tmpEntityBaseMB_.addr == entityBaseMB_.addr) );
			break;
		}
		case 18:
		{
			bool getEntityWithID = isOK;
			MF_ASSERT( getEntityWithID &&
					(entityData_.size() == tmpEntityData_.size()) &&
					(memcmp( entityData_.data(), tmpEntityData_.data(), entityData_.size()) == 0) &&
					outRec_.getBaseMB() &&
					(tmpEntityBaseMB_.id == entityBaseMB_.id) &&
					(tmpEntityBaseMB_.type() == entityBaseMB_.type()) &&
					(tmpEntityBaseMB_.component() == entityBaseMB_.component()) &&
					(tmpEntityBaseMB_.addr == entityBaseMB_.addr) );
			break;
		}
		case 20:
		{
			bool getNullBaseMBWithName = isOK;
			MF_ASSERT( getNullBaseMBWithName && outRec_.getBaseMB() == 0 );
			break;
		}
		case 21:
		{
			bool getNullBaseMBWithBadName = isOK;
			MF_ASSERT( !getNullBaseMBWithBadName );
			break;
		}
		case 24:
		{
			bool getEntityDataWithName = isOK;
			MF_ASSERT( getEntityDataWithName &&
					(entityData_.size() == tmpEntityData_.size()) &&
					(memcmp( entityData_.data(), tmpEntityData_.data(), entityData_.size()) == 0) );
			break;
		}
		default:
			MF_ASSERT( false );
			break;
	}

	this->nextStep();
}

void SelfTest::onPutEntityComplete( bool isOK, DatabaseID dbID )
{
	switch (stepNum_)
	{
		case 1:
		{
			bool createNewEntity = isOK;
			MF_ASSERT( createNewEntity );
			newEntityID_ = dbID;
			badEntityID_ = newEntityID_ + 9999;
			break;
		}
		case 8:
		{
			bool overwriteEntityData = isOK;
			MF_ASSERT( overwriteEntityData );
			entityData_.rewind();
			break;
		}
		case 10:
		{
			bool putNonExistEntityData = isOK;
			MF_ASSERT( !putNonExistEntityData );
			entityData_.rewind();
			break;
		}
		case 14:
		{
			bool putBaseMBwithID = isOK;;
			MF_ASSERT( putBaseMBwithID );
			break;
		}
		case 15:
		{
			bool putBaseMBwithBadID = isOK;
			MF_ASSERT( !putBaseMBwithBadID );
			break;
		}
		case 17:
		{
			bool updateBaseMBwithID = isOK;
			MF_ASSERT( updateBaseMBwithID );
			break;
		}
		case 19:
		{
			bool putNullBaseMBwithID = isOK;
			MF_ASSERT( putNullBaseMBwithID );
			break;
		}
		case 23:
		{
			bool putEntityData = isOK;
			MF_ASSERT( putEntityData && (dbID != 0) && (newEntityID_ != dbID) );
			newEntityID_ = dbID;

			entityData_.rewind();
			break;
		}
		default:
			MF_ASSERT( false );
			break;
	}

	this->nextStep();
}

void SelfTest::onDelEntityComplete( bool isOK )
{
	switch (stepNum_)
	{
		case 22:
		{
			bool delEntityByName = isOK;
			MF_ASSERT( delEntityByName );
			break;
		}
		case 25:
		{
			bool delEntityByID = isOK;
			MF_ASSERT( delEntityByID );
			break;
		}
		case 26:
		{
			bool delEntityByBadName = isOK;
			MF_ASSERT( !delEntityByBadName );
			break;
		}
		case 27:
		{
			bool delNonExistEntityByID = isOK;
			MF_ASSERT( !delNonExistEntityByID );
			break;
		}
		default:
			MF_ASSERT( false );
			break;
	}
	this->nextStep();
}

void Database::runSelfTest()
{
	SelfTest* selfTest = new SelfTest(*pDatabase_);
	selfTest->nextStep();
}
#endif	// DBMGR_SELFTEST

// -----------------------------------------------------------------------------
// Section: Served interfaces
// -----------------------------------------------------------------------------

#define DEFINE_SERVER_HERE
#include "db_interface.hpp"

// database.cpp
