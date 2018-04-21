/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifdef _WIN32
#pragma warning (disable : 4355) // 'this' used in base member initialiser list
#endif

#include "Python.h"		// See http://docs.python.org/api/includes.html

#include "baseappmgr.hpp"

#include "baseapp.hpp"
#include "baseappmgr_interface.hpp"

#include "watcher_forwarding_baseapp.hpp"

#include "cellapp/cellapp_interface.hpp"
#include "cellappmgr/cellappmgr_interface.hpp"
#include "cstdmf/memory_stream.hpp"
#include "cstdmf/timestamp.hpp"
#include "baseapp/baseapp_int_interface.hpp"
#include "loginapp/login_int_interface.hpp"
#include "dbmgr/db_interface.hpp"
#include "network/watcher_glue.hpp"
#include "network/portmap.hpp"

#include "server/bwconfig.hpp"
#include "server/reviver_subject.hpp"
#include "server/stream_helper.hpp"
#include "server/time_keeper.hpp"

#include <signal.h>
#include <limits>

DECLARE_DEBUG_COMPONENT( 0 )

/// BaseAppMgr Singleton.
BW_SINGLETON_STORAGE( BaseAppMgr )


void intSignalHandler( int /*sigNum*/ )
{
	BaseAppMgr * pMgr = BaseAppMgr::pInstance();

	if (pMgr != NULL)
	{
		pMgr->shutDown( false );
	}
}

/**
 *	This function asks the machined process at the destination IP address to
 * 	send a signal to the BigWorld process at the specified port.
 */
bool sendSignalViaMachined( const Mercury::Address& dest, int sigNum,
	Mercury::Nub& nub )
{
	SignalMessage sm;
	sm.signal_ = sigNum;
	sm.port_ = dest.port;
	sm.param_ = sm.PARAM_USE_PORT;

	Endpoint tempEP;
	tempEP.socket( SOCK_DGRAM );

	if (tempEP.good() && tempEP.bind() == 0)
	{
		sm.sendto( tempEP, htons( PORT_MACHINED ), dest.ip );
		return true;
	}

	return false;
}



// -----------------------------------------------------------------------------
// Section: Construction/Destruction
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
BaseAppMgr::BaseAppMgr( Mercury::Nub & nub ) :
	nub_( nub ),
	cellAppMgr_( nub_ ),
	lastBaseAppID_( 0 ),
	allowNewBaseApps_( true ),
	time_( 0 ),
	pTimeKeeper_( NULL ),
	updateHertz_( DEFAULT_GAME_UPDATE_HERTZ ),
	baseAppOverloadLevel_( 1.f ),
	createBaseRatio_( 4.f ),
	updateCreateBaseInfoPeriod_( 1 ),
	bestBaseAppAddr_( 0, 0 ),
	isRecovery_( false ),
	hasInitData_( false ),
	hasStarted_( false ),
	shouldShutDownOthers_( false ),
	shouldHardKillDeadBaseApps_( true ),
	onlyUseBackupOnSameMachine_( false ),
	useNewStyleBackup_( true ),
	shutDownServerOnBadState_(
		BWConfig::get( "shutDownServerOnBadState", true ) ),
	shutDownServerOnBaseAppDeath_(
		BWConfig::get( "shutDownServerOnBaseAppDeath", false ) ),
	isProduction_( BWConfig::get( "production", false ) ),
	deadBaseAppAddr_( Mercury::Address::NONE ),
	archiveCompleteMsgCounter_( 0 ),
	shutDownTime_( 0 ),
	shutDownStage_( SHUTDOWN_NONE ),
	baseAppTimeoutPeriod_( 0 ),
	baseAppOverloadStartTime_( 0 ),
	loginsSinceOverload_( 0 ),
	allowOverloadLogins_( 10 ),
	hasMultipleBaseAppMachines_( false )
{
	BWConfig::update( "gameUpdateHertz", updateHertz_ );

	cellAppMgr_.channel().isIrregular( true );

	float timeSyncPeriodInSeconds =
			BWConfig::get( "baseAppMgr/timeSyncPeriod", 60.f );
	syncTimePeriod_ =
		int( floorf( timeSyncPeriodInSeconds * updateHertz_ + 0.5f ) );

	BWConfig::update( "baseAppMgr/baseAppOverloadLevel", baseAppOverloadLevel_);

	BWConfig::update( "baseAppMgr/createBaseRatio", createBaseRatio_ );
	float updateCreateBaseInfoInSeconds =
		BWConfig::get( "baseAppMgr/updateCreateBaseInfoPeriod", 5.f );
	updateCreateBaseInfoPeriod_ =
		int( floorf( updateCreateBaseInfoInSeconds * updateHertz_ + 0.5f ) );

	BWConfig::update( "baseAppMgr/hardKillDeadBaseApps",
			shouldHardKillDeadBaseApps_ );
	BWConfig::update( "baseAppMgr/onlyUseBackupOnSameMachine",
			onlyUseBackupOnSameMachine_ );
	BWConfig::update( "baseAppMgr/useNewStyleBackup", useNewStyleBackup_ );

	// We don't support old-style BaseApp backup anymore
	if (!useNewStyleBackup_)
	{
		ERROR_MSG( "Old-style BaseApp backups are no longer supported. "
			"Using new-style backup instead.\n" );

		useNewStyleBackup_ = true;
	}

	float baseAppTimeout = 5.f;
	BWConfig::update( "baseAppMgr/baseAppTimeout", baseAppTimeout );
	baseAppTimeoutPeriod_ = int64( stampsPerSecondD() * baseAppTimeout );

	allowOverloadPeriod_ = uint64( stampsPerSecondD() *
			BWConfig::get( "baseAppMgr/overloadTolerancePeriod", 5.f ) );
	BWConfig::update( "baseAppMgr/overloadLogins", allowOverloadLogins_ );

	INFO_MSG( "\n---- Base App Manager ----\n" );
	INFO_MSG( "Address          = %s\n", nub_.address().c_str() );
	INFO_MSG( "Time Sync Period = %d\n", syncTimePeriod_ );
}


/**
 *	Destructor.
 */
BaseAppMgr::~BaseAppMgr()
{
	if (shouldShutDownOthers_)
	{
		BaseAppIntInterface::shutDownArgs	baseAppShutDownArgs = { false };

		{
			BaseApps::iterator iter = baseApps_.begin();
			while (iter != baseApps_.end())
			{
				iter->second->bundle() << baseAppShutDownArgs;
				iter->second->send();

				++iter;
			}
		}

		{
			BackupBaseApps::iterator iter = backupBaseApps_.begin();
			while (iter != backupBaseApps_.end())
			{
				iter->second->bundle() << baseAppShutDownArgs;
				iter->second->send();

				++iter;
			}
		}


		if (cellAppMgr_.channel().isEstablished())
		{
			Mercury::Bundle	& bundle = cellAppMgr_.bundle();
			CellAppMgrInterface::shutDownArgs cellAppmgrShutDownArgs = { false };
			bundle << cellAppmgrShutDownArgs;
			cellAppMgr_.send();
		}
	}

	// Make sure channels shut down cleanly
	nub_.processUntilChannelsEmpty();

	if (pTimeKeeper_)
	{
		delete pTimeKeeper_;
		pTimeKeeper_ = NULL;
	}
}


/**
 *	This method initialises this object.
 *
 *	@return True on success, false otherwise.
 */
bool BaseAppMgr::init(int argc, char * argv[])
{
	if (nub_.socket() == -1)
	{
		ERROR_MSG( "Failed to create Nub on internal interface.\n" );
		return false;
	}

	ReviverSubject::instance().init( &nub_, "baseAppMgr" );

	for (int i = 0; i < argc; ++i)
	{
		if (strcmp( argv[i], "-recover" ) == 0)
		{
			isRecovery_ = true;
			break;
		}
	}

	INFO_MSG( "isRecovery = %s\n", isRecovery_ ? "True" : "False" );

	if (isProduction_)
	{
		INFO_MSG( "BaseAppMgr::init: Production mode enabled\n" );
	}

	// register dead app callback with machined
	this->nub().registerDeathListener( BaseAppMgrInterface::handleBaseAppDeath,
		"BaseAppIntInterface" );

	if (!BW_INIT_ANONYMOUS_CHANNEL_CLIENT( dbMgr_, nub_,
				BaseAppMgrInterface, DBInterface, 0 ))
	{
		INFO_MSG( "BaseAppMgr::init: Database not ready yet.\n" );
	}

	BaseAppMgrInterface::registerWithNub( nub_ );

	Mercury::Reason reason =
		BaseAppMgrInterface::registerWithMachined( nub_, 0 );

	if (reason != Mercury::REASON_SUCCESS)
	{
		ERROR_MSG("BaseAppMgr::init: Unable to register with nub. "
				"Is machined running?\n");
		return false;
	}

	{
		nub_.registerBirthListener( BaseAppMgrInterface::handleCellAppMgrBirth,
			"CellAppMgrInterface" );

		Mercury::Address cellAppMgrAddr;
		reason = nub_.findInterface( "CellAppMgrInterface", 0, cellAppMgrAddr );

		if (reason == Mercury::REASON_SUCCESS)
		{
			cellAppMgr_.addr( cellAppMgrAddr );
		}
		else if (reason == Mercury::REASON_TIMER_EXPIRED)
		{
			INFO_MSG( "BaseAppMgr::init: CellAppMgr not ready yet.\n" );
		}
		else
		{
			ERROR_MSG( "BaseAppMgr::init: "
				"Failed to find CellAppMgr interface: %s\n",
				Mercury::reasonToString( (Mercury::Reason &)reason ) );

			return false;
		}

		nub_.registerBirthListener(
			BaseAppMgrInterface::handleBaseAppMgrBirth,
			"BaseAppMgrInterface" );
	}

	signal( SIGINT, ::intSignalHandler );
#ifndef _WIN32
	signal( SIGHUP, ::intSignalHandler );
#endif //ndef _WIN32

	BW_INIT_WATCHER_DOC( "baseappmgr" );

	BW_REGISTER_WATCHER( 0, "baseappmgr",
		"Base App Manager", "baseAppMgr", nub_ );

	this->addWatchers();

	return true;
}


// -----------------------------------------------------------------------------
// Section: Helpers
// -----------------------------------------------------------------------------


/**
 *  This method returns the BaseApp for the given address, or NULL if none
 *  exists.
 */
BaseApp * BaseAppMgr::findBaseApp( const Mercury::Address & addr )
{
	BaseApps::iterator it = baseApps_.find( addr );
	if (it != baseApps_.end())
		return it->second.get();
	else
		return NULL;
}


/**
 *	This method finds the least loaded BaseApp.
 *
 *	@return The least loaded BaseApp. If none exists, NULL is returned.
 */
BaseApp * BaseAppMgr::findBestBaseApp() const
{
	const BaseApp * pBest = NULL;

	float lowestLoad = 99999.f;
	BaseAppMgr::BaseApps::const_iterator iter = baseApps_.begin();

	while (iter != baseApps_.end())
	{
		float currLoad = iter->second->load();

		if (currLoad < lowestLoad)
		{
			lowestLoad = currLoad;
			pBest = iter->second.get();
		}

		iter++;
	}

	return const_cast< BaseApp * >( pBest );
}


/**
 *	This method finds the backup BaseApp with the least load.
 *
 *	@return The least loaded backup BaseApp. If none exists, NULL is returned.
 */
