/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef NETWORK_APP_HPP
#define NETWORK_APP_HPP

#include "unit_test_lib/multi_proc_test_case.hpp"
#include "third_party/CppUnitLite2/src/CppUnitLite2.h"

#include "network/interfaces.hpp"
#include "network/nub.hpp"
#include "cstdmf/timestamp.hpp"

// -----------------------------------------------------------------------------
// Section: NetworkApp
// -----------------------------------------------------------------------------

/**
 *	This class is used as a common base class for network apps.
 */
class NetworkApp : public MultiProcTestCase::ChildProcess,
	public Mercury::TimerExpiryHandler
{
public:
	NetworkApp() : nub_(), timerID_( 0 ) {}

	Mercury::Nub & nub()	{ return nub_; }

	virtual int handleTimeout( Mercury::TimerID id, void * arg ) { return 0; }

	virtual int run()
	{
		// Guarantee a proper random seed in each test app
		srand( (int)::timestamp() );

		nub_.processUntilBreak();

		return 0;
	}

protected:
	void startTimer( uint tickRate, void * arg = 0 )
	{
		if (timerID_)
		{
			WARNING_MSG( "App::startTimer: Already has a timer\n" );
			this->stopTimer();
		}

		timerID_ = nub_.registerTimer( tickRate, this, arg );
	}

	void stopTimer()
	{
		if (timerID_)
		{
			nub_.cancelTimer( timerID_ );
			timerID_ = Mercury::TIMER_ID_NONE;
		}
	}

	virtual void stop()
	{
		nub_.breakProcessing();
	}


	Mercury::Nub nub_;
	Mercury::TimerID timerID_;
};


/**
 *  Assertions that should be used in NetworkApps inside methods that have a
 *  void return type.
 */
#define NETWORK_APP_FAIL( MESSAGE )											\
	{																		\
		this->fail( MESSAGE );												\
		return;																\
	}

#define NETWORK_APP_ASSERT( COND )											\
	if (!(COND))															\
	{																		\
		this->fail( #COND );												\
		return;																\
	}																		\

#define NETWORK_APP_ASSERT_WITH_MESSAGE( COND, MESSAGE )					\
	if (!(COND))															\
	{																		\
		this->fail( MESSAGE );												\
		return;																\
	}																		\


/**
 *  Assertions that should be used in NetworkApps inside methods that have a
 *  non-void return type.
 */
#define NETWORK_APP_FAIL_RET( MESSAGE, RET )								\
	{																		\
		this->fail( MESSAGE );												\
		return RET;															\
	}

#define NETWORK_APP_ASSERT_RET( COND, RET )									\
	if (!(COND))															\
	{																		\
		this->fail( #COND );												\
		return RET;															\
	}																		\

#define NETWORK_APP_ASSERT_WITH_MESSAGE_RET( COND, MESSAGE, RET )			\
	if (!(COND))															\
	{																		\
		this->fail( MESSAGE );												\
		return RET;															\
	}																		\


#endif // NETWORK_APP_HPP
