/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TIME_KEEPER_HPP
#define TIME_KEEPER_HPP

#include "network/nub.hpp"

/**
 *	This class keeps track of tick times and makes sure they are synchronised
 *	with clocks running in other other places around the system.
 */
class TimeKeeper : public Mercury::TimerExpiryHandler,
	public Mercury::ReplyMessageHandler
{
public:
	TimeKeeper( Mercury::Nub & nub, Mercury::TimerID trackingTimerID,
			TimeStamp & tickCount, int idealTickFrequency,
			const Mercury::Address * masterAddress = NULL,
			const Mercury::InterfaceElement * masterRequest = NULL );
	virtual ~TimeKeeper();

	bool inputMasterReading( double reading );

	double readingAtLastTick() const;
	double readingNow() const;
	double readingAtNextTick() const;

	void synchroniseWithPeer( const Mercury::Address & address,
			const Mercury::InterfaceElement & request );
	void synchroniseWithMaster();

private:
	int64 offsetOfReading( double reading, uint64 stampsAtReceiptExt );

	void scheduleSyncCheck();

	virtual int handleTimeout( Mercury::TimerID id, void * arg );

	virtual void handleMessage( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data, void * );
	virtual void handleException( const Mercury::NubException &, void * );


	Mercury::Nub & nub_;
	Mercury::TimerID trackingTimerID_;

	TimeStamp & tickCount_;
	double idealTickFrequency_;

	uint64 nominalIntervalStamps_;
	Mercury::TimerID syncCheckTimerID_;

	const Mercury::Address			* masterAddress_;
	const Mercury::InterfaceElement	* masterRequest_;

	uint64 lastSyncRequestStamps_;
};


#endif // TIME_KEEPER_HPP
