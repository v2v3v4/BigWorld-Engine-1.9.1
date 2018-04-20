/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BGTASK_MANAGER_HPP
#define BGTASK_MANAGER_HPP

#include <list>

#include "cstdmf/concurrency.hpp"
#include "cstdmf/debug.hpp"

class BgTaskManager;


/**
 *	This interface is used to implement tasks that will be run by the 
 *	BgTaskManager.
 */
class BackgroundTask : public SafeReferenceCount
{
public:
	/**
	 *	This method is called to perform a task in the background thread. Derived
	 *	classes will often add themselves back to the manager at the end of this
	 *	method by calling <code>mgr.addMainThreadTask( this )</code>. This allows
	 *	this object to complete in the main thread.
	 */
	virtual void doBackgroundTask( BgTaskManager & mgr ) = 0;
	virtual void doMainThreadTask( BgTaskManager & mgr ) {};

protected:
	virtual ~BackgroundTask() {};
};

typedef SmartPointer< BackgroundTask > BackgroundTaskPtr;

/**
 *	This class encapsulate a task that can be submitted to the background task
 *	manager for processing. The task function and callback function must be
 *	static methods.
 *
 *	This class is for backwards compatibility and should probably not be used
 *	in new code.
 */
class CStyleBackgroundTask : public BackgroundTask
{
public:
	typedef void (*FUNC_PTR)( void * );

	CStyleBackgroundTask( FUNC_PTR bgFunc, void * bgArg,
		FUNC_PTR fgFunc = NULL, void * fgArg = NULL );

	void doBackgroundTask( BgTaskManager & mgr );
	void doMainThreadTask( BgTaskManager & mgr );

private:
	FUNC_PTR bgFunc_;
	void * bgArg_;

	FUNC_PTR fgFunc_;
	void * fgArg_;
};


/**
 *	This class encapsulates a working thread execute given
 *	tasks.
 */
class BackgroundTaskThread : public SimpleThread
{
public:
	BackgroundTaskThread( BgTaskManager & mgr );

private:
	static void s_start( void * arg );
	void run();

	BgTaskManager & mgr_;
};


/**
 *	This class is used to help in the completion on a thread. It informs the
 *	manager that the thread has gone.
 */
class ThreadFinisher : public BackgroundTask
{
public:
	ThreadFinisher( BackgroundTaskThread * pThread ) : pThread_( pThread ) {}

	virtual void doBackgroundTask( BgTaskManager & mgr ) {};
	virtual void doMainThreadTask( BgTaskManager & mgr );

private:
	BackgroundTaskThread * pThread_;
};


/**
 *	This class defines a background task manager that manages a pool
 *	of working threads. BackgroundTask objects are added to be processed by a
 *	background thread and then, possibly by the main thread again.
 */
class BgTaskManager
{
public:

	enum 
	{ 
		MIN = 0,
		LOW = 32,
		MEDIUM = 64,
		DEFAULT = MEDIUM,
		HIGH = 96,
		MAX = 128
	};

	BgTaskManager();
	~BgTaskManager();

	void tick();

	void startThreads( int numThreads );

	void stopAll( bool discardPendingTasks = true, bool waitForThreads = true );

	void addBackgroundTask( BackgroundTaskPtr pTask, int priority = DEFAULT );
	void addMainThreadTask( BackgroundTaskPtr pTask );

	/**
	 *	This method returns the total number of running threads. That is threads
	 *	that have not told the main thread that they have stopped. This is always
	 *	no less than numUnstoppedThreads.
	 */
	int numRunningThreads() const { return numRunningThreads_; }

	/**
	 *	This method returns the number of threads that are running that are not
	 *	pending for deletion.
	 */
	int numUnstoppedThreads() const { return numUnstoppedThreads_; }

	static BgTaskManager & instance();
	static void fini();

	// Used by background tasks.
	void onThreadFinished( BackgroundTaskThread * pThread ); // In main thread
	BackgroundTaskPtr pullBackgroundTask(); // In background thread

private:
	static BgTaskManager		* s_instance_;

	class BackgroundTaskList
	{
	public:
		void push( BackgroundTaskPtr pTask, int priority = DEFAULT );
		BackgroundTaskPtr pull();
		void clear();

	private:
		typedef std::list< std::pair< int, BackgroundTaskPtr > > List;
		List list_;
		SimpleMutex mutex_;
		SimpleSemaphore semaphore_;
	};

	BackgroundTaskList	bgTaskList_;

	typedef std::list< BackgroundTaskPtr > ForegroundTaskList;
	ForegroundTaskList fgTaskList_;
	SimpleMutex fgTaskListMutex_;

	int numRunningThreads_;
	int numUnstoppedThreads_;
};

#endif // BGTASK_MANAGER_HPP
