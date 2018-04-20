/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "bundle.hpp"
#include "nub.hpp"
#include "packet_filter.hpp"
#include "misc.hpp"
#include "message_filter.hpp"

#include "cstdmf/pool_allocator.hpp"

#include <list>

namespace Mercury
{

class Channel;
class PacketFilter;
class ReliableOrder;

/**
 *	@internal
 *	A template class to wrap a circular array of various sizes,
 *	as long as the size is a power of two.
 */
template <class T> class CircularArray
{
public:
	typedef CircularArray<T> OurType;

	CircularArray( uint size ) : data_( new T[size] ), mask_( size-1 ) { }
	~CircularArray()	{ delete [] data_; }

	uint size() const	{ return mask_+1; }

	const T & operator[]( uint n ) const	{ return data_[n&mask_]; }
	T & operator[]( uint n )				{ return data_[n&mask_]; }

private:
	CircularArray( const OurType & other );
	OurType & operator=( const OurType & other );

	T * data_;
	uint mask_;
};


/**
 *  A functor used to resolve ChannelIDs to Channels.  Used when a packet is
 *  received with FLAG_INDEXED_CHANNEL to figure out which channel to deliver it
 *  to.
 */
class ChannelFinder
{
public:
	virtual ~ChannelFinder() {};

	/**
	 *  Resolve the provided id to a Channel.  This will be called when an
	 *  indexed channel packet is received, before any messages are processed, so
	 *  this function should also set any context necessary for processing the
	 *  messages on the packet.
	 *
	 *  Callers should pass the bool parameter in as 'false', and the
	 *  ChannelFinder should set it to true if the ChannelFinder has dealt with
	 *  the packet and it should not be processed any further.
	 *
	 *  Should return NULL if the id cannot be resolved to a Channel.
	 */
	virtual Channel* find( ChannelID id, const Packet * pPacket,
		bool & rHasBeenHandled ) = 0;
};


/**
 *	Channels are used to indicate regular communication channels between two
 *	nubs. The nub can use these channels to optimise its reliability algorithms.
 *
 *	@note Any time you call 'bundle' you may get a different bundle to the one
 *	you got last time, because the Channel decided that the bundle was full
 *	enough to send. This does not occur on high latency channels (or else
 *	tracking numbers would get very confusing).
 *
 *	@note If you use more than one Channel on the same address, they share the
 *	same bundle. This means that:
 *
 *	@li Messages (and message sequences where used) must be complete between
 *		calls to 'bundle' (necessary due to note above anyway)
 *
 *	@li Each channel must say send before the bundle is actually sent.
 *
 *	@li Bundle tracking does not work with multiple channels; only the last
 *		Channel to call 'send' receives a non-zero tracking number (or possibly
 *		none if deleting a Channel causes it to be sent), and only the first
 *		Channel on that address receives the 'bundleLost' call.
 *
 * 	@ingroup mercury
 */
class Channel : public TimerExpiryHandler, public ReferenceCount
{
public:
	/**
	 *	The traits of a channel are used to decide the reliablity method.
	 *	There are two types of channels that we handle. The first is a
	 *	channel from server to server. These channels are low latency,
	 *	high bandwidth, and low loss. The second is a channel from client
	 *	to server, which is high latency, low bandwidth, and high loss.
	 *	Since bandwidth is scarce on client/server channels, only reliable
	 *	data is resent on these channels. Unreliable data is stripped from
	 *	dropped packets and discarded.
	 */
	enum Traits
	{
		/// This describes the properties of channel from server to server.
		INTERNAL = 0,

		/// This describes the properties of a channel from client to server.
		EXTERNAL = 1,
	};

	typedef void (*SendWindowCallback)( const Channel & );

	Channel( Nub & nub, const Address & address, Traits traits,
		float minInactivityResendDelay = 1.0,
		PacketFilterPtr pFilter = NULL, ChannelID id = CHANNEL_ID_NULL );

