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

#include "cstdmf/time_queue.hpp"

class Handler : public TimeQueueHandler
{
protected:
	virtual void handleTimeout( TimeQueueId id, void * pUser )
	{
	}

	virtual void onRelease( TimeQueueId id, void * pUser )
	{
		delete this;
	}
};

TEST( Purge )
{
	std::vector< TimeQueueId > ids;

	TimeQueue timeQueue;

	const uint NUM_TIMERS = 50;

	for (uint i = 0; i < NUM_TIMERS; ++i)
	{
		ids.push_back( timeQueue.add( i, 0, new Handler, NULL ) );
	}

	CHECK( timeQueue.size() == NUM_TIMERS );

	// Cancel more than half
	uint numToCancel = 2 * NUM_TIMERS / 3;

	for (uint i = 0; i < numToCancel; ++i)
	{
		timeQueue.cancel( ids[i] );
	}

	// If more than half of the timers are cancelled, they should be purged
	// immediately.
	CHECK( timeQueue.size() < NUM_TIMERS/2 );
}

TEST( BadPurge )
{
	// Check the case where purge is called while a once-off timer is being
	// cancelled.
	TimeQueue timeQueue;
	timeQueue.add( 1, 0, new Handler, NULL );
	timeQueue.process( 2 );
}

// test_time_queue.cpp