BackupBaseApp * BaseAppMgr::findBestBackup( const BaseApp & baseApp ) const
{
	if (backupBaseApps_.empty())
	{
		return NULL;
	}

	if (onlyUseBackupOnSameMachine_)
	{
		const BackupBaseApp * pBest = NULL;
		float bestLoad = std::numeric_limits< float >::max();

		Mercury::Address baseAppIPAddr( baseApp.addr().ip, 0 );
		BackupBaseApps::const_iterator pCandidate =
			backupBaseApps_.lower_bound( baseAppIPAddr );
		while ( (pCandidate != backupBaseApps_.end()) &&
				(pCandidate->first.ip == baseApp.addr().ip) )
		{
			float candidateLoad = pCandidate->second->load();
			if (candidateLoad < bestLoad)
			{
				pBest = pCandidate->second.get();
				bestLoad = candidateLoad;
			}
			++pCandidate;
		}
		return const_cast<BackupBaseApp *>( pBest );
	}

	BackupBaseApps::const_iterator iter = backupBaseApps_.begin();
	const BackupBaseApp * pBest = iter->second.get();
	++iter;

	while (iter != backupBaseApps_.end())
	{
		const BackupBaseApp * pCurr = iter->second.get();

		// Ideally, a backup on a different machine will be found.
		bool isBestOn = (pBest->addr().ip == baseApp.addr().ip);
		bool isCurrOn = (pCurr->addr().ip == baseApp.addr().ip);

		if (isCurrOn == isBestOn)
		{
			// If the machine is no better, choose the one with the lowest load.
			float currLoad = pCurr->load();

			if (currLoad < pBest->load())
			{
				pBest = pCurr;
			}
		}
		else if (!isCurrOn)
		{
			// If this is the first one on a different machine, it always wins.
			pBest = pCurr;
		}

		++iter;
	}

	return const_cast<BackupBaseApp *>( pBest );
}


/**
 *	This method returns the approximate number of bases on the server.
 */
int BaseAppMgr::numBases() const
{
	int count = 0;

	BaseAppMgr::BaseApps::const_iterator iter = baseApps_.begin();

	while (iter != baseApps_.end())
	{
		count += iter->second->numBases();

		iter++;
	}

	return count;
}


/**
 *	This method returns the approximate number of proxies on the server.
 */
int BaseAppMgr::numProxies() const
{
	int count = 0;

	BaseAppMgr::BaseApps::const_iterator iter = baseApps_.begin();

	while (iter != baseApps_.end())
	{
		count += iter->second->numProxies();

		iter++;
	}

	return count;
}


/**
 *	This method returns the minimum Base App load.
 */
float BaseAppMgr::minBaseAppLoad() const
{
	float load = 2.f;

	BaseAppMgr::BaseApps::const_iterator iter = baseApps_.begin();

	while (iter != baseApps_.end())
	{
		load = std::min( load, iter->second->load() );

		++iter;
	}

	return load;
}


/**
 *	This method returns the average Base App load.
 */
float BaseAppMgr::avgBaseAppLoad() const
{
	float load = 0.f;

	BaseAppMgr::BaseApps::const_iterator iter = baseApps_.begin();

	while (iter != baseApps_.end())
	{
		load += iter->second->load();

		++iter;
	}

	return baseApps_.empty() ? 0.f : load/baseApps_.size();
}


/**
 *	This method returns the maximum Base App load.
 */
float BaseAppMgr::maxBaseAppLoad() const
{
	float load = 0.f;

	BaseAppMgr::BaseApps::const_iterator iter = baseApps_.begin();

	while (iter != baseApps_.end())
	{
		load = std::max( load, iter->second->load() );

		++iter;
	}

	return load;
}


/**
 *	This method returns an ID for a new BaseApp.
 */
BaseAppID BaseAppMgr::getNextID()
{
	// Figure out an ID for it
	bool foundNext = false;

	while (!foundNext)
	{
		lastBaseAppID_ = (lastBaseAppID_+1) & 0x0FFFFFFF; 	// arbitrary limit

		foundNext = true;

		// TODO: Should add back support for making sure that we do not have
		// duplicate IDs. This is not too critical as this is now only really
		// used by the human user to make things easier.

		//foundNext = (baseApps_.find( lastBaseAppID_ ) == baseApps_.end()) &&
		//	(backupBaseApps_.find( lastBaseAppID_ ) == backupBaseApps_.end());
	}

	return lastBaseAppID_;
}


/**
 *  This method sends a Mercury message to all known baseapps.  The message
 *  payload is taken from the provided MemoryOStream.  If pExclude is non-NULL,
 *  nothing will be sent to that app.  If pReplyHandler is non-NULL, we start a
 *  request instead of starting a regular message.
 */
void BaseAppMgr::sendToBaseApps( const Mercury::InterfaceElement & ifElt,
	MemoryOStream & args, const BaseApp * pExclude,
	Mercury::ReplyMessageHandler * pHandler )
{
	for (BaseApps::iterator it = baseApps_.begin(); it != baseApps_.end(); ++it)
	{
		BaseApp & baseApp = *it->second;

		// Skip if we're supposed to exclude this app
		if (pExclude == &baseApp)
			continue;

		// Stream message onto bundle and send
		Mercury::Bundle & bundle = baseApp.bundle();

		if (!pHandler)
			bundle.startMessage( ifElt );
		else
			bundle.startRequest( ifElt, pHandler );

		// Note: This does not stream off from "args". This is so that we can
		// read the same data multiple times.
		bundle.addBlob( args.data(), args.size() );

		baseApp.send();
	}

	args.finish();
}


/**
 *  This method sends a Mercury message to all known baseapps.  The message
 *  payload is taken from the provided MemoryOStream.  If pExclude is non-NULL,
 *  nothing will be sent to that app.  If pReplyHandler is non-NULL, we start a
 *  request instead of starting a regular message.
 */
void BaseAppMgr::sendToBackupBaseApps( const Mercury::InterfaceElement & ifElt,
	MemoryOStream & args, const BackupBaseApp * pExclude,
	Mercury::ReplyMessageHandler * pHandler )
{
	for (BackupBaseApps::iterator it = backupBaseApps_.begin();
		 it != backupBaseApps_.end(); ++it)
	{
		BackupBaseApp & backup = *it->second;

		// Skip if we're supposed to exclude this app
		if (pExclude == &backup)
			continue;

		// Stream message onto bundle and send
		Mercury::Bundle & bundle = backup.bundle();

		if (!pHandler)
			bundle.startMessage( ifElt );
		else
			bundle.startRequest( ifElt, pHandler );

		bundle.addBlob( args.data(), args.size() );

		backup.send();
	}

	args.finish();
}


/**
 *	This method adds the watchers that are related to this object.
 */
void BaseAppMgr::addWatchers()
{
	Watcher * pRoot = &Watcher::rootWatcher();

	// number of local proxies
	MF_WATCH( "numBaseApps", *this, &BaseAppMgr::numBaseApps );
	MF_WATCH( "numBackupBaseApps", *this, &BaseAppMgr::numBackupBaseApps );

	MF_WATCH( "numBases", *this, &BaseAppMgr::numBases );
	MF_WATCH( "numProxies", *this, &BaseAppMgr::numProxies );

	MF_WATCH( "config/shouldShutDownOthers", shouldShutDownOthers_ );

	MF_WATCH( "config/createBaseRatio", createBaseRatio_ );
	MF_WATCH( "config/updateCreateBaseInfoPeriod",
			updateCreateBaseInfoPeriod_ );

	MF_WATCH( "baseAppLoad/min", *this, &BaseAppMgr::minBaseAppLoad );
	MF_WATCH( "baseAppLoad/average", *this, &BaseAppMgr::avgBaseAppLoad );
	MF_WATCH( "baseAppLoad/max", *this, &BaseAppMgr::maxBaseAppLoad );

	MF_WATCH( "config/baseAppOverloadLevel", baseAppOverloadLevel_ );

	Watcher * pBaseAppWatcher = BaseApp::makeWatcher();

	// map of these for locals
	pRoot->addChild( "baseApps", new MapWatcher<BaseApps>( baseApps_ ) );
	pRoot->addChild( "baseApps/*", pBaseAppWatcher );

	// map of these for locals
	pRoot->addChild( "backups", new MapWatcher<BaseApps>( baseApps_ ) );
	pRoot->addChild( "backups/*", pBaseAppWatcher );

	// other misc stuff
	MF_WATCH( "lastBaseAppIDAllocated", lastBaseAppID_ );

	pRoot->addChild( "nub", Mercury::Nub::pWatcher(), &nub_ );

	pRoot->addChild( "cellAppMgr", Mercury::Channel::pWatcher(),
		&cellAppMgr_.channel() );

	pRoot->addChild( "forwardTo", new BAForwardingWatcher() );

#if 0
	pRoot->addChild( "dbMgr", Mercury::Channel::pWatcher(),
		&this->dbMgr().channel() );
#endif

	MF_WATCH( "command/runScriptSingle", *this,
			MF_WRITE_ACCESSOR( std::string, BaseAppMgr, runScriptSingle ) );
	MF_WATCH( "command/runScriptAll", *this,
			MF_WRITE_ACCESSOR( std::string, BaseAppMgr, runScriptAll ) );
}

/**
 *	This method overrides the Mercury::TimerExpiryHandler method to handle
 *	timer events.
 */
int BaseAppMgr::handleTimeout( Mercury::TimerID /*id*/, void * arg )
{
	// Are we paused for shutdown?
	if ((shutDownTime_ != 0) && (shutDownTime_ == time_))
		return 0;

	switch (reinterpret_cast<uintptr>( arg ))
	{
		case TIMEOUT_GAME_TICK:
		{
			++time_;

			if (time_ % syncTimePeriod_ == 0)
			{
				pTimeKeeper_->synchroniseWithMaster();
			}

			this->checkForDeadBaseApps();

			if (time_ % updateCreateBaseInfoPeriod_ == 0)
			{
				this->updateCreateBaseInfo();
			}

			// TODO: Don't really need to do this each tick.
			{
				BaseApp * pBest = this->findBestBaseApp();

				if ((pBest != NULL) &&
					(bestBaseAppAddr_ != pBest->addr()) &&
					cellAppMgr_.channel().isEstablished())
				{
					bestBaseAppAddr_ = pBest->addr();
					Mercury::Bundle & bundle = cellAppMgr_.bundle();
					bundle.startMessage( CellAppMgrInterface::setBaseApp );
					bundle << bestBaseAppAddr_;
					cellAppMgr_.send();
				}
			}
		}
		break;
	}

	return 0;
}


/**
 *	This method is called periodically to check whether or not any base
 *	applications have timed out.
 */
void BaseAppMgr::checkForDeadBaseApps()
{
	uint64 currTime = ::timestamp();
	uint64 lastHeardTime = 0;
	BaseApps::iterator iter = baseApps_.begin();

	while (iter != baseApps_.end())
	{
		lastHeardTime = std::max( lastHeardTime,
				iter->second->channel().lastReceivedTime() );
		++iter;
	}

	const uint64 timeSinceAnyHeard = currTime - lastHeardTime;

	iter = baseApps_.begin();

	while (iter != baseApps_.end())
	{
		BaseApp * pApp = iter->second.get();

		if (pApp->hasTimedOut( currTime,
								baseAppTimeoutPeriod_,
								timeSinceAnyHeard ))
		{
			INFO_MSG( "BaseAppMgr::checkForDeadBaseApps: %s has timed out.\n",
					pApp->addr().c_str() );
			this->handleBaseAppDeath( pApp->addr() );
			// Only handle one timeout per check because the above call will
			// likely change the collection we are iterating over.
			return;
		}

		iter++;
	}
}


/**
 *	This method handles a message from a Base App that informs us on its current
 *	load.
 */
