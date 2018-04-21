/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "loginapp.hpp"

#include <sys/types.h>

#ifndef _WIN32
#include <sys/signal.h>
#include <pwd.h>
#else //ifndef _WIN32
#include <signal.h>
#endif // ndef _WIN32

#include "login_int_interface.hpp"
#include "status_check_watcher.hpp"

#include "dbmgr/db_interface.hpp"

#include "resmgr/bwresource.hpp"

#include "server/bwconfig.hpp"
#include "server/util.hpp"
#include "server/writedb.hpp"

#include "network/msgtypes.hpp"	// for angleToInt8
#include "network/watcher_glue.hpp"
#include "network/encryption_filter.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

/// Timer period when updating login statistics
const uint32 LoginApp::UPDATE_STATS_PERIOD = 1000000;

static uint gNumLogins = 0;
static uint gNumLoginFailures = 0;
static uint gNumLoginAttempts = 0;

static char g_buildID[81] = "Build ID";

uint32 g_latestVersion = uint32(-1);
uint32 g_impendingVersion = uint32(-1);

/// LoginApp Singleton.
BW_SINGLETON_STORAGE( LoginApp )

// -----------------------------------------------------------------------------
// Section: Misc
// -----------------------------------------------------------------------------

extern "C" void interruptHandler( int )
{
	if (LoginApp::pInstance())
	{
		LoginApp::pInstance()->intNub().breakProcessing();
	}
}

extern "C" void controlledShutDownHandler( int )
{
	if (LoginApp::pInstance())
	{
		LoginApp::pInstance()->controlledShutDown();
	}
}

bool commandStopServer( std::string & output, std::string & value )
{
	if (LoginApp::pInstance())
	{
		LoginApp::pInstance()->controlledShutDown();
	}

	return true;
}


// -----------------------------------------------------------------------------
// Section: DatabaseReplyHandler
// -----------------------------------------------------------------------------

/**
 *	An instance of this class is used to receive the reply from a call to
 *	the database.
 */
class DatabaseReplyHandler : public Mercury::ReplyMessageHandler
{
public:
	DatabaseReplyHandler(
		const Mercury::Address & clientAddr,
		const Mercury::ReplyID replyID,
		LogOnParamsPtr pParams );

	virtual ~DatabaseReplyHandler() {}

	virtual void handleMessage( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * arg );

	virtual void handleException( const Mercury::NubException & ne,
		void * arg );

private:
	Mercury::Address	clientAddr_;
	Mercury::ReplyID	replyID_;
	LogOnParamsPtr		pParams_;
};


// -----------------------------------------------------------------------------
// Section: LoginApp
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
LoginApp::LoginApp( Mercury::Nub & intNub, uint16 loginPort ) :
#ifdef USE_OPENSSL
	privateKey_( /* hasPrivate: */ true ),
#endif
	intNub_( intNub ),
	extNub_( htons( loginPort ),
			BWConfig::get( "loginApp/externalInterface",
				BWConfig::get( "externalInterface" ) ).c_str() ),
	netMask_(),
	externalIP_( 0 ),
	isControlledShutDown_( false ),
	isProduction_( BWConfig::get( "production", false ) ),
	systemOverloaded_( 0 ),
	allowLogin_( true ),
	allowProbe_( true ),
	logProbes_( false ),
	lastRateLimitCheckTime_( 0 ),
	numAllowedLoginsLeft_( 0 ),
	loginRateLimit_( 0 ),
	rateLimitDuration_( 0 ),
	statsTimerID_( 0 ),
	loginStats_()
{
	extNub_.isVerbose( BWConfig::get( "loginApp/verboseExternalNub", false ) );

	double maxLoginDelay = BWConfig::get( "loginApp/maxLoginDelay", 10.f );
	maxLoginDelay_ = uint64( maxLoginDelay * ::stampsPerSecondD() );

	// these must match those of the client
	extNub_.onceOffResendPeriod( CLIENT_ONCEOFF_RESEND_PERIOD );
	extNub_.onceOffMaxResends( CLIENT_ONCEOFF_MAX_RESENDS );

	// Mark extNub as 'external' so that once-off-reliability is disabled.
	extNub_.isExternal( true );
}


/**
 *	This method initialises this object.
 */