	static Channel * get( Nub & nub, const Address & address );

private:
	virtual ~Channel();

public:
	static void staticInit();

	void condemn();
	bool isCondemned() const { return isCondemned_; }

	void destroy();
	bool isDestroyed() const { return isDestroyed_; }

	bool isDead() const
	{
		return this->isCondemned() || this->isDestroyed();
	}

	void initFromStream( BinaryIStream & data, const Address & addr );
	void addToStream( BinaryOStream & data );

	Nub & nub()				{ return *pNub_; }

	INLINE const Mercury::Address & addr() const;
	void addr( const Mercury::Address & addr );

	Bundle & bundle();
	const Bundle & bundle() const;
	bool hasUnsentData() const;

	void send( Bundle * pBundle = NULL );
	void delayedSend();

	void sendIfIdle();

	void sendEvery( int microseconds );

	void reset( const Address & newAddr, bool warnOnDiscard = true );

	void configureFrom( const Channel & other );

	void switchNub( Nub * pDestNub );

	void startInactivityDetection( float inactivityPeriod,
			float checkPeriod = 1.f );

	uint64 lastReceivedTime() const		{ return lastReceivedTime_; }

	int windowSize() const;
	// sending:
	bool hasPacketBeenAcked( SeqNum seq ) const;
	// TODO: Remove this
	int earliestUnackedPacketAge() const	{ return this->sendWindowUsage(); }
	int latestAckedPacketAge() const;

	PacketFilterPtr pFilter() const { return pFilter_; }
	void pFilter(PacketFilterPtr pFilter) { pFilter_ = pFilter; }

	bool isIrregular() const	{ return isIrregular_; }
	void isIrregular( bool isIrregular );

	bool hasRemoteFailed() const { return hasRemoteFailed_; }
	void hasRemoteFailed( bool v );

	bool addResendTimer( SeqNum seq, Packet * p,
		const ReliableOrder * roBeg, const ReliableOrder * roEnd );
	bool delResendTimer( SeqNum seq );
	void checkResendTimers();
	void resend( SeqNum seq );
	uint64 roundTripTime() const { return roundTripTime_; }
	double roundTripTimeInSeconds() const
		{ return roundTripTime_/::stampsPerSecondD(); }

	std::pair< Packet*, bool > queueAckForPacket(
		Packet * p, SeqNum seq, const Address & srcAddr );

	bool isAnonymous() const { return isAnonymous_; }
	void isAnonymous( bool v );

	bool isOwnedByNub() const { return isAnonymous_ || isCondemned_; }

	bool hasUnackedCriticals() const { return unackedCriticalSeq_ != SEQ_NULL; }
	void resendCriticals();

	bool wantsFirstPacket() const { return wantsFirstPacket_; }
	void gotFirstPacket() { wantsFirstPacket_ = false; }
	bool shouldSendFirstReliablePacket() const;

	void dropNextSend() { shouldDropNextSend_ = true; }

	Traits traits() const { return traits_; }
	bool isExternal() const { return traits_ == EXTERNAL; }
	bool isInternal() const { return traits_ == INTERNAL; }

	bool shouldAutoSwitchToSrcAddr() const { return shouldAutoSwitchToSrcAddr_; }
	void shouldAutoSwitchToSrcAddr( bool b );

	SeqNum useNextSequenceID();
	void onPacketReceived( int bytes );

	const char * c_str() const;

	/// The id for indexed channels (or CHANNEL_ID_NULL if not indexed).
	ChannelID id() const	{ return id_; }

	/// The version of indexed channels (or 0 if not indexed).
	ChannelVersion version() const { return version_; }
	void version( ChannelVersion v ) { version_ = v; }

	bool isIndexed() const	{ return id_ != CHANNEL_ID_NULL; }
	bool isEstablished() const { return addr_.ip != 0; }

	void clearBundle();
	void bundlePrimer( BundlePrimer & primer );

