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

#include "cstdmf/debug.hpp"
#include "db_interface.hpp"
#include "database.hpp"
#include "network/logger_message_forwarder.hpp"
#include "server/bwconfig.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

#ifdef _WIN32  // WIN32PORT

void bwStop()
{
	Database * pDB = Database::pInstance();

	if (pDB != NULL)
	{
		pDB->shutDown();
	}
}

char szServiceDependencies[] = "machined";
#endif

#include "server/bwservice.hpp"

int doMain( Mercury::Nub & nub, int argc, char * argv[] )
{
	bool isUpgrade = false;
	bool isSyncTablesToDefs = false;
	for (int i=1; i<argc; i++)
	{
		if ((0 == strcmp( argv[i], "--upgrade" )) ||
					(0 == strcmp( argv[i], "-upgrade" )))
		{
			isUpgrade = true;
		}
		else if ((0 == strcmp( argv[i], "--sync-tables-to-defs" )) ||
					(0 == strcmp( argv[i], "-syncTablesToDefs" )))
		{
			isSyncTablesToDefs = true;
		}
	}

	// We don't do the standard static instance trick here because we need
	// to guarentee the shut down order of the objects to avoid an access
	// violation on shut down.
	Database database( nub );

	Database::InitResult result =
		database.init( isUpgrade, isSyncTablesToDefs );
	switch (result)
	{
		case Database::InitResultFailure:
			ERROR_MSG( "Failed to initialise the database\n" );
			return 1;
		case Database::InitResultSuccess:
			BW_SERVICE_UPDATE_STATUS( SERVICE_RUNNING, 0, 0 );
			database.run();
			break;
		case Database::InitResultAutoShutdown:
			database.finalise();
			break;
		default:
			MF_ASSERT(false);
			break;
	}

	return 0;
}

int BIGWORLD_MAIN( int argc, char * argv[] )
{
	Mercury::Nub nub( 0, BW_INTERNAL_INTERFACE( dbMgr ) );
	BW_MESSAGE_FORWARDER( DBMgr, dbMgr, nub );
	START_MSG( "DBMgr" );

	int result = doMain( nub, argc, argv );

	INFO_MSG( "DBMgr has shut down.\n" );

	return result;
}

// main.cpp