bool LoginApp::init( int argc, char * argv[], uint16 loginPort )
{

	if (isProduction_)
	{
		INFO_MSG( "LoginApp::init: Production mode enabled.\n" );
	}

	// Check if specified port on which the LoginApp should listen
	// is already used.  If it is and if the LoginApp configuration option
	// shouldShutdownIfPortUsed is set, then stop this LoginApp.

	if ((extNub_.socket() == -1) &&
		!BWConfig::get( "loginApp/shouldShutDownIfPortUsed", false ) &&
		(loginPort != 0))
	{
		INFO_MSG( "LoginApp::init: "
					"Couldn't bind ext nub to %d, trying any available port\n",
				  loginPort );
		extNub_.recreateListeningSocket( 0,
			BWConfig::get( "loginApp/externalInterface",
				BWConfig::get( "externalInterface" ) ).c_str() );
	}

	if (extNub_.socket() == -1)
	{
		ERROR_MSG( "Loginapp::init: "
			"Unable to bind to external interface on specified port %d.\n",
			loginPort );
		return false;
	}


#ifdef USE_OPENSSL
	std::string privateKeyPath = BWConfig::get( "loginApp/privateKey",
		"server/loginapp.privkey" );

	if (!privateKeyPath.empty())
	{
		if (!privateKey_.setKeyFromResource( privateKeyPath ))
		{
			return false;
		}
	}
	else
	{
		ERROR_MSG( "LoginApp::init: "
			"You must specify a private key to use with the "
			"<loginApp/privateKey> option in bw.xml\n" );

		return false;
	}
#endif

	if (intNub_.socket() == -1)
	{
		ERROR_MSG( "Failed to create Nub on internal interface.\n" );
		return false;
	}

	if ((extNub_.address().ip == 0) ||
			(intNub_.address().ip == 0))
	{
		ERROR_MSG( "LoginApp::init: Failed to open UDP ports. "
				"Maybe another process already owns it\n" );
		return false;
	}

	BW_INIT_WATCHER_DOC( "loginapp" );

	BWConfig::update( "loginApp/shutDownSystemOnExit", isControlledShutDown_ );
	MF_WATCH( "shutDownSystemOnExit", isControlledShutDown_ );

	std::string netMask = BWConfig::get( "loginApp/localNetMask" );
	netMask_.parse( netMask.c_str() );

	// Should use inet_aton but this does not work under Windows.
	std::string extAddr = BWConfig::get( "loginApp/externalAddress" );
	externalIP_ = inet_addr( extAddr.c_str() );

	if (netMask_.containsAddress( intNub_.address().ip ))
	{
		INFO_MSG( "Local subnet: %s\n", netMask.c_str() );
		INFO_MSG( "External addr: %s\n", extAddr.c_str() );
	}
	else
	{
		WARNING_MSG( "LoginApp::LoginApp: "
					"localNetMask %s does not match local ip %s\n",
				netMask.c_str(), intNub_.address().c_str() );
		INFO_MSG( "Not using localNetMask\n" );

		netMask_.clear();
	}

	MF_WATCH( "numLogins", gNumLogins );
	MF_WATCH( "numLoginFailures", gNumLoginFailures );
	MF_WATCH( "numLoginAttempts", gNumLoginAttempts );

	// ---- What used to be in loginsvr.cpp

	ReviverSubject::instance().init( &intNub_, "loginApp" );

	// make sure the nub came up ok
	if (extNub_.address().ip == 0)
	{
		CRITICAL_MSG( "login::init: Failed to start Nub.\n"
			"\tLog in port is probably in use.\n"
			"\tIs there another login server running on this machine?\n" );
		return false;
	}

	INFO_MSG( "External address = %s\n", extNub_.address().c_str() );
	INFO_MSG( "Internal address = %s\n", intNub_.address().c_str() );

	if (BW_INIT_ANONYMOUS_CHANNEL_CLIENT( dbMgr_, intNub_,
			LoginIntInterface, DBInterface, 0 ))
	{
		INFO_MSG( "LoginApp::init: DB addr: %s\n",
			this->dbMgr().channel().c_str() );
	}
	else
	{
		INFO_MSG( "LoginApp::init: Database not ready yet.\n" );
	}

	LoginInterface::registerWithNub( extNub_ );
	LoginIntInterface::registerWithNub( intNub_ );

	// Decide whether or not we're allowing logins and/or probes
	allowLogin_ = BWConfig::get( "loginApp/allowLogin", allowLogin_ );
	allowProbe_ = BWConfig::get( "loginApp/allowProbe", allowProbe_ );
	logProbes_ = BWConfig::get( "loginApp/logProbes", logProbes_ );
	MF_WATCH( "allowLogin", allowLogin_ );
	MF_WATCH( "allowProbe", allowProbe_ );
	MF_WATCH( "logProbes", logProbes_ );
	MF_WATCH( "systemOverloaded", systemOverloaded_ );

	if ( (allowProbe_) && (isProduction_) )
	{
		ERROR_MSG( "Production Mode: bw.xml/loginApp/allowProbe is enabled. "
			"This is a development-time feature only and should be disabled "
			"during production.\n" );
	}

	// Enable latency / loss on the external nub
	extNub_.setLatency( BWConfig::get( "loginApp/externalLatencyMin",
								BWConfig::get( "externalLatencyMin", 0.f ) ),
						BWConfig::get( "loginApp/externalLatencyMax",
								BWConfig::get( "externalLatencyMax", 0.f ) ) );
	extNub_.setLossRatio( BWConfig::get( "loginApp/externalLossRatio",
								BWConfig::get( "externalLossRatio", 0.f ) ) );
	if (extNub_.hasArtificialLossOrLatency())
	{
		WARNING_MSG( "LoginApp::init: External Nub loss/latency enabled\n" );
	}

	// Set up the rate limiting parameters
	uint32 rateLimitSeconds = BWConfig::get( "loginApp/rateLimitDuration",
		uint32( 0 ) );
	rateLimitDuration_ = rateLimitSeconds * stampsPerSecond();
	BWConfig::update( "loginApp/loginRateLimit", loginRateLimit_ );

	if (rateLimitSeconds)
	{
		INFO_MSG( "LoginApp::init: "
				"Login rate limiting enabled: period = %u, limit = %d\n",
			rateLimitSeconds, loginRateLimit_ );
	}

	numAllowedLoginsLeft_ = loginRateLimit_;
	lastRateLimitCheckTime_ = timestamp();
	MF_WATCH( "rateLimit/duration", *this,
			&LoginApp::rateLimitSeconds, &LoginApp::rateLimitSeconds );
	MF_WATCH( "rateLimit/loginLimit", loginRateLimit_ );

	Mercury::Reason reason =
		LoginIntInterface::registerWithMachined( intNub_, 0 );

	if (reason != Mercury::REASON_SUCCESS)
	{
		ERROR_MSG( "LoginApp::init: Unable to register with nub. "
			"Is machined running?\n");
		return false;
	}

	if (BWConfig::get( "loginApp/registerExternalInterface", true ))
	{
		LoginInterface::registerWithMachined( extNub_, 0 );
	}

	allowUnencryptedLogins_ =
		BWConfig::get( "loginApp/allowUnencryptedLogins", false );

	// Handle Ctrl+C
	signal( SIGINT, interruptHandler );
#ifndef _WIN32
	// TODO: Controlled shutdown cannot be started under Windows.
	signal( SIGUSR1, controlledShutDownHandler );
#endif

	// Make the external nub a slave to the internal one.
	intNub_.registerChildNub( &extNub_ );

	// Start up watchers
	BW_REGISTER_WATCHER( 0, "loginapp", "LoginApp", "loginApp", intNub_ );

	Watcher & root = Watcher::rootWatcher();
	root.addChild( "nub", Mercury::Nub::pWatcher(), &intNub_ );
	root.addChild( "nubExternal", Mercury::Nub::pWatcher(), &extNub_ );

	root.addChild( "command/statusCheck", new StatusCheckWatcher() );
	root.addChild( "command/shutDownServer",
			new NoArgCallableWatcher( commandStopServer,
				CallableWatcher::LOCAL_ONLY,
				"Shuts down the entire server" ) );

	// root.addChild( "dbMgr", Mercury::Channel::pWatcher(),
	//		&this->dbMgr().channel() );

	WatcherPtr pStatsWatcher = new DirectoryWatcher();
	pStatsWatcher->addChild( "rateLimited",
			makeWatcher( loginStats_, &LoginStats::rateLimited ) );
	pStatsWatcher->addChild( "repeatedForAlreadyPending",
			makeWatcher( loginStats_, &LoginStats::pending ) );
	pStatsWatcher->addChild( "failures",
			makeWatcher( loginStats_, &LoginStats::fails ) );
	pStatsWatcher->addChild( "successes",
			makeWatcher( loginStats_, &LoginStats::successes ) );
	pStatsWatcher->addChild( "all",
			makeWatcher( loginStats_, &LoginStats::all ) );

	{
		// watcher doesn't like const-ness
		static uint32 s_updateStatsPeriod = LoginApp::UPDATE_STATS_PERIOD;
		pStatsWatcher->addChild( "updatePeriod",
				makeWatcher( s_updateStatsPeriod ) );
	}

	root.addChild( "averages", pStatsWatcher );

	intNub_.registerTimer( UPDATE_STATS_PERIOD, &loginStats_, NULL );
	return true;
}