	Nub::FragmentedBundlePtr pFragments() { return pFragments_; }
	void pFragments( Nub::FragmentedBundlePtr pFragments ) { pFragments_ = pFragments; }

	static SeqNum seqMask( SeqNum x );
	static bool seqLessThan( SeqNum a, SeqNum b );

	static const SeqNum SEQ_SIZE = 0x10000000U;
	static const SeqNum SEQ_MASK = SEQ_SIZE-1;
	static const SeqNum SEQ_NULL = SEQ_SIZE;

	static WatcherPtr pWatcher();

	bool hasUnackedPackets() const	{ return oldestUnackedSeq_ != SEQ_NULL; }

	/// Returns how much of the send window is currently being used. This includes
	/// the overflow packets and so can be larger than windowSize_.
	int sendWindowUsage() const
	{
		return this->hasUnackedPackets() ?
			seqMask( largeOutSeqAt_ - oldestUnackedSeq_ ) : 0;
	}

	static void setSendWindowCallback( SendWindowCallback callback );
	static float sendWindowCallbackThreshold();
	static void sendWindowCallbackThreshold( float threshold );

	int pushUnsentAcksThreshold() const { return pushUnsentAcksThreshold_; }
	void pushUnsentAcksThreshold( int i ) { pushUnsentAcksThreshold_ = i; }

	/**
	 *	This method returns the number of packets sent on this channel. It does
	 *	not include resends.
	 */
	uint32	numPacketsSent() const		{ return numPacketsSent_; }

	/**
	 *	This method returns the number of packets received on this channel.
	 */
	uint32	numPacketsReceived() const	{ return numPacketsReceived_; }

	/**
	 *	This method returns the number of bytes sent on this channel. It does
	 *	not include bytes sent by resends.
	 */
	uint32	numBytesSent() const		{ return numBytesSent_; }

	/**
	 *	This method returns the number of bytes received by this channel.
	 */
	uint32	numBytesReceived() const	{ return numBytesReceived_; }

	/**
	 *	This method returns the number of packets resent by this channel.
	 */
	uint32	numPacketsResent() const	{ return numPacketsResent_; }

	/**
	 *	This method returns the number of reliable packets sent by this channel.
	 */
	uint32	numReliablePacketsSent() const { return numReliablePacketsSent_; }

	/**
	 *  This method returns the last time a reliable packet was sent for the
	 *  first time.
	 */
	uint64	lastReliableSendTime() const { return lastReliableSendTime_; }

	/**
	 *  This method returns the last time a reliable packet was sent for the
	 *  first time or re-sent.
	 */
	uint64	lastReliableSendOrResendTime() const
	{
		return std::max( lastReliableSendTime_, lastReliableResendTime_ );
	}

	/**
	 *	Set the channel's message filter to be a new reference to the given
	 *	message filter, releasing any reference to any previous message filter.
	 *
	 *	@param pMessageFilter 	the new message filter
	 */
	void pMessageFilter( MessageFilter * pMessageFilter )
	{
		pMessageFilter_ = pMessageFilter;
	}

	/**
	 *	Return a new reference to the message filter for this channel.
	 */
	MessageFilterPtr pMessageFilter()
	{
		return pMessageFilter_;
	}

	bool validateUnreliableSeqNum( const SeqNum seqNum );


	// External Channels
	static void setExternalMaxOverflowPackets( uint16 maxPackets )
	{
		Channel::s_maxOverflowPackets_[ 0 ] = maxPackets;
	}

	static uint16 getExternalMaxOverflowPackets()
	{
		return Channel::s_maxOverflowPackets_[ 0 ];
	}

	// Internal Channels
	static void setInternalMaxOverflowPackets( uint16 maxPackets )
	{
		Channel::s_maxOverflowPackets_[ 1 ] = maxPackets;
	}

	static uint16 getInternalMaxOverflowPackets()
	{
		return Channel::s_maxOverflowPackets_[ 1 ];
	}

