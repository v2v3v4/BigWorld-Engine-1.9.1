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

#include "db_consolidator.hpp"

#include "../../dbmgr/db_config.hpp"

#include "server/bwconfig.hpp"
#include "server/bwservice.hpp"
#include "resmgr/bwresource.hpp"
#include "network/logger_message_forwarder.hpp"

#include "cstdmf/memory_tracker.hpp"

#include <vector>
#include <fstream>

DECLARE_DEBUG_COMPONENT( 0 )

int main( int argc, char * argv[] )
{
	#ifdef _DEBUG
	g_memTracker.setReportOnExit( false );
	#endif

	BWResource bwresource;
	BWResource::init( argc, (const char **)argv );
	BWConfig::init( argc, argv );

	// We actually don't use the default listener created by the nub except
	// that we need the address of the internal interface so that our
	// remote file transfers can contact us.
	Mercury::Nub nub( 0, Mercury::Nub::USE_BWMACHINED );
	BW_MESSAGE_FORWARDER( ConsolidateDBs, dbMgr, nub );
	START_MSG( "consolidate_dbs" );

	// Make argv without --res and --ignoreSQLiteErrors arguments
	bool shouldStopOnError = true;
	bool shouldClearSecondaryDBEntries = false;
	int numArgs = 0;
	const char ** args = new const char*[argc];
	for (int i = 1; i < argc; ++i)
	{
		if (((strcmp( argv[i], "--res" ) == 0) || (strcmp( argv[i], "-r" ) == 0))
				&& i < (argc - 1))
		{
			++i;
			continue;
		}
		else if (strcmp( argv[i], "--ignore-sqlite-errors") == 0)
		{
			shouldStopOnError = false;
			continue;
		}
		else if (strcmp( argv[i], "--clear") == 0)
		{
			shouldClearSecondaryDBEntries = true;
			continue;
		}

		args[ numArgs ] = argv[i];
		++numArgs;
	}

	if (shouldClearSecondaryDBEntries)
	{
		if (numArgs > 0)
		{
			WARNING_MSG( "consolidate_dbs: The --clear option does not take "
					"additional arguments\n" );
		}
		return DBConsolidator::connectAndClearSecondaryDBEntries() ? 0 : -1;
	}

	// We currently only support specifying:
	// 1) Nothing on the command-line
	// OR
	// 2) The primary and a file containing all secondary database paths.
	// We can't make the paths an arg as it's numbers are arbitrary and
	// we may exceed command line limit.
	if (numArgs > 0 && numArgs < 2)
	{
		std::stringstream argsStrm;
		for (int i = 0; i < numArgs; ++i)
		{
			argsStrm << args[i] << ' ';
		}
		ERROR_MSG( "consolidate_dbs: Invalid command-line arguments: %s\n",
				argsStrm.str().c_str() );
		std::cout << "consolidate_dbs <primarydb> <secondarydb> ..."
				<< std::endl;

		return -1;
	}

	if (numArgs > 0)
	{
		std::vector< std::string > splits;

		// Get connection info for primary database
		std::string arg = args[0];
		char primaryDBArg[arg.size()+1];
	   	strcpy( primaryDBArg, arg.c_str() );
		char * str = strtok( primaryDBArg, ";" );
		while (str != NULL)
		{
			splits.push_back( str );
			str = strtok( NULL, ";" );
		}

		if (splits.size() != 5)
		{
			ERROR_MSG( "DBConsolidator: Invalid primary database connection "
					"information: %s\n", primaryDBArg );
			std::cout << "Primary database argument must be in the form "
					"<host>;<port>;<username>;<password>;<database>" <<
					std::endl;
			return -1;
		}

		DBConfig::Connection primaryDBConnectionInfo;
		primaryDBConnectionInfo.host = splits[0];
		primaryDBConnectionInfo.port = atoi( splits[1].c_str() );
		primaryDBConnectionInfo.username = splits[2];
		primaryDBConnectionInfo.password = splits[3];
		primaryDBConnectionInfo.database = splits[4];

		splits.clear();

		// Read secondary databases info
		std::vector< std::string > secondaryDBs;
		std::string secondaryDB;

		std::ifstream file( args[1] );
		if (file.is_open())
		{
			while (!file.eof())
			{
				getline( file, secondaryDB );
				if (secondaryDB.length() > 0)
				{
					secondaryDBs.push_back( secondaryDB );
				}
			}
			file.close();
		}

		// Now start consolidator
		// We are assuming that this is for snapshots, so we don't want to
		// update DBMgr with our status - therefore passing false as 3rd param.
		DBConsolidator consolidator( nub, watcherGlue, false,
				shouldStopOnError );
		return (consolidator.init( primaryDBConnectionInfo ) &&
				consolidator.consolidateSecondaryDBs( secondaryDBs )) ? 0 : -1;
	}
	else
	{
		// We are assuming that we are being run by DBMgr, therefore we do want
		// to update DBMgr with our status. Passing true as 3rd param.
		DBConsolidator consolidator( nub, watcherGlue, true,
				shouldStopOnError );
		return (consolidator.init() && consolidator.run()) ? 0 : -1;
	}

	delete [] args;
}
