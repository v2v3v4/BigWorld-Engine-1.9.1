/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "baseapp.hpp"
#include "baseappmgr.hpp"

#include "baseapp/baseapp_int_interface.hpp"

// -----------------------------------------------------------------------------
// Section: BaseApp
// -----------------------------------------------------------------------------

/**
 *	Constructor
 */
BaseApp::BaseApp( const Mercury::Address & intAddr,
			const Mercury::Address & extAddr,
			int id ) :
	ChannelOwner( BaseAppMgr::instance().nub(), intAddr ),
	externalAddr_( extAddr ),
	id_( id ),
	load_( 0.f ),
	numBases_( 0 ),
	numProxies_( 0 ),
	pBackup_( NULL )
{
	this->channel().isIrregular( true );
}


/**
 *	This method estimates the cost of adding an entity to the BaseApp.
 */
void BaseApp::addEntity()
{
	// TODO: Make this configurable and consider having different costs for
	// different entity types.
	load_ =+ 0.01f;
}


/**
 *	This static method makes a watcher associated with this object type.
 */
Watcher * BaseApp::makeWatcher()
{
	// generic watcher of a BaseApp structure
	Watcher * pWatchCacheVal = new DirectoryWatcher();
	BaseApp * pNullBaseApp = NULL;

	pWatchCacheVal->addChild( "id", new DataWatcher<BaseAppID>(
		pNullBaseApp->id_, Watcher::WT_READ_ONLY ) );
	pWatchCacheVal->addChild( "internalChannel",
			Mercury::ChannelOwner::pWatcher(),
			(ChannelOwner *)pNullBaseApp );
	pWatchCacheVal->addChild( "externalAddr", &Mercury::Address::watcher(),
		&pNullBaseApp->externalAddr_ );
	pWatchCacheVal->addChild( "load", new DataWatcher<float>(
		pNullBaseApp->load_, Watcher::WT_READ_ONLY ) );
	pWatchCacheVal->addChild( "numBases",
		new DataWatcher<int>( pNullBaseApp->numBases_,
			Watcher::WT_READ_ONLY ) );
	pWatchCacheVal->addChild( "numProxies",
		new DataWatcher<int>( pNullBaseApp->numProxies_,
			Watcher::WT_READ_ONLY ) );

	return pWatchCacheVal;
}


/**
 *	This method returns whether or not the Base App Manager has heard
 *	from this Base App in the timeout period.
 */
bool BaseApp::hasTimedOut( uint64 currTime, uint64 timeoutPeriod,
	   uint64 timeSinceAnyHeard ) const
{
	bool result = false;

	uint64 diff = currTime - this->channel().lastReceivedTime();
	result = (diff > timeoutPeriod);

	if (result)
	{
		INFO_MSG( "BaseApp::hasTimedOut: Timed out - %.2f (> %.2f) %s\n",
				double( (int64)diff )/stampsPerSecondD(),
				double( (int64)timeoutPeriod )/stampsPerSecondD(),
				(char *)this->addr() );

		// The logic behind the following block of code is that if we
		// haven't heard from any baseapp in a long time, the baseappmgr is
		// probably the misbehaving app and we shouldn't start forgetting
		// about baseapps.  If we want to shutdown our server on bad state,
		// we want to be able to return true when our last baseapp dies, so
		// relax the following check.
		if (!BaseAppMgr::instance().shutDownServerOnBadState())
		{
			if (timeSinceAnyHeard > timeoutPeriod/2)
			{
				INFO_MSG( "BaseApp::hasTimedOut: "
					"Last inform time not recent enough %f\n",
					double((int64)timeSinceAnyHeard)/stampsPerSecondD() );
				result = false;
			}
		}
	}

	return result;
}


// -----------------------------------------------------------------------------
// Section: BackupBaseApp
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
BackupBaseApp::BackupBaseApp( const Mercury::Address & addr, int id ) :
	ChannelOwner( BaseAppMgr::instance().nub(), addr ),
	id_( id ),
	load_( 0.f )
{
}


/**
 *	This method is called to associate this backup base application with the
 *	input base app.
 */
bool BackupBaseApp::backup( BaseApp & cache )
{
	if (!backedUp_.insert( &cache ).second)
	{
		ERROR_MSG( "BackupBaseApp::backup: "
			"%s already backed up\n", (char *)cache.addr() );
		return false;
	}

	if (cache.getBackup() != NULL)
	{
		cache.getBackup()->stopBackup( cache, true );
	}

	MF_ASSERT( cache.getBackup() == NULL );

	cache.setBackup( this );

	Mercury::Bundle & bundle = this->bundle();
	bundle.startMessage( BaseAppIntInterface::old_startBaseAppBackup );
	bundle << cache.addr();
	this->send();

	return true;
}


/**
 *	This method is called to stop the association of the backup base application
 *	with the input base app.
 */
bool BackupBaseApp::stopBackup( BaseApp & cache,
		bool tellBackupBaseApp )
{
	bool result = false;

	if (cache.getBackup())
	{
		if (tellBackupBaseApp)
		{
			Mercury::Bundle & bundle = this->bundle();
			bundle.startMessage( BaseAppIntInterface::old_stopBaseAppBackup );
			bundle << cache.addr();
			this->send();
		}

		result = (backedUp_.erase( &cache ) > 0);
		cache.setBackup( NULL );
	}

	return result;
}

// baseapp.cpp
