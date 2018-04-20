/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WAYPOINT_SET_HPP
#define WAYPOINT_SET_HPP

#include "waypoint.hpp"
#include "waypoint_chunk.hpp"


/**
 *	This class is a connected set of waypoints, generally in one chunk.
 *	It may contain connections to other sets (usually in other chunks)
 *
 *	@note These are considered to be owned by the waypoint chunk that
 *	contains them, so they do not keep a reference to it
 */
class WaypointSet
{
public:
	WaypointSet( WaypointChunk & chunk, int setNum );
	~WaypointSet();

	void clear();

	WaypointChunk & chunk()					{ return chunk_; }
	const WaypointChunk & chunk() const		{ return chunk_; }
	int setNum() const						{ return setNum_; }

	void addWaypoint( Waypoint * pWaypoint );
	void linkWaypoints();

	void bind( WaypointChunk & achunk );
	void loose( WaypointChunk & achunk );

	Waypoint * findWaypoint( WaypointID waypointID );
	Waypoint * findEnclosingWaypoint( const Vector3 & position );
	Waypoint * findClosestWaypoint( const Vector3 & position,
		float & foundDistSquared );

	WaypointID		endWaypointID() const;

	uint			getAdjacentSetCount() const;
	WaypointSet* 	getAdjacentSet(uint index) const;

	void			incRef() const			{ chunk_.incRef(); }
	void			decRef() const			{ chunk_.decRef(); }
	int				refCount() const		{ return chunk_.refCount(); }

private:
	void	cacheAdjacentSets() const;

	WaypointChunk &			chunk_;
	int						setNum_;

	typedef std::vector<Waypoint*>			Waypoints;
	typedef std::vector<WaypointSet*>		WaypointSets;

	Waypoints				waypoints_;

	mutable WaypointSets	adjacentSets_;
	mutable bool			adjacentSetsCurrent_;

	bool					visited_;
};


#endif // WAYPOINT_SET_HPP
