/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef DB_INTERFACE_UTILS_HPP
#define DB_INTERFACE_UTILS_HPP

#include "pyscript/script.hpp"
#include "network/channel.hpp"

namespace Mercury
{
	class Nub;
}
class BinaryIStream;
class BinaryOStream;

namespace DBInterfaceUtils
{
	bool executeRawDatabaseCommand( const std::string & command,
		PyObjectPtr pResultHandler, Mercury::Channel & channel );

	bool executeRawDatabaseCommand( const std::string & command,
		PyObjectPtr pResultHandler, Mercury::Nub& nub,
		const Mercury::Address& dbMgrAddr );

	/**
	 *	This class is used to represent a Binary Large OBject.
	 */
	struct Blob
	{
		const char*	pBlob;
		uint32		length;

		Blob() : pBlob( NULL ), length( 0 ) {}	// NULL blob
		Blob( const char * pData, uint32 len ) : pBlob( pData ), length( len )
		{}

		bool isNull() const 	{ return (pBlob == NULL); }
	};
	void addPotentialNullBlobToStream( BinaryOStream& stream, const Blob& blob );
	Blob getPotentialNullBlobFromStream( BinaryIStream& stream );
}

// These constants apply to the BaseAppIntInterface::logOnAttempt message.
// They are put here due to some strange multiple definition problem caused
// when DEFINE_INTERFACE_HERE and/or DEFINE_SERVER_HERE are defined.
namespace BaseAppIntInterface
{
	const uint8 LOG_ON_ATTEMPT_REJECTED = 0;
	const uint8 LOG_ON_ATTEMPT_TOOK_CONTROL = 1;
	const uint8 LOG_ON_ATTEMPT_NOT_EXIST = 2;
}

#endif // DB_INTERFACE_UTILS_HPP
