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

#include "baseappmgr.hpp"
#include "baseappmgr_interface.hpp"

#include "cellapp/cellapp_interface.hpp"
#include "cellappmgr/cellappmgr_interface.hpp"

#include "network/logger_message_forwarder.hpp"

#include "resmgr/bwresource.hpp"
#include "server/bwconfig.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

#ifdef _WIN32  // WIN32PORT

void bwStop()
{
    raise( SIGINT );
}

char szServiceDependencies[] = "cellappmgr";

#endif // _WIN32

#include "server/bwservice.hpp"

int doMain( Mercury::Nub & nub, int argc, char * argv[] )
{
	BaseAppMgr baseAppMgr( nub );

	BW_SERVICE_CHECK_POINT( 3000 );


	if (!baseAppMgr.init( argc, argv ))
	{
		ERROR_MSG( "main: init failed.\n" );
		return 1;
	}

	INFO_MSG( "---- BaseAppMgr is running ----\n" );

	BW_SERVICE_UPDATE_STATUS( SERVICE_RUNNING, 0, 0 );

	baseAppMgr.nub().processUntilBreak();

	return 0;
}

int BIGWORLD_MAIN( int argc, char * argv[] )
{
	Mercury::Nub nub( 0, BW_INTERNAL_INTERFACE( baseAppMgr ) );
	BW_MESSAGE_FORWARDER( BaseAppMgr, baseAppMgr, nub );
	START_MSG( "BaseAppMgr" );

	int result = doMain( nub, argc, argv );

	// Note: This is called after BaseAppMgr has been destroyed.
	INFO_MSG( "BaseAppMgr has shut down.\n" );

	return result;
}

#define DEFINE_INTERFACE_HERE
#include "cellappmgr/cellappmgr_interface.hpp"
#define DEFINE_INTERFACE_HERE
#include "cellapp/cellapp_interface.hpp"

// main.cpp
