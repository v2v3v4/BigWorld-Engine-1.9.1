/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WAYPOINT_CHUNK_HEADER
#define WAYPOINT_CHUNK_HEADER

#include <map>

#include "waypoint.hpp"
#include "physics2/hulltree.hpp"
#include "math/boundbox.hpp"

class WaypointSet;

/**
 *	This struct specifies a waypoint
 */
struct WPSpec
{
	WaypointSet * pWPSet;
	Waypoint * pWaypoint;
};


/**
 *	This class is the waypoint view of a chunk
 */
class WaypointChunk : public ReferenceCount, public HullContents
{
public:
	WaypointChunk(ChunkID chunkID);
	~WaypointChunk();

	const ChunkID& 		chunkID() const		{ return chunkID_; }
	const Vector3&		centre() const		{ return centre_; }
	float				volume() const		{ return volume_; }
	
	void addWaypoint(int set, Waypoint* pWaypoint);
	void addAdjacency(WaypointChunk* pWaypointChunk);
	void delAdjacency(WaypointChunk* pWaypointChunk);
	void linkWaypoints();
	void clear();

	const HullBorder&	hullBorder() const;
	void 				addPlane(const PlaneEq& plane);
	void				setBoundingBox( const BoundingBox & bb );
	bool				isOutsideChunk() const;
	bool				containsPoint(const Vector3& point);

	WaypointSet *		findWaypointSet( uint set );

	bool findWaypoint(WaypointID waypointID, WPSpec & wpspec);
	bool findEnclosingWaypoint(const Vector3& position, WPSpec & wpspec);
	bool findClosestWaypoint(const Vector3& position, WPSpec & wpspec);
	
private:

	// We don't use smart pointers to reference adjacencies,
	// since that would lead to circular references. Instead,
	// waypoint chunks are manually unlinked from adjacencies
	// on removal.
	
//	typedef std::map<ChunkID, WaypointChunk*>	AdjacencyMap;
	typedef std::vector<WaypointSet*>			WaypointSets;
//	typedef std::vector<WaypointSet*>			AdjacencyArray;
	typedef std::vector<WaypointChunk*>			WaypointChunks;
	
	ChunkID						chunkID_;
//	bool						visited_;
//	AdjacencyMap				adjacencyMap_;
//	WaypointVector				waypointVector_;
	WaypointSets				waypointSets_;
//	mutable AdjacencyArray		adjacencyArray_;
	WaypointChunks				adjacentChunks_;
	HullBorder					hullBorder_;
	BoundingBox					bb_;
	Vector3						centre_;
	float						volume_;
};

#endif	
