/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "mysql_thread.hpp"
#include "mysql_notprepared.hpp"
#include "db_config.hpp"

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT( 0 );

// -----------------------------------------------------------------------------
// Section: struct SpaceDataBinding
// -----------------------------------------------------------------------------
// Adds SpaceDataBinding to bindings.
MySqlBindings& operator<<( MySqlBindings& bindings, SpaceDataBinding& sdb )
{
	bindings << sdb.spaceKey;
	bindings << sdb.dataKey;
	bindings << sdb.data;

	return bindings;
}

/**
 *	This static method reads from the stream containing space data and inserts
 * 	the data into the database. Returns the number spaces in the stream.
 */
uint32 writeSpaceDataStreamToDB( MySql& connection,
		SpaceID& spaceIDBinding, MySqlStatement& insertSpaceIDStmt,
		SpaceDataBinding& spaceDataBinding, MySqlStatement& insertSpaceDataStmt,
		BinaryIStream& stream )
{
	uint32 numSpaces;
	stream >> numSpaces;

	for (uint32 spaceIndex = 0; spaceIndex < numSpaces; ++spaceIndex)
	{
		stream >> spaceIDBinding;

		connection.execute( insertSpaceIDStmt );

		uint32 numData;
		stream >> numData;

		for (uint32 dataIndex = 0; dataIndex < numData; ++dataIndex)
		{
			stream >> spaceDataBinding.spaceKey;
			stream >> spaceDataBinding.dataKey;
			stream >> spaceDataBinding.data;

			connection.execute( insertSpaceDataStmt );
		}
	}

	return numSpaces;
}

// -----------------------------------------------------------------------------
// Section: class MySQL::SecondaryDBOps
// -----------------------------------------------------------------------------
namespace MySQL
{

/**
 * 	Used for generating error information.
 *
 * 	@return	A string representation of this object.
 */
std::string MySQL::SecondaryDBOps::DbEntryBuffer::getAsString() const
{
	in_addr	addr;
	addr.s_addr = htonl( ip );
	char	addrStr[INET_ADDRSTRLEN];
	inet_ntop( AF_INET, &addr, addrStr, sizeof( addrStr ) );

	std::stringstream 	ss;
	ss << "addr=" << addrStr << ':' << port << ", appID=" << appID
			<< ", location=" << location.getString();
	return ss.str();
}

/**
 * 	Creates the bigworldSecondaryDatabases table.
 */
void SecondaryDBOps::createTable( MySql& connection )
{
	// Must fit in BLOB column
	MF_ASSERT( MAX_SECONDARY_DB_LOCATION_LENGTH < 1<<16 );

	connection.execute( "CREATE TABLE IF NOT EXISTS "
			"bigworldSecondaryDatabases (ip INT UNSIGNED NOT NULL, "
			"port SMALLINT UNSIGNED NOT NULL, appID INT NOT NULL, "
			"location BLOB NOT NULL, INDEX addr (ip, port, appID)) "
			"ENGINE="MYSQL_ENGINE_TYPE );
}

/**
 * 	Returns the statement for adding an entry into
 * 	bigworldSecondaryDatabases.
 */
MySqlStatement& SecondaryDBOps::addStmt( MySql& connection )
{
	if (!pAddStmt_.get())
	{
		pAddStmt_.reset( new MySqlStatement( connection,
				"INSERT INTO bigworldSecondaryDatabases "
				"(ip, port, appID, location) VALUES (?,?,?,?)" ) );
		MySqlBindings bindings;
		entryBuf_.addToBindings( bindings );
		pAddStmt_->bindParams( bindings );
	}

	return *pAddStmt_;
}

}	// namespace MySQL


// -----------------------------------------------------------------------------
// Section: class MySqlThreadData
// -----------------------------------------------------------------------------
/**
 *	Constructor. Create prepared statements.
 */
