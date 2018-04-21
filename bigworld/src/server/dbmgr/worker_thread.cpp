/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "worker_thread.hpp"

#include "cstdmf/timestamp.hpp"

// ----------------------------------------------------------------------------
// WorkerThread
// ----------------------------------------------------------------------------
/**
 *	Constructor. Creates a separate thread that sits idle until work is
 *	assigned.
 */
WorkerThread::WorkerThread( WorkerThreadMgr& mgr )
	: threadData_(mgr), thread_( &WorkerThread::threadMainLoop, &threadData_ )
{}

/**
 *	Destructor. Wait for any outstanding work to finish and then destroys
 *	worker thread.

 */
WorkerThread::~WorkerThread()
{
	threadData_.readySema.pull();
	threadData_.pTask = 0;
	threadData_.workSema.push();
}

/**
 *	Assigns the worker thread some work to do. This method should not be
 *	called from more than one thread at a time.
 *
 *	@param	task	The work to do.
 *	@return	True if successfully assigned. False if worker thread is already
 *			busy doing some work.
 */
bool WorkerThread::doTaskImpl( ITask* pTask )
{
	bool isOK = threadData_.readySema.pullTry();
	if (isOK)
	{
		threadData_.pTask = pTask;
		threadData_.workSema.push();
	}

	return isOK;
}

/**
 *	Main loop for worker thread.
 */
void WorkerThread::threadMainLoop( void* arg )
{
	ThreadData*	pData = reinterpret_cast<ThreadData*>(arg);
	while (true)
	{
		pData->workSema.pull();	// wait for work.
		if (pData->pTask)
		{
			pData->pTask->run();
			ITask*	pCompletedTask = pData->pTask;
			pData->readySema.push();	// ready for work

			pData->mgr.onTaskComplete( *pCompletedTask );
		}
		else
			break;	// end thread
	}
}



// ----------------------------------------------------------------------------
// WorkerThreadMgr
// ----------------------------------------------------------------------------
/**
 *	Constructor. There should be only one WorkerThreadMgr for the thread
 *	that creates WorkerThread.
 *
 *	@param	nub	The nub that is running the main loop for the parent thread.
 */
WorkerThreadMgr::WorkerThreadMgr( Mercury::Nub& nub )
	: nub_(nub), completedTasks_(), completedTasksLock_()
{
	MF_ASSERT(!nub_.getOpportunisticPoller());
	nub_.setOpportunisticPoller(this);

	// Set timer for polling in addition so opportunistic polling.
	timerID_ = nub_.registerTimer( 1000, this );

	#ifdef WORKERTHREAD_SELFTEST
		this->selfTest();
	#endif
}

WorkerThreadMgr::~WorkerThreadMgr()
{
	MF_ASSERT(nub_.getOpportunisticPoller() == this);
	nub_.cancelTimer( timerID_ );

	nub_.setOpportunisticPoller( 0 );
}

/**
 *	This function processes runs the completion activity for all the tasks
 *	that have finished their work in the worker threads.
 *
 *	@return	The number of tasks processed.
 */
int WorkerThreadMgr::processCompletedTasks()
{
	CompletedTasks	completedTasks;

	// Grab copy of completed tasks to minimise lock time.
	completedTasksLock_.grab();
	completedTasks.swap( completedTasks_ );
	completedTasksLock_.give();

	// Run task completion activity.
	for ( CompletedTasks::const_iterator i = completedTasks.begin();
		i < completedTasks.end(); ++i )
	{
		(*i)->onRunComplete();
	}

	return int(completedTasks.size());
}

/**
 *	This function blocks until the specified number of tasks has completed.
 *
 *	@param	numTasks	Wait for this number of tasks to complete.
 *	@param	timeoutMicroSecs	The amount of time to wait before giving up.
 *			-1 means never give up.
 *	@return	True if specified number of tasks completed before timeout. False
 *			if timed out.
 */
bool WorkerThreadMgr::waitForTaskCompletion( int numTasks,
                                             int timeoutMicroSecs )
{
	int numCompleted = this->processCompletedTasks();
	if (numCompleted < numTasks)
	{
		uint64	endTimeStamp = timestamp() +
					( uint64(timeoutMicroSecs) * stampsPerSecond() ) / 1000000;
		do
		{
			threadSleep( 100 );
			numCompleted += this->processCompletedTasks();
		} while ( (numCompleted < numTasks) &&
			      ( (timeoutMicroSecs < 0) || (timestamp() < endTimeStamp) ) );
	}

	return (numCompleted >= numTasks);
}