	// Indexed Channels
	static void setIndexedMaxOverflowPackets( uint16 maxPackets )
	{
		Channel::s_maxOverflowPackets_[ 2 ] = maxPackets;
	}

	static uint16 getIndexedMaxOverflowPackets()
	{
		return Channel::s_maxOverflowPackets_[ 2 ];
	}

	/// Should the process assert when the maximum number of overflow
	/// packets has been reached.
	static bool s_assertOnMaxOverflowPackets;

	static bool assertOnMaxOverflowPackets()
	{
		return Channel::s_assertOnMaxOverflowPackets;
	}

	static void assertOnMaxOverflowPackets( bool shouldAssert )
	{
		Channel::s_assertOnMaxOverflowPackets = shouldAssert;
	}

private:
	enum TimeOutType
	{
		TIMEOUT_INACTIVITY_CHECK,
		TIMEOUT_CHANNEL_PUSH
	};

	virtual int handleTimeout( TimerID, void * );

	Nub * 		pNub_;
	Traits		traits_;

	/// An indexed channel is a basically a way of multiplexing multiple channels
	/// between a pair of addresses.  Regular channels distinguish traffic solely on
	/// the basis of address, so in situations where you need multiple channels
	/// between a pair of nubs (i.e. channels between base and cell entities) you
	/// use indexed channels to keep the streams separate.
	ChannelID	id_;
	TimerID		channelPushTimerID_;
	TimerID		inactivityTimerID_;

	/// Stores the number of cycles without receiving a packet before reporting
	/// that this channel is inactive.
	uint64		inactivityExceptionPeriod_;

	/// Indexed channels have a 'version' number which basically tracks how many
	/// times they have been offloaded.  This allows us to correctly determine
	/// which incoming packets are out-of-date and also helps identify the most
	/// up-to-date information about lost entities in a restore situation.
	ChannelVersion version_;

	/// The time at which data was last received on this channel.
	uint64		lastReceivedTime_;

	PacketFilterPtr		pFilter_;
	Address				addr_;
	Bundle *			pBundle_;

	uint32			windowSize_;

	/// Generally, the sequence number of the next packet to be sent.
	SeqNum			smallOutSeqAt_; // This does not include packets in overflowPackets_
	SeqNum			largeOutSeqAt_; // This does include packets in overflowPackets_

	/// The sequence number of the oldest unacked packet such that there is at
	/// least one acked packet after it, or SEQ_NULL if none such exists.  This
	/// doesn't necessarily mean the packet is missing, it may have been delayed
	/// causing ACKs to come back out-of-order, or the ACKs may have been dropped.
	SeqNum			firstMissing_;

	/// The sequence number of the youngest unacked packet such that there is at
	/// least one acked packet after it, or SEQ_NULL if none such exists.
	SeqNum			lastMissing_;

	/// The sequence number of the oldest unacked packet on this channel.
	SeqNum			oldestUnackedSeq_;

	/// The last time a reliable packet was sent (for the first time) on this
	/// channel, as a timestamp.
	uint64			lastReliableSendTime_;

	/// The last time a reliable packet was resent on this channel.
	uint64			lastReliableResendTime_;

	/// The average round trip time for this channel, in timestamp units.
	uint64			roundTripTime_;

	/// The minimum time for a resend due to inactivity, used to stop thrashing
	/// when roundTripTime_ is low with respect to tick time.
	uint64			minInactivityResendDelay_;

	/// The last valid sequence number that was seen on an unreliable
	/// channel.
	SeqNum			unreliableInSeqAt_;

	/**
	 *	This class stores sent packets that may need to be resent.  These things
	 *	need to be fast so we pool them and use custom allocators.
	 */
	class UnackedPacket
	{
	public:
		UnackedPacket( Packet * pPacket = NULL );
		/*
		static void * operator new( size_t size )
		{
			return UnackedPacket::s_allocator_.allocate( size );
		}

		static void operator delete( void * pInstance )
		{
			UnackedPacket::s_allocator_.deallocate( pInstance );
		}
		*/
		SeqNum seq() const	{ return pPacket_->seq(); }

