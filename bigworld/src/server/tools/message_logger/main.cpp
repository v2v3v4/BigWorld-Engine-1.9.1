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

#include "logger.hpp"

#include "network/bundle.hpp"
#include "server/bwconfig.hpp"

#include <signal.h>

DECLARE_DEBUG_COMPONENT( 0 )

// -----------------------------------------------------------------------------
// Section: Signal handlers
// -----------------------------------------------------------------------------

namespace
{
Logger * gLogger;
bool g_finished = false;
}


void sigint( int /* sig */ )
{
	g_finished = true;
}


// win32 doesn't know about SIGHUP.
// TODO: find out the win32 way of doing things and implement that for win32.
#ifndef _WIN32
void sighup( int /* sig */ )
{
	if (gLogger != NULL)
	{
		gLogger->shouldRoll( true );
	}
}
#endif

// Dodgy way to initialise this at static init time.
extern bool g_shouldWriteToConsole;
static bool hack = (g_shouldWriteToConsole = false );

// -----------------------------------------------------------------------------
// Section: Main
// -----------------------------------------------------------------------------
#ifdef _WIN32  // WIN32PORT

void bwStop()
{
	raise( SIGINT );
}

char szServiceDependencies[] = "machined";

#endif // _WIN32

#include "server/bwservice.hpp"

int BIGWORLD_MAIN( int argc, char * argv[] )
{
	// LoggerMessageForwarder lForwarder( "BWLogger",
		// 	BWConfig::get( "loggerID", 0 ) );

	Logger logger;
	gLogger = &logger;

#ifndef _WIN32
	signal( SIGHUP, sighup );
#endif
	signal( SIGINT, sigint );
	signal( SIGTERM, sigint );

	// Enable error messages to go to syslog
	DebugMsgHelper::shouldWriteToSyslog( true );
	INFO_MSG( "---- Logger is running ----\n" );

	if (logger.init( argc, argv ))
	{
		while (!g_finished)
		{
			logger.handleNextMessage();
		}
	}
	else
		return 1;

	return 0;
}

// main.cpp
