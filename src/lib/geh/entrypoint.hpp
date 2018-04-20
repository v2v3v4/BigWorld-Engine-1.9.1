/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _ENTRY_POINT_HPP__
#define _ENTRY_POINT_HPP__

#include "handle.hpp"
#include "show_crash_msg.hpp"
#include <shlwapi.h>
#pragma comment( lib, "shlwapi.lib" )

#define HOOK( entryPoint )										\
	extern "C" int entryPoint();								\
	extern "C" int GEH##entryPoint()							\
	{															\
		if( StrStr( GetCommandLine(), "-crashdump" ) )			\
		{														\
			showDumpMsg();										\
			ExitProcess(0);										\
		}														\
		setupHandlers();										\
		return entryPoint();									\
	}

#endif//_ENTRY_POINT_HPP__
