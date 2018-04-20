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
#include "packet.hpp"
#include "endpoint.hpp"
#include "channel.hpp"

#include "cstdmf/binary_stream.hpp"
#include "cstdmf/concurrency.hpp"

DECLARE_DEBUG_COMPONENT2( "Network", 0 );

namespace Mercury
{

// -----------------------------------------------------------------------------
// Section: Packet
// -----------------------------------------------------------------------------

// The default max size for a packet is the MTU of an ethernet frame, minus the
// overhead of IP and UDP headers.  If you have special requirements for packet
// sizes (e.g. your client/server connection is running over VPN) you can edit
// this to whatever you need.
const int Packet::MAX_SIZE = PACKET_MAX_SIZE;

#if 0 // disabling custom allocator
PoolAllocator<SimpleMutex> Packet::s_allocator_( sizeof( Packet ),
	   "network/packetAllocator" );
#endif

/**
 *  Constructor.
 */
Packet::Packet() :
	next_( NULL ),
	msgEndOffset_( 0 ),
	footerSize_( 0 ),
	extraFilterSize_( 0 ),
	firstRequestOffset_( 0 ),
	pLastRequestOffset_( NULL ),
	nAcks_( 0 ),
	seq_( Channel::SEQ_NULL ),
	channelID_( CHANNEL_ID_NULL ),
	channelVersion_( 0 ),
	fragBegin_( Channel::SEQ_NULL ),
	fragEnd_( Channel::SEQ_NULL ),
	checksum_( 0 )
{
	piggyFooters_.beg_ = NULL;
}


/**
 *  This method returns the total length of this packet chain.
 */
int Packet::chainLength() const
{
	int count = 1;

	for (const Packet * p = this->next(); p != NULL; p = p->next())
	{
		++count;
	}

	return count;
}


/**
 *  This method is called to inform the Packet that a new request has been
 *  added.  It updates the 'next request offset' linkage as necessary.  The
 *  value passed in is the offset of the message header.
 */
void Packet::addRequest( Offset messageStart, Offset nextRequestLink )
{
	if (firstRequestOffset_ == 0)
	{
		firstRequestOffset_ = messageStart;
	}
	else
	{
		*pLastRequestOffset_ = BW_HTONS( messageStart );
	}

	// Remember the offset of this link for next time.
	pLastRequestOffset_ = (Offset*)(data_ + nextRequestLink);

	// Mark this request as the last one on this packet (for now).
	*pLastRequestOffset_ = 0;
}


/**
 *  This method does a recv on the endpoint into this packet's data array,
 *  setting the length correctly on a successful receive.  The return value is
 *  the return value from the low-level recv() call.
 */
int Packet::recvFromEndpoint( Endpoint & ep, Address & addr )
{
	int len = ep.recvfrom( data_, MAX_SIZE,
		(u_int16_t*)&addr.port, (u_int32_t*)&addr.ip );

	if (len >= 0)
	{
		this->msgEndOffset( len );
	}

	return len;
}


/**
 *  This method writes this packet to the provided stream.  This is used when
 *  offloading entity channels and the buffered and unacked packets need to be
 *  streamed too.  Packets are streamed slightly differently depending on
 *  whether they were buffered receives or unacked sends.
 */
void Packet::addToStream( BinaryOStream & data, const Packet * pPacket,
	int state )
{
	data << uint8( pPacket != NULL );

	if (pPacket)
	{
		// Unacked sends need to have the entirety of the packet data included.
		if (state == UNACKED_SEND)
		{
			data.appendString( pPacket->data(), pPacket->totalSize() );
		}

		// Buffered receives and chained fragments should only have the
		// unprocessed part of their data included.
		else
		{
			data.appendString( pPacket->data(), pPacket->msgEndOffset() );
		}

		data << pPacket->seq() << pPacket->channelID();

		// Chained fragments need to have the fragment IDs and first request
		// offset sent too
		if (state == CHAINED_FRAGMENT)
		{
			data << pPacket->fragBegin() << pPacket->fragEnd() <<
				pPacket->firstRequestOffset();
		}
	}
}


/**
 *  This method reconstructs a packet from a stream.
 */
PacketPtr Packet::createFromStream( BinaryIStream & data, int state )
{
	uint8 hasPacket;
	data >> hasPacket;

	if (!hasPacket)
		return NULL;

	PacketPtr pPacket = new Packet();
	int length = data.readStringLength();

	memcpy( pPacket->data_, data.retrieve( length ), length );
	pPacket->msgEndOffset( length );
	data >> pPacket->seq() >> pPacket->channelID();

	// Chained fragments have more footers
	if (state == CHAINED_FRAGMENT)
	{
		data >> pPacket->fragBegin() >> pPacket->fragEnd() >>
			pPacket->firstRequestOffset();
	}

	return pPacket;
}


/**
 *	This method dumps the packets contents to log output.
 */
void Packet::debugDump() const
{
	DEBUG_MSG( "Packet length is %d\n", this->totalSize() );

	int lineSize = 1024;
	char line[ 1024 ];
	char * s = line;

	for (long i=0; i < this->totalSize(); i++)
	{
		bw_snprintf( s, lineSize, "%02x ",
				(unsigned char)this->data()[i] ); s += 3;
		lineSize -= 3;
		if (i > 0 && i % 20 == 19)
		{
			DEBUG_MSG( "%s\n", line );
			s = line;
			lineSize = 1024;
		}
	}

	if (s != line)
	{
		DEBUG_MSG( "%s\n", line );
	}
}

} // namespace Mercury

// packet.cpp