void BaseAppMgr::informOfLoad( const BaseAppMgrInterface::informOfLoadArgs & args,
	   const Mercury::Address & addr )
{
	BaseApps::iterator iter = baseApps_.find( addr );

	if (iter != baseApps_.end())
	{
		iter->second->updateLoad( args.load, args.numBases, args.numProxies );
	}
	else
	{
		BackupBaseApps::iterator backupIter = backupBaseApps_.find( addr );

		if (backupIter != backupBaseApps_.end())
		{
			MF_ASSERT( (args.numBases == 0) && (args.numProxies == 0) );
			backupIter->second->updateLoad( args.load );
		}
		else
		{
			ERROR_MSG( "BaseAppMgr::informOfLoad: No BaseApp with address %s\n",
					(char *) addr );
		}
	}
}


// ----------------------------------------------------------------------------
// Section: Reply message handlers.
// ----------------------------------------------------------------------------

/**
 *	This class is used to handle reply messages from BaseApps when a
 *	createBase message has been sent. This sends the base creation reply back
 *	to the DBMgr.
 */
class CreateBaseReplyHandler : public Mercury::ReplyMessageHandler
{
public:
	CreateBaseReplyHandler( const Mercury::Address & srcAddr,
		 	Mercury::ReplyID replyID,
			const Mercury::Address & externalAddr ) :
		srcAddr_( srcAddr ),
		replyID_( replyID ),
		externalAddr_( externalAddr )
	{
	}

private:
	void handleMessage(const Mercury::Address& /*srcAddr*/,
		Mercury::UnpackedMessageHeader& /*header*/,
		BinaryIStream& data, void * /*arg*/)
	{
		EntityMailBoxRef ref;
		data >> ref;

		Mercury::ChannelSender sender( BaseAppMgr::getChannel( srcAddr_ ) );
		Mercury::Bundle & bundle = sender.bundle();

		bundle.startReply( replyID_ );

		if (ref.addr.ip != 0)
		{
			// Note: If this changes, check that BaseApp::logOnAttempt is ok.
			bundle << externalAddr_;
			// Should be EntityMailBoxRef and sessionKey
			bundle << ref;
			bundle.transfer( data, data.remainingLength() );
		}
		else
		{
			bundle << Mercury::Address( 0, 0 );
			bundle << "Unable to create base";
		}

		delete this;
	}

	void handleException(const Mercury::NubException& ne, void* /*arg*/)
	{
		Mercury::ChannelSender sender( BaseAppMgr::getChannel( srcAddr_ ) );
		Mercury::Bundle & bundle = sender.bundle();

		Mercury::Address addr;

		addr.ip = 0;
		addr.port = 0;

		bundle.startReply( replyID_ );
		bundle << addr;
		bundle << Mercury::reasonToString( ne.reason() );

		delete this;
	}

	Mercury::Address 	srcAddr_;
	Mercury::ReplyID	replyID_;
	Mercury::Address 	externalAddr_;
};


/**
 *	This class is used to handle reply messages and forward it on.
 */
class ForwardingReplyHandler : public Mercury::ReplyMessageHandler
{
public:
	ForwardingReplyHandler( const Mercury::Address & srcAddr,
			Mercury::ReplyID replyID ) :
		srcAddr_( srcAddr ),
		replyID_( replyID )
	{}

private:
	void handleMessage(const Mercury::Address& /*srcAddr*/,
		Mercury::UnpackedMessageHeader& header,
		BinaryIStream& data, void * /*arg*/)
	{
		Mercury::ChannelSender sender( BaseAppMgr::getChannel( srcAddr_ ) );
		Mercury::Bundle & bundle = sender.bundle();

		bundle.startReply( replyID_ );

		// For classes that derive.
		this->prependData( bundle, data );

		bundle.transfer( data, data.remainingLength() );

		delete this;
	}

	void handleException(const Mercury::NubException& ne, void* /*arg*/)
	{
		ERROR_MSG( "ForwardingReplyHandler::handleException: reason %d\n",
				ne.reason() );
		delete this;
	}

	virtual void prependData( Mercury::Bundle & bundle,
								BinaryIStream & data ) {};

	Mercury::Address 	srcAddr_;
	Mercury::ReplyID	replyID_;
};


/**
 *	This class is used to handle createEntity messages (from the DBMgr).
 */
class CreateEntityIncomingHandler : public Mercury::InputMessageHandler
{
public:
	CreateEntityIncomingHandler( void * /*arg*/ ) {}

private:
	virtual void handleMessage( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
	{
		BaseAppMgr & baseAppMgr = BaseAppMgr::instance();

		Mercury::Address baseAppAddr( 0, 0 );

		BaseApp * pBest = baseAppMgr.findBestBaseApp();

		if (pBest == NULL)
		{
			ERROR_MSG( "BaseAppMgr::createEntity: Could not find a BaseApp.\n");
			baseAppAddr.port =
					BaseAppMgrInterface::CREATE_ENTITY_ERROR_NO_BASEAPPS;

			Mercury::ChannelSender sender( BaseAppMgr::getChannel( srcAddr ) );
			Mercury::Bundle & bundle = sender.bundle();

			bundle.startReply( header.replyID );
			bundle << baseAppAddr;
			bundle << "No BaseApp could be found to add to.";

			return;
		}

		bool baseAppsOverloaded = (pBest->load() > baseAppMgr.baseAppOverloadLevel_);
		if (this->calculateOverloaded( baseAppsOverloaded ))
		{
			INFO_MSG( "BaseAppMgr::createEntity: All baseapps overloaded "
					"(best load=%.02f > overload level=%.02f.\n",
				pBest->load(), baseAppMgr.baseAppOverloadLevel_ );
			baseAppAddr.port =
				BaseAppMgrInterface::CREATE_ENTITY_ERROR_BASEAPPS_OVERLOADED;

			Mercury::ChannelSender sender( BaseAppMgr::getChannel( srcAddr ) );
			Mercury::Bundle & bundle = sender.bundle();

			bundle.startReply( header.replyID );
			bundle << baseAppAddr;
			bundle << "All BaseApps overloaded.";

			return;
		}

		// Copy the client endpoint address
		baseAppAddr = pBest->externalAddr();

		CreateBaseReplyHandler * pHandler =
			new CreateBaseReplyHandler( srcAddr, header.replyID,
				baseAppAddr );

		// Tell the BaseApp about the client's new proxy
		Mercury::Bundle	& bundle = pBest->bundle();
		bundle.startRequest( BaseAppIntInterface::createBaseWithCellData,
				pHandler );

		bundle.transfer( data, data.remainingLength() );
		pBest->send();

		// Update the load estimate.
		pBest->addEntity();
	}

	bool calculateOverloaded( bool baseAppsOverloaded )
	{
		BaseAppMgr & baseAppMgr = BaseAppMgr::instance();
		if (baseAppsOverloaded)
		{
			uint64 overloadTime;

			// Start rate limiting logins
			if (baseAppMgr.baseAppOverloadStartTime_ == 0)
				baseAppMgr.baseAppOverloadStartTime_ = timestamp();

			overloadTime = timestamp() - baseAppMgr.baseAppOverloadStartTime_;
			INFO_MSG( "CellAppMgr::Overloaded for "PRIu64"ms\n",
				overloadTime/(stampsPerSecond()/1000) );

			if ((overloadTime > baseAppMgr.allowOverloadPeriod_) ||
				(baseAppMgr.loginsSinceOverload_ >=
				 	baseAppMgr.allowOverloadLogins_))
			{
				return true;
			}
			else
			{
				// If we're not overloaded
				baseAppMgr.loginsSinceOverload_++;
				INFO_MSG( "BaseAppMgr::Logins since overloaded " \
					"(allowing max of %d): %d\n",
					baseAppMgr.allowOverloadLogins_,
					baseAppMgr.loginsSinceOverload_ );
			}
		}
		else
		{
			// Not overloaded, clear the timer
			baseAppMgr.baseAppOverloadStartTime_ = 0;
			baseAppMgr.loginsSinceOverload_ = 0;
		}

		return false;
	}
};


/**
 *	This class is used to handle the controlled shutdown stage that can be sent
 *	to all BaseApps at the same time.
 */
class SyncControlledShutDownHandler : public Mercury::ReplyMessageHandler
{
public:
	SyncControlledShutDownHandler( ShutDownStage stage, int count ) :
		stage_( stage ),
		count_( count )
	{
		if (count_ == 0)
			this->finalise();
	}

private:
	virtual void handleMessage( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data, void * )
	{
		this->decCount();
	}

	virtual void handleException( const Mercury::NubException & ne, void * )
	{
		this->decCount();
	}

	void finalise()
	{
		BaseAppMgr * pApp = BaseAppMgr::pInstance();

		if (pApp)
		{
			DEBUG_MSG( "All BaseApps have shut down, informing CellAppMgr\n" );
			Mercury::Bundle & bundle = pApp->cellAppMgr().bundle();
			bundle.startMessage( CellAppMgrInterface::ackBaseAppsShutDown );
			bundle << stage_;
			pApp->cellAppMgr().send();
		}

		delete this;
	}

	void decCount()
	{
		--count_;

		if (count_ == 0)
			this->finalise();
	}

	ShutDownStage stage_;
	int count_;
};


/**
 *	This class is used to handle the controlled shutdown stage that is sent to
 *	all BaseApps sequentially.
 */
class AsyncControlledShutDownHandler : public Mercury::ReplyMessageHandler
{
public:
	AsyncControlledShutDownHandler( ShutDownStage stage,
			std::vector< Mercury::Address > & addrs ) :
		stage_( stage ),
		numSent_( 0 )
	{
		addrs_.swap( addrs );
		this->sendNext();
	}
	virtual ~AsyncControlledShutDownHandler() {}

private:
	virtual void handleMessage( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data, void * )
	{
		DEBUG_MSG( "AsyncControlledShutDownHandler::handleMessage: "
			"BaseApp %s has finished stage %d\n",
			srcAddr.c_str(), stage_ );

		if (stage_ == SHUTDOWN_PERFORM)
		{
			BaseAppMgr * pApp = BaseAppMgr::pInstance();
			pApp->removeControlledShutdownBaseApp( srcAddr );
		}

		this->sendNext();
	}

	virtual void handleException( const Mercury::NubException & ne, void * )
	{
		ERROR_MSG( "AsyncControlledShutDownHandler::handleException: "
			"Reason = %s\n", Mercury::reasonToString( ne.reason() ) );
		this->sendNext();
	}

	void sendNext()
	{
		bool shouldDeleteThis = true;
		BaseAppMgr * pApp = BaseAppMgr::pInstance();

		if (pApp)
		{
			if (numSent_ < int(addrs_.size()))
			{
				Mercury::ChannelOwner * pChannelOwner =
					pApp->findChannelOwner( addrs_[ numSent_ ] );

				if (pChannelOwner != NULL)
				{
					Mercury::Bundle & bundle = pChannelOwner->bundle();
					bundle.startRequest(
							BaseAppIntInterface::controlledShutDown, this );
					shouldDeleteThis = false;
					bundle << stage_;
					bundle << 0;
					pChannelOwner->send();
				}
				else
				{
					WARNING_MSG( "AsyncControlledShutDownHandler::sendNext: "
									"Could not find channel for %s\n",
							(char *)addrs_[ numSent_ ] );
				}

				++numSent_;
			}
			else if (stage_ == SHUTDOWN_DISCONNECT_PROXIES)
			{
				// This object deletes itself.
				new AsyncControlledShutDownHandler( SHUTDOWN_PERFORM, addrs_ );
			}
			else
			{
				Mercury::Bundle & bundle = pApp->cellAppMgr().bundle();
				bundle.startMessage( CellAppMgrInterface::ackBaseAppsShutDown );
				bundle << stage_;
				pApp->cellAppMgr().send();
				pApp->shutDown( false );
			}

		}

		if (shouldDeleteThis)
			delete this;
	}

