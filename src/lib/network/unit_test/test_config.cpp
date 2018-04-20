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

#include <stdio.h>
#include "network/nub.hpp"

/**
 *  This test verifies that the local machine is correctly configured for
 *  running unit tests.
 */
TEST( Config )
{
	Endpoint ep;
	ep.socket( SOCK_DGRAM );

	ASSERT_WITH_MESSAGE( ep.good(), "Couldn't bind endpoint" );

	ASSERT_WITH_MESSAGE( ep.setBufferSize( SO_RCVBUF,
			Mercury::Nub::RECV_BUFFER_SIZE ),
		"Insufficient recv buffer to run tests" );
}
