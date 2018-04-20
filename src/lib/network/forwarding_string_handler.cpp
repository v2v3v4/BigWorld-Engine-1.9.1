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
#include <stdarg.h>

#include <vector>

#include "network/forwarding_string_handler.hpp"
#include "cstdmf/memory_stream.hpp"
#include "network/bsd_snprintf.h"

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Network", 0 );

ForwardingStringHandler::ForwardingStringHandler(
		const char *fmt, bool isSuppressible ) :
	fmt_( fmt ),
	numRecentCalls_( 0 ),
	isSuppressible_( isSuppressible )
{
	handleFormatString( fmt, *this );
}

void ForwardingStringHandler::onToken( char type, int cflags, int min, int max,
	int flags, uint8 base, int vflags )
{
	fmtData_.push_back( FormatData( type, cflags, vflags ) );
}

void ForwardingStringHandler::parseArgs( va_list argPtr, MemoryOStream &os )
{
	va_list args;
#ifdef _WIN32
	memcpy( &args, &argPtr, sizeof(va_list) );
#else
	va_copy( args, argPtr );
#endif

	for (unsigned i=0; i < fmtData_.size(); i++)
	{
		FormatData &fd = fmtData_[i];

		if (fd.vflags_ & VARIABLE_MIN_WIDTH)
			os << (WidthType)va_arg( args, int );
		if (fd.vflags_ & VARIABLE_MAX_WIDTH)
			os << (WidthType)va_arg( args, int );

		switch (fd.type_)
		{
		case 'd':
		{
			switch (fd.cflags_)
			{
			case DP_C_SHORT:
				os << (short)va_arg( args, int ); break;
			case DP_C_LONG:
				os << va_arg( args, long ); break;
			case DP_C_LLONG:
				os << va_arg( args, long long ); break;
			default:
				os << va_arg( args, int ); break;
			}
			break;
		}

		case 'o':
		case 'u':
		case 'x':
		{
			switch (fd.cflags_)
			{
			case DP_C_SHORT:
				os << (unsigned short)va_arg( args, unsigned int ); break;
			case DP_C_LONG:
				//The XBOX port does not like us using unsigned longs here
				//They both get streamed off the same so who cares?
				os << (long)va_arg( args, unsigned long ); break;
			case DP_C_LLONG:
				os << va_arg( args, unsigned long long ); break;
			default:
				os << va_arg( args, unsigned int ); break;
			}
			break;
		}

		case 'f':
		case 'e':
		case 'g':
		{
			// TODO: Consider downcasting doubles to floats here for logging
			switch (fd.cflags_)
			{
			case DP_C_LDOUBLE: os << va_arg( args, LDOUBLE ); break;
			default: os << va_arg( args, double ); break;
			}
			break;
		}

		case 's':
		{
			char *tmpArg = va_arg( args, char* );
			if (tmpArg == NULL)
			{
				os << "(null)";
			}
			else
			{
				os << tmpArg;
			}

			break;
		}

		case 'p':
		{
			os << va_arg( args, void* );
			break;
		}

		case 'c':
		{
			os << (char)va_arg( args, int );
			break;
		}

		case '*':
		{
			os << va_arg( args, int );
			break;
		}

		default:
			printf( "ForwardingStringHandler::onToken() Unknown type '%c'\n", fd.type_ );
			break;
		}
	}

	va_end( args );
}
