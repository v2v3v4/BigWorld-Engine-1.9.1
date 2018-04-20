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

#include "bundle.hpp"

#ifndef CODE_INLINE
#include "bundle.ipp"
#endif

#include "cstdmf/memory_tracker.hpp"
#include "cstdmf/concurrency.hpp"
#include "network/interface_element.hpp"
#include "network/channel.hpp"

DECLARE_DEBUG_COMPONENT2( "Network", 0 )

namespace Mercury
{

// -----------------------------------------------------------------------------
// Section: Bundle
// -----------------------------------------------------------------------------

/*
How requests and replies work (so I can get it straight):

When you make a request you put it on the bundle with a 'startRequest' message.
This means the bundle takes note of it and puts extra information (a reply id)
in the message's header.

When a request handler replies to a request, it puts it on the bundle with a
'startReply' message, passing in the replyID from the broken-out header info
struct passed to it. This means the bundle adds the special message of type
'REPLY_MESSAGE_IDENTIFIER', which is always handled by the system.
*/


/**
 * 	This constructor initialises an empty bundle for writing.
 */
Bundle::Bundle( uint8 spareSize, Channel * pChannel ) :
	firstPacket_( NULL ),
	currentPacket_( NULL ),
	finalised_( false ),
	extraSize_( spareSize ),
	pChannel_( pChannel )
{
	this->clear( /* firstTime: */ true );
}

/**
 * 	This constructor initialises a bundle, given a packet chain. It
 * 	is used internally within the Nub when reconstructing bundles for reading.
 *
 * 	@param p	Linked list of packets that form this bundle.
 */
Bundle::Bundle( Packet * p ) :
	firstPacket_( p ),
	currentPacket_( p ),
	finalised_( true ),
	extraSize_( 0 ),
	pChannel_( NULL )
{
	this->clear( /* firstTime: */ true );
}


/**
 * 	This is the destructor. It releases the packets used by this bundle.
 */
Bundle::~Bundle()
{
	this->dispose();
}

/**
 * 	This method flushes the messages from this bundle making it empty.
 */
void Bundle::clear( bool firstTime )
{
	// If this isn't the first time, then we need to flush everything.
	if (!firstTime)
	{
		this->dispose();
		finalised_ = false;
	}

	reliableDriver_ = false;
	// extraSize_ set in constructors
	reliableOrdersExtracted_ = 0;
	// pChannel_ set in constructors
	isCritical_ = false;
	curIE_ = NULL;
	msglen_ = 0;
	msgbeg_ = NULL;
	msgChunkOffset_ = 0;
	msgReliable_ = false;
	msgRequest_ = false;
	numMessages_ = 0;
	numReliableMessages_ = 0;

	// If we have a packet, it means we're being called from Bundle( Packet * )
	// so we shouldn't touch it.
	if (firstPacket_ == NULL)
	{
		firstPacket_ = new Packet();
		this->startPacket( firstPacket_.getObject() );
	}
}

/**
 * 	This method releases all memory used by the Bundle.
 */
void Bundle::dispose()
{
	firstPacket_ = NULL;
	currentPacket_ = NULL;

	replyOrders_.clear();

	reliableOrders_.clear();

	for (Piggybacks::iterator iter = piggybacks_.begin();
		 iter != piggybacks_.end(); ++iter)
	{
		delete *iter;
	}

	piggybacks_.clear();

	ackOrders_.clear();
}


/**
 * 	This method returns true if the bundle is empty of messages or any
 * 	data-carrying footers.
 */
bool Bundle::isEmpty() const
{
	// We check isReliable() because that indicates whether or not a sequence
	// number will be streamed onto this bundle during Nub::send().
	bool hasData =
		numMessages_ > 0 ||
		this->isMultiPacket() ||
		this->isReliable() ||
		this->hasDataFooters();

	return !hasData;
}


/**
 * 	This method returns the accumulated size of the bundle (including
 * 	headers, and including footers if it's been sent).
 *
 * 	@return	Number of bytes in this bundle.
 */
int Bundle::size() const
{
	int	total = 0;

	for (const Packet * p = firstPacket_.getObject(); p; p = p->next())
	{
		total += p->totalSize();
	}

	return total;
}

/**
 * This method returns the accumulated size of the bundle in packets
 *
 * @return Number of packets in this bundle.
 */
int Bundle::sizeInPackets() const
{
	return firstPacket_->chainLength();
}


/**
 * 	This method starts a new message on the bundle. The expected length
 *	should only be filled in if known (and only for variable-length messages)
 *	as a hint to whether to start this message on the current packet or to
 *	bring in a new one / send the current bundle.
 *
 * 	@param ie			The type of message to start.
 * 	@param reliable		True if the message should be reliable.
 */
void Bundle::startMessage( const InterfaceElement & ie, ReliableType reliable )
{
	// Piggybacks should only be added immediately before sending.
	MF_ASSERT( !currentPacket_->hasFlags( Packet::FLAG_HAS_PIGGYBACKS ) );
	MF_ASSERT( ie.name() );

	this->endMessage();
	curIE_ = &ie;
	msgReliable_ = reliable.isReliable();
	msgRequest_ = false;
	isCritical_ = (reliable == RELIABLE_CRITICAL);
	this->newMessage();

	reliableDriver_ |= reliable.isDriver();
}

/**
 * 	This method starts a new request message on the bundle, and call
 * 	ReplyMessageHandler when the reply comes in or the
 * 	timeout (in microseconds) expires, whichever comes first.
 * 	A timeout of <= 0 means never time out (NOT recommended)
 *
 * 	@param ie			The type of request to start.
 * 	@param handler		This handler receives the reply.
 * 	@param arg			User argument that is sent to the handler.
 * 	@param timeout		Time before a timeout exception is generated.
 * 	@param reliable		True if the message should be reliable.
 */
void Bundle::startRequest( const InterfaceElement & ie,
	ReplyMessageHandler * handler,
	void * arg,
	int timeout,
	ReliableType reliable )
{
	MF_ASSERT( handler );

	if (pChannel_ && timeout != DEFAULT_REQUEST_TIMEOUT)
	{
		// Requests never timeout on channels.
		WARNING_MSG( "Bundle::startRequest(%s): "
				"Non-default timeout set on a channel bundle\n",
			pChannel_->c_str() );
	}


	this->endMessage();
	curIE_ = &ie;
	msgReliable_ = reliable.isReliable();
	msgRequest_ = true;
	isCritical_ = (reliable == RELIABLE_CRITICAL);

	// Start a new message, and reserve extra space for the reply ID and the
	// next request offset.  The reply ID is actually written in Nub::send().
	ReplyID * pReplyID = (ReplyID *)this->newMessage(
		sizeof( ReplyID ) + sizeof( Packet::Offset ) );

	Packet::Offset messageStart = currentPacket_->msgEndOffset() -
		(ie.headerSize() +
			sizeof( ReplyID ) + sizeof( Packet::Offset ));
	Packet::Offset nextRequestLink = currentPacket_->msgEndOffset() -
		sizeof( Packet::Offset );

	// Update the request tracking stuff on the current packet.
	currentPacket_->addRequest( messageStart, nextRequestLink );

	// now make and add a reply order
	ReplyOrder	ro = {handler, arg, timeout, pReplyID};
	replyOrders_.push_back(ro);
		// it'd be nice to eliminate this unnecessary copy...

	// this packet has requests
	currentPacket_->enableFlags( Packet::FLAG_HAS_REQUESTS );

	reliableDriver_ |= reliable.isDriver();
}

/**
 * 	This method starts a reply to a request message. All replies are 4-byte
 * 	variable size. The first parameter, id, should be the replyID
 * 	from the message header of the request you're replying to.
 *
 * 	@param id			The id of the message being replied to
 * 	@param reliable		True if this reply should be reliable.
 */
void Bundle::startReply( ReplyID id, ReliableType reliable )
{
	this->endMessage();
	curIE_ = &InterfaceElement::REPLY;
	msgReliable_ = reliable.isReliable();
	msgRequest_ = false;
	isCritical_ = (reliable == RELIABLE_CRITICAL);
	this->newMessage();

	reliableDriver_ |= reliable.isDriver();

	// stream on the id (counts as part of the length)
	(*this) << id;
}

/**
 * 	This method is called by the Nub to add an acknowledgement for
 * 	a packet to the current bundle.
 *
 * 	@param seq	The sequence number of the packet being acknowledged.
 *
 *  @return     The number of acks on the current packet.
 */
int Bundle::addAck( SeqNum seq )
{
	this->reserveFooter( sizeof( seq ), Packet::FLAG_HAS_ACKS );

	AckOrder ao = { currentPacket_, seq };
	ackOrders_.push_back( ao );

	currentPacket_->nAcks()++;
	currentPacket_->enableFlags( Packet::FLAG_HAS_ACKS );

	return currentPacket_->nAcks();
}


/**
 *  This method returns true if this Bundle is owned by an external channel.
 */
bool Bundle::isOnExternalChannel() const
{
	return pChannel_ && pChannel_->isExternal();
}


/**
 *  This function returns a pointer to nBytes on a bundle.
 *  It assumes that the data will not fit in the current packet,
 *  so it adds a new one. This is a private function.
 *
 *  @param nBytes	Number of bytes to reserve.
 *
 *  @return	Pointer to the reserved data.
 */
void * Bundle::sreserve( int nBytes )
{
	this->endPacket( /* multiple: */ true );
	this->startPacket( new Packet() );

	void * writePosition = currentPacket_->back();
	currentPacket_->grow( nBytes );

	MF_ASSERT( currentPacket_->freeSpace() >= 0 );
	return writePosition;
}

/**
 * 	This method reserves the given number of bytes on the footer of
 * 	the current packet. (Or onto the next packet if there's no room.)
 */
void Bundle::reserveFooter( int nBytes, Packet::Flags flag )
{
	// If there's no room on this packet, or we can't add any more ACKs,
	// terminate it and start a fresh one.
	if ((nBytes > this->freeBytesInPacket()) ||
		(flag == Packet::FLAG_HAS_ACKS &&
			currentPacket_->nAcks() >= Packet::MAX_ACKS))
	{
		this->endPacket( /* multiple: */ true );
		this->startPacket( new Packet() );
	}

	currentPacket_->reserveFooter( nBytes );
}

/**
 * 	This method finalises the bundle before it is sent (called by the Nub)
 */
void Bundle::finalise()
{
	if (finalised_ == true) return;
	finalised_ = true;

	// make sure we're not sending a packet where the message
	// wasn't properly started...
	if (msgbeg_ == NULL && currentPacket_->msgEndOffset() != msgChunkOffset_)
	{
		CRITICAL_MSG( "Bundle::finalise: "
			"data not part of message found at end of bundle!\n");
	}

	this->endMessage();
	this->endPacket( /* multiple: */ false );

	// if we don't have a reliable driver then any reliable orders present
	// are all passengers (hangers on), so get rid of them.
	if (!reliableDriver_ && this->isOnExternalChannel())
		reliableOrders_.clear();

	// msgReliable_ is only set here if there are no msgs (only footers) on
	// the bundle, but the setter wants to indicate that it should still be
	// reliable
	bool isReliable =
		(reliableOrders_.size() || msgReliable_ || numReliableMessages_ > 0);

	// For all packets that are going on an external or reliable channel ensure
	// the sequence number has room on the packet as well as the required flag.
	// All external packets have a sequence number to avoid replay attacks.
	if (isReliable || this->isOnExternalChannel())
	{
		for (Packet * p = firstPacket_.getObject(); p; p = p->next())
		{
			// If the packet didn't have a sequence number, make room for one.
			if (!p->hasFlags( Packet::FLAG_HAS_SEQUENCE_NUMBER ))
			{
				p->reserveFooter( sizeof( SeqNum ) );
				p->enableFlags( Packet::FLAG_HAS_SEQUENCE_NUMBER );
			}

			if (isReliable)
			{
				p->enableFlags( Packet::FLAG_IS_RELIABLE );
			}
		}
	}
}

/**
 *  This method starts a new packet in this bundle.
 */
void Bundle::startPacket( Packet * p )
{
	Packet * prevPacket = currentPacket_;

	// Link the new packet into the chain if necessary.
	if (prevPacket)
	{
		prevPacket->chain( p );
	}

	currentPacket_ = p;
	currentPacket_->reserveFilterSpace( extraSize_ );

	// Inherit all relevant flags from the previous packet if there was one.
	const Packet::Flags inheritFlags =
		Packet::FLAG_ON_CHANNEL |
		Packet::FLAG_IS_RELIABLE |
		Packet::FLAG_IS_FRAGMENT |
		Packet::FLAG_HAS_SEQUENCE_NUMBER |
		Packet::FLAG_INDEXED_CHANNEL;

	if (prevPacket)
	{
		currentPacket_->setFlags( prevPacket->flags() & inheritFlags );
	}
	else
	{
		currentPacket_->setFlags( 0 );
	}

	currentPacket_->msgEndOffset( Packet::HEADER_SIZE );

	// if we're in the middle of a message start the next chunk here
	msgChunkOffset_ = currentPacket_->msgEndOffset();
}

/**
 *	This method end processing of the current packet, i.e. calculate its
 *	flags, and the correct size including footers.
 */
void Bundle::endPacket( bool multiple )
{
	// if this isn't the last pack add multiple stuff
	if (multiple)
	{
		if (this->isOnExternalChannel())
		{
			// add a partial reliable order if in the middle of a message
			if (msgbeg_ != NULL && msgReliable_)
				this->addReliableOrder();

			// add a gap reliable order to mark the end of the packet
			ReliableOrder rgap = { NULL, 0, 0 };
			reliableOrders_.push_back( rgap );
		}

		// Mark the bundle as fragmented
		currentPacket_->enableFlags( Packet::FLAG_HAS_SEQUENCE_NUMBER |
			Packet::FLAG_IS_FRAGMENT );
	}

	// if we're in the middle of a message add this chunk
	msglen_ += currentPacket_->msgEndOffset() - msgChunkOffset_;
	msgChunkOffset_ = uint16( currentPacket_->msgEndOffset() );

	// Now we go through and make footer space for all our footers that had
	// their space reserved implicitly by RESERVED_FOOTER_SIZE.

	if (this->packetFlags() & Packet::FLAG_HAS_REQUESTS)
	{
		currentPacket_->reserveFooter( sizeof( Packet::Offset ) );
	}

	if (this->packetFlags() & Packet::FLAG_IS_FRAGMENT)
	{
		currentPacket_->reserveFooter( sizeof( SeqNum ) * 2 );
	}

	if (this->packetFlags() & Packet::FLAG_HAS_SEQUENCE_NUMBER)
	{
		currentPacket_->reserveFooter( sizeof( SeqNum ) );
	}

	if (this->packetFlags() & Packet::FLAG_HAS_ACKS)
	{
		// The space for the ACKs themselves has already been reserved by
		// addAck(), so we just need to reserve space for the counter.
		currentPacket_->reserveFooter( sizeof( Packet::AckCount ) );
	}

	if (this->packetFlags() & Packet::FLAG_INDEXED_CHANNEL)
	{
		currentPacket_->reserveFooter(
			sizeof( ChannelID ) + sizeof( ChannelVersion ) );
	}
}


/**
 * 	This method finalises a message. It is called from a number of places
 *	whtin Bundle when necessary.
 */
void Bundle::endMessage()
{
	// nothing to do if no message yet
	if (msgbeg_ == NULL)
	{
		MF_ASSERT( currentPacket_->msgEndOffset() == Packet::HEADER_SIZE );
		return;
	}

	// record its details if it was reliable
	if (msgReliable_)
	{
		if (this->isOnExternalChannel())
		{
			this->addReliableOrder();
		}

		msgReliable_ = false;	// for sanity
		msgRequest_ = false;
	}

	// add the amt used in this packet to the length
	msglen_ += currentPacket_->msgEndOffset() - msgChunkOffset_;
	msgChunkOffset_ = Packet::Offset( currentPacket_->msgEndOffset() );

	// fill in headers for this msg
	curIE_->compressLength( msgbeg_, msglen_, *this );

	// set msgbeg_ back to NULL
	msgbeg_ = NULL;	// no longer effective to call this twice - oh well.
}

/**
 * 	This message begins a new message, with the given number of extra bytes in
 * 	the header. These extra bytes are normally used for request information.
 *
 * 	@param extra	Number of extra bytes to reserve.
 * 	@return	Pointer to the body of the message.
 */
char * Bundle::newMessage( int extra )
{
	// figure the length of the header
	int headerLen = curIE_->headerSize();
	if (headerLen == -1)
	{
		CRITICAL_MSG( "Mercury::Bundle::newMessage: "
			"tried to add a message with an unknown length format %d\n",
			(int)curIE_->lengthStyle() );
	}

	++numMessages_;

	if (msgReliable_)
	{
		++numReliableMessages_;
	}

	// make space for the header
	MessageID * pHeader = (MessageID *)this->qreserve( headerLen + extra );

	// set the start of this msg
	msgbeg_ = (uint8*)pHeader;
	msgChunkOffset_ = Packet::Offset( currentPacket_->msgEndOffset() );

	// write in the identifier
	*(MessageID*)pHeader = curIE_->id();

	// set the length to zero
	msglen_ = 0;
	msgextra_ = extra;

	// and return a pointer to the extra data
	return (char *)(pHeader + headerLen);
}

/**
 *	This internal method adds a reliable order for the current (reliable)
 *	message. Multiple orders are necessary if the message spans packets.
 */
void Bundle::addReliableOrder()
{
	MF_ASSERT( this->isOnExternalChannel() );

	uint8 * begInCur = (uint8*)currentPacket_->data() + msgChunkOffset_;
	uint8 * begInCurWithHeader = begInCur - msgextra_ - curIE_->headerSize();

	// If this message actually began on this packet, we can start from the
	// actual message header.  Otherwise, we have to settle for the part of the
	// message that's on this packet.
	if (msgbeg_ == begInCurWithHeader)
		begInCur = begInCurWithHeader;

	ReliableOrder rello = { begInCur,
							(uint8*)currentPacket_->back() - begInCur,
							msgRequest_ };

	reliableOrders_.push_back( rello );
}


/**
 * 	This method returns the vector of the reliable orders in this bundle
 *	that reference the given packet.
 */
void Bundle::reliableOrders( Packet * p,
	const ReliableOrder *& roBeg, const ReliableOrder *& roEnd )
{
	if (!reliableOrders_.empty())
	{
		const int roSize = reliableOrders_.size();
		if (firstPacket_ == currentPacket_)
		{
			MF_ASSERT( p == currentPacket_ );
			roBeg = &*reliableOrders_.begin();
			roEnd = roBeg + roSize;
		}
		else
		{
			if (p == firstPacket_) reliableOrdersExtracted_ = 0;

			roBeg = &*reliableOrders_.begin() + reliableOrdersExtracted_;
			const ReliableOrder * roFirst = &*reliableOrders_.begin();
			for (roEnd = roBeg;
				roEnd != roFirst + roSize && roEnd->segBegin != NULL;
				++roEnd) ; // scan

			reliableOrdersExtracted_ = (roEnd+1) - &*reliableOrders_.begin();
		}
	}
	else
	{
		roBeg = 0;
		roEnd = 0;
	}
}

/**
 * 	This method grabs all the reliable data from the source packet, and
 * 	appends it to this bundle. It handles the case where the source packet
 *	contains partial messages from a multi-packet bundle. It does nothing
 *	and returns false if the reliable data cannot all fit into the current
 *	packet.
 *
 * 	@param seq	Sequence number of the packet being piggybacked.
 * 	@param reliableOrders	Vector of reliable messages.
 *  @param p    The source packet to obtain the data from.
 *
 * 	@return True if there was room to piggyback this packet.
 */
bool Bundle::piggyback( SeqNum seq, const ReliableVector& reliableOrders,
	Packet *p )
{
	Packet::Flags flags =
		Packet::FLAG_HAS_SEQUENCE_NUMBER |
		Packet::FLAG_IS_RELIABLE |
		Packet::FLAG_ON_CHANNEL;

	Packet *origPacket = currentPacket_;

	// First figure out if we have enough space to piggyback these messages.
	// Allocate for packet header, sequence number footer, and 2-byte size
	// suffix.
	uint16 totalSize =
		sizeof( Packet::Flags ) + sizeof( SeqNum ) + sizeof( int16 );

	for (ReliableVector::const_iterator it = reliableOrders.begin();
		 it != reliableOrders.end(); ++it)
	{
		totalSize += it->segLength;

		// We don't support piggybacking requests at the moment.  This is OK
		// since there are hardly any between the client and the baseapp.
		if (it->segPartOfRequest)
		{
			WARNING_MSG( "Refused to piggyback request #%d\n", it->segBegin[0] );
			return false;
		}
	}

	// We also need to figure out if the dropped packet had piggybacks on it.
	// If so, we need to preserve these on the outgoing packet.  Yes this means
	// the piggyback has piggybacks on it.  Wheeeeee.
	if (p->hasFlags( Packet::FLAG_HAS_PIGGYBACKS ))
	{
		flags |= Packet::FLAG_HAS_PIGGYBACKS;
		totalSize += p->piggyFooters().len_;
	}

	if (totalSize > this->freeBytesInPacket())
	{
		return false;
	}

	// It fits, so tag packet with piggyback and reliable flags, because we are
	// about to discard the original packet and therefore can't afford to lose
	// this packet too.
	currentPacket_->enableFlags(
		Packet::FLAG_HAS_PIGGYBACKS |
		Packet::FLAG_IS_RELIABLE |
		Packet::FLAG_HAS_SEQUENCE_NUMBER );

	// Don't include the size suffix in the packet length
	Piggyback *pPiggy = new Piggyback( p, flags, seq,
		/* len: */ totalSize - sizeof( int16 ) );

	// Add each message to the Piggyback
	for (ReliableVector::const_iterator it = reliableOrders.begin();
		 it != reliableOrders.end(); ++it)
	{
		pPiggy->rvec_.push_back( *it );
	}

	piggybacks_.push_back( pPiggy );

	// Reserve enough footer space for the piggyback.  It's OK to do this late
	// since we've already worked out that this fits on the current packet.
	this->reserveFooter( totalSize, Packet::FLAG_HAS_PIGGYBACKS );

	bool piggybackDoesNotAddPacket = origPacket == currentPacket_;
	MF_ASSERT( piggybackDoesNotAddPacket );

	return true;
}


// -----------------------------------------------------------------------------
// Section: Bundle::iterator
// -----------------------------------------------------------------------------

/**
 * 	This method returns an iterator pointing to the first message in a bundle.
 */
Bundle::iterator Bundle::begin()
{
	return Bundle::iterator( firstPacket_.getObject() );
}

/**
 * 	This method returns an iterator pointing after the last message in a bundle.
 */
Bundle::iterator Bundle::end()
{
	return Bundle::iterator( NULL );
}

/**
 * 	This is the constructor for the Bundle iterator.
 */
Bundle::iterator::iterator( Packet * first ) :
	bodyEndOffset_( 0 ),
	offset_( 0 ),
	dataOffset_( 0 ),
	dataLength_( 0 ),
	dataBuffer_( NULL )
{
	// find the first packet with body data
	// (can have no body if only footers in packet)
	for (cursor_ = first; cursor_ != NULL; cursor_ = cursor_->next())
	{
		this->nextPacket();
		if (offset_ < bodyEndOffset_) break;
	}
}

/**
 * 	This is the copy constructor for the Bundle iterator.
 */
Bundle::iterator::iterator( const Bundle::iterator & i ) :
	cursor_( i.cursor_ ),
	bodyEndOffset_( i.bodyEndOffset_ ),
	offset_( i.offset_ ),
	dataOffset_( i.dataOffset_ ),
	dataLength_( i.dataLength_ ),
	dataBuffer_( NULL )
{
	if (i.dataBuffer_ != NULL)
	{
		dataBuffer_ = new char[dataLength_];
		memcpy( dataBuffer_, i.dataBuffer_, dataLength_ );
	}
}

/**
 * 	This is the destructor for the Bundle iterator.
 */
Bundle::iterator::~iterator()
{
	if (dataBuffer_ != NULL)
	{
		delete [] dataBuffer_;
		dataBuffer_ = NULL;
	}
}


using namespace std;

/**
 * 	This is the assignment operator for the Bundle iterator.
 */
const Bundle::iterator & Bundle::iterator::operator=(
	const Bundle::iterator & i )
{
	/*
	cursor_ = i.cursor_;
	bodyEndOffset_ = i.bodyEndOffset_;
	offset_ = i.offset_;
	dataOffset_ = i.dataOffset_;
	dataLength_ = i.dataLength_;
	dataBuffer_ = NULL;

	if (i.dataBuffer_ != NULL)
	{
		dataBuffer_ = new char[dataLength_];
		memcpy( dataBuffer_, i.dataBuffer_, dataLength_ );
	}
	*/
	if (this != &i)
	{
		this->~iterator();
		new (this)iterator( i );
	}
	return *this;
}


/**
 * 	This method sets up the iterator for the packet now at the cursor.
 */
void Bundle::iterator::nextPacket()
{
	nextRequestOffset_ = cursor_->firstRequestOffset();
	bodyEndOffset_ = cursor_->msgEndOffset();
	offset_ = cursor_->body() - cursor_->data();
}


/**
 * 	This method returns the identifier of the message that this iterator is
 *	currently pointing to.
 */
MessageID Bundle::iterator::msgID() const
{
	return *(MessageID*)(cursor_->data() + offset_);
}


/**
 *	This method unpacks the current message using the given
 *	interface element.
 *
 *	@param ie	InterfaceElement for the current message.
 *
 *	@return		Header describing the current message.
 */
UnpackedMessageHeader & Bundle::iterator::unpack( const InterfaceElement & ie )
{
	uint16	msgbeg = offset_;

	// read the standard header
	if (int(offset_) + ie.headerSize() > int(bodyEndOffset_))
	{
		ERROR_MSG( "Bundle::iterator::unpack( %s ): "
			"Not enough data on stream at %d for header (%d bytes, needed %d)\n",
			ie.name(), (int)offset_, (int)bodyEndOffset_ - (int)offset_,
			ie.headerSize() );

		goto error;
	}

	curHeader_.identifier = this->msgID();
	curHeader_.length = ie.expandLength( cursor_->data() + msgbeg, cursor_ );

	// If length is -1, then chances are we've had an overflow
	if (curHeader_.length == -1)
	{
		ERROR_MSG( "Bundle::iterator::unpack( %s ): "
			"Error unpacking header length at %d\n",
			ie.name(), (int)offset_ );

		goto error;
	}

	msgbeg += ie.headerSize();

	// let's figure out some flags
	if (nextRequestOffset_ != offset_)
	{
		curHeader_.flags = 0;
	}
	else
	{
		if (int(msgbeg) + sizeof(int)+sizeof(uint16) > int(bodyEndOffset_))
		{
			ERROR_MSG( "Bundle::iterator::unpack( %s ): "
				"Not enough data on stream at %hu for request ID and NRO "
				"(%hu left, needed %zu)\n",
				ie.name(), offset_, bodyEndOffset_ - msgbeg,
				sizeof( int ) + sizeof( uint16 ) );

			goto error;
		}

		curHeader_.replyID = BW_NTOHL( *(ReplyID*)(cursor_->data() + msgbeg) );
		msgbeg += sizeof(int);

		nextRequestOffset_ = BW_NTOHS( *(Packet::Offset*)(cursor_->data() + msgbeg) );
		msgbeg += sizeof(uint16);

		curHeader_.flags = Packet::FLAG_HAS_REQUESTS;
	}

	// and set up the fields about the message data
	if ((int(msgbeg) + curHeader_.length > int(bodyEndOffset_)) &&
		(cursor_->next() == NULL))
	{
		ERROR_MSG( "Bundle::iterator::unpack( %s ): "
			"Not enough data on stream at %d for payload (%d left, needed %d)\n",
			ie.name(), (int)offset_, (int)bodyEndOffset_ - msgbeg,
			curHeader_.length );

		goto error;
	}

	dataOffset_ = msgbeg;
	dataLength_ = curHeader_.length; // (copied since curHeader mods allowed)

	// If this is a special case of data length (where we put a four byte size on
	// the end), we need to now disregard these bytes.
	if (!ie.canHandleLength( dataLength_ ))
	{
		dataLength_ += sizeof( int32 );
	}

	return curHeader_;

error:
	curHeader_.flags = Packet::FLAG_IS_FRAGMENT; // fragment => corrupted
	ERROR_MSG( "Bundle::iterator::unpack: Got corrupted message header\n" );
	return curHeader_;
}


/**
 * 	This method returns the data for the message that the iterator
 * 	is currently pointing to.
 *
 * 	@return 	Pointer to message data.
 */
const char * Bundle::iterator::data()
{
	// does this message go off the end of the packet?
	if (dataOffset_ + dataLength_ <= bodyEndOffset_)
	{
		// no, ok, we're safe
		return cursor_->data() + dataOffset_;
	}

	// is there another packet? assert that there is because 'unpack' would have
	// flagged an error if the next packet was required but missing
	MF_ASSERT( cursor_->next() != NULL );
	if (cursor_->next() == NULL) return NULL;
	// also assert that data does not start mid-way into the next packet
	MF_ASSERT( dataOffset_ <= bodyEndOffset_ );	// (also implied by 'unpack')

	// is the entirety of the message data on the next packet?
	if (dataOffset_ == bodyEndOffset_ &&
		Packet::HEADER_SIZE + dataLength_ <= cursor_->next()->msgEndOffset())
	{
		// yes, easy then
		return cursor_->next()->body();
	}

	// ok, it's half here and half there, time to make a temporary buffer.
	// note that a better idea might be to return a stream from this function.
	dataBuffer_ = new char[dataLength_];
	Packet *thisPack = cursor_;
	uint16 thisOff = dataOffset_;
	uint16 thisLen;
	for (int len = 0; len < dataLength_; len += thisLen)
	{
		if (thisPack == NULL)
		{
			DEBUG_MSG( "Bundle::iterator::data: "
				"Run out of packets after %d of %d bytes put in temp\n",
				len, dataLength_ );
			return NULL;
		}
		thisLen = thisPack->msgEndOffset() - thisOff;
		if (thisLen > dataLength_ - len) thisLen = dataLength_ - len;
		memcpy( dataBuffer_ + len, thisPack->data() + thisOff, thisLen );
		thisPack = thisPack->next();
		thisOff = Packet::HEADER_SIZE;
	}
	return dataBuffer_;
}

/**
 * 	This operator advances the iterator to the next message.
 */
void Bundle::iterator::operator++(int)
{
	if (dataBuffer_ != NULL)
	{
		delete [] dataBuffer_;
		dataBuffer_ = NULL;
	}

	int biggerOffset = int(dataOffset_) + dataLength_;
	while (biggerOffset >= int(bodyEndOffset_))
	{
		// use up the data in this packet
		biggerOffset -= bodyEndOffset_;

		// move onto the next packet
		cursor_ = cursor_->next();
		if (cursor_ == NULL) break;

		// set up for the next packet
		this->nextPacket();

		// data starts after the header of the next packet
		biggerOffset += offset_;
	}
	offset_ = biggerOffset;
}


/**
 * 	This operator returns true if the given iterator is pointing to
 * 	the same message as this one.
 */
bool Bundle::iterator::operator==(const iterator & x) const
{
	return cursor_ == x.cursor_ && (cursor_ == NULL || offset_ == x.offset_);
}

/**
 * 	This operator returns true if the given iterator is pointing
 * 	to a different message from this one.
 */
bool Bundle::iterator::operator!=(const iterator & x) const
{
	return cursor_ != x.cursor_ || (cursor_ != NULL && offset_ != x.offset_);
}


// -----------------------------------------------------------------------------
// Section: UnpackedMessageHeader
// -----------------------------------------------------------------------------

/**
 *	This method returns the name of the message.
 */
const char * UnpackedMessageHeader::msgName() const
{
	return pNub ? pNub->msgName( identifier ) : "";
}


// -----------------------------------------------------------------------------
// Section: BundleSendingMap
// -----------------------------------------------------------------------------


/**
 *  This method returns the bundle for the given address, mapping the
 *  channel in if necessary.
 */
Bundle & BundleSendingMap::operator[]( const Address & addr )
{
	Channels::iterator iter = channels_.find( addr );

	if (iter != channels_.end())
	{
		return iter->second->bundle();
	}
	else
	{
		Channel * pChannel = nub_.findChannel( addr, true );
		channels_[ addr ] = pChannel;
		return pChannel->bundle();
	}
}


/**
 *  This method sends all the pending bundles on channels in this map.
 */
void BundleSendingMap::sendAll()
{
	for (Channels::iterator iter = channels_.begin();
		 iter != channels_.end(); ++iter)
	{
		iter->second->send();
	}

	channels_.clear();
}



} // namespace Mercury

// bundle.cpp
