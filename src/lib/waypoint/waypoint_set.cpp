/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "waypoint_set.hpp"
#include "waypoint_chunk.hpp"

#include <float.h>
#include <set>

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Waypoint", 0 )

/**
 *	Constructor
 */
WaypointSet::WaypointSet( WaypointChunk & chunk, int setNum ) :
	chunk_( chunk ),
	setNum_( setNum ),
	adjacentSetsCurrent_( false ),
	visited_( false )
{
}

/**
 *	Destructor
 */
WaypointSet::~WaypointSet()
{
	this->clear();
}

/**
 *	Clear out this waypoint set
 */
void WaypointSet::clear()
{
	Waypoints::iterator wit;
	for (wit = waypoints_.begin(); wit != waypoints_.end(); wit++)
	{
		delete *wit;
	}
	waypoints_.clear();

	adjacentSets_.clear();
	adjacentSetsCurrent_ = true;
}


/**
 *	Add this waypoint
 */
void WaypointSet::addWaypoint( Waypoint * pWaypoint )
{
	if (waypoints_.size() < pWaypoint->id() + 1)
		waypoints_.resize( pWaypoint->id() + 1 );
	waypoints_[ pWaypoint->id() ] = pWaypoint;
}


/**
 *	Link the waypoints in this set to each other
 */
void WaypointSet::linkWaypoints()
{
	Waypoints::iterator wit;
	Waypoint* pWaypoint;
	Waypoint* pAdjacent;

	for(wit = waypoints_.begin(); wit != waypoints_.end(); wit++)
	{
		pWaypoint = *wit;
		if (pWaypoint == NULL) continue;

		for (int i = 0; i < pWaypoint->vertexCount(); i++)
		{
			if (pWaypoint->adjacentChunkID(i) == chunk_.chunkID())
			{
				pAdjacent = this->findWaypoint(pWaypoint->adjacentID(i));

				if(!pAdjacent)
				{
					WARNING_MSG( "Waypoint %lu in chunk %s set %d "
						"linked to non-existent waypoint %lu on edge %d\n",
						pWaypoint->id(),
						chunk_.chunkID().c_str(),
						setNum_,
						pWaypoint->adjacentID(i),
						i );
				}
				else
				{
					pWaypoint->adjacentWaypoint(i, pAdjacent);
				}
			}
		}
	}

	adjacentSetsCurrent_ = false;
}


/**
 *	Bind any waypoints with external references to the given chunk
 */
void WaypointSet::bind( WaypointChunk & achunk )
{
	Waypoints::iterator wit;
	Waypoint* pWaypoint;

	for(wit = waypoints_.begin(); wit != waypoints_.end(); wit++)
	{
		pWaypoint = *wit;
		if (pWaypoint == NULL) continue;

		int vc = pWaypoint->vertexCount();
		for (int i = 0; i < vc; i++)
		{
			if (pWaypoint->adjacentChunkID(i) == achunk.chunkID())
			{
				// find what waypoint set it should connect to
				Vector2 midPos = (pWaypoint->vertexPosition(i) +
					pWaypoint->vertexPosition((i+1)%vc)) * 0.5f;
				Vector3 lookPos( midPos[0], pWaypoint->height(), midPos[1] );
				WPSpec wpspec;
				if (!achunk.findEnclosingWaypoint( lookPos, wpspec ) &&
					!achunk.findClosestWaypoint( lookPos, wpspec ))
				{
					WARNING_MSG( "Waypoint %lu in chunk %s set %d "
						"can find no set in chunk %s for edge %d\n",
						pWaypoint->id(),
						chunk_.chunkID().c_str(),
						setNum_,
						achunk.chunkID().c_str(),
						i );
				}
				else
				{
					pWaypoint->adjacentWaypointSet( i, wpspec.pWPSet );
				}
			}
		}
	}

	adjacentSetsCurrent_ = false;
}


/**
 *	Loose any bindings to the given chunk
 */
void WaypointSet::loose( WaypointChunk & achunk )
{
	Waypoints::iterator wit;
	Waypoint* pWaypoint;

	for(wit = waypoints_.begin(); wit != waypoints_.end(); wit++)
	{
		pWaypoint = *wit;
		if (pWaypoint == NULL) continue;

		int vc = pWaypoint->vertexCount();
		for (int i = 0; i < vc; i++)
		{
			// check the chunk is right
			if (pWaypoint->adjacentChunkID(i) == achunk.chunkID())
			{
				// also check the chunk ptr, in case there are two waypoint
				// chunks of the same name (can happen with ref counting)
				WaypointSet * pWPSet = pWaypoint->adjacentWaypointSet( i );
				if (pWPSet != NULL && &pWPSet->chunk() == &achunk)
				{
					pWaypoint->adjacentWaypointSet( i, NULL );
				}
			}
		}
	}

	adjacentSetsCurrent_ = false;
}


