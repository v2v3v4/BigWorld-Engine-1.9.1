/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _WAYPOINT_STATE_HEADER
#define _WAYPOINT_STATE_HEADER

#include "waypoint.hpp"
#include "waypoint_set.hpp"
#include "waypoint_chunk.hpp"


class WaypointGoalState;

/**
 *	This class represents a state within a search on a waypoint graph.
 */ 
class WaypointState
{
public:
	WaypointState();

	void	setWaypoint(const Waypoint* pWaypoint);
	void	setWPSet(const WaypointSet * pWPSet);
	void	setWaypointID(WaypointID waypointID);
	void	setPosition(const Vector3& position);
	
	const ChunkID&	chunkID() const		{ return pWPSet_->chunk().chunkID(); }
	uint			setNum() const		{ return pWPSet_->setNum(); }
	WaypointID 		waypointID() const	{ return waypointID_; }
	const Vector3&	position() const	{ return position_; }

	int 	compare(const WaypointState& other) const;
	bool	isGoal(const WaypointGoalState& goal) const;
	int		getAdjacencyCount() const;
	bool	getAdjacency(int index, WaypointState&, const WaypointGoalState&) const;
	float	distanceFromParent() const;
	float	distanceToGoal(const WaypointGoalState& goal) const;
	
protected:
	const Waypoint*		pWaypoint_;
	float				distanceFromParent_;
	ConstSmartPointer<WaypointSet>		pWPSet_;
	WaypointID			waypointID_;
	Vector3				position_;
};


/**
 * This class describes the goal state, which may include some static info
 * about the search that needn't be stored in each intermediate state.
 */
class WaypointGoalState : public WaypointState
{
public:
	WaypointGoalState() : WaypointState(), extraRadius_( 0.f ) { }

	float extraRadius() const	{ return extraRadius_; }
	void extraRadius( float v )	{ extraRadius_ = v; }

private:
	float extraRadius_;
};


#endif // _WAYPOINT_STATE_HEADER
