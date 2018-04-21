/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

# include "linux_machine_guard.hpp"

#include "bwmachined.hpp"
#include "server/bwservice.hpp"

static char usage[] =
	"Usage: %s [args]\n"
	"-f/--foreground   Run machined in the foreground (i.e. not as a daemon)\n";

int BIGWORLD_MAIN_NO_RESMGR( int argc, char * argv[] )
{
	bool daemon = true;
	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp( argv[i], "-f" ) || !strcmp( argv[i], "--foreground" ))
			daemon = false;

		else if (strcmp( argv[i], "--help" ) == 0)
		{
			printf( usage, argv[0] );
			return 0;
		}

		else
		{
			fprintf( stderr, "Invalid argument: '%s'\n", argv[i] );
			return 1;
		}
	}

	// Open syslog to allow us to log messages
	openlog( argv[0], 0, LOG_DAEMON );

	// Attempt to create the machined instance prior to becoming a daemon
	// to allow better error reporting in the init.d script
	BWMachined machined;

	// Turn ourselves into a daemon if required
	initProcessState( daemon );
	srand( (int)timestamp() );

	rlimit rlimitData = { RLIM_INFINITY, RLIM_INFINITY };
	setrlimit( RLIMIT_CORE, &rlimitData );

	BW_SERVICE_UPDATE_STATUS( SERVICE_RUNNING, 0, 0 );

	if (BWMachined::pInstance())
		return machined.run();
	else
		return 1;
}