MySqlThreadData::MySqlThreadData(  const DBConfig::Connection& connInfo,
								   int maxSpaceDataSize,
                                   const EntityDefs& entityDefs,
                                   const char * tblNamePrefix )
	: connection( connInfo ),
	typeMapping( connection, entityDefs, tblNamePrefix ),
	startTimestamp(0),
	putIDStatement_( new MySqlStatement( connection,
							"INSERT INTO bigworldUsedIDs (id) VALUES (?)" ) ),
	// The following two do not work as prepared statements. It
	// appears that the LIMIT argument cannot be prepared.
	getIDsStatement_( new MySqlUnPrep::Statement( connection,
							"SELECT id FROM bigworldUsedIDs LIMIT ? FOR UPDATE" ) ),
	delIDsStatement_( new MySqlUnPrep::Statement( connection,
							"DELETE FROM bigworldUsedIDs LIMIT ?" ) ),
	incIDStatement_( new MySqlStatement( connection,
							"UPDATE bigworldNewID SET id=id+?" ) ),
	getIDStatement_( new MySqlStatement( connection,
							"SELECT id FROM bigworldNewID LIMIT 1" ) ),
	setGameTimeStatement_( new MySqlStatement( connection,
								"UPDATE bigworldGameTime SET time=?" ) ),
	boundSpaceData_( maxSpaceDataSize ),
	writeSpaceStatement_( new MySqlStatement( connection,
							"REPLACE INTO bigworldSpaces (id) "
							"VALUES (?)" ) ),
	writeSpaceDataStatement_( new MySqlStatement( connection,
							"INSERT INTO bigworldSpaceData "
							"(id, spaceEntryID, entryKey, data) "
							"VALUES (?, ?, ?, ?)" ) ),
	delSpaceIDsStatement_( new MySqlStatement( connection,
							"DELETE from bigworldSpaces" ) ),
	delSpaceDataStatement_( new MySqlStatement( connection,
							"DELETE from bigworldSpaceData" ) ),
	ekey( 0, 0 )
{
	MySqlBindings b;

	b << boundID_;
	putIDStatement_->bindParams( b );
	getIDStatement_->bindResult( b );

	b.clear();
	b << boundLimit_;
	incIDStatement_->bindParams( b );

	b.clear();
	b << gameTime_;
	setGameTimeStatement_->bindParams( b );

	b.clear();
	b << boundSpaceID_;
	writeSpaceStatement_->bindParams( b );

	b.clear();
	b << boundSpaceID_;
	b << boundSpaceData_;
	writeSpaceDataStatement_->bindParams( b );

	// Do unprepared bindings.
	MySqlUnPrep::Bindings b2;
	b2 << boundID_;
	getIDsStatement_->bindResult( b2 );

	b2.clear();
	b2 << boundLimit_;
	getIDsStatement_->bindParams( b2 );
	delIDsStatement_->bindParams( b2 );
}


// -----------------------------------------------------------------------------
// Section: class PingTask
// -----------------------------------------------------------------------------
/**
 *	This class helps to keep the connection to MySQL server alive.
 */
class PingTask : public WorkerThread::ITask
{
	MySqlThreadResPool&	owner_;
	MySqlThreadData& 	threadData_;
	bool				pingOK_;

public:
	PingTask( MySqlThreadResPool& owner ) :
		owner_( owner ), threadData_( owner.acquireThreadDataAlways() ),
		pingOK_( true )
	{
		owner_.startThreadTaskTiming( threadData_ );
	}

	virtual ~PingTask()
	{
		uint64 duration = owner_.stopThreadTaskTiming( threadData_ );
		if (duration > THREAD_TASK_WARNING_DURATION)
			WARNING_MSG( "PingTask took %f seconds\n",
						double(duration) / stampsPerSecondD() );
		owner_.releaseThreadDataAlways( threadData_ );
	}

