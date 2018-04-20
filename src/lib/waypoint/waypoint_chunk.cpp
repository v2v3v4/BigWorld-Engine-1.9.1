/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "waypoint_chunk.hpp"

#include "waypoint_set.hpp"

#include <float.h>
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Waypoint", 0 )


/**
 *	Constructor
 */
WaypointChunk::WaypointChunk(ChunkID chunkID) :
	chunkID_( chunkID ),
	centre_( 0.f, 0.f, 0.f ),
	volume_( 0.f )
{
}

/**
 *	Destructor
 */
WaypointChunk::~WaypointChunk()
{
	this->clear();

	// delete all our waypoint sets
	for (WaypointSets::iterator waypointIter = waypointSets_.begin();
		waypointIter != waypointSets_.end();
		waypointIter++)
	{
		if (*waypointIter != NULL)
			delete *waypointIter;
	}
	waypointSets_.clear();
}


/**
 *	Note the WaypointChunk owns the waypoints and is responsible
 *	for deleting them.
 */
void WaypointChunk::addWaypoint(int set, Waypoint* pWaypoint)
{
	if (waypointSets_.size() < uint(set+1))
		waypointSets_.resize(set+1);
	if (waypointSets_[set] == NULL)
		waypointSets_[set] = new WaypointSet( *this, set );
	waypointSets_[set]->addWaypoint( pWaypoint );
}


/**
 *	Add the given adjacency to all our sets
 */
void WaypointChunk::addAdjacency(WaypointChunk* pAdjacentChunk)
{
	if (std::find( adjacentChunks_.begin(), adjacentChunks_.end(),
		pAdjacentChunk ) == adjacentChunks_.end())
	{
		adjacentChunks_.push_back( pAdjacentChunk );

		for (WaypointSets::iterator wit = waypointSets_.begin();
			wit != waypointSets_.end();
			wit++)
		{
			if (*wit != NULL)
				(*wit)->bind( *pAdjacentChunk );
		}
	}
}


/**
 *	Delete the given adjacency from all our sets
 */
void WaypointChunk::delAdjacency(WaypointChunk* pAdjacentChunk)
{
	WaypointChunks::iterator found = std::find(
		adjacentChunks_.begin(), adjacentChunks_.end(), pAdjacentChunk );
	if (found != adjacentChunks_.end())
	{
		adjacentChunks_.erase( found );

		for (WaypointSets::iterator wit = waypointSets_.begin();
			wit != waypointSets_.end();
			wit++)
		{
			if (*wit != NULL)
				(*wit)->loose( *pAdjacentChunk );
		}
	}
}


/**
 *	Link our waypoints in our sets to each other
 */
void WaypointChunk::linkWaypoints()
{
	for (WaypointSets::iterator wit = waypointSets_.begin();
		wit != waypointSets_.end();
		wit++)
	{
		if (*wit != NULL)
			(*wit)->linkWaypoints();
	}
}


/**
 *	Clear out the internals of this chunk
 */
void WaypointChunk::clear()
{
	WaypointChunks::iterator adjacencyIter;
	WaypointSets::iterator waypointIter;

	// delete all adjacencies
	for(adjacencyIter = adjacentChunks_.begin();
		adjacencyIter != adjacentChunks_.end();
		adjacencyIter++)
	{
		(*adjacencyIter)->delAdjacency( this );
		// could delete our own adjacencies to all these chunks
		// but there's really no point as they'll be cleared
	}
	adjacentChunks_.clear();

	// clear all our waypoint sets
	//  (don't delete them or it'll stuff up pointers to them)
	for(waypointIter = waypointSets_.begin();
		waypointIter != waypointSets_.end();
		waypointIter++)
	{
		if (*waypointIter != NULL)
			(*waypointIter)->clear();
	}
}


/**
 *	This method returns the given set
 */
WaypointSet * WaypointChunk::findWaypointSet( uint set )
{
	if (set >= waypointSets_.size()) return NULL;
	return waypointSets_[set];
}


/**
 *	This method finds a waypoint given a waypoint ID.
 */
