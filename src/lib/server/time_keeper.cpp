/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "time_keeper.hpp"
#include "cstdmf/debug.hpp"

#include "network/bundle.hpp"

DECLARE_DEBUG_COMPONENT( 0 )


// -----------------------------------------------------------------------------
// Section: TimeKeeper
// -----------------------------------------------------------------------------


/**
 *	Constructor
 */
TimeKeeper::TimeKeeper( Mercury::Nub & nub, Mercury::TimerID trackingTimerID,
		TimeStamp & tickCount, int idealTickFrequency,
		const Mercury::Address * masterAddress,
		const Mercury::InterfaceElement * masterRequest ) :
	nub_( nub ),
	trackingTimerID_( trackingTimerID ),
	tickCount_( tickCount ),
	idealTickFrequency_( double(idealTickFrequency) ),
	nominalIntervalStamps_( nub.timerIntervalTime( trackingTimerID ) ),
	syncCheckTimerID_( Mercury::TIMER_ID_NONE ),
	masterAddress_( masterAddress ),
	masterRequest_( masterRequest ),
	lastSyncRequestStamps_( 0 )
{
}

/**
 *	Destructor
 */
TimeKeeper::~TimeKeeper()
{
	if (syncCheckTimerID_ != 0) nub_.cancelTimer( syncCheckTimerID_ );
}


/**
 *	Input a game time reading from a master TimeKeeper.
 *	Do not call from the timer expiry callback.
 *	If the reading was unusable or way out then false is returned.
 */
bool TimeKeeper::inputMasterReading( double reading )
{
	uint64 now = timestamp();
	int64 roundTripStamps = now - lastSyncRequestStamps_;
	double roundTripTime = double( roundTripStamps ) / stampsPerSecondD();
	double readingNowValue = readingNow();

	// assume that half the round trip time ago, the master read from its
	// reading, and take the offset from that
	double offset = (reading + roundTripTime / 2) - readingNowValue;
	int64 offsetStamps = int64( offset * stampsPerSecond() );

	uint64 & intervalStamps = nub_.timerIntervalTime( trackingTimerID_ );
	uint64 increment = nominalIntervalStamps_ / 20;
/*
	TRACE_MSG( "TimeKeeper::inputMasterReading: \n"
			"reading = %.9f readingNowValue = %.9f\n "
			"offset = %.6fus, rtt=%.3fus\n",
		reading, readingNowValue,
		1000000 * offsetStamps / stampsPerSecondD(),
		1000000 * roundTripStamps / stampsPerSecondD() );
*/

	lastSyncRequestStamps_ = 0;

	int64 threshold = int64( nominalIntervalStamps_ / 20 +
		roundTripStamps / 2 );
	if (offsetStamps >  threshold )
	{
		// we are slow
		if (intervalStamps == nominalIntervalStamps_)
		{
			DEBUG_MSG( "TimeKeeper::inputMasterReading: "
				"shortening tick interval because we are slow "
				"by %.0fms\n",
				1000 * offsetStamps / stampsPerSecondD()
			);
			intervalStamps = nominalIntervalStamps_ - increment;
			this->scheduleSyncCheck();
		}
		else if (intervalStamps > nominalIntervalStamps_)
		{
			// we are now slow, and our interval was longer than normal
			DEBUG_MSG( "TimeKeeper:: we have slowed down enough, "
				"reverting to nominal tick interval\n" );
			intervalStamps = nominalIntervalStamps_;
		}
		else // if (intervalStamps < nominalIntervalStamps_)
		{
			// we are still slow, and our interval was shorter than normal
			// so we keep at it, and check after the next tick
			this->scheduleSyncCheck();
		}
	}
	else if (offsetStamps < -threshold)
	{
		// we are fast
		if (intervalStamps == nominalIntervalStamps_)
		{
			DEBUG_MSG( "TimeKeeper::inputMasterReading: "
				"lengthening tick interval because we are fast "
				"by %.0fms\n",
				-1000 * offsetStamps / stampsPerSecondD()
			);
			intervalStamps = nominalIntervalStamps_ + increment;
			this->scheduleSyncCheck();
		}
		else if (intervalStamps < nominalIntervalStamps_)
		{
			// we are now fast, and our interval was shorter than normal
			DEBUG_MSG( "TimeKeeper:: we sped up enough, "
					"reverting back to nominal tick interval\n" );
			intervalStamps = nominalIntervalStamps_;
		}
		else // if (intervalStamps > nominalIntervalStamps_)
		{
			// we are still fast, and our interval was longer than normal
			// so we keep at it, and check after the next tick
			this->scheduleSyncCheck();
		}
	}
	else
	{
		// we are within tolerance
		if (intervalStamps != nominalIntervalStamps_)
		{
			DEBUG_MSG( "TimeKeeper::inputMasterReading: "
				"reverting back to nominal tick interval\n" );
			intervalStamps = nominalIntervalStamps_;
		}
	}

	return true;
}