/**
 *	This method performs the main loop of this application.
 */
void LoginApp::run()
{
	intNub_.processUntilBreak();

	INFO_MSG( "LoginApp::run: Terminating normally.\n" );

	if (this->isDBReady() && isControlledShutDown_)
	{
		Mercury::Bundle	& dbBundle = this->dbMgr().bundle();
		DBInterface::controlledShutDownArgs args;
		args.stage = SHUTDOWN_REQUEST;
		dbBundle << args;
		this->dbMgr().send();

		intNub_.processUntilChannelsEmpty();
	}
}


/**
 *	This method sends a failure message back to the client.
 */
void LoginApp::sendFailure( const Mercury::Address & addr,
	Mercury::ReplyID replyID, int status, const char * pDescription,
	LogOnParamsPtr pParams )
{
	if (status == LogOnStatus::LOGIN_REJECTED_RATE_LIMITED)
	{
		loginStats_.incRateLimited();
	}
	else
	{
		loginStats_.incFails();
	}

	if (pDescription == NULL)
		pDescription = "";

	INFO_MSG( "LoginApp::sendFailure: "
		"LogOn for %s failed, LogOnStatus %d, description '%s'.\n",
			addr.c_str(), status, pDescription );

	++gNumLoginFailures;

	Mercury::Bundle bundle;

	// Replies to failed login attempts are not reliable as that would be a
	// DOS'ing vulnerability
	bundle.startReply( replyID, Mercury::RELIABLE_NO );
	bundle << (int8)status;
	bundle << pDescription;

	LoginApp & app = LoginApp::instance();
	app.extNub_.send( addr, bundle );

	if (*pDescription == 0)
	{
		WARNING_MSG( "LoginApp::sendFailure: "
			"Sent LogOnStatus %d without a description (bad form)", status );
	}

	// Erase the cache mapping for this attempt if appropriate
	if (pParams)
	{
		app.cachedLoginMap_.erase( addr );
	}
}


