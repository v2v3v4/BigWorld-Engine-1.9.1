/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "reviver.hpp"

#include "cstdmf/debug.hpp"
#include "network/bundle.hpp"
#include "network/logger_message_forwarder.hpp"
#include "resmgr/bwresource.hpp"
#include "server/bwconfig.hpp"

DECLARE_DEBUG_COMPONENT2( "Reviver", 0 )

#ifdef _WIN32

void bwStop()
{
	Reviver * pReviver = Reviver::pInstance();

	if (pReviver != NULL)
	{
		pReviver->shutDown();
	}
}

char szServiceDependencies[] = "machined";
#endif

// Needs to be after bwStop
#include "server/bwservice.hpp"


/**
 *	This method prints the usage of this program.
 */
void printHelp( const char * commandName )
{
	printf( "\n\n" );
	printf( "Usage: %s [OPTION]\n", commandName );
	printf(
"Monitors BigWorld server components and spawns a new process if a component\n"
"fails.\n"
"\n"
"  --add {baseAppMgr|cellAppMgr|dbMgr|loginApp}\n"
"  --del {baseAppMgr|cellAppMgr|dbMgr|loginApp}\n"
"\n" );

	printf(
"For example, the following monitors the DBMgr process and starts a new\n"
"instance if that one fails.\n"
"  %s --add dbMgr\n\n",
	 commandName );
}

int doMain( Mercury::Nub & nub, int argc, char * argv[] )
{
	Reviver reviver( nub );

	if (!reviver.init( argc, argv ))
	{
		ERROR_MSG( "Failed to initialise the reviver\n" );
		return 1;
	}

	BW_SERVICE_UPDATE_STATUS( SERVICE_RUNNING, 0, 0 );

	reviver.run();

	return 0;
}


int BIGWORLD_MAIN( int argc, char * argv[] )
{
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--help" ) == 0)
		{
			printHelp( argv[0] );
			return 0;
		}
	}

	Mercury::Nub nub( 0, BW_INTERNAL_INTERFACE( reviver ) );
	BW_MESSAGE_FORWARDER( Reviver, reviver, nub );
	START_MSG( "Reviver" );

	int result = doMain( nub, argc, argv );

	INFO_MSG( "Reviver has shut down.\n" );

	return result;
}

// main.cpp
