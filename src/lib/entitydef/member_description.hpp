/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MEMBER_DESCRIPTION_HPP
#define MEMBER_DESCRIPTION_HPP


/**
 *	This class is a base class for MethodDescription and DataDescription. It is
 *	used to store statistics about these instances.
 */
class MemberDescription
{
public:
	MemberDescription();

#if ENABLE_WATCHERS
	static WatcherPtr pWatcher();

	void countSentToOwnClient( int bytes ) const
	{
		sentToOwnClient_++;
		bytesSentToOwnClient_ += bytes;
	}

	void countSentToOtherClients( int bytes ) const
	{
		sentToOtherClients_++;
		bytesSentToOtherClients_ += bytes;
	}

	void countAddedToHistoryQueue( int bytes ) const
	{
		addedToHistoryQueue_++;
		bytesAddedToHistoryQueue_ += bytes;
	}

	void countSentToGhosts( int bytes ) const
	{
		sentToGhosts_++;
		bytesSentToGhosts_ += bytes;
	}

	void countSentToBase( int bytes ) const
	{
		sentToBase_++;
		bytesSentToBase_ += bytes;
	}

	void countReceived( int bytes ) const
	{
		received_++;
		bytesReceived_ += bytes;
	}

private:
	mutable uint32 sentToOwnClient_;
	mutable uint32 sentToOtherClients_;
	mutable uint32 addedToHistoryQueue_;
	mutable uint32 sentToGhosts_;
	mutable uint32 sentToBase_;
	mutable uint32 received_;
	mutable uint32 bytesSentToOwnClient_;
	mutable uint32 bytesSentToOtherClients_;
	mutable uint32 bytesAddedToHistoryQueue_;
	mutable uint32 bytesSentToGhosts_;
	mutable uint32 bytesSentToBase_;
	mutable uint32 bytesReceived_;
#endif // ENABLE_WATCHERS
};

#endif // MEMBER_DESCRIPTION_HPP
