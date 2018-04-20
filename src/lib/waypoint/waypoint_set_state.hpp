/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WAYPOINT_SET_STATE_HPP
#define WAYPOINT_SET_STATE_HPP

#include "waypoint_set.hpp"

/**
 *	This class represents the search state on a graph of WaypointSets
 */ 
class WaypointSetState
{
public:
	WaypointSetState();

	void	setWaypointSet(const WaypointSet* pWPSet);
	void	setPosition(const Vector3& position);

	const WaypointSet * pWPSet() const	{ return &*pWPSet_; }
	
	int 	compare(const WaypointSetState& other) const;
	bool	isGoal(const WaypointSetState& goal) const;
	int		getAdjacencyCount() const;
	bool	getAdjacency(int index, WaypointSetState& newState,
				const WaypointSetState& goal) const;
	float	distanceFromParent() const;
	float	distanceToGoal(const WaypointSetState& goal) const;
	
private:
	ConstSmartPointer<WaypointSet>		pWPSet_;
	float			distanceFromParent_;
	Vector3			position_;
};

#endif // WAYPOINT_SET_STATE_HPP