/**
 *	This method finds a waypoint given a waypoint ID.
 */ 
Waypoint* WaypointSet::findWaypoint( WaypointID waypointID )
{
	if (waypointID >= waypoints_.size()) return NULL;
	return waypoints_[ waypointID ];	// note: this may be NULL also
}


/**
 *	This method returns the waypoint that contains the given position, 
 *	or NULL if the waypoint is outside this set. This should be eventually
 *	optimised with a BSP or similar. For now it is a linear search.
 *
 *	@param position	Position to check
 *
 *	@return	The enclosing waypoint, or NULL.
 */
Waypoint* WaypointSet::findEnclosingWaypoint(const Vector3& position)
{
	Waypoints::iterator waypointIter;
	Waypoint* pWaypoint;

	for(waypointIter = waypoints_.begin();
		waypointIter != waypoints_.end();
		waypointIter++)
	{
		pWaypoint = *waypointIter;
		if(!pWaypoint)
			continue;

		if(pWaypoint->containsPoint(position.x, position.y, position.z))
			return pWaypoint;
	}

	return NULL;
}

/**
 *	This method returns the waypoint whose centre is nearest to the given
 *	position. It should generally be used in the case where there is no
 *	enclosing waypoint.
 *
 *	@param position	Position to check
 *	@param foundDistSquared	The squared distance to the closest point on
 *							the waypoint, or FLT_MAX if NULL was returned
 *
 *	@return The nearest waypoint, or NULL (if no waypoints in this chunk).
 */ 
Waypoint* WaypointSet::findClosestWaypoint( const Vector3& position,
	float & foundDistSquared )
{
	Waypoints::iterator waypointIter;
	Waypoint* pWaypoint;
	Waypoint* pBestWaypoint = NULL;
	float bestDistanceSquared = FLT_MAX;
	float distanceSquared;

	for(waypointIter = waypoints_.begin();
		waypointIter != waypoints_.end();
		waypointIter++)
	{
		pWaypoint = *waypointIter;
		if (!pWaypoint) continue;

		distanceSquared = (pWaypoint->centre() - position).lengthSquared();

		if(distanceSquared < bestDistanceSquared)
		{
			bestDistanceSquared = distanceSquared;
			pBestWaypoint = pWaypoint;
		}
	}

	foundDistSquared = bestDistanceSquared;
	return pBestWaypoint;
}


/**
 *	Return the end waypoint ID we know about, i.e. one above the maximum.
 */
WaypointID WaypointSet::endWaypointID() const
{
	return waypoints_.size();
}


/**
 *	Return the number of sets that we are adjacent to
 *
 *	@see cacheAdjacentSets
 */
uint	WaypointSet::getAdjacentSetCount() const
{
	if(!adjacentSetsCurrent_)
		this->cacheAdjacentSets();

	return adjacentSets_.size();
}


/**
 *	Get the adjacent set of the given index
 *
 *	@see cacheAdjacentSets
 */
WaypointSet* WaypointSet::getAdjacentSet(uint index) const
{
	if(!adjacentSetsCurrent_)
		this->cacheAdjacentSets();

	if (index >= adjacentSets_.size()) return NULL;
	return adjacentSets_[index];
}


/**
 *	Make up a cache of all the WaypointSets that any of the waypoints in
 *	our own set are adjacent to.
 */
void WaypointSet::cacheAdjacentSets() const
{
	std::set<WaypointSet*>	adjSetsSet;

	Waypoints::const_iterator wit;
	Waypoint* pWaypoint;

	for(wit = waypoints_.begin(); wit != waypoints_.end(); wit++)
	{
		pWaypoint = *wit;
		if (pWaypoint == NULL) continue;

		int vc = pWaypoint->vertexCount();
		for (int i = 0; i < vc; i++)
		{
			WaypointSet * pWPSet = pWaypoint->adjacentWaypointSet( i );
			if (pWPSet != NULL) adjSetsSet.insert( pWPSet );
		}
	}

	adjacentSets_.clear();
	std::set<WaypointSet*>::iterator sit;
	for (sit = adjSetsSet.begin(); sit != adjSetsSet.end(); sit++)
	{
		adjacentSets_.push_back( *sit );
	}

	adjacentSetsCurrent_ = true;
}

// waypoint_set.cpp