		PacketPtr pPacket_;

		/// The next packet after this one that is in between firstMissing_ and
		/// lastMissing_. Should be SEQ_NULL for lastMissing_ (and otherwise).
		SeqNum	nextMissing_;

		/// The outgoing sequence number on the channel the last time this
		/// packet was sent.
		SeqNum  lastSentAtOutSeq_;

		/// The time this packet was initially sent.
		uint64	lastSentTime_;

		/// Whether or not this packet has been resent.
		bool	wasResent_;

		/// A series of records detailing which parts of the packet were
		/// reliable, used when forming piggyback packets.
		ReliableVector reliableOrders_;

		static UnackedPacket * initFromStream(
			BinaryIStream & data, uint64 timeNow );

		static void addToStream(
			UnackedPacket * pInstance, BinaryOStream & data );

	private:
		/// Allocator for these objects
		// static PoolAllocator< SimpleMutex > s_allocator_;
	};

	CircularArray< UnackedPacket * > unackedPackets_;

	// These are the packets that do not yet fit on unackedPackets_
	typedef std::list< UnackedPacket * > OverflowPackets;
	OverflowPackets overflowPackets_;

	bool hasSeenOverflowWarning_;
	void addOverflowPacket( UnackedPacket *pPacket );

	static uint s_maxOverflowPackets_[3];

	uint getMaxOverflowPackets() const
	{
		if (this->isExternal())
		{
			return s_maxOverflowPackets_[ 0 ];
		}

		return s_maxOverflowPackets_[ 1 + !this->isIndexed() ];
	}

	void sendUnacked( UnackedPacket & unacked );

	/// The next packet that we expect to receive.
	SeqNum			inSeqAt_;

	/// Stores ordered packets that are received out-of-order.
	CircularArray< PacketPtr > bufferedReceives_;
	uint32 numBufferedReceives_;

	/// The fragment chain for the partially reconstructed incoming bundle on
	/// this channel, or NULL if incoming packets aren't fragments right now.
	Nub::FragmentedBundlePtr pFragments_;

	/// The ACK received with the highest sequence number.
	uint32			lastAck_;

	/// Stores the location in s_irregularChannels_;
	friend class IrregularChannels;
	IrregularChannels::iterator irregularIter_;

	/// Stores the location (if any) in the nub's keepAliveChannels_.
	friend class KeepAliveChannels;
	KeepAliveChannels::iterator keepAliveIter_;

	/// If true, this channel is checked periodically for resends. This also
	/// causes ACKs to be sent immediately instead of on the next outgoing
	/// bundle.
	bool			isIrregular_;

	/// If true, this channel has been condemned (i.e. detached from its
	/// previous owner and is awaiting death).
	bool			isCondemned_;

	/// If true, this channel should be considered destroyed. It may still be
	/// not yet destructed due to reference counting.
	bool			isDestroyed_;

	/// If set, this object will be used to prime the bundle after each call to
	/// Bundle::clear().
	BundlePrimer*	pBundlePrimer_;

	/// Used by CellAppChannels to indicate that we should not process further
	/// packets.
	bool			hasRemoteFailed_;

	/// If true, this channel is to an address that we don't really know much
	/// about, at least, not enough to be bothered writing a helper class for it
	/// on this app.  That means that the nub is responsible for creating and
	/// deleting this channel.
	bool			isAnonymous_;

	/// The highest unacked sequence number that is considered to be 'critical'.
	/// What this actually means is up to the app code, and is controlled by
	/// using the RELIABLE_CRITICAL reliability flag when starting messages.
	SeqNum			unackedCriticalSeq_;

	/// If non-zero and the number of ACKs on this channel's bundle exceeds this
	/// number, the bundle will be sent automatically, regardless of whether or
	/// not this channel is regular.
	int				pushUnsentAcksThreshold_;