/**
 *	Called by WorkerThread when its task is complete.
 */
void WorkerThreadMgr::onTaskComplete( WorkerThread::ITask& task )
{
	SimpleMutexHolder mutexHolder( completedTasksLock_ );
	// Currently in worker thread so remember task and let parent thread
	// process it at its leisure.
	completedTasks_.push_back( &task );
}

/**
 *	Nub::IOpportunisticPoller override
 */
void WorkerThreadMgr::poll()
{
	this->processCompletedTasks();
}

int WorkerThreadMgr::handleTimeout( Mercury::TimerID /*id*/, void * /*arg*/ )
{
	// Don't do anything because when timer goes off, it is considered
	// an "opportunity" for polling so poll() method will get called.
	return 0;
}


// ----------------------------------------------------------------------------
// WorkerThreadPool
// ----------------------------------------------------------------------------
/**
 *	Constructor. Starts threads idling and ready to go.
 *
 *	@param mgr	The managing object.
 *	@param numThreads The number of threads in this pool.
 */
WorkerThreadPool::WorkerThreadPool( WorkerThreadMgr& mgr, int numThreads )
	: mgr_(mgr), threads_(), freeThreads_()
{
	threads_.container.reserve( numThreads );
	for ( int i = 0; i < numThreads; ++i )
	{
		PoolItem* pItem = new PoolItem( mgr, *this );
		threads_.container.push_back( pItem );
	}

	freeThreads_ = threads_.container;
}

/**
 *	Assigns a task to a free worker thread.
 *
 *	@param	task	The task to assign.
 *	@return	True if OK. False if all threads are busy.
 */
bool WorkerThreadPool::doTask( WorkerThread::ITask& task )
{
	bool isOK = (freeThreads_.size() > 0);
	if (isOK)
	{
		PoolItem* pItem = freeThreads_.back();
		freeThreads_.pop_back();
		isOK = pItem->doTask(task);
		MF_ASSERT(isOK);
	}
	return isOK;
}

void WorkerThreadPool::doTaskInCurrentThread( WorkerThread::ITask& task )
{
	task.run();
	task.onRunComplete();
}

/**
 *	Wait for one task to complete.
 *
 *	@param	timeoutMicroSecs	The maximum amount of time to wait.
 *	-1 means wait forever.
 *	@return	True if didn't timeout.
 */
bool WorkerThreadPool::waitForOneTask( int timeoutMicroSecs )
{
	// Catering for multiple WorkerThreadPool in the same parent thread.
	int		numBusyTasksStart = this->getNumBusyThreads();
	uint64	endTimeStamp = timestamp() +
				( uint64(timeoutMicroSecs) * stampsPerSecond() ) / 1000000;
	bool isOK;
	do
	{
		int timeout = (timeoutMicroSecs >= 0) ?
						int(((endTimeStamp - timestamp())*1000000)/stampsPerSecond()) :
						-1;
		isOK = mgr_.waitForTaskCompletion( 1, timeout );
	} while (isOK && (numBusyTasksStart == this->getNumBusyThreads()));

	return isOK;
}

/**
 *	Wait for all outstanding tasks to complete.
 *
 *	@param	timeoutMicroSecs	The maximum amount of time to wait.
 *	-1 means wait forever.
 *	@return	True if didn't timeout.
 */
bool WorkerThreadPool::waitForAllTasks( int timeoutMicroSecs )
{
	// Catering for multiple WorkerThreadPool in the same parent thread.
	uint64	endTimeStamp = timestamp() +
				( uint64(timeoutMicroSecs) * stampsPerSecond() ) / 1000000;
	bool isOK = true;
	while (isOK && (this->getNumBusyThreads() > 0))
	{
		int timeout = (timeoutMicroSecs >= 0) ?
						int(((endTimeStamp - timestamp())*1000000)/stampsPerSecond()) :
						-1;
		isOK = mgr_.waitForTaskCompletion( this->getNumBusyThreads(), timeout );
	}

	return isOK;
}

/**
 *	Called by PoolItem when its task completes.
 */
void WorkerThreadPool::onTaskComplete( PoolItem& poolItem )
{
	freeThreads_.push_back( &poolItem );
}

