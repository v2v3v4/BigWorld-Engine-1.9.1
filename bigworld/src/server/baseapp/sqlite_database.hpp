/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SQLITE_DATABASE_HPP
#define SQLITE_DATABASE_HPP

#include "base.hpp"

#include "cstdmf/bgtask_manager.hpp"

class sqlite3;
class sqlite3_stmt;
class MemoryOStream;


/**
 *	This class is a SQLite database row.
*/
class Row: public ReferenceCount
{
public:
	Row( DatabaseID dbID, EntityTypeID typeID, TimeStamp time,
			MemoryOStream & stream,	WriteToDBReplyStructPtr pReplyStruct );
	~Row();

	void writeToDB( sqlite3_stmt & stmt ) const;
	void onWriteToDBComplete();

private:
	DatabaseID				dbID_;
	EntityTypeID			typeID_;
	TimeStamp				time_;
	char *					pBlob_;
	unsigned int			blobSize_;
	WriteToDBReplyStructPtr	pReplyStruct_;
};

typedef SmartPointer<Row> RowPtr;


/**
 *	This class represents a transaction.
*/
class Transaction: public ReferenceCount
{
public:
	Transaction() {};
	~Transaction();

	void writeToDB( RowPtr pRow );
	void commit( sqlite3 & con, const std::string & table );
	void onWriteToDBComplete();

	bool empty() const { return rows_.empty(); }

private:
	typedef std::vector<RowPtr> Rows;

	Rows rows_;
};

typedef SmartPointer<Transaction> TransactionPtr;


/**
 *	This class is a pool of transactions.
*/
class TransactionPool
{
public:
	TransactionPool();
	~TransactionPool();

	TransactionPtr acquire();
	void release( TransactionPtr pTrans );

	unsigned int size() const { return pool_.size(); }

private:
	typedef std::vector<TransactionPtr> Transactions;

	Transactions pool_;
	int poolSize_; //< Includes free and used transactions.
};


/**
 *	This class is an interface to a SQLite databases.
 */
class SqliteDatabase
{
public:
	SqliteDatabase( const std::string & filename, const std::string & dir );
	~SqliteDatabase();

	bool init( const std::string & checksum );

	const std::string& dbFilePath() const 	{ return path_; }

	void writeToDB( DatabaseID & dbID, EntityTypeID typeID, TimeStamp & time,
			MemoryOStream & stream,	WriteToDBReplyStructPtr pReplyStruct ) const;

	void commit();
	void commitBgTask( TransactionPtr pTrans, bool shouldFlip );
	void commitFgTask( TransactionPtr pTrans );

	void shouldFlip( bool flag )	{ shouldFlip_ = flag; }
	void isRegistered( bool flag )	{ isRegistered_ = flag; }

	void tick();

private:
	bool open();
	void close();

	bool writeChecksumTable( const std::string & checksum ) const;

	void flipTable();

	bool					isRegistered_;

	sqlite3 *				pCon_;

	const std::string		path_;

	const std::string *		pCurrTable_;
	bool					shouldFlip_;

	TransactionPool			transPool_;
	TransactionPtr			pTrans_;

	BgTaskManager			bgTaskMgr_;

	const std::string		flipTable_;
	const std::string		flopTable_;
	const std::string		dbIDColumn_;
	const std::string		typeIDColumn_;
	const std::string		timeColumn_;
	const std::string		blobColumn_;
	const std::string		checksumTable_;
	const std::string		checksumColumn_;

	const std::string		cpCmd_;
	const std::string		rmCmd_;
};


#endif // SQLITE_DATABASE_HPP
