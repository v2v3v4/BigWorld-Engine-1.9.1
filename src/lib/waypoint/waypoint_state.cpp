/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "waypoint_state.hpp"

#include "math/lineeq.hpp"

/**
 *	This is the constructor.
 */
WaypointState::WaypointState() :
	pWaypoint_(NULL),
	distanceFromParent_(0.0f),
	pWPSet_(NULL),
	waypointID_(0)
{
}

/**
 *	This method sets the waypoint for this state.
 *	Note the waypointID and chunkID are set separately.
 *	This is because a neighbouring waypoint in a different chunk
 *	does not have a waypoint pointer. This is good, because it
 *	means we will not follow adjacencies into another chunk.
 */
void WaypointState::setWaypoint(const Waypoint* pWaypoint)
{
	pWaypoint_ = pWaypoint;
}

/**
 *	This method sets the starting position within this waypoint.
 */
void WaypointState::setPosition(const Vector3& position)
{
	position_ = position;
}

/**
 *	This method sets the waypoint set of this waypoint.
 */
void WaypointState::setWPSet(const WaypointSet * pWPSet)
{
	pWPSet_ = pWPSet;
}

/**
 *	This method sets the ID of this waypoint.
 */
void WaypointState::setWaypointID(WaypointID waypointID)
{
	waypointID_ = waypointID;
}

/**
 *	This method is used for determining uniqueness of states. As far as
 *	we are concerned, the waypoint and waypoint set ptr determine uniqueness.
 *
 *	@return 	Negative if this is less than other
 *				Positive if this is greater than other
 *				Zero if this equals other
 */
int WaypointState::compare(const WaypointState& other) const
{
	/*
	// check sets first (it's fast)
	if (pWPSet_->setNum() == other.pWPSet_->setNum())
	{
		// now chunk names
		int chunkRes = strcmp( pWPSet_->chunk().chunkID().c_str(),
			  other.pWPSet_->chunk().chunkID().c_str() );
		if (chunkRes == 0)
		{
			// and finally waypoint ids
			return waypointID_ - other.waypointID_;
		}
		else
		{
			return chunkRes;
		}
	}
	else
	{
		return pWPSet_->setNum() - other.pWPSet_->setNum();
		//return int(&*pWPSet_) - int(&*other.pWPSet_);
	}
	*/
	if (pWPSet_ == other.pWPSet_)
		return waypointID_ - other.waypointID_;
	else
		return intptr(&*pWPSet_) - intptr(&*other.pWPSet_);
}

/**
 *	The goal state is defined by matching a waypoint set, a waypointID, or both.
 *	An NULL waypoint set or zero waypointID is a wildcard.
 *
 *	@return True if this state matches the goal state.
 */
bool WaypointState::isGoal(const WaypointGoalState& goal) const
{
	/*
	if (goal.pWPSet_)
	{
		if (goal.pWPSet_->setNum() != pWPSet_->setNum())
			return false;

		if (goal.pWPSet_->chunk().chunkID() != pWPSet_->chunk().chunkID())
			return false;
	}
	*/
	if (goal.pWPSet_ && goal.pWPSet_ != pWPSet_)
		return false;

	if (goal.waypointID_ && goal.waypointID_ != waypointID_)
		return false;

	return true;
}

/**
 *	This method returns the number of adjacencies.
 *
 *	@return	Number of adjacencies.
 */
int WaypointState::getAdjacencyCount() const
{
	// pWaypoint_ will be NULL in out-of-chunk states such as
	// those for goals and adjacent chunks
	if(pWaypoint_)
		return pWaypoint_->vertexCount();
	else
		return 0;
}

/**
 *	This method fills in the adjacentState reference with
 *	the state for a given adjacency.
 *
 *	@param index			Index of adjacency
 *	@param adjacency	State of adjacency is copied here
 *	@param goal				Goal state
 *
 *	@return True if this is a valid adjacency.
 */
bool WaypointState::getAdjacency(int index,
		WaypointState& adjacency, const WaypointGoalState& goal) const
{
	Vector2 src, dst, p1, p2, movementVector, next;
	float p, cp1, cp2;

	// If this edge on waypoint is not passable, forget it.

	const WaypointSet * pAdjSet = pWaypoint_->adjacentWaypointSet(index);
	if (pWaypoint_->adjacentWaypoint(index) == NULL && pAdjSet == NULL)
		return false;

	// We need 2d vectors for intersection tests.

	src.x = position_.x;
	src.y = position_.z;
	dst.x = goal.position_.x;
	dst.y = goal.position_.z;
	movementVector = dst - src;

	p1 = pWaypoint_->vertexPosition(index);
	p2 = pWaypoint_->vertexPosition((index + 1) % pWaypoint_->vertexCount());

	// move the pts towards each other if we have extra radius
	if (goal.extraRadius() > 0.f)
	{
		const float ger = goal.extraRadius();
		Vector2 edir( p2 - p1 );
		const float elen = edir.length();

		// for now can only pass edges that are long enough
		// (need to only move in conditionally, e.g. if adj to black)
		if (elen < ger * 2.f) return false;

		edir *= ger / elen;
		p1 += edir;
		p2 -= edir;
	}

	cp1 = movementVector.crossProduct(p1 - src);
	cp2 = movementVector.crossProduct(p2 - src);

	// If our desired path takes us through this line segment,
	// find the intersection and use it. Otherwise use the
	// vertex whose crossproduct is closest to zero.

	if(cp1 > 0.0f && cp2 < 0.0f)
	{
		LineEq moveLine(src, dst, true);
		LineEq edgeLine(p1, p2, true);
		p = moveLine.intersect(edgeLine);
		next = moveLine.param(p);
	}
	else if(fabs(cp1) < fabs(cp2))
	{
		next = p1;
	}
	else
	{
		next = p2;
	}

	adjacency.pWPSet_ = (pAdjSet == NULL) ? pWPSet_ : pAdjSet;
	adjacency.waypointID_ = pWaypoint_->adjacentID(index);
	adjacency.pWaypoint_ = pWaypoint_->adjacentWaypoint(index);
	adjacency.position_.x = next.x;
	adjacency.position_.y = position_.y;
	adjacency.position_.z = next.y;
	adjacency.distanceFromParent_ = (position_ - adjacency.position_).length();
	return true;
}

/**
 *	This method returns the distance from the parent state.
 */
float WaypointState::distanceFromParent() const
{
	return distanceFromParent_;
}

/**
 *	This method returns the estimated distance from the goal.
 */
float WaypointState::distanceToGoal(const WaypointGoalState& goal) const
{
	return (goal.position_ - position_).length();
}

// waypoint_state.cpp
