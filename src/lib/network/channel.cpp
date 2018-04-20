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

#include "cstdmf/memory_tracker.hpp"
#include "channel.hpp"

#ifndef CODE_INLINE
#include "channel.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "Network", 0 )

namespace Mercury
{

const int EXTERNAL_CHANNEL_SIZE = 256;
const int INTERNAL_CHANNEL_SIZE = 4096;
const int INDEXED_CHANNEL_SIZE = 512;


// Maximum number of overflow packets per channel type
// maximum size calculated by max overflow packets * packet size (MTU)
uint Channel::s_maxOverflowPackets_[] =
	{ 1024, // External channel.
	  8192, // Internal channel
	  4096  // Indexed channel (ie: entity channel).
	};

bool Channel::s_assertOnMaxOverflowPackets = false;

int Channel::s_sendWindowWarnThresholds_[] =
	{ INTERNAL_CHANNEL_SIZE / 4, INDEXED_CHANNEL_SIZE / 4 };

int Channel::s_sendWindowCallbackThreshold_ = INDEXED_CHANNEL_SIZE/2;
Channel::SendWindowCallback Channel::s_pSendWindowCallback_ = NULL;

namespace
{

class WatcherIniter
{
public:
	WatcherIniter()
	{
		Channel::staticInit();
	}
};

WatcherIniter s_watcherIniter_;

} // anonymous namespace

// -----------------------------------------------------------------------------
// Section: Channel
// -----------------------------------------------------------------------------

/**
 * 	This is the constructor.
 *
 * 	@param nub		The nub on which to send and receive messages
 * 	@param address	The address of our peer or Address::NONE for indexed
 *						channel.
 * 	@param traits	The traits of network this channel spans.
 *	@param minInactivityResendDelay  The minimum delay in seconds before
 *									packets are resent.
 *  @param pFilter	The packet filter to use for sending and receiving.
 *  @param id		The ID for indexed channels (if provided).
 */
Channel::Channel( Nub & nub, const Address & address, Traits traits,
		float minInactivityResendDelay,
		PacketFilterPtr pFilter, ChannelID id ):
	pNub_( &nub ),
	traits_( traits ),
	id_( id ),
	channelPushTimerID_( TIMER_ID_NONE ),
	inactivityTimerID_( TIMER_ID_NONE ),
	inactivityExceptionPeriod_( 0 ),
	version_( 0 ),
	lastReceivedTime_( 0 ),
	pFilter_( pFilter ),
	addr_( Address::NONE ),
	pBundle_( NULL ),
	windowSize_(	(traits != INTERNAL)    ? EXTERNAL_CHANNEL_SIZE :
					(id == CHANNEL_ID_NULL) ? INTERNAL_CHANNEL_SIZE :
											  INDEXED_CHANNEL_SIZE ),
	smallOutSeqAt_( 0 ),
	largeOutSeqAt_( 0 ),
	firstMissing_( SEQ_NULL ),
	lastMissing_( SEQ_NULL ),
	oldestUnackedSeq_( SEQ_NULL ),
	lastReliableSendTime_( 0 ),
	lastReliableResendTime_( 0 ),
	roundTripTime_( (traits == INTERNAL) ?
		stampsPerSecond() / 10 : stampsPerSecond() ),
	minInactivityResendDelay_(
		uint64( minInactivityResendDelay * stampsPerSecond() ) ),
	unreliableInSeqAt_( SEQ_NULL ),
	unackedPackets_( windowSize_ ),
	hasSeenOverflowWarning_( false ),
	inSeqAt_( 0 ),
	bufferedReceives_( windowSize_ ),
	numBufferedReceives_( 0 ),
	pFragments_( NULL ),
	lastAck_( seqMask( smallOutSeqAt_ - 1 ) ),
	irregularIter_( nub.irregularChannels().end() ),
	keepAliveIter_( nub.keepAliveChannels().end() ),
	isIrregular_( false ),
	isCondemned_( false ),
	isDestroyed_( false ),
	pBundlePrimer_( NULL ),
	hasRemoteFailed_( false ),
	isAnonymous_( false ),
	unackedCriticalSeq_( SEQ_NULL ),
	pushUnsentAcksThreshold_( 0 ),
	shouldAutoSwitchToSrcAddr_( false ),
	wantsFirstPacket_( false ),
	shouldDropNextSend_( false ),

	// Stats
	numPacketsSent_( 0 ),
	numPacketsReceived_( 0 ),
	numBytesSent_( 0 ),
	numBytesReceived_( 0 ),
	numPacketsResent_( 0 ),
	numReliablePacketsSent_( 0 ),

