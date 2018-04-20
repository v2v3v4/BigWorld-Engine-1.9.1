/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "stdafx.h"
#include "cstdmf/memory_tracker.hpp"
#include "unit_test_lib/unit_test.hpp"
#include "resmgr/bwresource.hpp"

#ifdef linux
#include <libgen.h>
#endif

int main( int argc, char* argv[] )
{
#ifdef ENABLE_MEMTRACKER
	MemTracker::instance().setCrashOnLeak( true );
#endif

#ifdef linux
	const char *dirName = dirname( argv[0] );
	chdir( dirName );
#endif

	new BWResource();

	// If res arguments are not provided then create our own res paths.
	bool resPathSpecified = false;
	for (int i = 1; i < argc; ++i)
	{
		if (strncmp( argv[i], "--res", 5 ) == 0)
		{
			resPathSpecified = true;
		}
	}
	if (!resPathSpecified)
	{
		const char * myResPath = "../../src/lib/resmgr/unit_test/res/";
		const char *myargv[] =
		{
			"resmgr_unit_test",
			"--res",
			myResPath
		};
		int myargc = ARRAY_SIZE( myargv );
		myargv[0] = argv[0]; // patch in the real application


		if (!BWResource::init( myargc, myargv ))
		{
			fprintf( stderr, "could not initialise BWResource\n" );
			return 1;
		}
	}
	// If arguments are provided then use the supplied res paths.
	else
	{
		if (!BWResource::init( argc, (const char**)argv ))
		{
			fprintf( stderr, "could not initialise BWResource\n" );
			return 1;
		}
	}

	int value = BWUnitTest::runTest( "resmgr", argc, argv );
	delete BWResource::pInstance();
	return value;
}

// main.cpp
