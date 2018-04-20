/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MERCURY_H
#define MERCURY_H

#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "network/basictypes.hpp"
#include "network/endpoint.hpp"
#include "network/machine_guard.hpp"

#include "cstdmf/stdmf.hpp"
#include "cstdmf/binary_stream.hpp"

#include "cstdmf/timestamp.hpp"

#include "interface_element.hpp"

/**
 *	This namespace contains most of the Mercury library. The Mercury library
 *	is responsible for most of the communication in BigWorld.
 */
namespace Mercury
{
class Bundle;
class InputMessageHandler;
class Nub;
class Packet;

// forward declarations
class Channel;


/**
 * 	The InterfaceMinder class manages a set of interfaces.
 * 	It provides an iterator for iterating over this set of
 * 	interfaces. This is what is needed by a Nub for the
 * 	serveInterface method.
 *
 * 	@see Nub::serveInterface
 *
 * 	@ingroup mercury
 */
class InterfaceMinder
{
public:
	InterfaceMinder( const char * name );

	InterfaceElement & add( const char * name, int8 lengthStyle,
			int lengthParam, InputMessageHandler * pHandler = NULL );

	InputMessageHandler * handler( int index );
	void handler( int index, InputMessageHandler * pHandler );
	const InterfaceElement & interfaceElement( uint8 id ) const;

	void registerWithNub( Nub & nub );
	Reason registerWithMachined( Nub & nub, int id ) const;

private:
	InterfaceElements		elements_;
	const char				*name_;
};

};	// end of namespace Mercury

#ifdef CODE_INLINE
#include "mercury.ipp"
#endif

#endif // MERCURY_H