	ShutDownStage stage_;
	std::vector< Mercury::Address > addrs_;
	int numSent_;
};


// -----------------------------------------------------------------------------
// Section: Handler methods
// -----------------------------------------------------------------------------

/**
 *	This method handes an <i>add</i> message from a BaseApp. It returns the new
 *	id that the BaseApp has.
 */
void BaseAppMgr::add( const Mercury::Address & srcAddr,
	Mercury::ReplyID replyID, const BaseAppMgrInterface::addArgs & args )
{
	MF_ASSERT( srcAddr == args.addrForCells );

	// If we're not allowing BaseApps to connect at the moment, just send back a
	// zero-length reply.
	if (!cellAppMgr_.channel().isEstablished() || !hasInitData_)
	{
		INFO_MSG( "BaseAppMgr::add: Not allowing BaseApp at %s to register "
				"yet\n", srcAddr.c_str() );

		Mercury::ChannelSender sender( BaseAppMgr::getChannel( srcAddr ) );
		sender.bundle().startReply( replyID );

		return;
	}

//	TRACE_MSG( "BaseAppMgr::add:\n" );
	if (!allowNewBaseApps_ || (shutDownStage_ != SHUTDOWN_NONE))
		return;	// just let it time out

	// Let the Cell App Manager know about the first base app. This is so that
	// the cell app can know about a base app. We will probably improve this
	// later.
	if (baseApps_.empty())
	{
		Mercury::Bundle	& bundle = cellAppMgr_.bundle();
		CellAppMgrInterface::setBaseAppArgs setBaseAppArgs;
		setBaseAppArgs.addr = args.addrForCells;
		bundle << setBaseAppArgs;
		cellAppMgr_.send();

		bestBaseAppAddr_ = args.addrForCells;
	}

	// Add it to our list of BaseApps
	BaseAppID id = this->getNextID();
	BaseApp * pBaseApp =
		new BaseApp( args.addrForCells, args.addrForClients, id );
	baseApps_[ srcAddr ].set( pBaseApp );

	// Need the following because operator char * of Mercury::Address uses
	// static storage.
	std::string cellNubStr = (char*)pBaseApp->addr();
	DEBUG_MSG( "BaseAppMgr::add:\n"
			"\tAllocated id    = %u\n"
			"\tBaseApps in use = %zu\n"
			"\tInternal nub    = %s\n"
			"\tExternal nub    = %s\n",
		id,
		baseApps_.size(),
		cellNubStr.c_str(),
		pBaseApp->externalAddr().c_str() );

	// Stream on the reply
	Mercury::Bundle & bundle = pBaseApp->bundle();
	bundle.startReply( replyID );

	BaseAppInitData initData;
	initData.id = id;
	initData.time = time_;
	initData.isReady = hasStarted_;

	bundle << initData;

	// Now stream on globals as necessary
	if (!globalBases_.empty())
	{
		GlobalBases::iterator iter = globalBases_.begin();

		while (iter != globalBases_.end())
		{
			bundle.startMessage( BaseAppIntInterface::addGlobalBase );
			bundle << iter->first << iter->second;

			++iter;
		}
	}

	if (!sharedBaseAppData_.empty())
	{
		SharedData::iterator iter = sharedBaseAppData_.begin();

		while (iter != sharedBaseAppData_.end())
		{
			bundle.startMessage( BaseAppIntInterface::setSharedData );
			bundle << SharedDataType( SHARED_DATA_TYPE_BASE_APP ) <<
				iter->first << iter->second;
			++iter;
		}
	}

	if (!sharedGlobalData_.empty())
	{
		SharedData::iterator iter = sharedGlobalData_.begin();

		while (iter != sharedGlobalData_.end())
		{
			bundle.startMessage( BaseAppIntInterface::setSharedData );
			bundle << SharedDataType( SHARED_DATA_TYPE_GLOBAL ) <<
				iter->first << iter->second;
			++iter;
		}
	}

	if (useNewStyleBackup_)
	{
		// This sends a bundle and so must be after initial send.
		this->adjustBackupLocations( pBaseApp->addr(), true );
	}
	else
	{
		BackupBaseApp * pBestBackup = this->findBestBackup( *pBaseApp );

		if (pBestBackup != NULL)
		{
			pBestBackup->backup( *pBaseApp );
		}
	}

	pBaseApp->send();
}


/**
 *	This method handes an <i>add</i> message from a BaseApp that wants to be a
 *	backup. It returns the new id that the BaseApp has.
 */
void BaseAppMgr::addBackup( const Mercury::Address & srcAddr,
	Mercury::ReplyID replyID,
	const BaseAppMgrInterface::addBackupArgs & args )
{
	if (!allowNewBaseApps_ || (shutDownStage_ != SHUTDOWN_NONE))
		return;	// just let it time out

	if (useNewStyleBackup_)
	{
		ERROR_MSG( "BaseAppMgr::addBackup: "
				"Backup BaseApps not used in new-style BaseApp backup (%s).\n",
			(char *)srcAddr );
		return; // just let it time out
	}

	BaseAppID id = this->getNextID();
	TRACE_MSG( "BaseAppMgr::addBackup: %s id = %u\n",
		(char *)args.addr, id );

	// Required for implementation of onlyUseBackupOnSameMachine_ to work.
	MF_ASSERT( srcAddr == args.addr );

	BackupBaseApp * pBaseApp = new BackupBaseApp( args.addr, id );
	backupBaseApps_[ srcAddr ].set( pBaseApp );

	Mercury::Bundle & bundle = pBaseApp->bundle();
	bundle.startReply( replyID );
	bundle << id << time_ << hasStarted_;

	pBaseApp->send();

	this->checkBackups();
}

namespace
{
	// Used to sort the BaseApps.
	// TODO: Currently, this is only using the reported CPU load. We probably
	// want to consider the number of proxy and base entities on the machine.
	template <class T>
	struct loadCmp
	{
		inline bool operator()( T * p1, T * p2 )
		{
			return p1->load() < p2->load();
		}
	};
}


/**
 *	This method updates information on the BaseApps about which other BaseApps
 *	they should create base entities on.
 */
void BaseAppMgr::updateCreateBaseInfo()
{
	// Description of createBaseAnywhere scheme:
	// Currently/Initially, a very simple scheme is being implemented. This may
	// be modified with some additional ideas if it is not effective enough.
	// This initial scheme is quite simple but it may be enough. A lot of the
	// balancing occurs from players logging in and out. These are always added
	// to the least loaded BaseApp.
	//
	// The basic scheme is that each BaseApp has a BaseApp assigned to it where
	// it should create Base entities. Only some of the BaseApps are destination
	// BaseApps.
	//
	// There are two configuration options createBaseRatio and
	// updateCreateBaseInfoPeriod. The createBaseRatio is the number of BaseApps
	// that a destination BaseApp will have pointing to it. For example, if this
	// createBaseRatio is 4, the least loaded quarter of the machines will each
	// have 4 BaseApps choosing them as the destination to create Base entities.
	//
	// updateCreateBaseInfoPeriod controls how often this information is
	// updated.
	//
	// Possible additions:
	// A situation that the initial scheme does not handle too well is when a
	// new BaseApp is added to a system that has a lot of heavily loaded
	// BaseApps. This unloaded BaseApp is consider equal to the other heavily
	// loaded BaseApps that are still members of the destination set. It may be
	// good enough that this fixes itself eventually as the loaded BaseApps come
	// in and out of the destination set. The logging in of players would also
	// help the situation.
	//
	// The BaseApps only know about one other BaseApp. We could let them know
	// about a number and they could create base entities on these randomly,
	// perhaps based on load. They could also create the base entities locally
	// if they are currently underloaded.
	//
	// Currently, members of the destination set are all considered equal. We
	// could consider their load in deciding how many BaseApps should have them
	// as a destination.
	//
	// Instead of this information being updated to all BaseApps at a regular
	// period, this information could be updated as needed. The BaseApps could
	// be kept sorted and the destination set updated as the BaseApp loads
	// changed. Only some BaseApps would need to be updated.


	// Get all of the BaseApps in a vector.
	// TODO: Could consider maintaining this vector as an optimisation.
	typedef std::vector< BaseApp * > BaseAppsVec;
	BaseAppsVec apps;
	apps.reserve( baseApps_.size() );

	BaseApps::iterator iter = baseApps_.begin();

	while (iter != baseApps_.end())
	{
		apps.push_back( iter->second.get() );
		++iter;
	}

	// Here the BaseApps are sorted so that we can find the least loaded
	// BaseApps. It does not really need to be completely sorted but it's
	// easy for now.
	std::sort( apps.begin(), apps.end(), loadCmp<BaseApp>() );

	int totalSize = apps.size();
	int destSize = int(totalSize/createBaseRatio_ + 0.99f);
	destSize = std::min( totalSize, std::max( 1, destSize ) );

	// Randomly shuffle so that the BaseApps are assigned to a random
	// destination BaseApp. It is probably good to have this randomisation to
	// help avoid degenerate cases.
	std::random_shuffle( apps.begin() + destSize, apps.end() );

	// Send this information to the BaseApps.
	for (size_t i = 0; i < apps.size(); ++i)
	{
		Mercury::Bundle & bundle = apps[ i ]->bundle();
		int destIndex = i % destSize;

		bundle.startMessage( BaseAppIntInterface::setCreateBaseInfo );
		bundle << apps[ destIndex ]->addr();

		apps[ i ]->send();
	}
}


/**
 *	This method is called to inform this BaseAppMgr about a base app during
 *	recovery from the death of an old BaseAppMgr.
 */
void BaseAppMgr::recoverBaseApp( const Mercury::Address & srcAddr,
			const Mercury::UnpackedMessageHeader & /*header*/,
			BinaryIStream & data )
{
	if (!isRecovery_)
	{
		WARNING_MSG( "BaseAppMgr::recoverBaseApp: "
				"Recovering when we were not started with -recover\n" );
		isRecovery_ = true;
	}

	Mercury::Address		addrForCells;
	Mercury::Address		addrForClients;
	Mercury::Address		backupAddress;
	BaseAppID				id;

	data >> addrForCells >> addrForClients >> backupAddress >> id >> time_;

	// hasStarted_ = true;
	this->startTimer();

	DEBUG_MSG( "BaseAppMgr::recoverBaseApp: %s, id = %u\n",
		(char *)addrForCells, id );

	lastBaseAppID_ = std::max( id, lastBaseAppID_ );

	BaseApp * pBaseApp = new BaseApp( addrForCells, addrForClients, id );
	baseApps_[ addrForCells ].set( pBaseApp );

	data >> pBaseApp->backupHash() >> pBaseApp->newBackupHash();

	if (backupAddress.ip != 0)
	{
		BackupBaseApps::iterator backupIter =
			backupBaseApps_.find( backupAddress );

		if (backupIter != backupBaseApps_.end())
		{
			backupIter->second->backedUp_.insert( pBaseApp );
			pBaseApp->setBackup( backupIter->second.get() );
		}
		else
		{
			std::string backupAddr = (char *)backupAddress;
			DEBUG_MSG( "BaseAppMgr::recoverBaseApp: "
					"Not yet setting backup of %s to %s\n",
				(char *)pBaseApp->addr(), backupAddr.c_str() );
			// Could store this so that we can do some error checking when the
			// backup is recovered.
		}
	}

	// Read all of the shared BaseApp data
	{
		uint32 numEntries;
		data >> numEntries;

		std::string key;
		std::string value;

		for (uint32 i = 0; i < numEntries; ++i)
		{
			data >> key >> value;
			sharedBaseAppData_[ key ] = value;
		}
	}

	// TODO: This is mildly dodgy. It's getting its information from the
	// BaseApps but would probably be more accurate if it came from the
	// CellAppMgr. It may clobber a valid change that has been made by the
	// CellAppMgr.

	// Read all of the shared Global data
	{
		uint32 numEntries;
		data >> numEntries;

		std::string key;
		std::string value;

		for (uint32 i = 0; i < numEntries; ++i)
		{
			data >> key >> value;
			sharedGlobalData_[ key ] = value;
		}
	}

	while (data.remainingLength() > 0)
	{
		std::pair< std::string, EntityMailBoxRef > value;
		data >> value.first >> value.second;

		MF_ASSERT( value.second.addr == srcAddr );

		if (!globalBases_.insert( value ).second)
		{
			WARNING_MSG( "BaseAppMgr::recoverBaseApp: "
					"Try to recover global base %s twice\n",
				value.first.c_str() );
		}
	}
}


/**
 *	This method is called to inform this BaseAppMgr about a backup base app
 *	during recovery from the death of an old BaseAppMgr.
 */
void BaseAppMgr::old_recoverBackupBaseApp( const Mercury::Address & srcAddr,
			const Mercury::UnpackedMessageHeader & /*header*/,
			BinaryIStream & data )
{
	Mercury::Address addr;
	BaseAppID id;
	data >> addr >> id;
	DEBUG_MSG( "BaseAppMgr::old_recoverBackupBaseApp: %s\n", (char *)addr );
	lastBaseAppID_ = std::max( id, lastBaseAppID_ );

	MF_ASSERT( addr == srcAddr );

	BackupBaseApp * pBaseApp = new BackupBaseApp( addr, id );
	backupBaseApps_[ addr ].set( pBaseApp );

	while (data.remainingLength() >= int(sizeof( Mercury::Address )))
	{
		Mercury::Address backupAddr;
		data >> backupAddr;

		BaseApps::iterator iter = baseApps_.find( backupAddr );

		if (iter != baseApps_.end())
		{
			pBaseApp->backedUp_.insert( iter->second.get() );
			iter->second->setBackup( pBaseApp );
		}
		else
		{
			std::string backupAddrStr = (char *)backupAddr;
			DEBUG_MSG( "BaseAppMgr::old_recoverBackupBaseApp: "
					"Not yet setting backup of %s to %s\n",
				(char *)iter->second->addr(), backupAddrStr.c_str() );
			// Could store this so that we can do some error checking when the
			// base app is recovered.
		}
	}
}


/**
 *	This method checks the backup application for the current Base Apps.
 *
 *	This is used in the old-style BaseApp backup.
 */
void BaseAppMgr::checkBackups()
{
	// If any Base Applications are not backed up, back them up now.
	BaseApps::iterator iter = baseApps_.begin();

	while (iter != baseApps_.end())
	{
		if (iter->second->getBackup() == NULL)
		{
			BackupBaseApp * pBackup = this->findBestBackup( *iter->second );

			if (pBackup != NULL)
			{
				pBackup->backup( *iter->second );
			}
		}

		++iter;
	}
}


/**
 *	This method handles the message from a BaseApp that it wants to be deleted.
 */
void BaseAppMgr::del( const BaseAppMgrInterface::delArgs & args,
					 const Mercury::Address & addr )
{
	TRACE_MSG( "BaseAppMgr::del: %u\n", args.id );

	// First try real Base Apps.

	if (this->onBaseAppDeath( addr, false ))
	{
		DEBUG_MSG( "BaseAppMgr::del: now have %zu base apps\n",
				baseApps_.size() );
	}
	else if (this->onBackupBaseAppDeath( addr ))
	{
		DEBUG_MSG( "BaseAppMgr::del: Now have %zu backup base apps\n",
			backupBaseApps_.size() );
	}
	else
	{
		ERROR_MSG( "BaseAppMgr: Error deleting %s id = %u\n",
			(char *)addr, args.id );
	}
}


/**
 *	This method adjusts who each BaseApp is backing up to. It is called whenever
 *	BaseApp is added or removed.
 *
 *	This is used by the new-style backup.
 */
void BaseAppMgr::adjustBackupLocations( const Mercury::Address & addr,
		bool isAdd )
{
	// The current scheme is that every BaseApp backs up to every other BaseApp.
	// Ideas for improvement:
	// - May want to cap the number of BaseApps that a BaseApp backs up to.
	// - May want to limit how much the hash changes backups. Currently, all old
	//    backups are discarded but if an incremental hash is used, the amount
	//    of lost backup information can be reduced.
	// - Incremental hash could be something like the following: when we have a
	//    non power-of-2 number of backups, we assume that some previous ones
	//    are repeated to always get a power of 2.
	//    Let n be number of buckets and N be next biggest power of 2.
	//    bucket = hash % N
	//    if bucket >= n:
	//      bucket -= N/2;
	//    When another bucket is added, an original bucket that was managing two
	//    virtual buckets now splits the load with the new bucket. When a bucket
	//    is removed, a bucket that was previously managing one virtual bucket
	//    now handles two.
	BaseApp * pNewBaseApp = NULL;

	if (isAdd)
	{
		pNewBaseApp = baseApps_[ addr ].get();
		MF_ASSERT( pNewBaseApp );
	}

	BaseApps::iterator iter = baseApps_.begin();

	bool hadMultipleBaseAppMachines = hasMultipleBaseAppMachines_;

	hasMultipleBaseAppMachines_ = false;

	// We check if everything is on the same machine
	while (iter != baseApps_.end())
	{
		if (baseApps_.begin()->first.ip != iter->first.ip)
		{
			hasMultipleBaseAppMachines_ = true;
			break;
		}
		iter++;
	}

	if (hasMultipleBaseAppMachines_ && !hadMultipleBaseAppMachines)
	{
		INFO_MSG( "Baseapps detected on multiple machines, switching to "
				  "multi-machine backup strategy.\n" );
	}

	if (!hasMultipleBaseAppMachines_ && hadMultipleBaseAppMachines)
	{
		INFO_MSG( "Baseapps detected on only one machine, falling back to "
				  "single-machine backup strategy.\n" );
	}

	iter = baseApps_.begin();

	while (iter != baseApps_.end())
	{
		if (addr != iter->first)
		{
			BaseApp & baseApp = *iter->second;

			if (baseApp.newBackupHash().empty())
			{
				baseApp.newBackupHash() = baseApp.backupHash();
			}
			else
			{
				// Stay with the previous newBackupHash
				WARNING_MSG( "BaseAppMgr::adjustBackupLocations: "
							"%s was still transitioning to a new hash.\n",
						(char *)iter->first );
			}

			// If backing up to was allowed previously, we can assume that it was
			// because there was no good places to backup
			if (hasMultipleBaseAppMachines_ && !hadMultipleBaseAppMachines)
			{
				MF_ASSERT( isAdd );
				baseApp.newBackupHash().clear();
			}

			// If backing up to the same machine was prohibited previously, make a
			// fully connected set
			else if (!hasMultipleBaseAppMachines_ && hadMultipleBaseAppMachines)
			{
				BaseApps::iterator inner = baseApps_.begin();

				while (inner != baseApps_.end())
				{
					if ((inner != iter) && (inner->first != addr))
					{
						baseApp.newBackupHash().push_back( inner->first );
					}
					++inner;
				}
			}

			if (isAdd)
			{
				if ((addr.ip != iter->first.ip) || !hasMultipleBaseAppMachines_)
				{
					baseApp.newBackupHash().push_back( addr );
					pNewBaseApp->newBackupHash().push_back( iter->first );
				}
			}
			else
			{
				baseApp.newBackupHash().erase( addr );

				// Could use a find() function, but none currently exists
				if (baseApp.backupHash().erase( addr ))
				{
					// The current backup is no longer valid.
					baseApp.backupHash().clear();
				}
			}

			Mercury::Bundle & bundle = baseApp.bundle();
			bundle.startMessage( BaseAppIntInterface::setBackupBaseApps );
			bundle << baseApp.newBackupHash();
			baseApp.send();
		}

		++iter;
	}

	if (isAdd)
	{
		Mercury::Bundle & bundle = pNewBaseApp->bundle();
		bundle.startMessage( BaseAppIntInterface::setBackupBaseApps );
		bundle << pNewBaseApp->newBackupHash();
		pNewBaseApp->send();
	}
}

/**
 *	This method checks and handles the case where a BaseApp may have stopped.
 */
bool BaseAppMgr::onBaseAppDeath( const Mercury::Address & addr,
							   bool shouldRestore )
{
	shouldRestore = shouldRestore && !useNewStyleBackup_;

	BaseApps::iterator iter = baseApps_.find( addr );

	if (iter != baseApps_.end())
	{
		INFO_MSG( "BaseAppMgr::onBaseAppDeath: baseapp%02d @ %s\n",
			iter->second->id(), (char *)addr );

		BaseApp & baseApp = *iter->second;
		BackupBaseApp * pBackup = baseApp.getBackup();
		bool controlledShutDown = false;

		if (pBackup != NULL)
		{
			pBackup->stopBackup( baseApp, !shouldRestore );
		}
		else if (shouldRestore)
		{
			ERROR_MSG( "BaseAppMgr::onBaseAppDeath: "
				"Unable to restore %s. No backup available.\n", (char *)addr );
			shouldRestore = false;

			if (shutDownServerOnBadState_)
			{
				controlledShutDown = true;
			}
		}

		if (shouldHardKillDeadBaseApps_)
		{
			// Make sure it's really dead, otherwise backup will have
			// trouble taking over its address.
			INFO_MSG( "BaseAppMgr::onBaseAppDeath: Sending SIGQUIT to %s\n",
					(char *) baseApp.addr() );
			if (!sendSignalViaMachined( baseApp.addr(), SIGQUIT, nub_ ))
			{
				ERROR_MSG( "BaseAppMgr::onBaseAppDeath: Failed to send "
						"SIGQUIT to %s\n", (char *) baseApp.addr() );
			}
		}


		if (shouldRestore)
		{
			baseApp.id( pBackup->id() );

			Mercury::Bundle & bundle = pBackup->bundle();
			bundle.startMessage( BaseAppIntInterface::old_restoreBaseApp );
			bundle << baseApp.addr() << baseApp.externalAddr();

			pBackup->send();

			this->onBackupBaseAppDeath( pBackup->addr() );

			this->checkBackups();
		}
		else
		{
			if (shutDownServerOnBaseAppDeath_)
			{
				controlledShutDown = true;
				NOTICE_MSG( "BaseAppMgr::onBaseAppDeath: "
						"shutDownServerOnBaseAppDeath is enabled. "
						"Shutting down server\n" );
			}
			else if (baseApp.backupHash().empty())
			{
				// TODO: What should be done if there is no backup or it's not
				// yet ready.
				if (baseApp.newBackupHash().empty())
				{
					ERROR_MSG( "BaseAppMgr::onBackupBaseAppDeath: "
							"No backup for %s\n", (char *)iter->first );
				}
				else
				{
					ERROR_MSG( "BaseAppMgr::onBackupBaseAppDeath: "
							"Backup not ready for %s\n", (char *)iter->first );
				}

				if (shutDownServerOnBadState_)
				{
					controlledShutDown = true;
				}
			}

			{
				Mercury::Bundle & bundle = cellAppMgr_.bundle();
				bundle.startMessage(
						CellAppMgrInterface::handleBaseAppDeath );
				bundle << iter->first;
				bundle << baseApp.backupHash();
				cellAppMgr_.send();
			}

			if (!useNewStyleBackup_)
			{
				this->checkGlobalBases( addr );
			}

			// Tell all other baseapps that the dead one is gone.
			unsigned int numBaseAppsAlive = baseApps_.size() - 1;
			if (numBaseAppsAlive > 0 && !controlledShutDown)
			{
				MemoryOStream args;
				args << iter->first << baseApp.backupHash();

				this->sendToBaseApps(
					BaseAppIntInterface::handleBaseAppDeath, args, &baseApp );

				deadBaseAppAddr_ = iter->first;
				archiveCompleteMsgCounter_ = numBaseAppsAlive;
			}

			// Adjust globalBases_ for new mapping
			{
				GlobalBases::iterator gbIter = globalBases_.begin();

				while (gbIter != globalBases_.end())
				{
					EntityMailBoxRef & mailbox = gbIter->second;

					if (mailbox.addr == addr)
					{
						Mercury::Address newAddr =
							baseApp.backupHash().addressFor( mailbox.id );
						mailbox.addr.ip = newAddr.ip;
						mailbox.addr.port = newAddr.port;
					}

					++gbIter;
				}
			}

			baseApps_.erase( iter );

			if (useNewStyleBackup_)
			{
				this->adjustBackupLocations( addr, false );
			}
		}

		if (controlledShutDown)
		{
			this->controlledShutDownServer();
		}
		else
		{
			this->updateCreateBaseInfo();
		}

		return true;
	}

	return false;
}


/**
 *	This method checks and handles the case where a backup Base App may have
 *	stopped.
 */
bool BaseAppMgr::onBackupBaseAppDeath( const Mercury::Address & addr )
{
	BackupBaseApps::iterator iter = backupBaseApps_.find( addr );

	if (iter != backupBaseApps_.end())
	{
		BackupBaseApp::BackedUpSet backedUpSet = iter->second->backedUp_;
		backupBaseApps_.erase( iter );

		BackupBaseApp::BackedUpSet::iterator iter = backedUpSet.begin();

		while (iter != backedUpSet.end())
		{
			BaseApp & baseApp = **iter;

			// Tell them that they don't have a backup.
			baseApp.setBackup( NULL );
			Mercury::Bundle & bundle = baseApp.bundle();
			bundle.startMessage( BaseAppIntInterface::old_setBackupBaseApp );
			bundle << Mercury::Address( 0, 0 );
			baseApp.send();

			BackupBaseApp * pBestBackup = this->findBestBackup( baseApp );

			if (pBestBackup != NULL)
			{
				pBestBackup->backup( baseApp );
			}
			else
			{
				WARNING_MSG( "BaseAppMgr::onBackupBaseAppDeath: "
					"No backup available for %s\n",  (char *)baseApp.addr() );
			}

			++iter;
		}

		return true;
	}

	return false;
}


/**
 *	This method handles a BaseApp finishing its controlled shutdown.
 */
void BaseAppMgr::removeControlledShutdownBaseApp(
		const Mercury::Address & addr )
{
	TRACE_MSG( "BaseAppMgr::removeControlledShutdownBaseApp: %s\n",
			addr.c_str() );

	baseApps_.erase( addr );
}


/**
 *	This method shuts down this process.
 */
void BaseAppMgr::shutDown( bool shutDownOthers )
{
	INFO_MSG( "BaseAppMgr::shutDown: shutDownOthers = %d\n",
			shutDownOthers );
	// Note: Shouldn't do much here because it is called from a signal handler.
	shouldShutDownOthers_ = shutDownOthers;
	nub_.breakProcessing();
}


/**
 *	This method responds to a shutDown message.
 */
void BaseAppMgr::shutDown( const BaseAppMgrInterface::shutDownArgs & args )
{
	this->shutDown( args.shouldShutDownOthers );
}


/**
 *	This method responds to a message telling us what stage of the controlled
 *	shutdown process the server is at.
 */
void BaseAppMgr::controlledShutDown(
		const BaseAppMgrInterface::controlledShutDownArgs & args )
{
	INFO_MSG( "BaseAppMgr::controlledShutDown: stage = %d\n", args.stage );

	switch (args.stage)
	{
		case SHUTDOWN_REQUEST:
		{
			Mercury::Bundle & bundle = cellAppMgr_.bundle();
			CellAppMgrInterface::controlledShutDownArgs args;
			args.stage = SHUTDOWN_REQUEST;
			bundle << args;
			cellAppMgr_.send();
			break;
		}

		case SHUTDOWN_INFORM:
		{
			shutDownStage_ = args.stage;
			shutDownTime_ = args.shutDownTime;

			// Inform all base apps.
			{
				SyncControlledShutDownHandler * pHandler =
					new SyncControlledShutDownHandler(
						args.stage, baseApps_.size() + backupBaseApps_.size() );

				// Inform backup base apps.

				for (BackupBaseApps::iterator it = backupBaseApps_.begin();
						it != backupBaseApps_.end(); it++)
				{
					BackupBaseApp & baseApp = *it->second;
					Mercury::Bundle & bundle = baseApp.bundle();
					bundle.startRequest(
							BaseAppIntInterface::controlledShutDown, pHandler );
					bundle << args.stage;
					bundle << args.shutDownTime;

					// This reply may take a little while. Currently relying on
					// the default timeout which is 5 seconds.
					baseApp.send();
				}

				// Inform normal base apps.
				MemoryOStream payload;
				payload << args.stage << args.shutDownTime;
				this->sendToBaseApps( BaseAppIntInterface::controlledShutDown,
					payload, NULL, pHandler );
			}

			break;
		}

		case SHUTDOWN_PERFORM:
		{
			this->startAsyncShutDownStage( SHUTDOWN_DISCONNECT_PROXIES );
			break;
		}

		case SHUTDOWN_TRIGGER:
		{
			this->controlledShutDownServer();
			break;
		}

		case SHUTDOWN_NONE:
		case SHUTDOWN_DISCONNECT_PROXIES:
			break;
	}
}


/**
 *
 */
void BaseAppMgr::startAsyncShutDownStage( ShutDownStage stage )
{
	std::vector< Mercury::Address > addrs;
	addrs.reserve( baseApps_.size() + backupBaseApps_.size() );

	for (BackupBaseApps::iterator it = backupBaseApps_.begin();
			it != backupBaseApps_.end(); it++)
	{
		addrs.push_back( it->first );
	}

	BaseApps::iterator iter = baseApps_.begin();

	while (iter != baseApps_.end())
	{
		addrs.push_back( iter->first );

		++iter;
	}

	// This object deletes itself.
	new AsyncControlledShutDownHandler( stage, addrs );
}


/**
 *  Trigger a controlled shutdown of the entire server.
 */
void BaseAppMgr::controlledShutDownServer()
{
	// First try to trigger controlled shutdown via the loginapp
	Mercury::Address loginAppAddr;
	Mercury::Reason reason = nub_.findInterface(
		"LoginIntInterface", -1, loginAppAddr );

	if (reason == Mercury::REASON_SUCCESS)
	{
		Mercury::ChannelSender sender( BaseAppMgr::getChannel( loginAppAddr ) );
		Mercury::Bundle & bundle = sender.bundle();

		bundle.startMessage( LoginIntInterface::controlledShutDown );

		INFO_MSG( "BaseAppMgr::controlledShutDownServer: "
			"Triggering server shutdown via LoginApp @ %s\n",
			loginAppAddr.c_str() );

		return;
	}
	else
	{
		ERROR_MSG( "BaseAppMgr::controlledShutDownServer: "
			"Couldn't find a LoginApp to trigger server shutdown\n" );
	}

	// Next try to trigger shutdown via the DBMgr
	if (this->dbMgr().channel().isEstablished())
	{
		Mercury::Bundle & bundle = this->dbMgr().bundle();
		DBInterface::controlledShutDownArgs::start( bundle ).stage =
			SHUTDOWN_REQUEST;
		this->dbMgr().send();

		INFO_MSG( "BaseAppMgr::controlledShutDownServer: "
				"Triggering server shutdown via DBMgr\n" );
		return;
	}
	else
	{
		ERROR_MSG( "BaseAppMgr::controlledShutDownServer: "
			"Couldn't find the DBMgr to trigger server shutdown\n" );
	}

	// Alright, the shutdown starts with me then
	BaseAppMgrInterface::controlledShutDownArgs args;
	args.stage = SHUTDOWN_REQUEST;
	INFO_MSG( "BaseAppMgr::controlledShutDownServer: "
		"Starting controlled shutdown here (no LoginApp or DBMgr found)\n" );
	this->controlledShutDown( args );
}

/**
 *	This method replies whether if the server has been started.
 */
void BaseAppMgr::requestHasStarted( const Mercury::Address & srcAddr,
		const Mercury::UnpackedMessageHeader& header,
		BinaryIStream & data )
{
	Mercury::ChannelSender sender( BaseAppMgr::getChannel( srcAddr ) );
	Mercury::Bundle & bundle = sender.bundle();

	bundle.startReply( header.replyID );
	bundle << hasStarted_;

	return;
}

/**
 *	This method processes the initialisation data from DBMgr.
 */
void BaseAppMgr::initData( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
{
	if (hasInitData_)
	{
		ERROR_MSG( "BaseAppMgr::initData: Ignored subsequent initialisation "
				"data from %s\n", addr.c_str() );
		return;
	}

	// Save DBMgr config and time for BaseApps
	TimeStamp gameTime;
	data >> gameTime;
	if (time_ == 0)
	{
		// __kyl__(12/8/2008) XML database always sends 0 as the game time.
		time_ = gameTime;
		INFO_MSG( "BaseAppMgr::initData: game time=%.1f\n",
				this->gameTimeInSeconds() );
	}
	// else
		// Recovery case. We should be getting the game time from BaseApps.

	int32	maxAppID;
	data >> maxAppID;
	if (maxAppID > lastBaseAppID_)
	{
		// __kyl__(12/8/2008) XML database always sends 0 as the max app ID.
		lastBaseAppID_ = maxAppID;
		INFO_MSG( "BaseAppMgr::initData: lastBaseAppIDAllocated=%d\n",
				lastBaseAppID_ );
	}

	hasInitData_ = true;
}

/**
 *	This method processes a message from the DBMgr that restores the spaces
 * 	(and space data). This comes via the BaseAppMgr mainly because
 * 	DBMgr doesn't have a channel to CellAppMgr and also because BaseAppMgr
 * 	tells DBMgr when to "start" the system.
 */
void BaseAppMgr::spaceDataRestore( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
{
	MF_ASSERT( !hasStarted_ && hasInitData_ );

	// Send spaces information to CellAppMgr
	{
		Mercury::Bundle & bundle = cellAppMgr_.bundle();
		bundle.startMessage( CellAppMgrInterface::prepareForRestoreFromDB );
		bundle << time_;
		bundle.transfer( data, data.remainingLength() );
		cellAppMgr_.send();
	}
}


/**
 *	This method handles a message to set a shared data value. This may be
 *	data that is shared between all BaseApps or all BaseApps and CellApps. The
 *	BaseAppMgr is the authoritative copy of BaseApp data but the CellAppMgr is
 *	the authoritative copy of global data (i.e. data shared between all BaseApps
 *	and all CellApps).
 */
void BaseAppMgr::setSharedData( BinaryIStream & data )
{
	bool sendToBaseApps = true;
	SharedDataType dataType;
	std::string key;
	std::string value;
	data >> dataType >> key >> value;

	if (dataType == SHARED_DATA_TYPE_BASE_APP)
	{
		sharedBaseAppData_[ key ] = value;
	}
	else if (dataType == SHARED_DATA_TYPE_GLOBAL)
	{
		sharedGlobalData_[ key ] = value;
	}
	else if ((dataType == SHARED_DATA_TYPE_GLOBAL_FROM_BASE_APP) ||
		(dataType == SHARED_DATA_TYPE_CELL_APP))
	{
		if (dataType == SHARED_DATA_TYPE_GLOBAL_FROM_BASE_APP)
		{
			dataType = SHARED_DATA_TYPE_GLOBAL;
		}

		// Because BaseApps don't have channels to the CellAppMgr
		// we will forward these messages on its behalf
		Mercury::Bundle & bundle = cellAppMgr_.bundle();
		bundle.startMessage( CellAppMgrInterface::setSharedData );
		bundle << dataType << key << value;
		cellAppMgr_.send();

		// Make sure we don't tell the BaseApps about this yet, wait
		// for CellAppMgr to notify us.
		sendToBaseApps = false;
	}
	else
	{
		ERROR_MSG( "BaseAppMgr::setSharedData: Invalid dataType %d\n",
				dataType );
		return;
	}

	if (sendToBaseApps)
	{
		MemoryOStream payload;
		payload << dataType << key << value;

		this->sendToBaseApps( BaseAppIntInterface::setSharedData, payload );
		this->sendToBackupBaseApps( BaseAppIntInterface::setSharedData, payload );
	}
}


/**
 *	This method handles a message to delete a shared data value. This may be
 *	data that is shared between all BaseApps or all BaseApps and CellApps. The
 *	BaseAppMgr is the authoritative copy of BaseApp data but the CellAppMgr is
 *	the authoritative copy of global data (i.e. data shared between all BaseApps
 *	and all CellApps).
 */
void BaseAppMgr::delSharedData( BinaryIStream & data )
{
	bool sendToBaseApps = true;
	SharedDataType dataType;
	std::string key;
	data >> dataType >> key;

	if (dataType == SHARED_DATA_TYPE_BASE_APP)
	{
		sharedBaseAppData_.erase( key );
	}
	else if (dataType == SHARED_DATA_TYPE_GLOBAL)
	{
		sharedGlobalData_.erase( key );
	}
	else if ((dataType == SHARED_DATA_TYPE_GLOBAL_FROM_BASE_APP) ||
		(dataType == SHARED_DATA_TYPE_CELL_APP))
	{
		if (dataType == SHARED_DATA_TYPE_GLOBAL_FROM_BASE_APP)
		{
			dataType = SHARED_DATA_TYPE_GLOBAL;
		}

		// Because BaseApps don't have channels to the CellAppMgr
		// we will forward these messages on its behalf
		Mercury::Bundle & bundle = cellAppMgr_.bundle();
		bundle.startMessage( CellAppMgrInterface::delSharedData );
		bundle << dataType << key;
		cellAppMgr_.send();

		// Make sure we don't tell the BaseApps about this yet, wait
		// for CellAppMgr to notify us.
		sendToBaseApps = false;
	}
	else
	{
		ERROR_MSG( "BaseAppMgr::delSharedData: Invalid dataType %d\n",
				dataType );
		return;
	}

	MemoryOStream payload;
	payload << dataType << key;

	if (sendToBaseApps)
	{
		this->sendToBaseApps( BaseAppIntInterface::delSharedData, payload );
		this->sendToBackupBaseApps( BaseAppIntInterface::delSharedData,
			payload );
	}
}


/**
 *	This class is used to handle the changes to the hash once new hash has been
 *	primed.
 */
class FinishSetBackupDiffVisitor : public BackupHash::DiffVisitor
{
public:
	FinishSetBackupDiffVisitor( const Mercury::Address & realBaseAppAddr ) :
   		realBaseAppAddr_( realBaseAppAddr )
	{}

	virtual void onAdd( const Mercury::Address & addr,
			uint32 index, uint32 virtualSize, uint32 prime )
	{
		BaseApp * pBaseApp = BaseAppMgr::instance().findBaseApp( addr );

		if (pBaseApp)
		{
			Mercury::Bundle & bundle = pBaseApp->bundle();
			BaseAppIntInterface::startBaseEntitiesBackupArgs & args =
				args.start( bundle );

			args.realBaseAppAddr = realBaseAppAddr_;
			args.index = index;
			args.hashSize = virtualSize;
			args.prime = prime;
			args.isInitial = false;

			pBaseApp->send();
		}
		else
		{
			ERROR_MSG( "FinishSetBackupDiffVisitor::onAdd: No BaseApp for %s\n",
					addr.c_str() );
		}
	}

	virtual void onChange( const Mercury::Address & addr,
			uint32 index, uint32 virtualSize, uint32 prime )
	{
		this->onAdd( addr, index, virtualSize, prime );
	}

	virtual void onRemove( const Mercury::Address & addr,
			uint32 index, uint32 virtualSize, uint32 prime )
	{
		BaseApp * pBaseApp = BaseAppMgr::instance().findBaseApp( addr );

		if (pBaseApp)
		{
			Mercury::Bundle & bundle = pBaseApp->bundle();
			BaseAppIntInterface::stopBaseEntitiesBackupArgs & args =
				args.start( bundle );

			args.realBaseAppAddr = realBaseAppAddr_;
			args.index = index;
			args.hashSize = virtualSize;
			args.prime = prime;
			args.isPending = false;

			pBaseApp->send();
		}
	}

private:
	Mercury::Address realBaseAppAddr_;
};



/**
 *	This method handles a message from a BaseApp informing us that it is ready
 *	to use its new backup hash.
 */
void BaseAppMgr::useNewBackupHash( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
{
	BackupHash backupHash;
	BackupHash newBackupHash;

	data >> backupHash >> newBackupHash;

	BaseApp * pBaseApp = this->findBaseApp( addr );

	if (pBaseApp)
	{
		FinishSetBackupDiffVisitor visitor( addr );
		backupHash.diff( newBackupHash, visitor );
		pBaseApp->backupHash().swap( newBackupHash );
		pBaseApp->newBackupHash().clear();
	}
	else
	{
		WARNING_MSG( "BaseAppMgr::useNewBackupHash: "
				"No BaseApp %s. It may have just died.?\n", (char *)addr );
	}
}


/**
 *	This method handles a message from a BaseApp informing us that it has
 *	completed a full archive cycle. Only BaseApps with secondary databases
 *	enabled should send this message.
 */
void BaseAppMgr::informOfArchiveComplete( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
{
	BaseApp * pBaseApp = this->findBaseApp( addr );

	if (!pBaseApp)
	{
		ERROR_MSG( "BaseAppMgr::informOfArchiveComplete: No BaseApp with "
				"address %s\n",	(char *) addr );
		return;
	}

	Mercury::Address deadBaseAppAddr;
	data >> deadBaseAppAddr;

	// Only interested in the last death
	if (deadBaseAppAddr != deadBaseAppAddr_)
	{
		return;
	}

	--archiveCompleteMsgCounter_;

	if (archiveCompleteMsgCounter_ == 0)
	{
		// Tell DBMgr which secondary databases are still active
		Mercury::Bundle & bundle = this->dbMgr().bundle();
		bundle.startMessage( DBInterface::updateSecondaryDBs );

		bundle << uint32(baseApps_.size());

		for (BaseApps::iterator iter = baseApps_.begin();
			   iter != baseApps_.end(); ++iter)
		{
			bundle << iter->second->id();
		}

		this->dbMgr().send();
	}
}


/**
 *	This method responds to a message from the DBMgr that tells us to start.
 */
void BaseAppMgr::startup( const Mercury::Address & addr,
		const Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data )
{
	if (hasStarted_)
	{
		WARNING_MSG( "BaseAppMgr::ready: Already ready.\n" );
		return;
	}

	INFO_MSG( "BaseAppMgr is starting\n" );

	this->startTimer();

	// Start the CellAppMgr
	{
		Mercury::Bundle & bundle = cellAppMgr_.bundle();
		bundle.startMessage( CellAppMgrInterface::startup );
		cellAppMgr_.send();
	}

	// Start the BaseApps.
	{
		if (baseApps_.empty())
		{
			CRITICAL_MSG( "BaseAppMgr::ready: "
				"No Base apps running when started.\n" );
		}

		// Tell all the baseapps to start up, but only one is the bootstrap
		bool bootstrap = true;
		for (BaseApps::iterator it = baseApps_.begin();	it != baseApps_.end(); ++it)
		{
			BaseApp & baseApp = *it->second;
			Mercury::Bundle & bundle = baseApp.bundle();
			bundle.startMessage( BaseAppIntInterface::startup );
			bundle << bootstrap;

			baseApp.send();
			bootstrap = false;
		}

		// ... and the backup ones
		bootstrap = false;	// Make sure backup baseapps don't bootstrap
		for (BackupBaseApps::iterator it = backupBaseApps_.begin();
			it != backupBaseApps_.end();
			it++)
		{
			BackupBaseApp & backup = *it->second;
			Mercury::Bundle & bundle = backup.bundle();
			bundle.startMessage( BaseAppIntInterface::startup );
			bundle << bootstrap;

			backup.send();
		}
	}
}


/**
 *	This method starts the game timer.
 */
void BaseAppMgr::startTimer()
{
	if (!hasStarted_)
	{
		hasStarted_ = true;
		Mercury::TimerID gtid = nub_.registerTimer( 1000000/updateHertz_,
				this,
				reinterpret_cast< void * >( TIMEOUT_GAME_TICK ) );
		pTimeKeeper_ = new TimeKeeper( nub_, gtid, time_, updateHertz_,
			&cellAppMgr_.addr(), &CellAppMgrInterface::gameTimeReading );
	}
}


/**
 *	This class is used to handle replies from the CellAppMgr to the checkStatus
 *	request.
 */
class CheckStatusReplyHandler : public ForwardingReplyHandler
{
public:
	CheckStatusReplyHandler( const Mercury::Address & srcAddr,
			Mercury::ReplyID replyID ) :
		ForwardingReplyHandler( srcAddr, replyID ) {};

	virtual void prependData( Mercury::Bundle & bundle,
		   BinaryIStream & data )
	{
		uint8 isOkay;
		data >> isOkay;

		bundle << isOkay << BaseAppMgr::instance().numBaseApps();
	}
};


/**
 *	This method handles a request from the DBMgr for our status. The status from
 *	the CellAppMgr is retrieved and then both returned.
 */
void BaseAppMgr::checkStatus( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
{
	if (cellAppMgr_.channel().isEstablished())
	{
		Mercury::Bundle & bundle = cellAppMgr_.bundle();
		bundle.startRequest( CellAppMgrInterface::checkStatus,
			   new CheckStatusReplyHandler( addr, header.replyID ) );
		bundle.transfer( data, data.remainingLength() );
		cellAppMgr_.send();
	}
	else
	{
		IF_NOT_MF_ASSERT_DEV( this->dbMgr().addr() == addr )
		{
			return;
		}

		Mercury::Bundle & bundle = this->dbMgr().bundle();
		bundle.startReply( header.replyID );
		bundle << uint8(false) << this->numBaseApps() << 0;
		bundle << "No CellAppMgr";
		this->dbMgr().send();
	}
}


/**
 *	This method is called to let the BaseAppMgr know that there is a new
 *	CellAppMgr.
 */
void BaseAppMgr::handleCellAppMgrBirth(
	const BaseAppMgrInterface::handleCellAppMgrBirthArgs & args )
{
	INFO_MSG( "BaseAppMgr::handleCellAppMgrBirth: %s\n", (char *)args.addr );

	if (!cellAppMgr_.channel().isEstablished() && (args.addr.ip != 0))
	{
		INFO_MSG( "BaseAppMgr::handleCellAppMgrBirth: "
					"CellAppMgr is now ready.\n" );
	}

	cellAppMgr_.addr( args.addr );

	// Reset the bestBaseAppAddr to allow the CellAppMgr to be
	// notified next game tick.
	bestBaseAppAddr_.ip = 0;
	bestBaseAppAddr_.port = 0;
}


/**
 *	This method is called when another BaseAppMgr is started.
 */
void BaseAppMgr::handleBaseAppMgrBirth(
	const BaseAppMgrInterface::handleBaseAppMgrBirthArgs & args )
{
	if (args.addr != nub_.address())
	{
		WARNING_MSG( "BaseAppMgr::handleBaseAppMgrBirth: %s\n",
				(char *)args.addr );
		this->shutDown( false );
	}
}


/**
 *	This method is called when a cell application has died unexpectedly.
 */
void BaseAppMgr::handleCellAppDeath( const Mercury::Address & /*addr*/,
		const Mercury::UnpackedMessageHeader & /*header*/,
		BinaryIStream & data )
{
	TRACE_MSG( "BaseAppMgr::handleCellAppDeath:\n" );

	// Make a local memory stream with the data so we can add it to the bundle
	// for each BaseApp.
	MemoryOStream payload;
	payload.transfer( data, data.remainingLength() );

	this->sendToBaseApps( BaseAppIntInterface::handleCellAppDeath, payload );
}


/**
 *  This method is called by machined to inform us of a base application that
 *  has died unexpectedly.
 */
void BaseAppMgr::handleBaseAppDeath(
				const BaseAppMgrInterface::handleBaseAppDeathArgs & args )
{
	this->handleBaseAppDeath( args.addr );
}


/**
 *	This method handles a BaseApp dying unexpectedly.
 */
void BaseAppMgr::handleBaseAppDeath( const Mercury::Address & addr )
{
	if (shutDownStage_ != SHUTDOWN_NONE)
		return;

	INFO_MSG( "BaseAppMgr::handleBaseAppDeath: dead app on %s\n", (char*)addr );

	if (!this->onBaseAppDeath( addr, true ))
	{
		this->onBackupBaseAppDeath( addr );
	}
}


/**
 *	This method creates an entity on an appropriate BaseApp.
 */
void BaseAppMgr::createBaseEntity( const Mercury::Address & srcAddr,
		const Mercury::UnpackedMessageHeader& header,
		BinaryIStream & data )
{
	BaseApp * pBest = this->findBestBaseApp();

	if (pBest)
	{
		Mercury::Bundle & bundle = pBest->bundle();
		bundle.startRequest( BaseAppIntInterface::createBaseWithCellData,
			new ForwardingReplyHandler( srcAddr, header.replyID ) );
		bundle.transfer( data, data.remainingLength() );
		pBest->send();
	}
	else
	{
		Mercury::ChannelSender sender( BaseAppMgr::getChannel( srcAddr ) );
		Mercury::Bundle & bundle = sender.bundle();

		bundle.startReply( header.replyID );
		bundle << (uint8)0;
		bundle << "No proxy could be found to add to.";

		return;
	}
}


/**
 *	This method attempts to add a global base.
 */
void BaseAppMgr::registerBaseGlobally( const Mercury::Address & srcAddr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
{
	// Figure out which baseapp sent this message
	BaseApp * pSender = this->findBaseApp( srcAddr );
	IF_NOT_MF_ASSERT_DEV( pSender )
	{
		ERROR_MSG( "BaseAppMgr::registerBaseGlobally: "
			"Got message from unregistered app @ %s, registration aborted\n",
			srcAddr.c_str() );

		return;
	}

	std::pair< std::string, EntityMailBoxRef > value;

	data >> value.first >> value.second;

	INFO_MSG( "BaseAppMgr::registerBaseGlobally: %s\n", value.first.c_str() );

	int8 successCode = 0;

	if (globalBases_.insert( value ).second)
	{
		successCode = 1;

		MemoryOStream args;
		args << value.first << value.second;

		this->sendToBaseApps( BaseAppIntInterface::addGlobalBase,
			args, pSender );
	}

	// Send the ack back to the sender.
	Mercury::Bundle & bundle = pSender->bundle();
	bundle.startReply( header.replyID );
	bundle << successCode;
	pSender->send();
}


/**
 *	This method attempts to update global base list when a baseApp disappears.
 */

void BaseAppMgr::checkGlobalBases( const Mercury::Address & deadBaseAppAddr )
{
	BaseApp & deadBaseApp = *this->findBaseApp( deadBaseAppAddr );
	std::vector< std::string > deadGlobalBases;

	for (GlobalBases::iterator iter = globalBases_.begin();
		   iter != globalBases_.end(); ++iter)
	{
		if (iter->second.addr == deadBaseAppAddr)
		{
			// mark for deletion
			deadGlobalBases.push_back( iter->first );
		}
	}

	// TODO: We shouldn't really send a packet for each dead base, these could
	// be grouped on a single bundle.
	while (!deadGlobalBases.empty())
	{
		std::string deadGB = deadGlobalBases.back();

		if (globalBases_.erase( deadGB ))
		{
			// Tell all the apps that the global base is gone
			MemoryOStream args;
			args << deadGB;
			this->sendToBaseApps( BaseAppIntInterface::delGlobalBase,
				args, &deadBaseApp );
		}
		else
		{
			ERROR_MSG( "BaseAppMgr::checkGlobalBases: Unable to erase %s\n",
					deadGB.c_str() );
		}
		deadGlobalBases.pop_back();
	}
}

/**
 *	This method attempts to remove a global base.
 */
void BaseAppMgr::deregisterBaseGlobally( const Mercury::Address & srcAddr,
			const Mercury::UnpackedMessageHeader & /*header*/,
			BinaryIStream & data )
{
	std::string label;
	data >> label;

	INFO_MSG( "BaseAppMgr::delGlobalBase: %s\n", label.c_str() );

	if (globalBases_.erase( label ))
	{
		BaseApp * pSrc = this->findBaseApp( srcAddr );
		MemoryOStream payload;
		payload << label;

		this->sendToBaseApps( BaseAppIntInterface::delGlobalBase,
				payload, pSrc );
	}
	else
	{
		ERROR_MSG( "BaseAppMgr::delGlobalBase: Failed to erase %s\n",
			label.c_str() );
	}
}


/**
 *	This method returns the BaseApp or BackupBaseApp associated with the input
 *	address.
 */
Mercury::ChannelOwner *
		BaseAppMgr::findChannelOwner( const Mercury::Address & addr )
{
	{
		BaseApps::iterator iter = baseApps_.find( addr );

		if (iter != baseApps_.end())
		{
			return iter->second.get();
		}
	}

	{
		BackupBaseApps::iterator iter = backupBaseApps_.find( addr );

		if (iter != backupBaseApps_.end())
		{
			return iter->second.get();
		}
	}

	return NULL;
}


/**
 *  This method runs a script on an appropriate BaseApp.
 */
void BaseAppMgr::runScript( const Mercury::Address & srcAddr,
		const Mercury::UnpackedMessageHeader& header,
		BinaryIStream & data )
{
	int8 broadcast;
	std::string script;
	data >> broadcast;
	data >> script;

	this->runScript( script, broadcast );
}


/**
 *	This method handles watcher messages setting "command/runScriptAll". It
 *	runs the input string on all BaseApps.
 */
void BaseAppMgr::runScriptAll( std::string script )
{
	this->runScript( script, 1 );
}


/**
 *	This method handles watcher messages setting "command/runScriptSingle". It
 *	runs the input string on one BaseApp.
 */
void BaseAppMgr::runScriptSingle( std::string script )
{
	this->runScript( script, 0 );
}


/**
 *  This method runs a script on an appropriate BaseApp.
 */
void BaseAppMgr::runScript( const std::string & script, int8 broadcast )
{
	if (broadcast != 0)
	{
		MemoryOStream payload;
		payload << script;

		if ((broadcast & 1) != 0)
		{
			this->sendToBaseApps( BaseAppIntInterface::runScript, payload );
		}

		if ((broadcast & 2) != 0)
		{
			this->sendToBackupBaseApps( BaseAppIntInterface::runScript,
					payload );
		}
	}
	else
	{
		BaseApp * pBest = this->findBestBaseApp();

		if (pBest == NULL)
		{
			ERROR_MSG( "No proxy could be found to run script\n" );
			return;
		}

		Mercury::Bundle & bundle = pBest->bundle();
		bundle.startMessage( BaseAppIntInterface::runScript );
		bundle << script;
		pBest->send();
	}
}


// -----------------------------------------------------------------------------
// Section: Handlers
// -----------------------------------------------------------------------------

/**
 *	Objects of this type are used to handle normal messages.
 */
template <class ARGS_TYPE>
class BaseAppMgrMessageHandler : public Mercury::InputMessageHandler
{
	public:
		typedef void (BaseAppMgr::*Handler)( const ARGS_TYPE & args );

		// Constructors
		BaseAppMgrMessageHandler( Handler handler ) : handler_( handler ) {}

	private:
		virtual void handleMessage( const Mercury::Address & /*srcAddr*/,
				Mercury::UnpackedMessageHeader & /*header*/,
				BinaryIStream & data )
		{
			ARGS_TYPE args;
			data >> args;
			(BaseAppMgr::instance().*handler_)( args );
		}

		Handler handler_;
};

/**
 *	Objects of this type are used to handle messages that also want the source
 *	address
 */
template <class ARGS_TYPE>
class BaseAppMgrMessageHandlerWithAddr : public Mercury::InputMessageHandler
{
	public:
		typedef void (BaseAppMgr::*Handler)( const ARGS_TYPE & args,
			const Mercury::Address & addr );

		// Constructors
		BaseAppMgrMessageHandlerWithAddr( Handler handler ) :
			handler_( handler ) {}

	private:
		virtual void handleMessage( const Mercury::Address & srcAddr,
				Mercury::UnpackedMessageHeader & /*header*/,
				BinaryIStream & data )
		{
			ARGS_TYPE args;
			data >> args;
			(BaseAppMgr::instance().*handler_)( args, srcAddr );
		}

		Handler handler_;
};


/**
 *	Objects of this type are used to handle request messages.
 */
template <class ARGS_TYPE>
class BaseAppMgrReturnMessageHandler : public Mercury::InputMessageHandler
{
	public:
		typedef void (BaseAppMgr::*Handler)(
				const Mercury::Address & srcAddr,
				Mercury::ReplyID replyID,
				const ARGS_TYPE & args );

		// Constructors
		BaseAppMgrReturnMessageHandler( Handler handler ) : handler_( handler ) {}

	private:
		virtual void handleMessage( const Mercury::Address & srcAddr,
				Mercury::UnpackedMessageHeader & header,
				BinaryIStream & data )
		{
			ARGS_TYPE args;
			data >> args;

			(BaseAppMgr::instance().*handler_)( srcAddr, header.replyID, args );
		}

		Handler handler_;
};


/**
 *	Objects of this type are used to handle request messages that have variable
 *	length input.
 */
class BaseAppMgrVarLenMessageHandler : public Mercury::InputMessageHandler
{
public:
	typedef void (BaseAppMgr::*Handler)( BinaryIStream & stream );

	BaseAppMgrVarLenMessageHandler( Handler handler ) : handler_( handler ) {}

private:
	virtual void handleMessage( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
	{
		(BaseAppMgr::instance().*handler_)( data );
	}

	Handler handler_;
};




/**
 *	Objects of this type are used to handle request messages that have variable
 *	length input.
 */
class BaseAppMgrRawMessageHandler : public Mercury::InputMessageHandler
{
public:
	typedef void (BaseAppMgr::*Handler)(
			const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & stream );

	BaseAppMgrRawMessageHandler( Handler handler ) : handler_( handler ) {}

private:
	virtual void handleMessage( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
	{
		(BaseAppMgr::instance().*handler_)( srcAddr, header, data );
	}

	Handler handler_;
};


#ifndef CODE_INLINE
#include "baseappmgr.ipp"
#endif

#define DEFINE_SERVER_HERE
#include "baseappmgr_interface.hpp"

// baseappmgr.cpp
