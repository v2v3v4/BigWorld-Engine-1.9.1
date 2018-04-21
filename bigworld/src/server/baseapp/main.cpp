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

#include <signal.h>
#include <set>

//#include "common/syslog.hpp"
#include "baseapp.hpp"
#include "cstdmf/timestamp.hpp"
#include "network/logger_message_forwarder.hpp"
#include "resmgr/bwresource.hpp"
#include "server/bwconfig.hpp"
#include "network/watcher_glue.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

#ifdef _WIN32  // WIN32PORT

void bwStop()
{
	raise( SIGINT );
}

char szServiceDependencies[] = "machined";

#endif // _WIN32

#include "server/bwservice.hpp"


// ----------------------------------------------------------------------------
// Section: Main
// ----------------------------------------------------------------------------

/**
 *	This function implements most of the functionality of the main function. Its
 *	main purpose is to scope the lifespan of the BaseApp instance.
 */
int doMain( Mercury::Nub & nub, int argc, char * argv[] )
{
	BaseApp baseApp( nub );

	// calculate the clock speed
	stampsPerSecond();

	if (!baseApp.init( argc, argv ))
	{
		ERROR_MSG( "main: init failed.\n" );
		return 1;
	}

	INFO_MSG( "---- BaseApp is running ----\n" );

	do
	{
		try
		{
			baseApp.intNub().processContinuously();
			break;
		}
		catch (Mercury::NubException & ne)
		{
			Mercury::Address addr = Mercury::Address::NONE;
			bool hasAddr = ne.getAddress( addr );

			switch (ne.reason())
			{
				// REASON_WINDOW_OVERFLOW is omitted here because that case is
				// checked for during sending.
				case Mercury::REASON_INACTIVITY:
				case Mercury::REASON_NO_SUCH_PORT:

					baseApp.addressDead( addr, ne.reason() );
					break;

				default:

					if (hasAddr)
					{
						char buf[ 256 ];
						snprintf( buf, sizeof( buf ),
							"processContinuously( %s )", addr.c_str() );

						baseApp.intNub().reportException( ne , buf );
					}
					else
					{
						baseApp.intNub().reportException(
							ne, "processContinuously" );
					}
					break;
			}
		}
	}
	while (!baseApp.intNub().processingBroken());

	baseApp.intNub().reportPendingExceptions( true /* reportBelowThreshold */ );
	baseApp.extNub().reportPendingExceptions( true /* reportBelowThreshold */ );

	return 0;
}


/*
 *	This implements the main function.
 */
int BIGWORLD_MAIN( int argc, char * argv[] )
{
	Mercury::Nub nub( 0, BW_INTERNAL_INTERFACE( baseApp ) );
	BW_MESSAGE_FORWARDER( BaseApp, baseApp, nub );
	START_MSG( "BaseApp" );

	int result = doMain( nub, argc, argv );

	{
		std::string result;
		std::string desc;
		Watcher::Mode mode;
		Watcher::rootWatcher().getAsString(NULL, "network/unackedPacketAllocator/numInPoolUsed", result, desc, mode);
	}
	// Called after BaseApp has been destroyed.
	INFO_MSG( "BaseApp has shut down.\n" );

	return result;
}

// main.cpp