/**
 *	This method is the one that actually receives the login requests.
 */
void LoginApp::login( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data )
{

	if (rateLimitDuration_ &&
			(timestamp() > lastRateLimitCheckTime_ + rateLimitDuration_) )
	{
		// reset number of allowed logins per time block if we're rate limiting
		numAllowedLoginsLeft_ = loginRateLimit_;
		lastRateLimitCheckTime_ = timestamp();
	}

	if (!allowLogin_)
	{
		WARNING_MSG( "LoginApp::login: "
			"Dropping login attempt from %s as logins aren't allowed yet\n",
			source.c_str() );

		this->sendFailure( source, header.replyID,
			LogOnStatus::LOGIN_REJECTED_LOGINS_NOT_ALLOWED,
			"Logins currently not permitted" );
		data.finish();
		return;
	}
	if (!source.ip)
	{
		// spoofed address trying to login as web client!
		ERROR_MSG( "LoginApp::login: Spoofed empty address\n" );
		data.retrieve( data.remainingLength() );
		loginStats_.incFails();
		return;
	}

	bool isReattempt = (cachedLoginMap_.count( source ) != 0);
 	INFO_MSG( "LoginApp::login: %s from %s\n",
		isReattempt ? "Re-attempt" : "Attempt", source.c_str() );

	++gNumLoginAttempts;

	uint32 version = 0;
	data >> version;

	if (data.error())
	{
		ERROR_MSG( "LoginApp::login: "
			"Not enough data on stream (%d bytes total)\n",
			header.length );

		this->sendFailure( source, header.replyID,
			LogOnStatus::LOGIN_MALFORMED_REQUEST,
			"Undersized login message");

		return;
	}

	if (version != LOGIN_VERSION)
	{
		ERROR_MSG( "LoginApp::login: "
			"User at %s tried to log on with version %u. Expected %u\n",
			source.c_str(), version, LOGIN_VERSION );

		char msg[BUFSIZ];

		bw_snprintf( msg, sizeof(msg), "Incorrect protocol version. "
				"Client version is %u, server version is %u. "
				"Your %s is out of date.", version, LOGIN_VERSION,
			  	version < LOGIN_VERSION ? "client" : "server" );

		this->sendFailure( source, header.replyID,
			LogOnStatus::LOGIN_BAD_PROTOCOL_VERSION, msg );

		data.finish();

		return;
	}

	bool isRateLimited = (rateLimitDuration_ && numAllowedLoginsLeft_ == 0);
	if (isRateLimited)
	{
		NOTICE_MSG( "LoginApp::login: "
				"Login from %s not allowed due to rate limiting\n",
				source.c_str() );

		this->sendFailure( source, header.replyID,
				LogOnStatus::LOGIN_REJECTED_RATE_LIMITED,
				"Logins temporarily disallowed due to rate limiting" );
		data.finish();
		return;
	}

	if (!this->isDBReady())
	{
		INFO_MSG( "LoginApp::login: "
			"Attempted login when database not yet ready.\n" );

		this->sendFailure( source, header.replyID,
			LogOnStatus::LOGIN_REJECTED_DB_NOT_READY, "DB not ready" );

		return;
	}

	if (systemOverloaded_ != 0)
	{
		if (systemOverloadedTime_ + stampsPerSecond() < timestamp())
		{
			systemOverloaded_ = 0;
		}
		else
		{
			INFO_MSG( "LoginApp::login: "
				"Attempted login when system overloaded or not yet ready.\n" );
			this->sendFailure( source, header.replyID,
				systemOverloaded_, "System overloaded wait state." );
			return;
		}
	}

	// Read off login parameters
	LogOnParamsPtr pParams = new LogOnParams();

	// Save the message so we can have multiple attempts to read it
	int dataLength = data.remainingLength();
	const void * pDataData = data.retrieve( dataLength );


	// First check whether this is a repeat attempt from a recent pending
	// login before attempting to decrypt and log in.
	if (this->handleResentPendingAttempt( source, header.replyID ))
	{
		// ignore this one, it's in progress
		loginStats_.incPending();
		return;
	}


#ifdef USE_OPENSSL
	Mercury::PublicKeyCipher * pPrivateKey = &privateKey_;
#else
	Mercury::PublicKeyCipher * pPrivateKey = NULL;
#endif

	do
	{
		MemoryIStream attempt = MemoryIStream( pDataData, dataLength );

		if (pParams->readFromStream( attempt, pPrivateKey ))
		{
			// We are successful, move on
			break;
		}

		if (pPrivateKey && allowUnencryptedLogins_)
		{
			// If we tried using encryption, have another go without it
			pPrivateKey = NULL;
			continue;
		}

		// Nothing left to try, bail out
		this->sendFailure( source, header.replyID,
			LogOnStatus::LOGIN_MALFORMED_REQUEST,
			"Could not destream login parameters. Possibly caused by "
			"mis-matching LoginApp keypair." );
		return;
		// Does not reach here
	}
	while (false);

	// First check whether this is a repeat attempt from a recent
	// resolved login before attempting to log in.
	if (this->handleResentCachedAttempt( source, pParams, header.replyID ))
	{
		// ignore this one, we've seen it recently
		return;
	}

	if (rateLimitDuration_)
	{
		// We've done the hard work of decrypting the logon parameters now, so
		// we count this as a login with regards to rate-limiting.
		--numAllowedLoginsLeft_;
	}

	// Check that it has encryption key if we disallow unencrypted logins
	if (pParams->encryptionKey().empty() && !allowUnencryptedLogins_)
	{
		this->sendFailure( source, header.replyID,
			LogOnStatus::LogOnStatus::LOGIN_MALFORMED_REQUEST,
			"No encryption key supplied, and server is not allowing "
				"unencrypted logins." );
		return;
	}


	INFO_MSG( "Logging in %s{%s} (%s)\n",
		pParams->username().c_str(),
		pParams->password().c_str(),
		source.c_str() );

	// Remember that this attempt is now in progress and discard further
	// attempts from that address for some time after it completes.
	cachedLoginMap_[ source ].reset();
	cachedLoginMap_[ source ].pParams( pParams );

	DatabaseReplyHandler * pDBHandler =
		new DatabaseReplyHandler( source, header.replyID, pParams );

	Mercury::Bundle	& dbBundle = this->dbMgr().bundle();
	dbBundle.startRequest( DBInterface::logOn, pDBHandler );

	dbBundle << source << false /*off channel*/ << *pParams;

	this->dbMgr().send();
}


