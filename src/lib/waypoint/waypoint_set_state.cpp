/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "waypoint_set_state.hpp"

#include "waypoint_chunk.hpp"

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT(0);

/**
 *	This is the constructor.
 */
WaypointSetState::WaypointSetState() :
	pWPSet_( NULL ),
	distanceFromParent_( 0.0f )
{
}

/**
 *	This method sets the waypoint set for this state.
 */
void WaypointSetState::setWaypointSet(const WaypointSet* pWPSet)
{
	pWPSet_ = pWPSet;
}

/**
 *	This method sets the position for this state.
 */
void WaypointSetState::setPosition(const Vector3& position)
{
	position_ = position;
}

/**
 *	This method compares this state with another one.
 *	Another WaypointSetState will be considered equivalent
 *	if its WaypointSet pointer is the same. The position is
 *	not taken into account.
 *
 *	@return 	Negative if this is less than other
 *				Positive if this is greater than other
 *				Zero if this equals other
 */
int WaypointSetState::compare(const WaypointSetState& other) const
{
	return int(uintptr(&*pWPSet_) > uintptr(&*other.pWPSet_)) -
		int(uintptr(&*pWPSet_) < uintptr(&*other.pWPSet_));
}

/**
 *	This method returns true if this state matches the
 *	given goal state.
 *
 *	@param	goal The goal state to check against
 *	@return True if this state matches the goal.
 */
bool WaypointSetState::isGoal(const WaypointSetState& goal) const
{
	return pWPSet_ == goal.pWPSet_;
}

/**
 *	This method returns the number of adjacencies for this state.
 */
int WaypointSetState::getAdjacencyCount() const
{
	return pWPSet_->getAdjacentSetCount();
}

/**
 *	This method returns an adjacent state.
 *
 *	@param index	Index of the state to return.
 *	@param newState	The new state is returned here.
 *	@param goal		The goal state is passed in here.
 *	@return True if there is an adjacent state with this index.
 */
bool WaypointSetState::getAdjacency(int index, WaypointSetState& newState,
		const WaypointSetState&) const
{
	WaypointSet * pWPSet = pWPSet_->getAdjacentSet( index );
	if (pWPSet == NULL) return false;

	// make sure the chunks are not the same or else the distance would
	// be zero! (which would completely stuff up AStar)
	MF_ASSERT( &pWPSet->chunk() != &pWPSet_->chunk() );

	newState.pWPSet_ = pWPSet;
	newState.position_ = pWPSet->chunk().centre();
	newState.distanceFromParent_ = (newState.position_ - position_).length();

	return true;
}

/**
 *	This method returns the distance from the parent state.
 */
float WaypointSetState::distanceFromParent() const
{
	return distanceFromParent_;
}

/**
 *	This method returns the distance to the given goal state.
 */
float WaypointSetState::distanceToGoal(const WaypointSetState& goal) const
{
	return (goal.position_ - position_).length();
}

// waypoint_set_state.cpp
