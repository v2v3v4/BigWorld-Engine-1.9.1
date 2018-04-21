/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MYSQL_THREAD_HPP
#define MYSQL_THREAD_HPP

#include <list>

#include "worker_thread.hpp"
#include "mysql_wrapper.hpp"
#include "mysql_typemapping.hpp"
#include "mysql_database.hpp"
#include "mysql_named_lock.hpp"

// -----------------------------------------------------------------------------
// Section: Constants
// -----------------------------------------------------------------------------
#define THREAD_TASK_WARNING_DURATION 		stampsPerSecond()
#define THREAD_TASK_TIMING_RESET_DURATION 	(5 * stampsPerSecond())

// -----------------------------------------------------------------------------
// Section: struct SpaceDataBinding
// -----------------------------------------------------------------------------
/**
 *	Bindings for space data in bigworldSpaceData.
 */
struct SpaceDataBinding
{
	int64 		spaceKey;
	uint16 		dataKey;
	MySqlBuffer	data;

	SpaceDataBinding( int maxSpaceDataSize ) : data( maxSpaceDataSize ) {}
};
MySqlBindings& operator<<( MySqlBindings& bindings, SpaceDataBinding& sdb );

uint32 writeSpaceDataStreamToDB( MySql& connection,
		SpaceID& spaceIDBinding, MySqlStatement& insertSpaceIDStmt,
		SpaceDataBinding& spaceDataBinding, MySqlStatement& insertSpaceDataStmt,
		BinaryIStream& stream );

// -----------------------------------------------------------------------------
// Section: class MySQL::SecondaryDBOps
// -----------------------------------------------------------------------------
namespace MySQL
{
	/**
	 * 	This class contains all the prepared statements and buffers related
	 * 	to the bigworldSecondaryDatabases table.
	 */
	class SecondaryDBOps
	{
	public:
		struct DbEntryBuffer
		{
			uint32		ip;
			uint16		port;
			int32		appID;
			MySqlBuffer	location;

			DbEntryBuffer() :
				ip( 0 ), port( 0 ), location( MAX_SECONDARY_DB_LOCATION_LENGTH )
			{}

			void set( uint32 ipAddr, uint16	portNum, int32 appId,
					const std::string loc )
			{
				ip = ipAddr;
				port = portNum;
				appID = appId;
				location.setString( loc );
			}

			template <class BINDINGS>
			void addToBindings( BINDINGS& bindings )
			{
				bindings << ip << port << appID << location;
			}

			std::string getAsString() const;
		};

	private:
		DbEntryBuffer					entryBuf_;
		std::auto_ptr<MySqlStatement> 	pAddStmt_;

	public:

		static void createTable( MySql& connection );

		DbEntryBuffer&	entryBuf()	{ return entryBuf_; }
		const DbEntryBuffer& entryBuf()	const { return entryBuf_; }
		MySqlStatement& addStmt( MySql& connection );
	};
}	// namespace MySQL

// -----------------------------------------------------------------------------
// Section: class MySqlThreadData
// -----------------------------------------------------------------------------
/**
 *	This struct holds the data that gets passed to worker thread for
 * 	processing. For efficiency, things that are expensive instantiate gets
 * 	put here so that they are re-used. For convenience, things that are common
 * 	to a lot of tasks are put here so that each task doesn't have to redeclare
 * 	them.
 */
struct MySqlThreadData
{
	MySql				connection;
	MySqlTypeMapping	typeMapping;
	uint64				startTimestamp;

	EntityID boundID_;
	int boundLimit_;
	std::auto_ptr<MySqlStatement> putIDStatement_;
	std::auto_ptr<MySqlUnPrep::Statement> getIDsStatement_;
	std::auto_ptr<MySqlUnPrep::Statement> delIDsStatement_;

	std::auto_ptr<MySqlStatement> incIDStatement_;
	std::auto_ptr<MySqlStatement> getIDStatement_;

	TimeStamp gameTime_;
	std::auto_ptr<MySqlStatement> setGameTimeStatement_;

	SpaceID boundSpaceID_;
	SpaceDataBinding boundSpaceData_;
	std::auto_ptr<MySqlStatement> writeSpaceStatement_;
	std::auto_ptr<MySqlStatement> writeSpaceDataStatement_;

	std::auto_ptr<MySqlStatement> delSpaceIDsStatement_;
	std::auto_ptr<MySqlStatement> delSpaceDataStatement_;

	MySQL::SecondaryDBOps secondaryDBOps_;

	// Temp variables common to various tasks.
	EntityDBKey			ekey;
	bool				isOK;
	std::string			exceptionStr;

