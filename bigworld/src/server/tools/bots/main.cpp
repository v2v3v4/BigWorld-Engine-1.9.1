/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "main_app.hpp"
#include "space_data_manager.hpp"
#include "network/bundle.hpp"
#include "network/logger_message_forwarder.hpp"
#include "server/bwconfig.hpp"

DECLARE_DEBUG_COMPONENT2( "Bots", 0 )

#ifdef _WIN32
#include <signal.h>

void bwStop()
{
	raise( SIGINT );
}

char szServiceDependencies[] = "machined";
#endif // _WIN32

#include "server/bwservice.hpp"

extern bool g_shouldWriteToConsole;

int BIGWORLD_MAIN( int argc, char * argv[] )
{
	g_shouldWriteToConsole = true;

	bool shouldLog = BWConfig::get( "bots/shouldLog", true );

	MainApp app;
	SpaceDataManager sdm;

	BW_MESSAGE_FORWARDER2( Bots, bots, shouldLog, app.nub() );

	if (app.init( argc, argv ))
	{
		START_MSG( "Bots" );
		app.run();
	}

	return 0;
}

// main.cpp
