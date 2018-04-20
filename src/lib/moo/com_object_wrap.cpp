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
#include "com_object_wrap.hpp"

#if PROFILE_D3D_RESOURCE_RELEASE

bool g_doProfileD3DResourceRelease = false;

// Types for storing release timings
typedef std::pair< uint64, uint32> TimeAndCounter;
typedef std::map< std::string, TimeAndCounter > Record;
static Record record;


// Use stack tracker to work out what the caller was for ComObject release,
// and tabulate each type of release.
void ProfileD3DResourceRelease( uint64 t )
{
	if ( !g_doProfileD3DResourceRelease )
		return;

	std::string callerName = StackTracker::getStackItem( 0 );
	Record::iterator ri = record.find( callerName );

	if ( ri != record.end() )
	{
		ri->second.first += t;
		ri->second.second += 1;
	}
	else
	{
		record[ callerName ] = std::make_pair(t,1);
	}
}

void DumpD3DResourceReleaseResults()
{
	Record::const_iterator ri = record.begin();

	INFO_MSG( "Dumping D3D resource release results...\n\n" );
	INFO_MSG( "Resource, Total time, Total calls\n" );

	while ( ri != record.end() )
	{
		double seconds = double(ri->second.first)/double( stampsPerSecond() );

		INFO_MSG("%s, %f, %d\n",
			ri->first.c_str(), seconds, ri->second.second );
		ri++;
	}
}

#endif // PROFILE_D3D_RESOURCE_RELEASE