// -----------------------------------------------------------------------------
// Section: DatabaseReplyHandler
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
DatabaseReplyHandler::DatabaseReplyHandler(
		const Mercury::Address & clientAddr,
		Mercury::ReplyID replyID,
		LogOnParamsPtr pParams ) :
	clientAddr_( clientAddr ),
	replyID_( replyID ),
	pParams_( pParams )
{
	DEBUG_MSG( "DBReplyHandler created at %f\n",
		float(double(timestamp())/stampsPerSecondD()) );
}


/**
 *	This method is called by the nub when a message comes back from the system.
 *	It deletes itself at the end.
 */
void DatabaseReplyHandler::handleMessage(
	const Mercury::Address & /*source*/,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data,
	void * /*arg*/ )
{
	uint8 status;
	data >> status;

	if (status != LogOnStatus::LOGGED_ON)
	{
		if (data.remainingLength() > 0)
		{
			std::string msg;
			data >> msg;
			LoginApp::instance().sendFailure( clientAddr_, replyID_, status,
				msg.c_str(), pParams_ );
		}
		else
		{
			LoginApp::instance().sendFailure( clientAddr_, replyID_, status,
				"Database returned an unelaborated error. Check DBMgr log.",
				pParams_ );
		}

		LoginApp & app = LoginApp::instance();
		if ((app.systemOverloaded() == 0 &&
			 status == LogOnStatus::LOGIN_REJECTED_BASEAPP_OVERLOAD) ||
				status == LogOnStatus::LOGIN_REJECTED_CELLAPP_OVERLOAD ||
				status == LogOnStatus::LOGIN_REJECTED_DBMGR_OVERLOAD)
		{
			DEBUG_MSG( "DatabaseReplyHandler::handleMessage(%s): "
					"failure due to overload (status=%x)\n",
				clientAddr_.c_str(), status );
			app.systemOverloaded( status );
		}
		delete this;
		return;
	}

	if (data.remainingLength() < int(sizeof( LoginReplyRecord )))
	{
		ERROR_MSG( "DatabaseReplyHandler::handleMessage: "
						"Login failed. Expected %zu bytes got %d\n",
				sizeof( LoginReplyRecord ), data.remainingLength() );

		if (data.remainingLength() == sizeof(LoginReplyRecord) - sizeof(int))
		{
			ERROR_MSG( "DatabaseReplyHandler::handleMessage: "
					"This can occur if a login is attempted to an entity type "
					"that is not a Proxy.\n" );

			LoginApp::instance().sendFailure( clientAddr_, replyID_,
				LogOnStatus::LOGIN_CUSTOM_DEFINED_ERROR,
				"Database returned a non-proxy entity type.",
				pParams_ );
		}
		else
		{
			LoginApp::instance().sendFailure( clientAddr_, replyID_,
				LogOnStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE,
				"Database returned an unknown error.",
				pParams_ );
		}

		delete this;
		return;
	}

	LoginReplyRecord lrr;
	data >> lrr;

	// If the client has an external address, send them the firewall
	// address instead!

	if (!LoginApp::instance().netMask().containsAddress( clientAddr_.ip ))
	{
		INFO_MSG( "DatabaseReplyHandler::handleMessage: "
				"Redirecting external client %s to firewall.\n",
			clientAddr_.c_str() );
		lrr.serverAddr.ip = LoginApp::instance().externalIP();
	}

	LoginApp::instance().sendAndCacheSuccess( clientAddr_,
			replyID_, lrr, pParams_ );

	gNumLogins++;

	delete this;
}