	// WorkerThread::ITask overrides
	virtual void run()
	{
		pingOK_ = threadData_.connection.ping();
	}
	virtual void onRunComplete()
	{
		if (!pingOK_)
		{
			MySqlError error(threadData_.connection.get());
			ERROR_MSG( "MySQL connection ping failed: %s\n",
						error.what() );
			threadData_.connection.onFatalError( error );
		}
		delete this;
	}
};


// -----------------------------------------------------------------------------
// Section: class MySqlThreadResPool
// -----------------------------------------------------------------------------
/**
 *	Constructor. Start worker threads and connect to MySQL database. Also creates
 *	MySqlThreadData for each thread.
 */
MySqlThreadResPool::MySqlThreadResPool( WorkerThreadMgr& threadMgr,
										Mercury::Nub& nub,
									    int numConnections,
										int maxSpaceDataSize,
								        const DBConfig::Connection& connInfo,
                                        const EntityDefs& entityDefs,
                                        bool shouldLockDB )
	: threadPool_( threadMgr, numConnections - 1 ), threadDataPool_(),
	freeThreadData_(),
	mainThreadData_( connInfo, maxSpaceDataSize, entityDefs ),
	dbLock_( mainThreadData_.connection, connInfo.generateLockName(),
			shouldLockDB ),
	nub_( nub ),
	opCount_( 0 ), opDurationTotal_( 0 ), resetTimeStamp_( timestamp() )
{
	int numThreads = threadPool_.getNumFreeThreads();
	MF_ASSERT( (numThreads == 0) || ((numThreads > 0) && mysql_thread_safe()) );

	// Do thread specific MySQL initialisation.
	struct InitMySqlTask : public WorkerThread::ITask
	{
		virtual void run()	{	mysql_thread_init();	}
		virtual void onRunComplete() {}
	} initMySqlTask;
	while ( threadPool_.doTask( initMySqlTask ) ) {}
	threadPool_.waitForAllTasks();

	// Create data structures for use in worker threads.
	threadDataPool_.container.reserve( numThreads );
	for ( int i = 0; i < numThreads; ++i )
	{
		threadDataPool_.container.push_back(
			new MySqlThreadData( connInfo, maxSpaceDataSize, entityDefs ) );
	}

	freeThreadData_ = threadDataPool_.container;

	// Set 30-minute keep alive timer to keep connections alive.
	keepAliveTimerID_ = nub_.registerTimer( 1800000000, this );
}

/**
 *	Mercury::TimerExpiryHandler overrides.
 */
int MySqlThreadResPool::handleTimeout( Mercury::TimerID id, void * arg )
{
	MF_ASSERT( id == keepAliveTimerID_ );
	// Ping all free threads. Busy threads are already doing something so
	// doesn't need pinging to keep alive.
	ThreadDataVector::size_type numFreeConnections = freeThreadData_.size();
	// TRACE_MSG( "Pinging %lu MySQL connections to keep them alive\n",
	//			numFreeConnections + 1 );	// + 1 for main thread.
	for ( ThreadDataVector::size_type i = 0; i < numFreeConnections; ++i )
	{
		PingTask* pTask = new PingTask( *this );
		this->threadPool().doTask( *pTask );
	}
	if (!mainThreadData_.connection.ping())
		ERROR_MSG( "MySQL connection ping failed: %s\n",
					mainThreadData_.connection.getLastError() );

	return 0;
}

MySqlThreadResPool::~MySqlThreadResPool()
{
	nub_.cancelTimer( keepAliveTimerID_ );

	threadPool_.waitForAllTasks();
	// Do thread specific MySQL clean up.
	struct CleanupMySqlTask : public WorkerThread::ITask
	{
		virtual void run()	{	mysql_thread_end();	}
		virtual void onRunComplete() {}
	} cleanupMySqlTask;
	while ( threadPool_.doTask( cleanupMySqlTask ) ) {}
	threadPool_.waitForAllTasks();
}

/**
 *	Gets a free MySqlThreadData from the pool. The returned MySqlThreadData
 *	is now "busy".
 *
 *	@return	Free MySqlThreadData. 0 if all MySqlThreadData are "busy".
 */