	// Message filter
	pMessageFilter_( NULL )
{
	// This corresponds to the decRef in Channel::destroy.
	this->incRef();

	if (pFilter_ && id_ != CHANNEL_ID_NULL)
	{
		CRITICAL_MSG( "Channel::Channel: "
			"PacketFilters are not supported on indexed channels (id:%d)\n",
			id_ );
	}

	// Initialise the bundle
	this->clearBundle();

	// This registers non-indexed channels with the nub.
	this->addr( address );

	for (uint i = 0; i < unackedPackets_.size(); i++)
	{
		unackedPackets_[i] = NULL;
		bufferedReceives_[i] = NULL;
	}
}


/**
 *  Static initialisation for watchers etc.
 */
void Channel::staticInit()
{
	// TODO: Update the thresholds from bw.xml, probably in a global <network>
	// section.

#ifdef MF_SERVER
	// This is only interesting on the server.

	MF_WATCH( "network/internalSendWindowSizeThreshold",
		s_sendWindowWarnThresholds_[ 0 ] );

	MF_WATCH( "network/indexedSendWindowSizeThreshold",
		s_sendWindowWarnThresholds_[ 1 ] );
#endif

}


/**
 *  This static method will look in the provided nub for an existing anonymous
 *  channel to the specified address, and if found will mark it as no longer
 *  being anonymous and return it.  If not found, the regular constructor is
 *  called and a new channel is returned.
 */
Channel * Channel::get( Nub & nub, const Address & address )
{
	Channel * pChannel = nub.findChannel( address );

	if (pChannel)
	{
		MF_ASSERT( pChannel->isAnonymous() );

		// This brings the channel back in sync with the state it would have
		// been in from a normal (explicit) construction.
		pChannel->isAnonymous( false );

		INFO_MSG( "Channel::get: "
			"Claimed anonymous channel to %s\n",
			pChannel->c_str() );

		if (pChannel->isCondemned())
		{
			WARNING_MSG( "Channel::get: "
				"Returned condemned channel to %s\n",
				pChannel->c_str() );
		}
	}
	else
	{
		pChannel = new Channel( nub, address, INTERNAL );
	}

	return pChannel;
}


/**
 *	This method sets the address of this channel. If necessary, it is registered
 *	with the nub.
 */
void Channel::addr( const Mercury::Address & addr )
{
	if (addr_ != addr)
	{
		lastReceivedTime_ = ::timestamp();

		if (!this->isIndexed())
		{
			if (addr_ != Address::NONE)
			{
				MF_VERIFY( pNub_->deregisterChannel( *this ) );
			}

			addr_ = addr;

			if (addr_ != Address::NONE)
			{
				MF_VERIFY( pNub_->registerChannel( *this ) );
			}
		}
		else
		{
			addr_ = addr;
		}
	}
}


/**
 * 	Destructor.
 */
Channel::~Channel()
{
	MF_ASSERT( isDestroyed_ );

	pNub_->onChannelGone( this );

	this->reset( Address::NONE );

	if (channelPushTimerID_ != TIMER_ID_NONE)
	{
		pNub_->cancelTimer( channelPushTimerID_ );
	}

	delete pBundle_;
}


/**
 *	This method schedules this channel for deletion once all of its packets have
 *	been acked.
 */
void Channel::condemn()
{
	if (this->isCondemned())
	{
		WARNING_MSG( "Channel::condemn( %s ): Already condemned.\n",
			   this->c_str() );
		return;
	}

	// Send any unsent traffic that may have accumulated here.
	if (this->hasUnsentData())
	{
		if (this->isEstablished())
		{
			this->send();
		}
		else
		{
			WARNING_MSG( "Channel::condemn( %s ): "
				"Unsent data was lost because channel not established\n",
				this->c_str() );
		}
	}

	// Since you aren't going to be actively sending on this channel anymore, it
	// must be marked as irregular.
	this->isIrregular( true );

	isCondemned_ = true;

	// Note: This call may delete this channel.
	pNub_->condemnedChannels().add( this );
}


/**
 *	This method "destroys" this channel. It should be considered similar to
 *	delete pChannel except that their may be other references remaining.
 */
void Channel::destroy()
{
	IF_NOT_MF_ASSERT_DEV( !isDestroyed_ )
	{
		return;
	}

	isDestroyed_ = true;

	this->decRef();
}


/**
 *	Add the provided UnackedPacket to the the overflow list, checking how
 *	large the overflow has become. Warn if the overflow is starting to get
 *	large, and assert if it has exceeded MAX_OVERFLOW_PACKETS.
 */
void Channel::addOverflowPacket( UnackedPacket * pPacket )
{
	uint maxOverflowPackets = this->getMaxOverflowPackets();

	if (maxOverflowPackets != 0)
	{
		// Only assert if we're explicitly told to
		if (s_assertOnMaxOverflowPackets)
		{
			MF_ASSERT( overflowPackets_.size() < maxOverflowPackets);
		}

		// Warn if the overflow size has grown to 1/2 of the MAX size
		if (overflowPackets_.size() > (maxOverflowPackets / 2))
		{
			if (!hasSeenOverflowWarning_)
			{
				WARNING_MSG( "Channel::addOverflowPacket: Overflow packet "
					"list size (%zu) exceeding safety threshold (%u).\n",
					overflowPackets_.size(), (maxOverflowPackets / 2) );
				hasSeenOverflowWarning_ = true;
			}
		}
		else if (hasSeenOverflowWarning_)
		{
			if (overflowPackets_.size() < (maxOverflowPackets / 3))
			{
				hasSeenOverflowWarning_ = false;
			}
		}
	}

	overflowPackets_.push_back( pPacket );
}


/**
 *	This method reconstructs this channel from streamed data. It is used for
 *	streaming the entity channel when the real cell entity is offloaded.
 *
 *	This assumes that this object was constructed with the same arguments as the
 *	source channel.
 */
void Channel::initFromStream( BinaryIStream & data,
	   const Mercury::Address & addr )
{
	uint64 timeNow = timestamp();
	lastReceivedTime_ = timeNow;
	addr_ = addr;

	data >> version_;
	data >> smallOutSeqAt_;
	data >> largeOutSeqAt_;
	data >> oldestUnackedSeq_;

	uint32 count = (oldestUnackedSeq_ == SEQ_NULL) ?
					0 : seqMask( largeOutSeqAt_ - oldestUnackedSeq_ );

	lastAck_ = (oldestUnackedSeq_ != SEQ_NULL) ?
		seqMask( oldestUnackedSeq_ - 1 ) : seqMask( smallOutSeqAt_ - 1 );

	firstMissing_ = SEQ_NULL;
	lastMissing_ = SEQ_NULL;

	// This loop destreams the unacked sends (i.e. fills unackedPackets_).
	for (uint32 i = 0; i < count; ++i)
	{
		SeqNum currSeq = seqMask( oldestUnackedSeq_ + i );

		UnackedPacket * pUnacked = UnackedPacket::initFromStream( data, timeNow );

		if (i >= windowSize_)
		{
			MF_ASSERT( pUnacked );
			this->addOverflowPacket( pUnacked );
		}
		else if (pUnacked)
		{
			unackedPackets_[ currSeq ] = pUnacked;
		}
		else
		{
			// Each time we hit a slot that has been acked, it is the new lastAck_.
			lastAck_ = currSeq;

			// The first time we hit an acked slot, we know that the oldest
			// unacked seq is the firstMissing_ and that the packet before this one
			// must be the lastMissing_ (for now).
			if (firstMissing_ == SEQ_NULL)
			{
				firstMissing_ = oldestUnackedSeq_;
				lastMissing_ = seqMask( currSeq - 1 );
			}

			// If firstMissing_ is already set and the packet before this one is
			// unacked, then this slot is the new lastMissing_.
			else if (unackedPackets_[ currSeq - 1 ])
			{
				lastMissing_ = seqMask( currSeq - 1 );
			}
		}
	}

	SeqNum seq = firstMissing_;
	UnackedPacket * pPrev = unackedPackets_[ firstMissing_ ];

	while (seq != lastMissing_)
	{
		seq = seqMask( seq + 1 );

		UnackedPacket * pCurr = unackedPackets_[ seq ];

		if (pCurr)
		{
			pPrev->nextMissing_ = seq;
			pPrev = pCurr;
		}
	}

	// Start debugging
	SeqNum			firstMissing;
	SeqNum			lastMissing;
	uint32			lastAck;

	data >> firstMissing >> lastMissing >> lastAck;

	MF_ASSERT( firstMissing == firstMissing_ );
	MF_ASSERT( lastMissing == lastMissing_ );
	MF_ASSERT( lastAck == lastAck_ );
	// End debugging

	lastReliableSendTime_ = timeNow;
	lastReliableResendTime_ = timeNow;

	roundTripTime_ = minInactivityResendDelay_ / 2;

	// Now we destream the buffered receives.
	data >> inSeqAt_;
	data >> numBufferedReceives_;
	int numToReceive = numBufferedReceives_;

	for (uint32 i = 1; i < windowSize_ && numToReceive > 0; ++i)
	{
		PacketPtr pPacket =
			Packet::createFromStream( data, Packet::BUFFERED_RECEIVE );

		bufferedReceives_[ inSeqAt_ + i ] = pPacket;

		if (pPacket)
		{
			--numToReceive;
		}
	}

	// Destream any chained fragments
	uint16 numChainedFragments;
	Packet * pPrevPacket = NULL;

	data >> numChainedFragments;

	for (uint16 i = 0; i < numChainedFragments; i++)
	{
		PacketPtr pPacket =
			Packet::createFromStream( data, Packet::CHAINED_FRAGMENT );

		// Create the FragmentedBundleInfo after we destream the first one.
		if (pFragments_ == NULL)
		{
			pFragments_ = new Nub::FragmentedBundle(
				pPacket->fragEnd(),
				pPacket->fragEnd() - pPacket->seq() + 1 - numChainedFragments,
				timestamp(),
				pPacket.getObject() );
		}
		else
		{
			pPrevPacket->chain( pPacket.getObject() );
		}

		// This should be fine despite the fact that pPrevPacket is not a
		// smartpointer because there should be a reference to pPacket at some
		// point in pFragments_'s packet chain.
		pPrevPacket = pPacket.getObject();
	}

	data >> unackedCriticalSeq_ >> wantsFirstPacket_;

	// If this channel is irregular, make sure its resends will be tracked.
	// Without this, no resends will happen until the next time this channel
	// sends.
	pNub_->irregularChannels().addIfNecessary( *this );

#if 0
	data >> numPacketsSent_;
	data >> numPacketsReceived_;
	data >> numBytesSent_;
	data >> numBytesReceived_;
	data >> numPacketsResent_;
#endif

	MF_ASSERT( firstMissing_ == SEQ_NULL ||
			unackedPackets_[ firstMissing_ ] != NULL );
	MF_ASSERT( (firstMissing_ == SEQ_NULL) == (lastMissing_ == SEQ_NULL) );
	MF_ASSERT( (firstMissing_ == SEQ_NULL) ||
			(firstMissing_ == lastMissing_) ||
			(unackedPackets_[ firstMissing_ ]->nextMissing_ <= lastMissing_) );
	MF_ASSERT( firstMissing_ <= lastMissing_ );
}


/**
 *  This method writes this channel's state to the provided stream so that it
 *  can be reconstructed with initFromStream().
 */
void Channel::addToStream( BinaryOStream & data )
{
	// Avoid having to stream this with the channel.
	if (this->hasUnsentData())
	{
		this->send();
	}

	// Increment version number for peer
	data << seqMask( version_ + 1 );

	data << smallOutSeqAt_;
	data << largeOutSeqAt_;
	data << oldestUnackedSeq_;

	uint32 count = this->sendWindowUsage();

	MF_ASSERT( (count == 0) || unackedPackets_[ oldestUnackedSeq_ ] );

	for (uint32 i = 0; i < std::min( count, windowSize_ ); ++i)
	{
		UnackedPacket::addToStream( unackedPackets_[ oldestUnackedSeq_ + i ],
				data );
	}

	MF_ASSERT( overflowPackets_.empty() ||
		(count == windowSize_ + overflowPackets_.size()) );

	for (OverflowPackets::iterator iter = overflowPackets_.begin();
			iter != overflowPackets_.end();
			++iter)
	{
		UnackedPacket::addToStream( *iter, data );
	}

	data << firstMissing_ << lastMissing_ << lastAck_;

	data << inSeqAt_;

	data << numBufferedReceives_;
	int numToSend = numBufferedReceives_;

	for (uint32 i = 1; i < windowSize_ && numToSend > 0; ++i)
	{
		const Packet * pPacket = bufferedReceives_[ inSeqAt_ + i ].getObject();
		Packet::addToStream( data, pPacket, Packet::BUFFERED_RECEIVE );

		if (pPacket)
		{
			--numToSend;
		}
	}

	// Stream on chained fragments
	if (pFragments_)
	{
		data << (uint16)pFragments_->pChain_->chainLength();

		for (const Packet * p = pFragments_->pChain_.getObject();
			 p != NULL; p = p->next())
		{
			Packet::addToStream( data, p, Packet::CHAINED_FRAGMENT );
		}
	}
	else
	{
		data << (uint16)0;
	}

	data << unackedCriticalSeq_ << wantsFirstPacket_;

	MF_ASSERT( !hasRemoteFailed_ );

#if 0
	data << numPacketsSent_;
	data << numPacketsReceived_;
	data << numBytesSent_;
	data << numBytesReceived_;
	data << numPacketsResent_;
#endif
}


/**
 * 	This method schedules a send to occur regularly.
 *
 * 	@param microseconds		The interval at which to send.
 */
void Channel::sendEvery( int microseconds )
{
	if (channelPushTimerID_ != TIMER_ID_NONE)
	{
		pNub_->cancelTimer( channelPushTimerID_ );
		channelPushTimerID_ = TIMER_ID_NONE;
	}

	if (microseconds)
	{
		channelPushTimerID_ = pNub_->registerTimer(
				microseconds, this, (void*)TIMEOUT_CHANNEL_PUSH );
	}
}


/**
 *	This method returns the bundle associated with this channel.
 */
Bundle & Channel::bundle()
{
	return *pBundle_;
}


/**
 *	This method returns the bundle associated with this channel.
 */
const Bundle & Channel::bundle() const
{
	return *pBundle_;
}


/**
 *  This method returns true if this channel's bundle has any unsent data on it,
 *  excluding messages that may have been put there by the BundlePrimer.
 */
bool Channel::hasUnsentData() const
{
	// Unreliable messages written by the bundle primer are not counted here.
	const int primeMessages =
		pBundlePrimer_ ? pBundlePrimer_->numUnreliableMessages() : 0;

	return pBundle_->numMessages() > primeMessages ||
		pBundle_->hasDataFooters() ||
		pBundle_->isReliable();
}


/**
 *	This method sends a bundle on this channel and resends unacked packets as
 *	necessary.  By default it sends the channel's own bundle, however it can
 *	also send a bundle passed in from the outside.
 */
void Channel::send( Bundle * pBundle )
{
	// Don't do anything if the remote process has failed
	if (hasRemoteFailed_)
	{
		WARNING_MSG( "Channel::send( %s ): "
			"Not doing anything due to remote process failure\n",
			this->c_str() );

		return;
	}

	bool isSendingOwnBundle = (pBundle == NULL);

	// If we are not sending the channel's bundle, then we basically want to
	// make sure that the bundle is modified the same way the channel's own
	// bundle is in clearBundle().
	if (!isSendingOwnBundle)
	{
		// If for some reason we start sending external bundles on indexed
		// channels, it's probably OK to just enable the flag here instead of
		// asserting.  Can't see why we would need to interleave bundles on an
		// indexed channel like that though.
		MF_ASSERT( !this->isIndexed() );

		// We don't MF_ASSERT( !this->shouldSendFirstReliablePacket() ) because
		// it's OK for the first two packets on a channel to both have this
		// flag.  This could happen if the first send() on this channel is not
		// the channel's own bundle.  We just enable this flag like we would
		// have in clearBundle() if it was the channel's own bundle.
		if (this->shouldSendFirstReliablePacket())
		{
			pBundle->firstPacket_->enableFlags( Packet::FLAG_CREATE_CHANNEL );
		}

		// If this channel uses a bundle primer, then the external bundle won't have
		// been set up correctly.  We don't support sending external bundles on
		// channels with bundle primers yet.
		MF_ASSERT( !pBundlePrimer_ );
	}
	else
	{
		pBundle = pBundle_;
	}

	// All internal traffic must be marked as reliable by the startMessage calls.
	MF_ASSERT( this->isExternal() ||
		pBundle->numMessages() == 0 ||
		pBundle->isReliable() );

	this->checkResendTimers();

	// If we're sending the channel's bundle and it's empty, just don't do it.
	// It's important to do this after the call to checkResendTimers() so that
	// channels that are marked as regular but don't have any actual data to
	// send will still check their resends when they call this method.
	if (isSendingOwnBundle && !this->hasUnsentData())
	{
		return;
	}

	// Enable artificial loss if required.
	if (shouldDropNextSend_)
	{
		pNub_->dropNextSend();
		shouldDropNextSend_ = false;
	}

	pNub_->send( addr_, *pBundle, this );

	// Update our stats
	++numPacketsSent_;
	numBytesSent_ += pBundle->size();

	if (pBundle->isReliable())
	{
		++numReliablePacketsSent_;
	}

	// Channels that do not send regularly are added to a collection to do their
	// resend checking periodically.
	pNub_->irregularChannels().addIfNecessary( *this );

	// If the bundle that was just sent was critical, the sequence number of its
	// last packet is the new unackedCriticalSeq_.
	if (pBundle->isCritical())
	{
		unackedCriticalSeq_ =
			pBundle->firstPacket_->seq() + pBundle->sizeInPackets() - 1;
	}

	// Clear the bundle
	if (isSendingOwnBundle)
	{
		this->clearBundle();
	}
	else
	{
		pBundle->clear();
	}
}


/**
 *	This method schedules this channel to send at the next available sending opportunity.
 */
void Channel::delayedSend()
{
	if (this->isIrregular())
	{
		pNub_->delayedSend( this );
	}
}


/**
 *	This method calls send on this channel if it has not sent for a while and
 *	is getting close to causing resends.
 */
void Channel::sendIfIdle()
{
	if (this->isEstablished())
	{
		if (this->lastReliableSendOrResendTime() <
				::timestamp() - minInactivityResendDelay_/2)
		{
			this->send();
		}
	}
}


/**
 *	This method records a packet that may need to be resent later if it is not
 *	acknowledged. It is called by the Nub when it sends a packet on our behalf.
 *
 *	@return false if the window size was exceeded.
 */
bool Channel::addResendTimer( SeqNum seq, Packet * p,
		const ReliableOrder * roBeg, const ReliableOrder * roEnd )
{
	MF_ASSERT( (oldestUnackedSeq_ == SEQ_NULL) ||
			unackedPackets_[ oldestUnackedSeq_ ] );
	MF_ASSERT( seq == p->seq() );

	UnackedPacket * pUnackedPacket = new UnackedPacket( p );

	// If this channel has no unacked packets, record this as the oldest.
	if (oldestUnackedSeq_ == SEQ_NULL)
	{
		oldestUnackedSeq_ = seq;
	}

	// Fill it in
	pUnackedPacket->lastSentAtOutSeq_ = seq;

	uint64 now = ::timestamp();
	pUnackedPacket->lastSentTime_ = now;
	lastReliableSendTime_ = now;

	pUnackedPacket->wasResent_ = false;
	pUnackedPacket->nextMissing_ = SEQ_NULL;

	if (roBeg != roEnd)
	{
		pUnackedPacket->reliableOrders_.assign( roBeg, roEnd );
	}

	bool isOverflow = false;

	// Make sure that we have not overflowed and the record for this sequence
	// number is empty.
	if (!overflowPackets_.empty() || unackedPackets_[ seq ] != NULL)
	{
		if (this->nub().isVerbose())
		{
			WARNING_MSG( "Channel::addResendTimer( %s ):"
							"Window size exceeded, buffering #%u\n",
						this->c_str(), pUnackedPacket->pPacket_->seq() );
		}

		MF_ASSERT( seq == seqMask( largeOutSeqAt_ - 1 ) );

		isOverflow = true;
		this->addOverflowPacket( pUnackedPacket );
		MF_ASSERT( seqMask( smallOutSeqAt_ + overflowPackets_.size() ) ==
				largeOutSeqAt_ );
	}
	else
	{
		unackedPackets_[ seq ] = pUnackedPacket;
		smallOutSeqAt_ = largeOutSeqAt_;
		MF_ASSERT( overflowPackets_.empty() );
	}

	MF_ASSERT( (oldestUnackedSeq_ == SEQ_NULL) ||
			unackedPackets_[ oldestUnackedSeq_ ] );

	return !isOverflow;
}


/**
 *	This method removes a packet from the collection of packets that have been
 *	sent but not acknowledged. It is called by the Nub when it receives an
 *  acknowledgement to a packet that this channel caused to be sent.
 *
 *  Returns false on error, true otherwise.
 */
bool Channel::delResendTimer( SeqNum seq )
{
	MF_ASSERT( firstMissing_ == SEQ_NULL || unackedPackets_[ firstMissing_ ] );
	MF_ASSERT( (firstMissing_ == SEQ_NULL) == (lastMissing_ == SEQ_NULL) );
	MF_ASSERT( (firstMissing_ == SEQ_NULL) ||
			(firstMissing_ == lastMissing_) ||
			(unackedPackets_[ firstMissing_ ]->nextMissing_ <= lastMissing_) );
	MF_ASSERT( firstMissing_ <= lastMissing_ );
	MF_ASSERT( (oldestUnackedSeq_ == SEQ_NULL) ||
			unackedPackets_[ oldestUnackedSeq_ ] );

	// Make sure the sequence number is valid
	if (seqMask( seq ) != seq)
	{
		ERROR_MSG( "Channel::delResendTimer( %s ): "
			"Got out-of-range seq #%u (outseq: #%u)\n",
			this->c_str(), seq, smallOutSeqAt_ );

		return false;
	}

	// Make sure it lies within our window size.
	// Equal to window size is fine ... just :)
	if (seqMask( (smallOutSeqAt_-1) - seq ) >= windowSize_)
	{
		WARNING_MSG( "Channel::delResendTimer( %s ): "
			"Called for seq #%u outside window #%u (maybe ok)\n",
			this->c_str(), seq, smallOutSeqAt_ );

		return true;
	}

	// now make sure there's actually a packet there
	UnackedPacket * pUnackedPacket = unackedPackets_[ seq ];
	if (pUnackedPacket == NULL)
	{
		/*
		if (this->isInternal())
		{
			DEBUG_MSG( "Channel::delResendTimer( %s ): "
				"Called for already acked packet #%d (ok)\n",
				this->c_str(), (int)seq );
		}
		*/
		return true;
	}

	// Update the average RTT for this channel, if this packet hadn't already
	// been resent.
	if (!pUnackedPacket->wasResent_)
	{
		const uint64 RTT_AVERAGE_DENOM = 10;

		roundTripTime_ = ((roundTripTime_ * (RTT_AVERAGE_DENOM - 1)) +
			(timestamp() - pUnackedPacket->lastSentTime_)) / RTT_AVERAGE_DENOM;
	}

	// If this packet was the critical one, we're no longer in a critical state!
	if (unackedCriticalSeq_ == seq)
	{
		unackedCriticalSeq_ = SEQ_NULL;
	}

	// If we released the oldest unacked packet, figure out the new one
	if (seq == oldestUnackedSeq_)
	{
		// If we acked a "missing" packet, its next one is now the oldest
		if (pUnackedPacket->nextMissing_ != SEQ_NULL)
		{
			oldestUnackedSeq_ = pUnackedPacket->nextMissing_;
			MF_ASSERT( (oldestUnackedSeq_ == SEQ_NULL) ||
					(unackedPackets_[ oldestUnackedSeq_ ] != NULL) );
		}

		// Otherwise, walk forward to the next non-NULL packet
		else
		{
			oldestUnackedSeq_ = SEQ_NULL;
			for (uint i = seqMask( seq+1 );
					i != smallOutSeqAt_;
					i = seqMask( i+1 ))
			{
				if (unackedPackets_[ i ])
				{
					oldestUnackedSeq_ = i;
					break;
				}
			}
		}
	}

	// If the incoming seq is after the last ack, then it is the new last ack
	if (seqLessThan( lastAck_, seq ))
	{
		lastAck_ = seq;
	}

	// now see if this ack was for a "missing" packet
	if (lastMissing_ != SEQ_NULL &&				// we have missing packets &&
		seqMask( lastMissing_ - seq ) < windowSize_)	// seq <= lastMissing_
	{
		SeqNum preLook = SEQ_NULL;
		UnackedPacket * pPreLookRR = NULL;

		// find the parent of the missing packet in the list ...
		SeqNum look = firstMissing_;
		UnackedPacket * pLookRR = unackedPackets_[look];
		while (look != SEQ_NULL && look != seq)
		{
			preLook = look;
			pPreLookRR = pLookRR;

			look = pLookRR->nextMissing_;
			pLookRR = unackedPackets_[look];
		}

		// and unlink it
		if (pPreLookRR == NULL)
			firstMissing_ = pLookRR->nextMissing_;
		else
			pPreLookRR->nextMissing_ = pLookRR->nextMissing_;

		if (seq == lastMissing_)
			lastMissing_ = preLook;

		if (this->isInternal())
		{
			DEBUG_MSG( "Channel::delResendTimer( %s ): "
					"Got ack for missing packet #%d inside window #%d\n",
				this->c_str(), (int)seq, (int)smallOutSeqAt_ );
		}
	}
	// ok, see if it causes suspected "missing" packets
	else if (seq != seqMask( smallOutSeqAt_ - windowSize_ ))
	{
		// Mark all unacked packets before this ack as "missing"
		SeqNum nextNewMissing = SEQ_NULL;
		SeqNum oldLastMissing = lastMissing_;
		const uint32 windowMask = windowSize_ - 1;

		for (SeqNum look = seqMask( seq-1 );
			(look & windowMask) != ((smallOutSeqAt_-1) & windowMask);
			look = seqMask( look-1 ))
		{
			UnackedPacket * pLookRR = unackedPackets_[look];

			if (pLookRR == NULL)
				break;

			pLookRR->nextMissing_ = nextNewMissing;

			if (nextNewMissing == SEQ_NULL)
				lastMissing_ = look;

			nextNewMissing = look;
		}

		// If there are new "missing" packets
		if (nextNewMissing != SEQ_NULL)
		{
			if (this->isInternal())
			{
				DEBUG_MSG( "Channel::delResendTimer( %s ): "
					"Ack for #%d inside window #%d created missing packets "
					"back to #%d\n",
					this->c_str(), (int)seq,
					(int)smallOutSeqAt_, (int)nextNewMissing );
			}

			// Record the first "missing" packet (or attach previous missing)
			if (firstMissing_ == SEQ_NULL)
			{
				firstMissing_ = nextNewMissing;
			}
			else
			{
				unackedPackets_[ oldLastMissing ]->nextMissing_ =
					nextNewMissing;
			}
		}
	}
	else
	{
		// if this was at the edge of a window then it didn't create "missing"
		// packets.
		MF_ASSERT( firstMissing_ == SEQ_NULL ||
			unackedPackets_[ firstMissing_ ] );
	}

	// Now we can release the unacked packet
	delete pUnackedPacket;
	unackedPackets_[ seq ] = NULL;

	MF_ASSERT( firstMissing_ == SEQ_NULL || unackedPackets_[ firstMissing_ ] );
	MF_ASSERT( (firstMissing_ == SEQ_NULL) == (lastMissing_ == SEQ_NULL) );
	MF_ASSERT( (firstMissing_ == SEQ_NULL) ||
			(firstMissing_ == lastMissing_) ||
			(unackedPackets_[ firstMissing_ ]->nextMissing_ <= lastMissing_) );
	MF_ASSERT( firstMissing_ <= lastMissing_ );
	MF_ASSERT( oldestUnackedSeq_ == SEQ_NULL ||
			unackedPackets_[ oldestUnackedSeq_ ] );

	while (!overflowPackets_.empty() &&
			(unackedPackets_[ smallOutSeqAt_ ] == NULL))
	{
		SeqNum currSeqNum = overflowPackets_.front()->seq();
		MF_ASSERT( currSeqNum == smallOutSeqAt_ );

		unackedPackets_[ smallOutSeqAt_ ] = overflowPackets_.front();

		smallOutSeqAt_ = seqMask( smallOutSeqAt_ + 1 );

		// Pop it off before resend lest it recurses back to delResendTimer()
		// if it has been piggybacked.
		overflowPackets_.pop_front();
		if (oldestUnackedSeq_ == SEQ_NULL)
		{
			oldestUnackedSeq_ = currSeqNum;
		}
		this->sendUnacked( *unackedPackets_[ currSeqNum ] );

	}

	return true;
}


/**
 *	This method resends any unacked packets as appropriate. This can be because
 *	of time since last sent, receiving later acks before earlier ones.
 */
void Channel::checkResendTimers()
{
	// Don't do anything if the remote process has failed
	if (hasRemoteFailed_)
	{
		WARNING_MSG( "Channel::checkResendTimers( %s ): "
			"Not doing anything due to remote process failure\n",
			this->c_str() );

		return;
	}

	// Resend "missing" packets, if appropriate
	bool resentMissing = false;
	SeqNum seq = firstMissing_;

	while (seq != SEQ_NULL)
	{
		UnackedPacket & missing = *unackedPackets_[ seq ];

		// We need to store this ahead of time because resend() can cause
		// 'missing' to be deleted (in delResendTimer()).
		SeqNum nextMissing = missing.nextMissing_;

		// If we've seen an ack for a packet that is after this one, then it
		// needs to be sent again
		if (seqLessThan( missing.lastSentAtOutSeq_, lastAck_ ))
		{
			this->resend( seq );

			resentMissing = true;
		}

		seq = nextMissing;
	}

	// We don't need to bother with the oldest unacked seq stuff below if we've
	// resent missing packets, since we've generated reliable traffic.
	if (resentMissing)
	{
		return;
	}

	// If we have unacked packets that are getting a bit old, then resend the
	// ones that are older than we'd like.  Anything that has taken more than
	// twice the RTT on the channel to come back is considered to be too old.
	if (oldestUnackedSeq_ != SEQ_NULL)
	{
		uint64 now = timestamp();
		uint64 thresh = std::max( roundTripTime_*2, minInactivityResendDelay_ );
		uint64 lastReliableSendTime = this->lastReliableSendOrResendTime();

		// We resend all unacked packets that haven't been (re)sent recently,
		// up until the first acked packet.
		SeqNum seq = oldestUnackedSeq_;

		while (seqLessThan( seq, smallOutSeqAt_ ) && unackedPackets_[ seq ])
		{
			UnackedPacket & unacked = *unackedPackets_[ seq ];

			if (now - unacked.lastSentTime_ > thresh)
			{
				if (this->nub().isVerbose())
				{
					WARNING_MSG( "Channel::checkResendTimers( %s ): "
						"Resending unacked packet #%u due to inactivity "
						"(Packet %.3fs, Channel %.3fs, RTT %.3fs)\n",
						this->c_str(),
						unacked.pPacket_->seq(),
						(now - unacked.lastSentTime_) / stampsPerSecondD(),
						(now - lastReliableSendTime) / stampsPerSecondD(),
						roundTripTime_ / stampsPerSecondD() );
					// unacked.pPacket_->debugDump();
				}

				this->resend( seq );
			}

			seq = seqMask( seq + 1 );
		}
	}
}


/**
 *  Resends an un-acked packet by the most sensible method available.
 */
void Channel::resend( SeqNum seq )
{
	++numPacketsResent_;

	UnackedPacket & unacked = *unackedPackets_[ seq ];

	// If possible, piggypack this packet onto the next outgoing bundle
	if (this->isExternal() &&
		!unacked.pPacket_->hasFlags( Packet::FLAG_IS_FRAGMENT ) &&
		(unackedPackets_[ smallOutSeqAt_ ] == NULL)) // Not going to overflow
	{
		if (this->bundle().piggyback(
				seq, unacked.reliableOrders_, unacked.pPacket_.getObject() ))
		{
			this->delResendTimer( seq );
			return;
		}
	}

	// Otherwise just send as normal.
	if (this->isInternal())
	{
		/*WARNING_MSG( "Channel::resend( %s ): Resending #%d (outSeqAt #%d)\n",
		  this->c_str(), seq, smallOutSeqAt_ );*/
	}

	// If there are any acks on this packet, then they will be resent too, but
	// it does no harm.
	this->sendUnacked( unacked );
}


/**
 *  Resends an un-acked packet by the most sensible method available.
 */
void Channel::sendUnacked( UnackedPacket & unacked )
{
	pNub_->sendPacket( addr_, unacked.pPacket_.get(), this, true );

	unacked.lastSentAtOutSeq_ = smallOutSeqAt_;
	unacked.wasResent_ = true;

	uint64 now = timestamp();
	unacked.lastSentTime_ = now;
	lastReliableResendTime_ = now;
}


/**
 *	This method is called by the Nub when it receives a packet that was sent on
 *	the other side of this channel.  It just adds an ACK to the next outgoing
 *	bundle on this channel.  If the queued ACKs haven't been sent by the time
 *	the Nub finishes processing the incoming packet, they will be sent
 *	immediately.  This allows multiple ACKs to accumulate on a single return
 *	packet (along with reply messages), whilst still guaranteeing that they will
 *	be delivered quickly.
 */
std::pair< Packet*, bool > Channel::queueAckForPacket( Packet *p, SeqNum seq,
	const Address & srcAddr )
{
	// Make sure the sequence number is valid
	if (seqMask( seq ) != seq)
	{
		ERROR_MSG( "Channel::queueAckForPacket( %s ): "
			"Got out-of-range incoming seq #%u (inSeqAt: #%u)\n",
			this->c_str(), seq, inSeqAt_ );

		return std::make_pair( (Packet*)NULL, false );
	}

	// Switch the address on this channel if necessary
	if (addr_ != srcAddr)
	{
		if (shouldAutoSwitchToSrcAddr_)
		{
			// If the packet is out of date, drop it.
			if (seqLessThan( p->channelVersion(), version_ ))
			{
				WARNING_MSG( "Channel::queueAckForPacket( %s ): "
					"Dropping packet from old addr %s (v%u < v%u)\n",
					this->c_str(), srcAddr.c_str(),
					p->channelVersion(), version_ );

				return std::make_pair( (Packet*)NULL, true );
			}

			// We switch address if the version number is acceptable.  We switch
			// on equal version numbers because the first packet from a cell
			// entity sets the address and is version 0.
			else
			{
				version_ = p->channelVersion();
				this->addr( srcAddr );
			}
		}
		else
		{
			ERROR_MSG( "Channel::queueAckForPacket( %s ): "
				"Got packet #%u from wrong source address: %s\n",
				this->c_str(), seq, srcAddr.c_str() );

			return std::make_pair( (Packet*)NULL, false );
		}
	}

	// It's possible we could get a packet from our own address with an
	// increased version in some rapid offloading situations.  Remember, at this
	// point we haven't processed the sequence number and therefore at this
	// point the packet can be out of order, therefore it's possible to get an
	// increase in version from the same address having never received a packet
	// from the intermediate offload app.
	//
	// You will almost always get a packet like this on the cell entity channel
	// straight after a restore, since the base entity channel will be a higher
	// version, so the first packet from the base will cause the version update.
	else if (shouldAutoSwitchToSrcAddr_ &&
		seqLessThan( version_, p->channelVersion() ))
	{
		version_ = p->channelVersion();

		WARNING_MSG( "Channel::queueAckForPacket( %s ): "
			"Updating to v%u without changing address\n",
			this->c_str(), version_ );
	}

	// Always add an ACK.
	int acksOnPacket = pBundle_->addAck( seq );

	// Push the outgoing bundle immediately if required
	if (pushUnsentAcksThreshold_ &&
			(acksOnPacket >= pushUnsentAcksThreshold_))
	{
		if (this->nub().isVerbose())
		{
			DEBUG_MSG( "Channel::queueAckForPacket( %s ): "
					"Pushing %d unsent ACKs due to inactivity\n",
				this->c_str(), acksOnPacket );
		}

		this->send();
	}

	// check the good case first
	if (seq == inSeqAt_)
	{
		inSeqAt_ = seqMask( inSeqAt_+1 );

		Packet * pPrev = p;
		Packet * pBufferedPacket = bufferedReceives_[ inSeqAt_ ].getObject();

		// Attach as many buffered packets as possible to this one.
		while (pBufferedPacket != NULL)
		{
			// Link it to the prev packet then remove it from the buffer.
			pPrev->chain( pBufferedPacket );
			bufferedReceives_[ inSeqAt_ ] = NULL;
			--numBufferedReceives_;

			// Advance to the next buffered packet.
			pPrev = pBufferedPacket;
			inSeqAt_ = seqMask( inSeqAt_+1 );
			pBufferedPacket = bufferedReceives_[ inSeqAt_ ].getObject();
		}

		return std::make_pair( p, true );
	}

	// see if we've got this one before. We have if seq < inSeqAt_.
	if (seqLessThan( seq, inSeqAt_ ))
	{
		if (this->nub().isVerbose())
		{
			DEBUG_MSG( "Channel::queueAckForPacket( %s ): "
					"Discarding already-seen packet #%d below inSeqAt #%d\n",
				this->c_str(), (int)seq, (int)inSeqAt_ );
		}

		this->nub().incNumDuplicatePacketsReceived();

		return std::make_pair( (Packet*)NULL, true );
	}

	// make sure it's in range
	if (seqMask(seq - inSeqAt_) > windowSize_)
	{
		WARNING_MSG( "Channel::queueAckForPacket( %s ): "
				"Sequence number #%d is way out of window #%d!\n",
			this->c_str(), (int)seq, (int)inSeqAt_ );

		return std::make_pair( (Packet*)NULL, true );
	}

	// ok - we'll buffer this packet then....
	PacketPtr & rpBufferedPacket = bufferedReceives_[ seq ];

	// ... but only if we don't already have it
	if (rpBufferedPacket != NULL)
	{
		DEBUG_MSG( "Channel::queueAckForPacket( %s ): "
			"Discarding already-buffered packet #%d\n",
			this->c_str(), (int)seq );
	}
	else
	{
		rpBufferedPacket = p;
		++numBufferedReceives_;

		DEBUG_MSG( "Channel::queueAckForPacket( %s ): "
			"Buffering packet #%d above #%d\n",
			this->c_str(), (int)seq, (int)inSeqAt_ );
	}

	// I'm afraid you're going to have to wait Mr Nub.
	return std::make_pair( (Packet*)NULL, true );
}


/**
 *  This method sets the anonymous state for this channel.
 */
void Channel::isAnonymous( bool anonymous )
{
	isAnonymous_ = anonymous;

	// Anonymity means we need keepalive checking (and vice versa).
	if (isAnonymous_)
	{
		pNub_->keepAliveChannels().addIfNecessary( *this );
	}
	else
	{
		pNub_->keepAliveChannels().delIfNecessary( *this );
	}

	// Anonymity means irregularity too.
	this->isIrregular( isAnonymous_ );
}


/**
 *  This method resends all unacked packets on this Channel, up to and including
 *  the critical packet with the highest sequence number.
 */
void Channel::resendCriticals()
{
	if (unackedCriticalSeq_ == SEQ_NULL)
	{
		WARNING_MSG( "Channel::resendCriticals( %s ): "
			"Called with no unacked criticals!\n",
			this->c_str() );

		return;
	}

	// Resend all unacked sends up to the highest critical.
	for (SeqNum seq = oldestUnackedSeq_;
		 seq != seqMask( unackedCriticalSeq_ + 1 );
		 seq = seqMask( seq + 1 ))
	{
		if (unackedPackets_[ seq ])
		{
			this->resend( seq );
		}
	}
}


/**
 *  Returns true if the next outgoing bundle on this channel should be marked
 *  with FLAG_CREATE_CHANNEL.
 */
bool Channel::shouldSendFirstReliablePacket() const
{
	return this->isInternal() &&
		(numReliablePacketsSent_ == 0) &&
		(smallOutSeqAt_ == 0);
}


/**
 *  This method configures this channel to auto switch its address to the
 *  source address of incoming packets.  Enabling this is only allowed for
 *  indexed channels.
 */
void Channel::shouldAutoSwitchToSrcAddr( bool b )
{
	shouldAutoSwitchToSrcAddr_ = b;
	MF_ASSERT( !shouldAutoSwitchToSrcAddr_ || this->isIndexed() );
}


/**
 *	This method returns a string representation of this channel which is useful
 *	in output messages.
 *
 *	Note: a static string is returned so this cannot be called twice in
 *	succession.
 */
const char * Channel::c_str() const
{
	static char dodgyString[ 40 ];

	int length = addr_.writeToString( dodgyString, sizeof( dodgyString ) );

	if (this->isIndexed())
	{
		length += bw_snprintf( dodgyString + length,
			sizeof( dodgyString ) - length,	"/%d", id_ );
	}

	// Annotate condemned channels with an exclamation mark.
	if (isCondemned_)
	{
		length += bw_snprintf( dodgyString + length,
			sizeof( dodgyString ) - length,	"!" );
	}

	return dodgyString;
}


/**
 *  This method clears the bundle on this channel and gets it ready to have a
 *  new set of messages added to it.
 */
void Channel::clearBundle()
{
	if (!pBundle_)
	{
		pBundle_ = new Bundle( pFilter_ ? pFilter_->maxSpareSize() : 0, this );
	}
	else
	{
		pBundle_->clear();
	}

	// If this channel is indexed, add the indexed channel flag to the bundle
	// now since it will have just been cleared.
	if (this->isIndexed())
	{
		pBundle_->firstPacket_->enableFlags( Packet::FLAG_INDEXED_CHANNEL );
	}

	// If this is the first reliable outbound packet, flag it.
	if (this->shouldSendFirstReliablePacket())
	{
		pBundle_->firstPacket_->enableFlags( Packet::FLAG_CREATE_CHANNEL );
	}

	// If we have a bundle primer, now's the time to call it!
	if (pBundlePrimer_)
	{
		pBundlePrimer_->primeBundle( *pBundle_ );
	}
}


/**
 *  This method sets the BundlePrimer object for this channel.  If the channel's
 *  bundle is empty, it will be primed.
 */
void Channel::bundlePrimer( BundlePrimer & primer )
{
	pBundlePrimer_ = &primer;

	if (pBundle_->numMessages() == 0)
	{
		primer.primeBundle( *pBundle_ );
	}
}


/**
 *	This method handles the channel's timer events.
 */
int Channel::handleTimeout( TimerID, void * arg )
{
	switch (reinterpret_cast<uintptr>( arg ))
	{
		case TIMEOUT_INACTIVITY_CHECK:
		{
			if (timestamp() - lastReceivedTime_ > inactivityExceptionPeriod_)
			{
				// TODO: Check that this is safe to do here
				throw NubExceptionWithAddress( REASON_INACTIVITY, addr_ );
			}
			break;
		}

		// This implements the sendEvery method.
		case TIMEOUT_CHANNEL_PUSH:
		{
			if (this->isEstablished())
			{
				this->send();
			}
			break;
		}
	}

	return 0;
}


/**
 *	This method resets this channel to be as if it had just been constructed. It
 *	will deregister the channel (but does not clear the index).
 */
void Channel::reset( const Address & newAddr, bool warnOnDiscard )
{
	// Don't do anything if the address hasn't changed.
	if (newAddr == addr_)
	{
		return;
	}

	// Clear unacked sends
	if (this->hasUnackedPackets())
	{
		int numUnacked = 0;

		for (uint i = 0; i < unackedPackets_.size(); i++)
		{
			if (unackedPackets_[i])
			{
				// if (warnOnDiscard)
				// {
				// 	unackedPackets_[i]->pPacket_->debugDump();
				// }
				++numUnacked;
				delete unackedPackets_[i];
				unackedPackets_[i] = NULL;
			}
		}

		while (!overflowPackets_.empty())
		{
			++numUnacked;
			delete overflowPackets_.front();
			overflowPackets_.pop_front();
		}

		if (warnOnDiscard && numUnacked > 0)
		{
			WARNING_MSG( "Channel::reset( %s ): "
				"Forgetting %d unacked packet(s)\n",
				this->c_str(), numUnacked );
		}
	}

	// Clear buffered receives
	if (numBufferedReceives_ > 0)
	{
		if (warnOnDiscard)
		{
			WARNING_MSG( "Channel::reset( %s ): "
				"Discarding %u buffered packet(s)\n",
				this->c_str(), numBufferedReceives_ );
		}

		for (uint i=0;
			 i < bufferedReceives_.size() && numBufferedReceives_ > 0;
			 i++)
		{
			if (bufferedReceives_[i] != NULL)
			{
				bufferedReceives_[i] = NULL;
				--numBufferedReceives_;
			}
		}
	}

	// Clear any chained fragments.
	if (pFragments_)
	{
		if (warnOnDiscard)
		{
			WARNING_MSG( "Channel::reset( %s ): "
				"Forgetting %d unprocessed packets in the fragment chain\n",
				this->c_str(), pFragments_->pChain_->chainLength() );
		}

		pFragments_ = NULL;
	}

	// Reset fields.
	inSeqAt_ = 0;
	smallOutSeqAt_ = 0;
	largeOutSeqAt_ = 0;
	lastReceivedTime_ = ::timestamp();
	lastAck_ = seqMask( smallOutSeqAt_ - 1 );
	firstMissing_ = SEQ_NULL;
	lastMissing_ = SEQ_NULL;
	oldestUnackedSeq_ = SEQ_NULL;
	lastReliableSendTime_ = 0;
	lastReliableResendTime_ = 0;
	roundTripTime_ =
		this->isInternal() ? stampsPerSecond() / 10 : stampsPerSecond();
	hasRemoteFailed_ = false;
	unackedCriticalSeq_ = SEQ_NULL;
	wantsFirstPacket_ = false;
	shouldDropNextSend_ = false;
	numPacketsSent_ = 0;
	numPacketsReceived_ = 0;
	numBytesSent_ = 0;
	numBytesReceived_ = 0;
	numPacketsResent_ = 0;
	numReliablePacketsSent_ = 0;

	// Increment the version, since we're not going to be talking to the same
	// channel on the other side anymore.
	if (this->isIndexed())
	{
		version_ = seqMask( version_ + 1 );
	}

	this->clearBundle();

	// Clear this channel from any monitoring collections.
	pNub_->irregularChannels().delIfNecessary( *this );
	pNub_->keepAliveChannels().delIfNecessary( *this );

	// not sure about cancelling the inactivity timer... but it is not
	// expected to be used on the channels we are resetting, and it is
	// the right thing to do anyway as inactivity is to be expected
	// (or is it? channel can't really stay around for too long in a
	// half-created state... which reset doesn't do anyway -
	// it just resets .... hmmm)
	if (inactivityTimerID_ != TIMER_ID_NONE)
	{
		pNub_->cancelTimer( inactivityTimerID_ );
		inactivityTimerID_ = TIMER_ID_NONE;
	}

	// If this channel was previously established, we will wait for a packet
	// with FLAG_CREATE_CHANNEL, since we don't want to accept any packets from
	// the old connection.
	if (this->isEstablished())
	{
		wantsFirstPacket_ = true;
	}

	// This handles deregistering too.
	this->addr( newAddr );

	pNub_->cancelRequestsFor( this );

	// If we're establishing this channel, call the bundle primer, since
	// we just cleared the bundle.
	if (this->isEstablished() && pBundlePrimer_)
	{
		this->bundlePrimer( *pBundlePrimer_ );
	}
}


/**
 *  This method copies configuration settings from one channel to another.
 */
void Channel::configureFrom( const Channel & other )
{
	this->isIrregular( other.isIrregular() );
	this->shouldAutoSwitchToSrcAddr( other.shouldAutoSwitchToSrcAddr() );
	this->pushUnsentAcksThreshold( other.pushUnsentAcksThreshold() );

	// We don't support setting this fields post-construction, so for now, just
	// make sure the channels match.
	MF_ASSERT( traits_ == other.traits_ );
	MF_ASSERT( minInactivityResendDelay_ == other.minInactivityResendDelay_ );
}


/**
 *  This method transfers this Channel to a different Nub.
 */
void Channel::switchNub( Nub * pDestNub )
{
	pNub_->irregularChannels().delIfNecessary( *this );
	pNub_->keepAliveChannels().delIfNecessary( *this );
	pNub_->deregisterChannel( *this );

	pNub_ = pDestNub;

	pNub_->registerChannel( *this );
	irregularIter_ = pNub_->irregularChannels().end();
	pNub_->irregularChannels().addIfNecessary( *this );
	keepAliveIter_ = pNub_->keepAliveChannels().end();
	pNub_->keepAliveChannels().addIfNecessary( *this );
}


/**
 *	This method starts detection of inactivity on this channel. If nothing is
 *	received for the input period amount of time, an INACTIVITY exception is
 *	thrown.
 *
 *	@param period	The number of seconds without receiving a packet before
 *		throwing an exception.
 *	@param checkPeriod The number of seconds between checking for inactivity.
 */
void Channel::startInactivityDetection( float period, float checkPeriod )
{
	if (inactivityTimerID_ != TIMER_ID_NONE)
	{
		pNub_->cancelTimer( inactivityTimerID_ );
	}

	inactivityExceptionPeriod_ = uint64( period * stampsPerSecond() );
	lastReceivedTime_ = timestamp();

	inactivityTimerID_ = pNub_->registerTimer( int( checkPeriod * 1000000 ),
									this, (void *)TIMEOUT_INACTIVITY_CHECK );
}


/**
 *	This method determines whether or not the given packet has
 *	been acked. If the packet falls outside the resend window
 *	(e.g. has not yet been sent) then it is considered acked.
 *
 *	To find out the sequence number of a packet, call the
 *	peekNextSequenceID method before the packet is sent.
 */
bool Channel::hasPacketBeenAcked( SeqNum seq ) const
{
	if (seqMask( (smallOutSeqAt_-1) - seq ) >= windowSize_)
	{
		return true;
	}
	else
	{
		return unackedPackets_[ seq ] == NULL;
	}
}


/**
 *	This method returns the age of the latest acked packet on the channel.
 *	It is the opposite coarse measure of channel latency to
 *	@see earliestUnackedPacketAge, as this gives a 'lower bound' kind of result.
 *	If all packets have been acked (or none have been sent) then 0 is returned.
 */
int Channel::latestAckedPacketAge() const
{
	for (uint32 look = 0; look < windowSize_; look++)
	{
		SeqNum seq = seqMask((smallOutSeqAt_-1) - look);

		if (unackedPackets_[ seq ] == NULL)
		{
			return (int)look;
		}
	}

	// channel is not looking good if we got to here!
	return (int)windowSize_ + overflowPackets_.size();
}


/**
 *	This method sets whether this channel sends irregularly and indicates that
 *	its resends are managed globally.
 */
void Channel::isIrregular( bool isIrregular )
{
	isIrregular_ = isIrregular;

	// Channels that do not send regularly are added to a collection to do their
	// resend checking periodically.
	pNub_->irregularChannels().addIfNecessary( *this );
}


/**
 * 	This method returns the next sequence ID, and then increments it.
 *
 * 	@return The next sequence ID.
 */
SeqNum Channel::useNextSequenceID()
{
	SeqNum	retSeq = largeOutSeqAt_;
	largeOutSeqAt_ = seqMask( largeOutSeqAt_ + 1 );

	if (this->isInternal())
	{
		int usage = this->sendWindowUsage();
		int & threshold = this->sendWindowWarnThreshold();

		if (usage > threshold)
		{
			WARNING_MSG( "Channel::useNextSequenceID( %s ): "
							"Send window backlog is now %d packets, "
							"exceeded previous max of %d, "
							"critical size is %u\n",
						this->c_str(), usage, threshold, windowSize_ );

			threshold = usage;
		}

		if (this->isIndexed() &&
				(s_pSendWindowCallback_ != NULL) &&
				(usage > s_sendWindowCallbackThreshold_))
		{
			(*s_pSendWindowCallback_)( *this );
		}
	}

	return retSeq;
}


/**
 * Validates whether the provided sequence number from an unreliable
 * packet looks to be valid. That is, is it larger that the previous
 * sequence number seen, within the window size considered valid.
 */
bool Channel::validateUnreliableSeqNum( const SeqNum seqNum )
{
	if (seqNum != seqMask( seqNum ))
	{
		WARNING_MSG( "Channel:validateUnreliableSeqNum: "
			"Invalid sequence number (%d).\n", seqNum );
		return false;
	}

	if (Channel::seqLessThan( seqNum, unreliableInSeqAt_ ) &&
			(unreliableInSeqAt_ != SEQ_NULL))
	{
		WARNING_MSG( "Channel:validateUnreliableSeqNum: Received an invalid "
			"seqNum (%d) on an unreliable channel. Last valid seqNum (%d)\n",
			seqNum, unreliableInSeqAt_ );
		return false;
	}

	// Only store the new seqNum if it has been completely validated.
	unreliableInSeqAt_ = seqNum;
	return true;
}


/**
 *	This method sets whether the remote process has failed.
 */
void Channel::hasRemoteFailed( bool v )
{
	hasRemoteFailed_ = v;

	// If this channel is anonymous, then no-one else is going to clean it up,
	// so have the nub clean it up now.
	if (isAnonymous_)
	{
		INFO_MSG( "Cleaning up dead anonymous channel to %s\n",
			this->c_str() );

		pNub_->delAnonymousChannel( addr_ );
	}
}


/**
 *	This method is called to indicate that a packet associated with this channel
 *	has been received.
 */
void Channel::onPacketReceived( int bytes )
{
	lastReceivedTime_ = timestamp();
	++numPacketsReceived_;
	numBytesReceived_ += bytes;
}


#if ENABLE_WATCHERS
/**
 *	This static function returns a watcher that can be used to watch Channels.
 */
WatcherPtr Channel::pWatcher()
{
	static DirectoryWatcherPtr pWatcher = NULL;

	if (pWatcher == NULL)
	{
		pWatcher = new DirectoryWatcher();

		Channel * pNull = NULL;

#define ADD_WATCHER( PATH, MEMBER )		\
		pWatcher->addChild( #PATH, makeWatcher( pNull->MEMBER ) );

		ADD_WATCHER( addr,				addr_ );
		ADD_WATCHER( packetsSent,		numPacketsSent_ );
		ADD_WATCHER( packetsReceived,	numPacketsReceived_ );
		ADD_WATCHER( bytesSent,			numBytesSent_ );
		ADD_WATCHER( bytesReceived,		numBytesReceived_ );
		ADD_WATCHER( packetsResent,		numPacketsResent_ );
		ADD_WATCHER( reliablePacketsResent,		numReliablePacketsSent_ );

		ADD_WATCHER( isIrregular,		isIrregular_ );

		pWatcher->addChild( "roundTripTime",
				makeWatcher( *pNull, &Channel::roundTripTimeInSeconds ) );
	}

	return pWatcher;
}
#endif


/**
 *	This static method sets the callback associated with the send window usage
 *	for an internal, indexed channel exceeding the sendWindowCallbackThreshold.
 */
void Channel::setSendWindowCallback( SendWindowCallback callback )
{
	s_pSendWindowCallback_ = callback;
}


/**
 *	This static method sets the threshold for when to call the send-window
 *	callback. If an internal, indexed channel's send-window get larger than
 *	this number of packets, the callback set in setSendWindowCallback is called.
 */
void Channel::sendWindowCallbackThreshold( float threshold )
{
	s_sendWindowCallbackThreshold_ = int( threshold * INDEXED_CHANNEL_SIZE );
}


/**
 *	This static method returns the threshold for when the send-window callback
 *	is called.
 */
float Channel::sendWindowCallbackThreshold()
{
	return float(s_sendWindowCallbackThreshold_)/INDEXED_CHANNEL_SIZE;
}


// -----------------------------------------------------------------------------
// Section: UnackedPacket
// -----------------------------------------------------------------------------


//PoolAllocator< SimpleMutex > Channel::UnackedPacket::s_allocator_(
//		sizeof( Channel::UnackedPacket ), "network/unackedPacketAllocator" );


Channel::UnackedPacket::UnackedPacket( Packet * pPacket ) :
	pPacket_( pPacket )
{}


/**
 *	This method reads this object from the input stream.
 */
Channel::UnackedPacket * Channel::UnackedPacket::initFromStream(
	BinaryIStream & data, uint64 timeNow )
{
	PacketPtr pPacket = Packet::createFromStream( data, Packet::UNACKED_SEND );

	if (pPacket)
	{
		UnackedPacket * pInstance = new UnackedPacket( pPacket.getObject() );

		data >> pInstance->lastSentAtOutSeq_;

		pInstance->lastSentTime_ = timeNow;
		pInstance->wasResent_ = false;
		pInstance->nextMissing_ = SEQ_NULL;

		return pInstance;
	}
	else
	{
		return NULL;
	}
}


/**
 *	This method adds this object to the input stream.
 */
void Channel::UnackedPacket::addToStream(
	UnackedPacket * pInstance, BinaryOStream & data )
{
	if (pInstance)
	{
		Packet::addToStream( data, pInstance->pPacket_.getObject(),
			Packet::UNACKED_SEND );

		data << pInstance->lastSentAtOutSeq_;
	}
	else
	{
		Packet::addToStream( data, (Packet*)NULL, Packet::UNACKED_SEND );
	}
}


// -----------------------------------------------------------------------------
// Section: ChannelOwner
// -----------------------------------------------------------------------------

/**
 *  This method switches this ChannelOwner to a different address.  We can't
 *  simply call through to Channel::addr() because there might already be an
 *  anonymous channel to that address.  We need to look it up and claim the
 *  anonymous one if it already exists.
 */
void ChannelOwner::addr( const Address & addr )
{
	MF_ASSERT( pChannel_ );

	// Don't do anything if it's already on the right address
	if (this->addr() == addr)
	{
		return;
	}

	Nub & nub = pChannel_->nub();

	// Get a new channel to the right address.
	Channel * pNewChannel = Channel::get( nub, addr );

	// Configure the new channel like the old one, and then throw it away.
	pNewChannel->configureFrom( *pChannel_ );
	pChannel_->condemn();

	// Put the new channel in its place.
	pChannel_ = pNewChannel;
}


} // namespace Mercury

// channel.cpp
