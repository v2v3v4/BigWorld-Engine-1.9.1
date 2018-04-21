/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef DB_PROGRESS_HPP
#define DB_PROGRESS_HPP

#include "cstdmf/timestamp.hpp"

#include <sstream>

class DBConsolidator;

/**
 * 	This object is passed around to various operations to so that there is a
 * 	a single object that knows about the progress of consolidation and can
 * 	report it to DBMgr.
 */
class ProgressReporter
{
public:
	ProgressReporter( DBConsolidator& consolidator, int numDBs ) :
		consolidator_( consolidator ),
		reportInterval_( stampsPerSecond()/2 ),	// Half a second
		lastReportTime_( timestamp() ),
		numDBs_( numDBs ),
		doneDBs_( 0 ),
		numEntitiesInCurDB_( 0 ),
		doneEntitiesInCurDB_( 0 )
	{}

	void onStartConsolidateDB( const std::string& dbName, int numEntities )
	{
		++doneDBs_;
		curDBName_ = dbName;
		numEntitiesInCurDB_ = numEntities;
		doneEntitiesInCurDB_ = 0;

		this->reportProgress();
	}
	void onConsolidatedRow()
	{
		++doneEntitiesInCurDB_;
		if (this->timeSinceLastReport() > reportInterval_)
		{
			this->reportProgress();
		}
	}

private:
	uint64 timeSinceLastReport() const
	{
		return timestamp() - lastReportTime_;
	}
	void reportProgress()
	{
		// Generate string
		std::stringstream ss;
		ss << "Consolidating " << curDBName_ << " (" << doneEntitiesInCurDB_
			<< '/' << numEntitiesInCurDB_ << " entities)"
			<< " (" << doneDBs_ << '/' << numDBs_ << " databases)";

		// __kyl__ A bit dodgy, we set the DBMgr status watcher directly.
		consolidator_.setDBMgrStatusWatcher( ss.str() );
	}

	DBConsolidator& consolidator_;

	uint64	reportInterval_;
	uint64	lastReportTime_;

	int			numDBs_;
	int			doneDBs_;	// Actualy counts the one currently being done
	std::string curDBName_;
	int			numEntitiesInCurDB_;
	int			doneEntitiesInCurDB_;
};

#endif 	// DB_PROGRESS_HPP
