/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "db_config.hpp"

#include "server/bwconfig.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT( 0 );

namespace DBConfig
{
	/**
	 *	Generates a name used by all BigWorld processes to lock the database.
	 *	Only one connection can successfully obtain a lock with this name
	 *	at any one time.
	 */
	std::string Connection::generateLockName() const
	{
		std::string lockName( "BigWorld ");
		lockName += database;

		return lockName;
	}

	/**
	 *	Constructor. Reads config info from bw.xml
	 */
	Server::Server()
		: serverInfos_( 1 )
	{
		// Default config if <dbMgr> section is missing.
		ServerInfo& primaryServer = serverInfos_.front();
		primaryServer.configName = "<primary>";
		primaryServer.connectionInfo.host = "localhost";
		primaryServer.connectionInfo.port = 0;
		primaryServer.connectionInfo.username = "bigworld";
		primaryServer.connectionInfo.password = "bigworld";
		primaryServer.connectionInfo.database = "";

		// Get primary server
		readConnectionInfo( primaryServer.connectionInfo, "dbMgr" );

		// Get backup servers.
		{
			std::vector< std::string > childrenNames;
			BWConfig::getChildrenNames( childrenNames, "dbMgr/backupDatabases" );

			serverInfos_.reserve( serverInfos_.size() + childrenNames.size() );
			const ServerInfo& primaryServer = serverInfos_.front();
			for ( std::vector< std::string >::const_iterator iter =
					childrenNames.begin(); iter != childrenNames.end(); ++iter )
			{
				// By default backups look like the primary server so users
				// can override only those fields that are different.
				serverInfos_.push_back( primaryServer );
				ServerInfo& backupServer = serverInfos_.back();
				backupServer.configName = *iter;
				readConnectionInfo( backupServer.connectionInfo,
						"dbMgr/backupDatabases/" + *iter );
			}
		}

		pCurServer_ = serverInfos_.begin();
	}

	/**
	 *	This method sets the serverInfo from the given data section.
	 */
	void Server::readConnectionInfo( Connection& connectionInfo,
			const std::string& parentPath )
	{
		BWConfig::update( (parentPath + "/host").c_str(), connectionInfo.host );
		BWConfig::update( (parentPath + "/port").c_str(), connectionInfo.port );
		std::string username = BWConfig::get( (parentPath + "/username").c_str() );
		std::string password = BWConfig::get( (parentPath + "/password").c_str() );
		connectionInfo.username = username;
		connectionInfo.password = password;

		if ( !BWConfig::update( (parentPath + "/databaseName").c_str(),
				connectionInfo.database ) )
		{
			// For backwards compatability fall back onto dbMgr/name
			if ( BWConfig::update( (parentPath + "/name").c_str(),
					connectionInfo.database ) )
			{
				WARNING_MSG( "Server::readConnectionInfo: dbMgr/name has been "
						"deprecated, use dbMgr/databaseName instead.\n" );
			}
			else
			{
				ERROR_MSG( "Server::readConnectionInfo: dbMgr/databaseName "
					   "has not been set.\n" );
			}
		}
	}

}	// namespace DBConfig