/**
 *	This method is called by the nub when no message comes back from the system,
 *	or some other error occurs. It deletes itself at the end.
 */
void DatabaseReplyHandler::handleException(
	const Mercury::NubException & ne,
	void * /*arg*/ )
{
	LoginApp::instance().sendFailure( clientAddr_, replyID_,
		LogOnStatus::LOGIN_REJECTED_DBMGR_OVERLOAD, "No reply from DBMgr.",
		pParams_ );

	WARNING_MSG( "DatabaseReplyHandler: got an exception (%s)\n",
			Mercury::reasonToString( ne.reason() ) );
	DEBUG_MSG( "DBReplyHandler timed out at %f\n",
			float(double(timestamp())/stampsPerSecondD()) );

	delete this;
}


// -----------------------------------------------------------------------------
// Section: ProbeHandler
// -----------------------------------------------------------------------------

/**
 *	This method handles the probe message.
 */
void LoginApp::probe( const Mercury::Address & source,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & /*data*/ )
{
	if (logProbes_)
	{
		INFO_MSG( "LoginApp::probe: Got probe from %s\n", source.c_str() );
	}

	if (!allowProbe_ || header.length != 0) return;

	Mercury::Bundle bundle;
	bundle.startReply( header.replyID );

	char buf[256];
	gethostname( buf, sizeof(buf) ); buf[sizeof(buf)-1]=0;
	bundle << PROBE_KEY_HOST_NAME << buf;

#ifndef _WIN32
	struct passwd *pwent = getpwuid( getUserId() );
	const char * username = pwent ? (pwent->pw_name ? pwent->pw_name : "") : "";
	bundle << PROBE_KEY_OWNER_NAME << username;

	if (!pwent)
	{
		ERROR_MSG( "LoginApp::probe: "
			"Process uid %d doesn't exist on this system!\n", getUserId() );
	}

#else
	DWORD bsz = sizeof(buf);
	if (!GetUserName( buf, &bsz )) buf[0]=0;
	bundle << PROBE_KEY_OWNER_NAME << buf;
#endif

	bw_snprintf( buf, sizeof(buf), "%d", gNumLogins );
	bundle << PROBE_KEY_USERS_COUNT << buf;

	bundle << PROBE_KEY_UNIVERSE_NAME << BWConfig::get( "universe" );
	bundle << PROBE_KEY_SPACE_NAME << BWConfig::get( "space" );

	bundle << PROBE_KEY_BINARY_ID << g_buildID;

	extNub_.send( source, bundle );
}