// ----------------------------------------------------------------------------
// WorkerThreadPool::PoolItem
// ----------------------------------------------------------------------------
/**
 *	WorkerThread::ITask override.
 */
void WorkerThreadPool::PoolItem::run()
{
	pOrigTask_->run();
}

/**
 *	WorkerThread::ITask override.
 */
void WorkerThreadPool::PoolItem::onRunComplete()
{
	pool_.onTaskComplete( *this );
	pOrigTask_->onRunComplete();
}

#ifdef WORKERTHREAD_SELFTEST
// ----------------------------------------------------------------------------
// Test
// ----------------------------------------------------------------------------
class CountSheep : public WorkerThread::ITask
{
	int		id_;
	int		numSheep_;
	bool	isAsleep_;

	static int globalId_;

public:
	CountSheep( int numSheep ) :
	  id_(globalId_++), numSheep_(numSheep), isAsleep_(false)
	{
		printf( "CountSheep%d prepared\n", id_ );
	}

	bool isAsleep()	{	return isAsleep_;	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

int CountSheep::globalId_ = 1;

void CountSheep::run()
{
	printf( "CountSheep%d start\n", id_ );
	int	count = 1;
	while (count <= numSheep_)
	{
		threadSleep(1000000);
		printf( "CountSheep%d sheep %d\n", id_, count );
		++count;
	}
}

void CountSheep::onRunComplete()
{
	printf( "CountSheep%d complete\n", id_ );
	isAsleep_ = true;
}

void WorkerThreadMgr::selfTest()
{
	WorkerThreadPool	pool( *this, 3 );
	CountSheep			insomnia1( 10 );
	CountSheep			insomnia2( 5 );
	CountSheep			insomnia3( 20 );

	// Use up all threads.
	bool isOK = pool.doTask( insomnia1 );
	MF_ASSERT(isOK);
	isOK = pool.doTask( insomnia2 );
	MF_ASSERT(isOK);
	isOK = pool.doTask( insomnia3 );
	MF_ASSERT(isOK);
	MF_ASSERT(pool.getNumFreeThreads() == 0);

	// All threads should be busy now.
	CountSheep			insomnia4( 2 );
	isOK = pool.doTask( insomnia4 );
	MF_ASSERT(!isOK && !insomnia4.isAsleep());

	// Do task in main thread.
	pool.doTaskInCurrentThread( insomnia4 );
	MF_ASSERT(insomnia4.isAsleep());

	// Wait for 1 task.
	isOK = pool.waitForOneTask();
	MF_ASSERT( isOK && (pool.getNumFreeThreads() == 1) && insomnia2.isAsleep() );

	// Wait for all tasks.
	CountSheep	insomnia5( 5 );
	isOK = pool.doTask( insomnia5 );
	MF_ASSERT(isOK && (pool.getNumFreeThreads() == 0) );
	isOK = pool.waitForAllTasks();
	MF_ASSERT(isOK && (pool.getNumFreeThreads() == 3) &&
		insomnia1.isAsleep() && insomnia3.isAsleep() && insomnia5.isAsleep() );

	// Wait for 1 task with timeout
	CountSheep	insomnia6( 7 );
	isOK = pool.doTask( insomnia6 );
	MF_ASSERT(isOK);
	isOK = pool.waitForOneTask( 4000000 );
	MF_ASSERT( !isOK && (pool.getNumBusyThreads() == 1) );
	isOK = pool.waitForOneTask( 4000000 );
	MF_ASSERT( isOK && insomnia6.isAsleep() );

	// Wait for all tasks with timeout
	CountSheep	insomnia7( 5 );
	CountSheep	insomnia8( 10 );
	isOK = pool.doTask( insomnia7 );
	MF_ASSERT(isOK);
	isOK = pool.doTask( insomnia8 );
	MF_ASSERT(isOK);
	isOK = pool.waitForAllTasks( 1000000 );
	MF_ASSERT( !isOK && (pool.getNumBusyThreads() == 2) );
	isOK = pool.waitForAllTasks( 6000000 );
	MF_ASSERT( !isOK && (pool.getNumBusyThreads() == 1) );
	isOK = pool.waitForAllTasks( 5000000 );
	MF_ASSERT( isOK && (pool.getNumBusyThreads() == 0) );
}
#endif	// WORKERTHREAD_SELFTEST
