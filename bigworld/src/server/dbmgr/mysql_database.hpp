/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MYSQL_DATABASE_HPP
#define MYSQL_DATABASE_HPP

#include "network/interfaces.hpp"

#include "idatabase.hpp"

class MySql;
class MySqlThreadResPool;
class MySqlThreadData;

namespace DBConfig
{
	struct Connection;
	class Server;
}

uint32 initInfoTable( MySql& connection );

/**
 *	This class is an implementation of IDatabase for the MySQL database.
 */
class MySqlDatabase : public IDatabase, public Mercury::TimerExpiryHandler
{
private:
	MySqlDatabase();
public:
	static MySqlDatabase * create();
	~MySqlDatabase();

	virtual bool startup( const EntityDefs&, bool isFaultRecovery, 
							bool isUpgrade, bool isSyncTablesToDefs );
	virtual bool shutDown();

	virtual void mapLoginToEntityDBKey(
		const std::string & logOnName, const std::string & password,
		IDatabase::IMapLoginToEntityDBKeyHandler& handler );
	virtual void setLoginMapping( const std::string & username,
		const std::string & password, const EntityDBKey& ekey,
		IDatabase::ISetLoginMappingHandler& handler );

	virtual void getEntity( IDatabase::IGetEntityHandler& handler );
	virtual void putEntity( const EntityDBKey& ekey, EntityDBRecordIn& erec,
		IPutEntityHandler& handler );
	virtual void delEntity( const EntityDBKey & ekey,
		IDatabase::IDelEntityHandler& handler );

	virtual void executeRawCommand( const std::string & command,
		IDatabase::IExecuteRawCommandHandler& handler );

	virtual void putIDs( int numIDs, const EntityID * ids );
	virtual void getIDs( int numIDs, IDatabase::IGetIDsHandler& handler );

	// Backing up spaces.
	virtual void writeSpaceData( BinaryIStream& spaceData );

	virtual bool getSpacesData( BinaryOStream& strm );
	virtual void restoreEntities( EntityRecoverer& recoverer );

	virtual void setGameTime( TimeStamp gameTime );

	virtual void getBaseAppMgrInitData(
			IGetBaseAppMgrInitDataHandler& handler );

	// BaseApp death handler
	virtual void remapEntityMailboxes( const Mercury::Address& srcAddr,
			const BackupHash & destAddrs );

	// Secondary database entries
	virtual void addSecondaryDB( const IDatabase::SecondaryDBEntry& entry );
	virtual void updateSecondaryDBs( const BaseAppIDs& ids,
			IUpdateSecondaryDBshandler& handler );
	virtual void getSecondaryDBs( IDatabase::IGetSecondaryDBsHandler& handler );
	virtual uint32 getNumSecondaryDBs();
	virtual int clearSecondaryDBs();

	// DB locking
	virtual bool lockDB();
	virtual bool unlockDB();

	// Mercury::TimerExpiryHandler override
	virtual int handleTimeout( Mercury::TimerID id, void * arg );

	MySqlThreadResPool& getThreadResPool()	{	return *pThreadResPool_;	}
	MySqlThreadData& getMainThreadData();

	int getMaxSpaceDataSize() const { return maxSpaceDataSize_; }

	DBConfig::Server& 			getServerConfig();

	bool hasFatalConnectionError()	{	return reconnectTimerID_ != Mercury::TIMER_ID_NONE;	}
	void onConnectionFatalError();
	bool restoreConnectionToDb();

	void onWriteSpaceOpStarted()	{	++numWriteSpaceOpsInProgress_;	}
	void onWriteSpaceOpCompleted()	{	--numWriteSpaceOpsInProgress_;	}

	uint watcherGetNumBusyThreads() const;
	double watcherGetBusyThreadsMaxElapsedSecs() const;
	double watcherGetAllOpsCountPerSec() const;
	double watcherGetAllOpsAvgDurationSecs() const;

private:
	void printConfigStatus();

	// BigWorld internal tables initialisation
	void createSpecialBigWorldTables( MySql& connection );
	static void initSpecialBigWorldTables( MySql& connection,
			const EntityDefs& entityDefs );
	static bool checkSpecialBigWorldTables( MySql& connection );

	static uint32 getNumSecondaryDBs( MySql& connection );

private:
	MySqlThreadResPool* pThreadResPool_;

	int maxSpaceDataSize_;
	int numConnections_;
	int numWriteSpaceOpsInProgress_;

	Mercury::TimerID	reconnectTimerID_;
	size_t reconnectCount_;
};

#endif
