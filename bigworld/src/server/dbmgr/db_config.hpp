/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef DATABASE_CONFIG_HPP
#define DATABASE_CONFIG_HPP

#include <string>
#include <vector>

#ifdef USE_MYSQL
#include "mysql_wrapper.hpp"
#endif

#include "cstdmf/stdmf.hpp"

namespace DBConfig
{
	/**
	 *	This struct contains the information to connect to a database server.
	 */
	struct Connection
	{
		Connection(): port( 0 ) {}
		
		std::string	host;
	    unsigned int port;
	    std::string	username;
	    std::string	password;
	    std::string	database;

	    std::string generateLockName() const;
	};

	/**
	 *	This class holds information required to connect to the database server
	 * 	and its backup databases.
	 */
	class Server
	{
	public:
		struct ServerInfo
		{
			std::string 	configName;
			Connection		connectionInfo;
		};

	private:
		typedef std::vector<ServerInfo> ServerInfos;

		ServerInfos				serverInfos_;
		ServerInfos::iterator	pCurServer_;

	public:
		Server();

		const ServerInfo& getCurServer() const
		{
			return *pCurServer_;
		}

		size_t getNumServers() const
		{
			return serverInfos_.size();
		}

		bool gotoNextServer()
		{
			++pCurServer_;
			if (pCurServer_ == serverInfos_.end())
				pCurServer_ = serverInfos_.begin();
			return (pCurServer_ != serverInfos_.begin());
		}

		void gotoPrimaryServer()
		{
			pCurServer_ = serverInfos_.begin();
		}

	private:
		static void readConnectionInfo( Connection& connectionInfo,
				const std::string& parentPath );
	};

}	// namespace DBConfig

#endif /*DATABASE_CONFIG_HPP*/
