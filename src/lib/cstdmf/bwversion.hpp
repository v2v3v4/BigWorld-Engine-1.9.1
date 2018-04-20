/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BWVERSION_HPP
#define BWVERSION_HPP

#include <string>

#include "stdmf.hpp"

namespace BWVersion
{
	uint16 majorNumber();
	uint16 minorNumber();
	uint16 patchNumber();
	uint16 reservedNumber();

	void majorNumber( uint16 number );
	void minorNumber( uint16 number );
	void patchNumber( uint16 number );
	void reservedNumber( uint16 number );

	const std::string & versionString();
}

#endif // BWVERSION_HPP
