/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "db_status.hpp"

#include "network/watcher_glue.hpp"

/**
 * 	Initialiser
 */
DBStatus::DBStatus( Status status, const std::string& detail ) :
	status_( status ),
	detail_( detail )
{}

/**
 * 	Register our watchers.
 */
void DBStatus::registerWatchers()
{
	MF_WATCH( "status", status_, Watcher::WT_READ_ONLY,
			"Status of this process. Mainly relevant during startup and "
			"shutdown" );
	MF_WATCH( DBSTATUS_WATCHER_STATUS_DETAIL_PATH, detail_,
			// Slightly dodgy, but consolidate_dbs actually updates our watcher.
			Watcher::WT_READ_WRITE,
			"Human readable information about the current status of this "
			"process. Mainly relevant during startup and shutdown." );
	MF_WATCH( "hasStarted", *this, &DBStatus::hasStarted );
}

/**
 * 	Sets the current status
 */
void DBStatus::set( Status status, const std::string& detail )
{
	status_ = status;
	detail_ = detail;
}
