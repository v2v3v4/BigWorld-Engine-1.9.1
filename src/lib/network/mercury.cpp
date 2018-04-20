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

#include "network/mercury.hpp"

#ifndef CODE_INLINE
#include "mercury.ipp"
#endif

#include "nub.hpp"

DECLARE_DEBUG_COMPONENT2( "Network", 0 );

namespace Mercury
{



// -----------------------------------------------------------------------------
// Section: InterfaceMinder
// -----------------------------------------------------------------------------

/**
 * 	This method adds an interface element (Mercury method) to the interface minder.
 *  @param name             Name of the interface element.
 * 	@param lengthStyle		Specifies whether the message is fixed or variable.
 *	@param lengthParam		This depends on lengthStyle.
 *	@param pHandler			The message handler for this interface.
 */
InterfaceElement & InterfaceMinder::add( const char * name,
	int8 lengthStyle, int lengthParam, InputMessageHandler * pHandler )
{
	// Set up the new bucket and add it to the list
	InterfaceElement element( name, elements_.size(), lengthStyle, lengthParam,
		pHandler );

	elements_.push_back( element );
	return elements_.back();
}


/**
 * 	This method registers all the minded interfaces with a Nub.
 *
 * 	@param nub				The nub with which to register the interfaces.
 */
void InterfaceMinder::registerWithNub( Nub & nub )
{
	for (uint i=0; i < elements_.size(); ++i)
	{
		InterfaceElement & element = elements_[i];
		nub.serveInterfaceElement( element, i, element.pHandler() );
	}
}


/**
 *  This method registers this interface with machined on behalf of the nub.
 */
Reason InterfaceMinder::registerWithMachined( Nub & nub, int id ) const
{
	return nub.registerWithMachined( name_, id );
}


/**
 *	This function converts a watcher string to an address.
 */
bool watcherStringToValue( const char * valueStr, Mercury::Address & value )
{
	int a1, a2, a3, a4, a5;

	if (sscanf( valueStr, "%d.%d.%d.%d:%d",
				&a1, &a2, &a3, &a4, &a5 ) != 5)
	{
		WARNING_MSG( "watcherStringToValue: "
				"Cannot convert '%s' to an Address.\n", valueStr );
		return false;
	}

	value.ip = (a1 << 24)|(a2 << 16)|(a3 << 8)|a4;

	value.port = uint16(a5);
	value.port = ntohs( value.port );
	value.ip = ntohl( value.ip );

	return true;
}


/**
 *	This function converts an address to a watcher string.
 */
std::string watcherValueToString( const Mercury::Address & value )
{
	return (char *)value;
}

}	// end of namespace Mercury


#ifdef CHECK_LOSS
void checkLoss()
{
	FILE * f = fopen( "/proc/net/snmp", "r" );
	if (!f)
	{
		ERROR_MSG( "checkLoss(): Could not open /proc/net/snmp: %s\n",
			strerror( errno ) );
		return;
	}
	char	aline[256];

	int ipCount = 0;
	int udpCount = 0;

	int32 inDiscards = 0;
	int32 outDiscards = 0;
	int32 udpErrors = 0;

	while (fgets( aline, 256, f ) != NULL)
	{
		if ((strncmp( aline, "Ip:", 3 ) == 0) && (++ipCount == 2))
		{
			// Forwarding DefaultTTL InReceives InHdrErrors InAddrErrors
			// ForwDatagrams InUnknownProtos InDiscards InDelivers
			// OutRequests OutDiscards OutNoRoutes ReasmTimeout ReasmReqds
			// ReasmOKs ReasmFails FragOKs FragFails FragCreates

			int rv = sscanf( aline, "Ip:%*d%*d%*d%*d%*d"
					"%*d%*d%d%*d"
					"%*d%d%*d%*d%*d"
					"%*d%*d%*d%*d%*d", &inDiscards, &outDiscards );
			if (rv != 2)
			{
				ERROR_MSG( "inDiscards = %d. outDiscards = %d. rv = %d\n",
						inDiscards, outDiscards, rv );
			}
		}

		if ((strncmp( aline, "Udp:", 4 ) == 0) && (++udpCount == 2))
		{
			int rv = sscanf( aline, "Udp:%*d%*d%d", &udpErrors );

			if (rv != 1)
			{
				ERROR_MSG( "udpErrors = %d rv=%d\n", udpErrors, rv );
			}
		}
	}

	fclose(f);

	static int32 old_inDiscards = 0;
	static int32 old_outDiscards = 0;
	static int32 old_udpErrors = 0;

	if (old_inDiscards != inDiscards)
	{
		DEBUG_MSG( "inDiscards: %d = (%d - %d)\n",
				inDiscards - old_inDiscards,
				inDiscards, old_inDiscards );
		old_inDiscards = inDiscards;
	}

	if (old_outDiscards != outDiscards)
	{
		DEBUG_MSG( "outDiscards: %d = (%d - %d)\n",
				outDiscards - old_outDiscards,
				outDiscards, old_outDiscards );
		old_outDiscards = outDiscards;
	}

	if (old_udpErrors != udpErrors)
	{
		DEBUG_MSG( "udpErrors: %d = (%d - %d)\n",
				udpErrors - old_udpErrors,
				udpErrors, old_udpErrors );
		old_udpErrors = udpErrors;
	}
}
#endif // CHECK_LOSS

// mercury.cpp