inline MySqlThreadData* MySqlThreadResPool::acquireThreadDataInternal()
{
	MySqlThreadData* pThreadData = 0;
	if (freeThreadData_.size() > 0)
	{
		pThreadData = freeThreadData_.back();
		freeThreadData_.pop_back();
	}
	return pThreadData;
}

/**
 *	Gets a free MySqlThreadData from the pool. MySqlThreadData is now "busy".
 *
 *	@return	Free MySqlThreadData. 0 if all MySqlThreadData are "busy".
 */
MySqlThreadData* MySqlThreadResPool::acquireThreadData(int timeoutMicroSeconds)
{
	MySqlThreadData* pThreadData = this->acquireThreadDataInternal();
	if (!pThreadData && (timeoutMicroSeconds != 0))
	{
		threadPool_.waitForOneTask(timeoutMicroSeconds);
		pThreadData = this->acquireThreadDataInternal();
	}

	return pThreadData;
}

/**
 *	Puts a "busy" MySqlThreadData back into the pool.
 */
void MySqlThreadResPool::releaseThreadData( MySqlThreadData& threadData )
{
	freeThreadData_.push_back( &threadData );
	MF_ASSERT( freeThreadData_.size() <= threadDataPool_.container.size() );

	// Apply pending thread data jobs.
	for ( ThreadDataProcJobs::iterator it = threadDataJobs_.begin();
			it != threadDataJobs_.end();  )
	{
		ThreadDataProcJobs::iterator cur = it;
		++it;

		this->applyThreadDataJob( threadData, *cur );
		if (cur->unprocessedItems.size() == 0)
		{
			cur->processor.done();
			threadDataJobs_.erase( cur );
		}
	}
}

/**
 *	Gets a free MySqlThreadData from the pool, or return the main thread's
 * 	MySqlThreadData if none is free - but only if the total number of threads
 * 	is less than mainThreadThreshold. If there are no free MySqlThreadData and
 * 	the total number of threads is more than mainThreadThreshold, then this
 * 	function blocks until a thread becomes free.
 *
 *	@return	Free MySqlThreadData. 0 if all MySqlThreadData are "busy".
 */
MySqlThreadData& MySqlThreadResPool::acquireThreadDataAlways( int
		mainThreadThreshold )
{
	MySqlThreadData* pThreadData = this->acquireThreadDataInternal();
	if (!pThreadData)
	{
		if (this->getNumConnections() <= mainThreadThreshold)
		{
			// When total number of threads is small, then we want to re-use the
			// main thread to do work.
			pThreadData = &mainThreadData_;
		}
		else
		{
			// When total number of threads is large, then we want to leave the
			// main thread free to hand out work to worker threads..
			threadPool_.waitForOneTask();
			pThreadData = this->acquireThreadDataInternal();
			MF_ASSERT( pThreadData );
		}
	}
	return *pThreadData;
}

/**
 *	Releases a MySqlThreadData acquired via acquireThreadDataAlways().
 */
void MySqlThreadResPool::releaseThreadDataAlways( MySqlThreadData& threadData )
{
	if (!this->isMainThreadData( threadData ))
	{
		this->releaseThreadData( threadData );
	}
}

/**
 *	This function submits the task to one of our threads. To do so you must've
 * 	already acquired one MySqlThreadData.
 */
void MySqlThreadResPool::doTask( WorkerThread::ITask& task,
		MySqlThreadData& threadData )
{
	if (!threadData.connection.hasFatalError())
	{
		if (this->isMainThreadData( threadData ))
		{
			WorkerThreadPool::doTaskInCurrentThread( task );
		}
		else
		{
			MF_VERIFY( threadPool().doTask( task ) );
		}
	}
	else
	{
		task.onRunComplete();
	}
}

