/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"
#include "py_callback.hpp"

#include <stack>
#include <list>

#ifndef MF_SERVER
// Not available on server yet, pending refactoring.

namespace Script
{
	/**
	 * Store a callback function that can tell caller the total time that
	 * game has been running. The application should call setTotalGameTimeFn
	 * to set this up.
	 */
	TotalGameTimeFn s_totalGameTimeFn = NULL;

	/**
	 * Set the callback function.
	 */
	void setTotalGameTimeFn( TotalGameTimeFn fn )
	{
		s_totalGameTimeFn = fn;
	}

	/**
	 * Get total amount of time that game has been running.
	 */
	double getTotalGameTime()
	{
		return s_totalGameTimeFn();
	}

	/**
	*	This union is used by the BigWorld client callback
	*	system. It provides a handle to a callback request.
	*/
	union TimerHandle
	{
		uint32 i32;
		struct
		{
			uint16 id;
			uint16 issueCount;
		};
	};

	typedef std::stack<TimerHandle>		TimerHandleStack;
	TimerHandleStack					gFreeTimerHandles;

	/**
	*	This structure is used by the BigWorld client callback
	*	system.  It records a single callback request.
	*/
	struct TimerRecord
	{
		/**
		*	This method returns whether or not the input record occurred later than
		*	this one.
		*
		*	@return True if input record is earlier (higher priority),
		*		false otherwise.
		*/
		bool operator <( const TimerRecord & b ) const
		{
			return b.time < this->time;
		}

		double		time;			///< The time of the record.
		PyObject	* function;		///< The function associated with the record.
		PyObject	* arguments;	///< The arguments associated with the record.
		const char	* source;		///< The source of this timer record
		TimerHandle	handle;			///< The handle issued for this callback.
	};


	typedef std::list<TimerRecord>	Timers;
	Timers	gTimers;

	/**
	*	Clears and releases all existing timers.
	*/
	void clearTimers()
	{
		// this has to be called at a different time
		// than fini, that's why it's a separate method
		for( Timers::iterator iTimer = gTimers.begin(); iTimer != gTimers.end(); iTimer++ )
		{
			Py_DECREF( iTimer->function );
			Py_DECREF( iTimer->arguments );
		}
		gTimers.clear();
	}

	/**
	*	This function calls any script timers which have expired by now
	*/
	void tick( double timeNow )
	{
		const int MAX_TIMER_CALLS_PER_FRAME = 1000;

		Timers  timersToCall;
		Timers::iterator iter = gTimers.begin();
		while ( iter != gTimers.end() ) 
		{
			if ( iter->time <= timeNow )
			{
				timersToCall.push_back( *iter );
				iter = gTimers.erase( iter );
			}
			else
			{
				++iter;
			}
		}

		// Using a reverse iterator, since the TimerRecord comparison operator causes
		// the sorted list to be in reverse order (earlier timers are later in the list).
		stable_sort( timersToCall.begin(), timersToCall.end() );
		int numExpired = 0;
		Timers::reverse_iterator revIter = timersToCall.rbegin();
		for ( ; revIter != timersToCall.rend() && numExpired < MAX_TIMER_CALLS_PER_FRAME; ++revIter )
		{
			TimerRecord& timer = *revIter;

			gFreeTimerHandles.push( timer.handle );

			Script::call( timer.function, timer.arguments, timer.source );
			// Script::call decrefs timer.function and timer.arguments for us

			numExpired++;
		}


		if (numExpired >= MAX_TIMER_CALLS_PER_FRAME)
		{
			// If there are too many to run this frame, put the remainder back into the main list.
			for ( ; revIter != timersToCall.rend(); ++revIter )
			{
				TimerRecord& timer = *revIter;
				gTimers.push_back( timer );
			}

			ERROR_MSG( "BigWorldClientScript::tick: Loop interrupted because"
				" too many timers (> %d) wanted to expire this frame!",
				numExpired );
		}
	}