/**
 *	This class is used to handle messages from this process Mercury interfaces.
 */
class LoginAppRawMessageHandler : public Mercury::InputMessageHandler
{
	public:
		typedef void (LoginApp::*Handler)(
			const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & stream );

		// Constructors
		LoginAppRawMessageHandler( Handler handler ) :
			handler_( handler )
		{}

	private:
		virtual void handleMessage( const Mercury::Address & srcAddr,
				Mercury::UnpackedMessageHeader & header,
				BinaryIStream & data )
		{
			(LoginApp::instance().*handler_)( srcAddr, header, data );
		}

		Handler handler_;
};

LoginAppRawMessageHandler gLoginHandler( &LoginApp::login );
LoginAppRawMessageHandler gProbeHandler( &LoginApp::probe );
LoginAppRawMessageHandler gShutDownHandler( &LoginApp::controlledShutDown );


/**
 *	This method sends a reply to a client indicating that logging in has been
 *	successful. It also caches this information so that it can be resent if
 *	necessary.
 */
void LoginApp::sendAndCacheSuccess( const Mercury::Address & addr,
		Mercury::ReplyID replyID, const LoginReplyRecord & replyRecord,
		LogOnParamsPtr pParams )
{
	this->sendSuccess( addr, replyID, replyRecord,
		pParams->encryptionKey() );

	cachedLoginMap_[ addr ].replyRecord( replyRecord );

	// Do not let the map get too big. Just check every so often to get rid of
	// old caches.

	if (cachedLoginMap_.size() > 100)
	{
		CachedLoginMap::iterator iter = cachedLoginMap_.begin();

		while (iter != cachedLoginMap_.end())
		{
			CachedLoginMap::iterator prevIter = iter;
			++iter;

			if (prevIter->second.isTooOld())
			{
				cachedLoginMap_.erase( prevIter );
			}
		}
	}
}


/**
 *	This method sends a reply to a client indicating that logging in has been
 *	successful.
 */
void LoginApp::sendSuccess( const Mercury::Address & addr,
	Mercury::ReplyID replyID, const LoginReplyRecord & replyRecord,
	const std::string & encryptionKey )
{
	Mercury::Bundle b;
	b.startReply( replyID );
	b << (int8)LogOnStatus::LOGGED_ON;

#ifdef USE_OPENSSL
	if (!encryptionKey.empty())
	{
		// We have to encrypt the reply record because it contains the session key
		Mercury::EncryptionFilter filter( encryptionKey );
		MemoryOStream clearText;
		clearText << replyRecord;
		filter.encryptStream( clearText, b );
	}
	else
#endif
	{
		b << replyRecord;
	}

	loginStats_.incSuccesses();

	this->extNub().send( addr, b );
}


/**
 *	This method checks whether there is a login in progress from this
 *	address.
 */
bool LoginApp::handleResentPendingAttempt( const Mercury::Address & addr,
		Mercury::ReplyID replyID )
{
	CachedLoginMap::iterator iter = cachedLoginMap_.find( addr );

	if (iter != cachedLoginMap_.end())
	{
		CachedLogin & cache = iter->second;

		if (cache.isPending())
		{
			DEBUG_MSG( "Ignoring repeat attempt from %s "
					"while another attempt is in progress (for %s)\n",
				addr.c_str(),
				cache.pParams()->username().c_str() );

			return true;
		}
	}
	return false;
}


