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

#include "basictypes.hpp"
#include "cstdmf/binary_stream.hpp"
#include "cstdmf/watcher.hpp"

#ifdef _WIN32
#ifndef _XBOX360
#include <Winsock.h>
#endif
#elif defined( PLAYSTATION3 )
#include <netinet/in.h>
#else
#include <arpa/inet.h>
#endif

DECLARE_DEBUG_COMPONENT2( "Network", 0 )

// -----------------------------------------------------------------------------
// Section: Direction3D
// -----------------------------------------------------------------------------

/**
 *  Output streaming for directions.
 */
BinaryOStream& operator<<( BinaryOStream &out, const Direction3D &d )
{
	return out << d.roll << d.pitch << d.yaw;
}


/**
 *  Input streaming for directions.
 */
BinaryIStream& operator>>( BinaryIStream &in, Direction3D &d )
{
	return in >> d.roll >> d.pitch >> d.yaw;
}


// -----------------------------------------------------------------------------
// Section: Address
// -----------------------------------------------------------------------------

namespace Mercury
{

char Address::s_stringBuf[ 2 ][ Address::MAX_STRLEN ];
int Address::s_currStringBuf = 0;
const Address Address::NONE( 0, 0 );

/**
 *	This method write the IP address to the input string.
 */
int Address::writeToString( char * str, int length ) const
{
	uint32	hip = ntohl( ip );
	uint16	hport = ntohs( port );

	return bw_snprintf( str, length,
		"%d.%d.%d.%d:%d",
		(int)(uchar)(hip>>24),
		(int)(uchar)(hip>>16),
		(int)(uchar)(hip>>8),
		(int)(uchar)(hip),
		(int)hport );
}


/**
 *	This operator returns the address as a string.
 *	Note that it uses a static string, so the address is only valid until
 *	the next time this operator is called. Use with care when dealing
 *	with multiple threads.
 */
char * Address::c_str() const
{
	char * buf = Address::nextStringBuf();
	this->writeToString( buf, MAX_STRLEN );
    return buf;
}


/**
 *	This operator returns the address as a string excluding the port.
 *	Note that it uses a static string, so the address is only valid until
 *	the next time an address is converted to a string. Use with care when
 *	dealing with multiple threads.
 */
const char * Address::ipAsString() const
{
	uint32	hip = ntohl( ip );
	char * buf = Address::nextStringBuf();

	bw_snprintf( buf, MAX_STRLEN, "%d.%d.%d.%d",
		(int)(uchar)(hip>>24),
		(int)(uchar)(hip>>16),
		(int)(uchar)(hip>>8),
		(int)(uchar)(hip) );

    return buf;
}


#if ENABLE_WATCHERS
/**
 * 	This method returns a watcher for this address.
 */
Watcher & Address::watcher()
{
	// TODO: This is deprecated. The above should be used instead.
	static MemberWatcher<char *,Address>	* watchMe = NULL;

	if (watchMe == NULL)
	{
		watchMe = new MemberWatcher<char *,Address>(
			*((Address*)NULL),
			&Address::operator char*,
			static_cast< void (Address::*)( char* ) >( NULL )
			);
	}

	return *watchMe;
}
#endif


/**
 *  Output streaming for addresses.  Note that we don't use the streaming
 *  operators because they will do endian conversions on big endian systems.
 *  These values need to be in the same byte order on both systems so we just
 *  use the raw methods.
 */
BinaryOStream& operator<<( BinaryOStream &os, const Address &a )
{
	os.insertRaw( a.ip );
	os.insertRaw( a.port );
	os << a.salt;

	return os;
}


/**
 *  Input streaming for addresses.
 */
BinaryIStream& operator>>( BinaryIStream &is, Address &a )
{
	is.extractRaw( a.ip );
	is.extractRaw( a.port );
	is >> a.salt;

	return is;
}


/**
 *  This method returns the next buffer to be used for making string
 *  representations of addresses.  It just flips between the two available
 *  buffers.
 */
char * Address::nextStringBuf()
{
	s_currStringBuf = (s_currStringBuf + 1) % 2;
	return s_stringBuf[ s_currStringBuf ];
}

} // namespace Mercury





// basictypes.cpp
