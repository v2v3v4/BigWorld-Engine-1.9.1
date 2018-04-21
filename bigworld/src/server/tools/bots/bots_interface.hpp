/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#if defined( DEFINE_INTERFACE_HERE ) || defined( DEFINE_SERVER_HERE )
	#undef BOTS_INTERFACE_HPP
#endif

#ifndef BOTS_INTERFACE_HPP
#define BOTS_INTERFACE_HPP

#include "network/interface_minder.hpp"

#define MF_BOTS_MSG( NAME )												\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,									\
		BotsStructMessageHandler< BotsInterface::NAME##Args >,			\
		&MainApp::NAME )												\


// -----------------------------------------------------------------------------
// Section: Interior interface
// -----------------------------------------------------------------------------

#pragma pack(push,1)
BEGIN_MERCURY_INTERFACE( BotsInterface )
	
END_MERCURY_INTERFACE()
#pragma pack(pop)

#endif // BOTS_INTERFACE_HPP