/**
 *	This method checks whether there is a cached login attempt from this
 *	address. If there is, it is assumed that the previous reply was dropped and
 *	this one is resent.
 */
bool LoginApp::handleResentCachedAttempt( const Mercury::Address & addr,
		LogOnParamsPtr pParams, Mercury::ReplyID replyID )
{
	CachedLoginMap::iterator iter = cachedLoginMap_.find( addr );

	if (iter != cachedLoginMap_.end())
	{
		CachedLogin & cache = iter->second;
		if (!cache.isTooOld() && *cache.pParams() == *pParams)
		{
			DEBUG_MSG( "%s retransmitting successful login to %s\n",
					   addr.c_str(),
					   cache.pParams()->username().c_str() );
			this->sendSuccess( addr, replyID, cache.replyRecord(),
				cache.pParams()->encryptionKey() );

			return true;
		}
	}

	return false;
}


/**
 *  Handles incoming shutdown requests.  This is basically another way of
 *  triggering a controlled system shutdown instead of sending a SIGUSR1.
 */
void LoginApp::controlledShutDown( const Mercury::Address &source,
	Mercury::UnpackedMessageHeader &header,
	BinaryIStream &data )
{
	INFO_MSG( "LoginApp::controlledShutDown: "
		"Got shutdown command from %s\n", source.c_str() );

	this->controlledShutDown();
}


/**
 *	This method handles a message telling us the overload status of the system.
 *	It is received on the internal nub.
 */
/*
void LoginApp::systemOverloadStatus(
	const LoginIntInterface::systemOverloadStatusArgs & args )
{
	if (systemOverloaded_ && !args.overloaded)
	{
		DEBUG_MSG( "LoginApp::systemOverloadStatus: "
			"System no longer overloaded.\n" );
	}
	else if (!systemOverloaded_ && args.overloaded)
	{
		DEBUG_MSG( "LoginApp::systemOverloadStatus: "
			"System has become overloaded.\n" );
	}

	systemOverloaded_ = args.overloaded;
}
*/


// -----------------------------------------------------------------------------
// Section: CachedLogin
// -----------------------------------------------------------------------------

/**
 *  This method returns true if this login is pending, i.e. we are waiting on
 *  the DBMgr to tell us whether or not the login is successful.
 */
bool LoginApp::CachedLogin::isPending() const
{
	return creationTime_ == 0;
}


/**
 *	This method returns whether or not this cache is too old to use.
 */
bool LoginApp::CachedLogin::isTooOld() const
{
	const uint64 MAX_LOGIN_DELAY = LoginApp::instance().maxLoginDelay();

	return !this->isPending() &&
		(::timestamp() - creationTime_ > MAX_LOGIN_DELAY);
}


/**
 *  This method sets the reply record into this cached object, and is called
 *  when the DBMgr replies.
 */
void LoginApp::CachedLogin::replyRecord( const LoginReplyRecord & record )
{
	replyRecord_ = record;
	creationTime_ = ::timestamp();
}


// -----------------------------------------------------------------------------
// Section: InputMessageHandlers
// -----------------------------------------------------------------------------

/**
 *	Class for struct-style Mercury message handler objects.
 */
template <class Args> class LoginAppStructMessageHandler :
	public Mercury::InputMessageHandler
{
public:
	typedef void (LoginApp::*Handler)( const Args & args );

	LoginAppStructMessageHandler( Handler handler ) :
		handler_( handler )
	{}

private:
	virtual void handleMessage( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data )
	{
		Args * pArgs = (Args*)data.retrieve( sizeof(Args) );
		(LoginApp::instance().*handler_)( *pArgs );
	}

	Handler handler_;
};


// -----------------------------------------------------------------------------
// Section: LoginStats
// -----------------------------------------------------------------------------


// Make the EMA bias equivalent to having the most recent 5 samples represent
// 86% of the total weight. This is purely arbitrary, and may be adjusted to
// increase or decrease the sensitivity of the login statistics as reported in
// the 'averages' watcher directory.
static const uint WEIGHTING_NUM_SAMPLES = 5;

/**
 * The EMA bias for the login statistics.
 */
const float LoginApp::LoginStats::BIAS = 2.f / (WEIGHTING_NUM_SAMPLES + 1);

LoginApp::LoginStats::LoginStats():
			fails_( BIAS ),
			rateLimited_( BIAS ),
			pending_( BIAS ),
			successes_( BIAS ),
			all_( BIAS )
{}

#define DEFINE_SERVER_HERE
#include "login_int_interface.hpp"
// loginapp.cpp
