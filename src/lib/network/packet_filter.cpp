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

#include "packet_filter.hpp"

#ifndef CODE_INLINE
#include "packet_filter.ipp"
#endif

#include "channel.hpp"

namespace Mercury
{

// ----------------------------------------------------------------
// Section: PacketFilter
// ----------------------------------------------------------------

/*
 *	Documented in header file.
 */
Reason PacketFilter::send(
	Nub & nub, const Mercury::Address & addr, Packet * pPacket )
{
	return nub.basicSendWithRetries( addr, pPacket );
}


/**
 *	Documented in header file.
 */
Reason PacketFilter::recv(
	Nub & nub, const Mercury::Address & addr, Packet * pPacket )
{
	return nub.processFilteredPacket( addr, pPacket );
}

} // namespace Mercury

// packet_filter.cpp