	MySqlThreadData(  const DBConfig::Connection& connInfo,
					  int maxSpaceDataSize,
					  const EntityDefs& entityDefs,
					  const char * tblNamePrefix = TABLE_NAME_PREFIX );
};

// -----------------------------------------------------------------------------
// Section: class MySqlThreadResPool
// -----------------------------------------------------------------------------
/**
 *	This class is the thread resource pool used by MySqlDatabase to process
 *	incoming requests in parallel.
 */
class MySqlThreadResPool : public Mercury::TimerExpiryHandler
{
public:
	// Interface for doing something to a MySqlThreadData.
	struct IThreadDataProcessor
	{
		virtual void process( MySqlThreadData& threadData ) = 0;
		virtual void done() = 0;
	};

private:
	typedef std::vector< MySqlThreadData* > ThreadDataVector;

	struct ThreadDataProcJob
	{
		IThreadDataProcessor& 	processor;
		ThreadDataVector		unprocessedItems;

		ThreadDataProcJob( IThreadDataProcessor& proc,
				const ThreadDataVector& items )
			: processor( proc ), unprocessedItems( items ) {}
	};
	typedef std::list< ThreadDataProcJob > ThreadDataProcJobs;

	WorkerThreadPool					threadPool_;
	auto_container< ThreadDataVector >	threadDataPool_;
	ThreadDataVector					freeThreadData_;
	MySqlThreadData 					mainThreadData_;
	MySQL::NamedLock					dbLock_;

	Mercury::Nub&						nub_;
	Mercury::TimerID					keepAliveTimerID_;

	ThreadDataProcJobs					threadDataJobs_;

	uint								opCount_;
	uint64								opDurationTotal_;
	uint64								resetTimeStamp_;

public:
	MySqlThreadResPool( WorkerThreadMgr& threadMgr,
						Mercury::Nub& nub,
		                int numConnections,
						int maxSpaceDataSize,
		                const DBConfig::Connection& connInfo,
	                    const EntityDefs& entityDefs,
	                    bool shouldLockDB = true );
	virtual ~MySqlThreadResPool();

	int	getNumConnections()	{	return int(threadDataPool_.container.size()) + 1;	}
	MySqlThreadData& getMainThreadData()	{ return mainThreadData_; }
	MySqlThreadData* acquireThreadData( int timeoutMicroSeconds = -1 );
	void releaseThreadData( MySqlThreadData& threadData );
	MySqlThreadData& acquireThreadDataAlways( int mainThreadThreshold = 1 );
	void releaseThreadDataAlways( MySqlThreadData& threadData );
	bool isMainThreadData( MySqlThreadData& threadData ) const
	{	return &threadData == &mainThreadData_;	}

	void doTask( WorkerThread::ITask& task, MySqlThreadData& threadData );

	void applyToAllThreadDatas( IThreadDataProcessor& processor );

	void startThreadTaskTiming( MySqlThreadData& threadData );
	uint64 stopThreadTaskTiming( MySqlThreadData& threadData );

	double getBusyThreadsMaxElapsedSecs() const;
	double getOpCountPerSec() const;
	double getAvgOpDuration() const;

	WorkerThreadPool& threadPool()	{ return threadPool_; }
	const WorkerThreadPool& getThreadPool() const	{ return threadPool_; }

	// Mercury::TimerExpiryHandler overrides
	virtual int handleTimeout( Mercury::TimerID id, void * arg );

	bool lockDB()			{ return dbLock_.lock(); }
	bool unlockDB()			{ return dbLock_.unlock(); }
	bool isDBLocked() const	{ return dbLock_.isLocked(); }

private:
	MySqlThreadData* acquireThreadDataInternal();
	void applyThreadDataJob( MySqlThreadData& threadData,
			ThreadDataProcJob& job );
};

// -----------------------------------------------------------------------------
// Section: class MySqlThreadTask
// -----------------------------------------------------------------------------
/**
 *	This class is the base class for all MySqlDatabase operations that needs
 *	to be executed in a separate thread. It handles to the acquisition and
 *	release of thread resources.
 */
class MySqlThreadTask :	public WorkerThread::ITask
{
	MySqlDatabase&		owner_;
	MySqlThreadData&	threadData_;
	bool				isTaskReady_;

public:
	MySqlThreadTask( MySqlDatabase& owner, int acquireMainThreadThreshold = 3 )
		: owner_(owner),
		threadData_( owner_.getThreadResPool().acquireThreadDataAlways(
						acquireMainThreadThreshold ) ),
		isTaskReady_(true)
	{}

