/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef DB_CONSOLIDATOR_HPP
#define DB_CONSOLIDATOR_HPP

#include "db_file_transfer.hpp"
#include "db_consolidator_error.hpp"

#include "../../dbmgr/db_entitydefs.hpp"
#include "../../dbmgr/idatabase.hpp"
#include "../../dbmgr/mysql_typemapping.hpp"

#include "cstdmf/singleton.hpp"
#include "network/nub.hpp"

class sqlite3_stmt;
class SqliteConnection;
class WatcherNub;
class ProgressReporter;
namespace MySQL
{
	class NamedLock;
}

/**
 * 	Consolidates data from remote secondary databases.
 */
class DBConsolidator : public MachineGuardMessage::ReplyHandler,
		public Singleton<DBConsolidator>
{
	typedef std::vector< sqlite3_stmt* > SQLiteStatements;

public:
	DBConsolidator( Mercury::Nub& nub, WatcherNub& watcherNub,
			bool shouldReportToDBMgr, bool shouldStopOnError );
	~DBConsolidator();

	bool init();
	bool init( const DBConfig::Connection& primaryDBConnectionInfo );
	bool run();
	bool consolidateSecondaryDBs( const FileNames & filePaths );
	void abort();

	// MachineGuardMessage::ReplyHandler overrides
	virtual bool onPidMessage( PidMessage &pm, uint32 addr );
	virtual bool onProcessStatsMessage( ProcessStatsMessage &psm, uint32 addr );

	void setDBMgrStatusWatcher( const std::string& status );

	static bool connectAndClearSecondaryDBEntries();

private:
	void initDBMgrAddr();
	bool getSecondaryDBInfos( SecondaryDBInfos& secondaryDBInfos );
	bool startRemoteProcess( uint32 remoteIP, const char * command,
			int argc, const char **argv );
	bool consolidateSecondaryDB( MySqlTransaction& transaction,
			const std::string& filePath, ProgressReporter& progressReporter );
	bool checkEntityDefsDigestMatch( const std::string& quotedDigest );
	bool checkEntityDefsMatch( MySql& connection );
	bool checkEntityDefsMatch( SqliteConnection& connection );
	static int getNumRows( SqliteConnection& connection,
			const std::string& tblName );
	static void orderTableByAge( SQLiteStatements& tables );
	bool consolidateSecondaryDBTable( SqliteConnection& connection,
			sqlite3_stmt* selectStmt,
			MySqlTransaction& transaction, ProgressReporter& progressReporter );
	void cleanUp();

	static bool connect( const DBConfig::Connection& connectionInfo,
			std::auto_ptr<MySql>& pConnection,
			std::auto_ptr<MySQL::NamedLock>& pLock );
	static bool clearSecondaryDBEntries( MySql& connection,
			uint& numEntriesCleared );

	Mercury::Nub&			nub_;
	WatcherNub& 			watcherNub_;

	Mercury::Address 		dbMgrAddr_;

	std::auto_ptr<MySql>	pPrimaryDBConnection_;
	std::auto_ptr<MySQL::NamedLock> primaryDBLock_;
	EntityDefs				entityDefs_;
	MySqlEntityTypeMappings	entityTypeMappings_;

	std::string				consolidationDir_;

	DBConsolidatorErrors	consolidationErrors_;
	bool					shouldStopOnError_;

	// The game time of entities that we've consolidated.
	typedef std::map< EntityKey, TimeStamp > ConsolidatedTimes;
	ConsolidatedTimes consolidatedTimes_;

	// Flag for aborting our wait loop.
	bool					shouldAbort_;
};

#endif /*DB_CONSOLIDATOR_HPP*/