/**
 *	Schedules a synchronization check; a timer is set which calls
 *	synchronizeWithMaster().
 */
void TimeKeeper::scheduleSyncCheck()
{
	if (syncCheckTimerID_ != 0)
	{
		TRACE_MSG( "TimeKeeper::scheduleSyncCheck: existing timer ID\n" );
		return;
	}

	double deliveryIn = double(int64(nub_.timerDeliveryTime(
		trackingTimerID_ ) - timestamp()));
	double s = std::max( deliveryIn / stampsPerSecondD(), 0.001 );
	int callbackPeriod = int( s * 1000000.0) + 1000;
	syncCheckTimerID_ = nub_.registerCallback( callbackPeriod, this );
}


/**
 *	Return the reading of the time we are keeping at the last tick
 */
double TimeKeeper::readingAtLastTick() const
{
	return double(tickCount_) / idealTickFrequency_;
}

/**
 *	Return a reading of the time we are keeping right now
 */
double TimeKeeper::readingNow() const
{
	double ticksAtNext = double(tickCount_+1);
	double intervalStamps =
		double(nub_.timerIntervalTime( trackingTimerID_ ));

	uint64 stampsAtNext = nub_.timerDeliveryTime( trackingTimerID_ );
	int64 stampsSinceNext = int64(timestamp() - stampsAtNext);
	// stampsSinceNext will hopefully be negative :)
	double ticksSinceNext = double(stampsSinceNext) / intervalStamps;

	return (ticksAtNext + ticksSinceNext) / idealTickFrequency_;
}

/**
 *	Return the reading of the time we are keeping at the next tick
 */
double TimeKeeper::readingAtNextTick() const
{
	return double(tickCount_+1) / idealTickFrequency_;
}


/**
 *	This method synchronises the time maintained by this time keeper
 *	with that maintained by the given peer. A reply is not expected.
 *	The message is not sent reliably.
 */
void TimeKeeper::synchroniseWithPeer( const Mercury::Address & address,
	const Mercury::InterfaceElement & request )
{
	Mercury::Bundle b;
	b.startMessage( request );
	b << this->readingNow();
	nub_.send( address, b );
}

/**
 *	This method initiates a query with the master time keeper provided
 *	on constructor, in order to synchronise with its clock
 */
void TimeKeeper::synchroniseWithMaster()
{
	MF_ASSERT( masterAddress_ != NULL );
	MF_ASSERT( masterRequest_ != NULL );

	if ( *masterAddress_ == Mercury::Address::NONE )
	{
		WARNING_MSG( "TimeKeeper::synchroniseWithMaster: Skipping because "
				"master is not ready\n" );
		return;
	}

	if (lastSyncRequestStamps_)
	{
		WARNING_MSG( "TimeKeeper::synchroniseWithMaster: in progress\n" );
		return;
	}
	Mercury::Bundle b;
	b.startRequest( *masterRequest_, this );
	b << this->readingNow();
	nub_.send( *masterAddress_, b );

	lastSyncRequestStamps_ = timestamp();
}