	virtual ~MySqlThreadTask()
	{
		if (threadData_.connection.hasFatalError())
		{
			ERROR_MSG( "MySqlDatabase: %s\n",
					threadData_.connection.getFatalErrorStr().c_str() );
			owner_.onConnectionFatalError();
		}

		owner_.getThreadResPool().releaseThreadDataAlways( threadData_ );
	}

	MySqlThreadData& getThreadData()	{	return threadData_;	}
	const MySqlThreadData& getThreadData() const {	return threadData_;	}
	MySqlDatabase& getOwner()			{	return owner_;	}
	void setTaskReady( bool isReady )	{	isTaskReady_ = isReady;	}
	bool isTaskReady() const			{	return isTaskReady_;	}

	void startThreadTaskTiming()
	{	owner_.getThreadResPool().startThreadTaskTiming( this->getThreadData() );	}
	uint64 stopThreadTaskTiming()
	{	return owner_.getThreadResPool().stopThreadTaskTiming( this->getThreadData() );	}

	void doTask()
	{
		if (isTaskReady_)
			owner_.getThreadResPool().doTask( *this, threadData_ );
		else
			this->onRunComplete();
	}

	// Does some standard things at the beginning of a task.
	void standardInit()
	{
		this->startThreadTaskTiming();
		this->getThreadData().exceptionStr.clear();
	}

	// Does some standard things at the end of a task.
	template < class ERROR_CONFIG >
	bool standardOnRunComplete()
	{
		MySqlThreadData& threadData = this->getThreadData();
		bool hasError = threadData.exceptionStr.length();
		if (hasError)
		{
			ERROR_MSG( "MySqlDatabase::%s( %s ): %s\n",
					ERROR_CONFIG::errorMethodName(),
					this->getTaskInfo().c_str(),
					threadData.exceptionStr.c_str() );
		}

		uint64 duration = this->stopThreadTaskTiming();
		if (duration > ERROR_CONFIG::threadTaskWarningDuration())
		{
			WARNING_MSG( "MySqlDatabase::%s( %s ): Task took %f seconds\n",
						ERROR_CONFIG::errorMethodName(),
						this->getTaskInfo().c_str(),
						double(duration)/stampsPerSecondD() );
		}

		return hasError;
	}

	// Default threadTaskWarningDuration() used by standardOnRunComplete()
	static uint64 threadTaskWarningDuration()
	{
		return THREAD_TASK_WARNING_DURATION;
	}

	// Returns a bit of information about the task, used by
	// standardOnRunComplete() to log error messages.
	virtual std::string getTaskInfo() const
	{
		return std::string();
	}
};

// -----------------------------------------------------------------------------
// Section: Support for running in transactions
// -----------------------------------------------------------------------------
/**
 *	Wrapper class for queries that uses data in MySqlThreadData. This
 * 	class can be passed to wrapInTransaction().
 */
template <class QUERY>
class ThreadDataQuery
{
	MySqlThreadData& 	threadData_;
	QUERY&				query_;

public:
	ThreadDataQuery( MySqlThreadData& threadData, QUERY& query ) :
		threadData_( threadData ), query_( query )
	{}

	void execute( MySql& connection )
	{
		query_.execute( connection, threadData_ );
	}

	void setExceptionStr( const char * str )
	{
		threadData_.exceptionStr = str;
		threadData_.isOK = false;
	}
};

/**
 *	Runs QUERY in a transaction. For queries that uses MySqlThreadData.
 */
template <class QUERY>
bool wrapInTransaction( MySql& connection, MySqlThreadData& threadData,
		QUERY& query )
{
	ThreadDataQuery< QUERY > tQuery( threadData, query );
	return wrapInTransaction( connection, tQuery );
}

/**
 *	Helper class to run a MySqlStatement. Objects of this class can be passed
 * 	to the wrapInTransaction() above.
 */
class SingleStatementQuery
{
	MySqlStatement& stmt_;
public:
	SingleStatementQuery( MySqlStatement& stmt ) : stmt_( stmt ) {}
	void execute( MySql& connection, MySqlThreadData& threadData )
	{
		connection.execute( stmt_ );
	}
};
/**
 *	Runs the statement in a transaction.
 */
inline bool wrapInTransaction( MySql& connection, MySqlThreadData& threadData,
		MySqlStatement& stmt )
{
	SingleStatementQuery query( stmt );
	return wrapInTransaction( connection, threadData, query );
}

#endif /*MYSQL_THREAD_HPP*/