/**
 *	This function applies processor to all the MySqlThreadData in our list.
 * 	For MySqlThreadData objects currently "busy" i.e. executing in another
 * 	thread, we wait till they are returned to us before applying processor.
 * 	When this function returns, you cannot assume that all MySqlThreadData has
 * 	processor applied to them. processor will be applied "as soon as possible".
 */
void MySqlThreadResPool::applyToAllThreadDatas( IThreadDataProcessor& processor )
{
	threadDataJobs_.push_back( ThreadDataProcJob( processor,
								threadDataPool_.container ) );

	// Apply processor to all free threads now.
	ThreadDataProcJob& job = threadDataJobs_.back();
	for ( ThreadDataVector::iterator it = freeThreadData_.begin();
			it != freeThreadData_.end(); ++it )
	{
		this->applyThreadDataJob( **it, job );
	}
	// Apply to main thread.
	processor.process( mainThreadData_ );

	if (job.unprocessedItems.size() == 0)
	{
		processor.done();
		threadDataJobs_.pop_back();
	}
}

/**
 *	Applies the processor to the MySqlThreadData if it is not already processed.
 * 	Removed threadData from the unprocessed list.
 */
void MySqlThreadResPool::applyThreadDataJob( MySqlThreadData& threadData,
	ThreadDataProcJob& job )
{
	ThreadDataVector::iterator pFound =
			find( job.unprocessedItems.begin(), job.unprocessedItems.end(),
					&threadData );
	if (pFound != job.unprocessedItems.end())
	{
		job.processor.process( **pFound );
		job.unprocessedItems.erase( pFound );
	}
}

/**
 *	This function is called to mark the start of a thread task (represented by
 * 	its	MySqlThreadData).
 */
void MySqlThreadResPool::startThreadTaskTiming( MySqlThreadData& threadData )
{
	threadData.startTimestamp = timestamp();
	// Reset our counter every 5 seconds (to catch transient behaviour)
	if ((threadData.startTimestamp - resetTimeStamp_) > THREAD_TASK_TIMING_RESET_DURATION)
	{
		resetTimeStamp_ = threadData.startTimestamp;
		opDurationTotal_ = 0;
		opCount_ = 0;
	}
}

/**
 *	This function is called to mark the end of a thread task (represented by
 * 	its	MySqlThreadData).
 *
 * 	Returns the duration which this thread task took, in number of CPU
 * 	clock cycles (i.e. same unit as the timestamp() function).
 */
uint64 MySqlThreadResPool::stopThreadTaskTiming( MySqlThreadData& threadData )
{
	uint64	opDuration = timestamp() - threadData.startTimestamp;
	opDurationTotal_ += opDuration;
	++opCount_;
	threadData.startTimestamp = 0;

	return opDuration;
}

/**
 *	Gets the largest elapsed time of any thread that is currently busy
 *	(in seconds)
 */
double MySqlThreadResPool::getBusyThreadsMaxElapsedSecs() const
{
	uint64 maxElapsed = 0;
	uint64 curTimestamp = timestamp();
	for ( ThreadDataVector::const_iterator i = threadDataPool_.container.begin();
			i != threadDataPool_.container.end(); ++i )
	{
		if ((*i)->startTimestamp > 0)	// busy thread
		{
			uint64 elapsedTimestamp = curTimestamp - (*i)->startTimestamp;
		 	if (elapsedTimestamp > maxElapsed)
		 		maxElapsed = elapsedTimestamp;
		}
	}
	return double(maxElapsed)/stampsPerSecondD();
}

/**
 *	Gets the number of operations per second. Only completed operations are
 *  counted.
 */
double MySqlThreadResPool::getOpCountPerSec() const
{
	return double(opCount_)/(double(timestamp() - resetTimeStamp_)/stampsPerSecondD());
}

/**
 *	Gets the average duration of an operation (in seconds). Only completed
 * 	operations are counted.
 */
double MySqlThreadResPool::getAvgOpDuration() const
{
	return (opCount_ > 0)
				? double(opDurationTotal_/opCount_)/stampsPerSecondD() : 0;
}