	/**
	*	This function adds a script 'timer' to be called next tick
	*
	*	It is used by routines which want to make script calls but can't
	*	because they're in the middle of something scripts might mess up
	*	(like iterating over the scene to tick or draw it)
	*
	*	The optional age parameter specifies the age of the call,
	*	i.e. how far in the past it wanted to be made.
	*	Older calls are called back first.
	*
	*	@note: This function steals the references to both fn and args
	*/
	void callNextFrame( PyObject * fn, PyObject * args,
								const char * reason, double age )
	{
		TimerHandle handle;
		if(!gFreeTimerHandles.empty())
		{
			handle = gFreeTimerHandles.top();
			handle.issueCount++;
			gFreeTimerHandles.pop();
		}
		else
		{
			if (gTimers.size() >= USHRT_MAX)
			{
				PyErr_SetString( PyExc_TypeError, "callNextFrame: Callback handle overflow." );
				return;
			}
			handle.id = gTimers.size() + 1;
			handle.issueCount = 1;
		}

		TimerRecord newTR = { getTotalGameTime() - age, fn, args, reason, { handle.i32 } };
		gTimers.push_back( newTR );
	}
}

/*~ function BigWorld.callback
*  Registers a callback function to be called after a certain time,
*  but not before the next tick.
*  @param time A float describing the delay in seconds before function is
*  called.
*  @param function Function to call. This function must take 0 arguments.
*  @return int A handle that can be used to cancel the callback.
*/
/**
*	Registers a callback function to be called after a certain time,
*	 but not before the next tick. (If registered during a tick
*	 and it has expired then it will go off still - add a miniscule
*	 amount of time to BigWorld.time() to prevent this if unwanted)
*	Non-positive times are interpreted as offsets from the current time.
*/
static PyObject * py_callback( PyObject * args )
{
	double		time = 0.0;
	PyObject *	function = NULL;

	if (!PyArg_ParseTuple( args, "dO", &time, &function ) ||
		function == NULL || !PyCallable_Check( function ) )
	{
		PyErr_SetString( PyExc_TypeError, "BigWorld.callback: "
			"Argument parsing error." );
		return NULL;
	}

	if (time < 0) time = 0.0;

	time = Script::getTotalGameTime() + time;
	Py_INCREF( function );


	Script::TimerHandle handle;
	if(!Script::gFreeTimerHandles.empty())
	{
		handle = Script::gFreeTimerHandles.top();
		handle.issueCount++;
		Script::gFreeTimerHandles.pop();
	}
	else
	{
		if (Script::gTimers.size() >= USHRT_MAX)
		{
			PyErr_SetString( PyExc_TypeError, "py_callback: Callback handle overflow." );
			return NULL;
		}

		handle.id = Script::gTimers.size() + 1;
		handle.issueCount = 1;
	}


	Script::TimerRecord	newTR =
	{ time, function, PyTuple_New(0), "BigWorld Callback: ", { handle.i32 } };
	Script::gTimers.push_back( newTR );


	PyObject * pyId = PyInt_FromLong(handle.i32);
	return pyId;
}
PY_MODULE_FUNCTION( callback, BigWorld )


/*~ function BigWorld.cancelCallback
*  Cancels a previously registered callback.
*  @param int An integer handle identifying the callback to cancel.
*  @return None.
*/
/**
*	Cancels a previously registered callback.
*   Safe behavior is NOT guarantied when canceling an already executed
*   or canceled callback.
*/
static PyObject * py_cancelCallback( PyObject * args )
{
	Script::TimerHandle handle;

	if (!PyArg_ParseTuple( args, "i", &handle.i32 ) )
	{
		PyErr_SetString( PyExc_TypeError, "py_cancelCallback: Argument parsing error." );
		return NULL;
	}

	for( Script::Timers::iterator iTimer = Script::gTimers.begin(); 
		iTimer != Script::gTimers.end(); iTimer++ )
	{
		if( iTimer->handle.i32 == handle.i32 )
		{
			Py_DECREF( iTimer->function );
			Py_DECREF( iTimer->arguments );
			Script::gFreeTimerHandles.push( iTimer->handle );
			std::swap<>( *iTimer, Script::gTimers.back() );
			Script::gTimers.pop_back();
			break;
		}
	}

	Py_Return;
}
PY_MODULE_FUNCTION( cancelCallback, BigWorld )

#endif // MF_SERVER