bool WaypointChunk::findWaypoint(WaypointID waypointID, WPSpec & wpspec)
{
	WaypointSets::iterator wit;
	WaypointSet * pWPSet;
	Waypoint * pWaypoint;

	for (wit = waypointSets_.begin(); wit != waypointSets_.end(); wit++)
	{
		pWPSet = *wit;
		if (pWPSet == NULL) continue;

		pWaypoint = pWPSet->findWaypoint( waypointID );
		if (pWaypoint != NULL)
		{
			wpspec.pWPSet = pWPSet;
			wpspec.pWaypoint = pWaypoint;
			return true;
		}
	}

	return false;
}

/**
 *	This method returns the spec of the waypoint that contains the given
 *	position, or NULL if the waypoint is outside all the sets of this chunk.
 *
 *	@param position	Position to check
 *	@param wpspec	Spec of found waypoint
 *
 *	@return	True if one was found
 */
bool WaypointChunk::findEnclosingWaypoint( const Vector3& position,
	WPSpec & wpspec )
{
	WaypointSets::iterator wit;
	WaypointSet * pWPSet;
	Waypoint * pWaypoint;

	for (wit = waypointSets_.begin(); wit != waypointSets_.end(); wit++)
	{
		pWPSet = *wit;
		if (pWPSet == NULL) continue;

		pWaypoint = pWPSet->findEnclosingWaypoint( position );
		if (pWaypoint != NULL)
		{
			wpspec.pWPSet = pWPSet;
			wpspec.pWaypoint = pWaypoint;
			return true;
		}
	}

	return false;
}

/**
 *	This method returns the waypoint whose centre is nearest to the given
 *	position. It should generally be used in the case where there is no
 *	enclosing waypoint.
 *
 *	@param position	Position to check
 *	@param	wpspec On success, set to the nearest waypoint.
 *
 *	@return True on success, otherwise false (if no waypoints in this chunk).
 */
bool WaypointChunk::findClosestWaypoint( const Vector3& position,
	WPSpec & wpspec )
{
	WaypointSets::iterator wit;
	WaypointSet * pWPSet;
	Waypoint * pWaypoint;
	float bestDistanceSquared = FLT_MAX;
	float distanceSquared = 0.f;

	wpspec.pWPSet = NULL;
	wpspec.pWaypoint = NULL;

	for (wit = waypointSets_.begin(); wit != waypointSets_.end(); wit++)
	{
		pWPSet = *wit;
		if (pWPSet == NULL) continue;

		pWaypoint = pWPSet->findClosestWaypoint( position, distanceSquared );
		if (distanceSquared < bestDistanceSquared && pWaypoint != NULL)
		{
			bestDistanceSquared = distanceSquared;
			wpspec.pWPSet = pWPSet;
			wpspec.pWaypoint = pWaypoint;
		}
	}

	return wpspec.pWaypoint != NULL;
}


/**
 *	Return our hull border planes
 */
const HullBorder& WaypointChunk::hullBorder() const
{
	return hullBorder_;
}

/**
 *	Add the given plane to our border
 */
void WaypointChunk::addPlane(const PlaneEq& plane)
{
	hullBorder_.push_back(plane);
}

/**
 *	Set our bounding box to the given one
 */
void WaypointChunk::setBoundingBox( const BoundingBox & bb )
{
	bb_ = bb;
	Vector3 delta = bb.maxBounds() - bb.minBounds();
	volume_ = delta.x * delta.y * delta.z;
	centre_ = (bb.maxBounds() + bb.minBounds()) * 0.5f;
}


/**
 *	Return whether or not this is an outside chunk
 */
bool WaypointChunk::isOutsideChunk() const
{
	return chunkID_.length() >= 9 && chunkID_[8] == 'o';
}

/**
 *	Return whether or not this point falls under our jurisdiction
 */
bool WaypointChunk::containsPoint( const Vector3& point )
{
	if (!bb_.intersects( point )) return false;

	for(unsigned int i = 0; i < hullBorder_.size(); i++)
	{
		if(!hullBorder_[i].isInFrontOf(point))
			return false;
	}

	return true;
}

// waypoint_chunk.cpp