	/// If true, this indexed channel will automatically switch its address to
	/// the source address of incoming packets.
	bool			shouldAutoSwitchToSrcAddr_;

	/// If true, this channel will drop all incoming packets unless they are
	/// flagged as FLAG_CREATE_CHANNEL.  This is only used by Channels that are
	/// reset() and want to ensure that they don't buffer any delayed incoming
	/// packets from the old connection.
	bool			wantsFirstPacket_;

	/// If true, this channel will artificially drop its next send().  This is
	/// used to help debug BigWorld in lossy network environments.
	bool			shouldDropNextSend_;

	/// The send window sizes where warnings are triggered.  This should be
	/// indexed with a bool indicating whether we're talking about indexed or
	/// plain internal channels.  This grows each time it is exceeded.  If it
	/// keeps growing to the point where window overflows happen, dev asserts
	/// will be triggered.
	static int 		s_sendWindowWarnThresholds_[2];

	int & sendWindowWarnThreshold()
	{
		return s_sendWindowWarnThresholds_[ this->isIndexed() ];
	}

	static int		s_sendWindowCallbackThreshold_;
	static SendWindowCallback s_pSendWindowCallback_;

	// Statistics
	uint32	numPacketsSent_;
	uint32	numPacketsReceived_;
	uint32	numBytesSent_;
	uint32	numBytesReceived_;
	uint32	numPacketsResent_;
	uint32	numReliablePacketsSent_;

	// Message filter
	MessageFilterPtr pMessageFilter_;
};


typedef SmartPointer< Channel > ChannelPtr;


/**
 *	This class is a simple base class for classes that want to own a channel.
 */
class ChannelOwner
{
public:
	ChannelOwner( Nub & nub, const Address & address = Address::NONE,
			Channel::Traits traits = Channel::INTERNAL,
			float minInactivityResendDelay = 1.0,
			PacketFilterPtr pFilter = NULL ) :
		pChannel_( traits == Channel::INTERNAL ?
			Channel::get( nub, address ) :
			new Channel( nub, address, traits,
				minInactivityResendDelay, pFilter ) )
	{
		// minInactivityResendDelay and pFilter aren't passed through to
		// Channel::get() so they must be the default values.
		MF_ASSERT( traits == Channel::EXTERNAL ||
			(minInactivityResendDelay == 1.0 && pFilter == NULL) );
	}

	~ChannelOwner()
	{
		pChannel_->condemn();
		pChannel_ = NULL;
	}

	Bundle & bundle()				{ return pChannel_->bundle(); }
	const Address & addr() const	{ return pChannel_->addr(); }
	const char * c_str() const		{ return pChannel_->c_str(); }
	void send( Bundle * pBundle = NULL ) { pChannel_->send( pBundle ); }

	Channel & channel()					{ return *pChannel_; }
	const Channel & channel() const		{ return *pChannel_; }

	void addr( const Address & addr );

#if ENABLE_WATCHERS
	static WatcherPtr pWatcher()
	{
		return new BaseDereferenceWatcher( Channel::pWatcher() );
	}
#endif

private:
	Channel * pChannel_;
};


/**
 *  This class is a wrapper class for a Channel that automatically sends on
 *  destruct if the channel is irregular.  Recommended for use in app code when
 *  you don't want to have to keep figuring out if channels you get with
 *  findChannel() are regular or not.
 */
class ChannelSender
{
public:
	ChannelSender( Channel & channel ) :
		channel_( channel )
	{}

	~ChannelSender()
	{
		if (channel_.isIrregular())
		{
			channel_.delayedSend();
		}
	}

	Mercury::Bundle & bundle() { return channel_.bundle(); }
	Mercury::Channel & channel() { return channel_; }

private:
	Channel & channel_;
};

} // namespace Mercury

#ifdef CODE_INLINE
#include "channel.ipp"
#endif

#endif // CHANNEL_HPP