/**
 *	This private method finds the offset of the given reading which was
 *	made at the given number of stamps, from what our internal number
 *	of stamps would have been when we would have given that reading.
 *	The result is positive iff our stamps would be (or were) less than the
 *	given stamps, i.e. iff our clock is running faster than the given one.
 */
int64 TimeKeeper::offsetOfReading( double reading, uint64 stampsAtReceiptExt )
{
	// figure out what we think the corresponding timestamp
	// for that game time should be if we were there internally
	double readingAtNextTick = this->readingAtNextTick();

	uint64 intervalTime = nub_.timerIntervalTime( trackingTimerID_ );
	uint64 deliveryTime = nub_.timerDeliveryTime( trackingTimerID_ );

	double stampsPerReadingUnit = double(intervalTime) * idealTickFrequency_;
	// above is like stamps per second, but scaled by adjusted intervalTime

	// At 'deliveryTime', we will be giving the reading 'readingAtNextTick'.
	// So now just subtract the difference in readings converted into
	// stamps, from that known deliveryTime (or add if we are very slow),
	// and we will know the stamps that we would have given 'reading' at.

	uint64 stampsAtReceiptInt = deliveryTime -
		uint64(int64((readingAtNextTick-reading) * stampsPerReadingUnit));
	return int64(stampsAtReceiptExt - stampsAtReceiptInt);
}


/**
 *	This is called when it is time to reset the interval of the tracking
 *	timer back to its nominal level.
 */
int TimeKeeper::handleTimeout( Mercury::TimerID id, void * arg )
{
	if (id == syncCheckTimerID_)
	{
		nub_.cancelTimer( syncCheckTimerID_ );
		syncCheckTimerID_ = 0;
		if (masterAddress_)
		{
			this->synchroniseWithMaster();
		}
		else
		{
			// we're the master
			uint64 & intervalStamps = nub_.timerIntervalTime(
				trackingTimerID_ );
			intervalStamps = nominalIntervalStamps_;
		}
		return 0;
	}
	return 0;
}


/**
 *	This is called when someone has selected us to be a ReplyMessageHandler.
 *	We assume that it was for a game time sync message and that the
 *	reply stream has one 'double' on it.
 */
void TimeKeeper::handleMessage( const Mercury::Address & source,
	Mercury::UnpackedMessageHeader & header, BinaryIStream & data, void * )
{
	if (header.length != sizeof(double))
	{
		ERROR_MSG( "TimeKeeper::handleMessage: "
			"Reply from %s expected to be just one 'double' but length is %d\n",
			(char*)source, header.length );
		return;
	}

	double reading;
	data >> reading;

	// we assume that this is a master reading
	if (!this->inputMasterReading( reading ))
	{
		// if it didn't like it then try again later
		if (masterAddress_ != NULL && masterRequest_ != NULL)
		{
			//DEBUG_MSG( "TimeKeeper::handleMessage: "
			//	"Master reading unusable or way out, will try again\n" );

			this->scheduleSyncCheck();
		}
	}
}

/**
 *	This is called when someone has selected us to be a ReplyMessageHandler.
 *	We don't do anything here except print out a warning.
 */
void TimeKeeper::handleException( const Mercury::NubException & exception,
		void * )
{
	if (exception.reason() == Mercury::REASON_TIMER_EXPIRED)
	{
		WARNING_MSG( "TimeKeeper::handleException: "
			"Reply to game time sync request timed out\n" );
	}
	else
	{
		ERROR_MSG( "TimeKeeper::handleException: %s\n",
				Mercury::reasonToString( exception.reason() ) );
	}

	if (syncCheckTimerID_ != 0)
	{
		nub_.cancelTimer( syncCheckTimerID_ );
		syncCheckTimerID_ = 0;
	}
	lastSyncRequestStamps_ = 0;
}

// time_keeper.cpp
