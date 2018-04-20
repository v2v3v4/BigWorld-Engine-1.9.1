/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _WAYPOINT_HEADER
#define _WAYPOINT_HEADER

#include <string>
#include <vector>
#include "resmgr/datasection.hpp"
#include "math/vector2.hpp"
#include "cstdmf/binary_stream.hpp"
#include "network/basictypes.hpp"

const int CHUNK_ADJACENT_CONSTANT = 30000;

/**
 *	This is a unique ID for a waypoint within a chunk.
 */ 
typedef unsigned long WaypointID;

/**
 *	This is a unique ID for a chunk.
 *	(Here might not be the best place to define it)
 */ 
typedef std::string ChunkID;

class WaypointSet;

/**
 *	This class represents a waypoint. It is a convex polygon. Each edge
 *	on the polygon may be adjacent to another waypoint. 
 */ 
class Waypoint
{
public:
	Waypoint();

	void				readFromSection(DataSectionPtr, const ChunkID& chunkID);
	void				writeToSection(DataSectionPtr, const ChunkID& chunkID);

	void				readFromStream(BinaryIStream& stream);
	void				writeToStream(BinaryOStream& stream);

	int					vertexCount() const;

	const Vector2&		vertexPosition(int index) const;
	uint32				edgeFlags(int index) const;
	WaypointID			adjacentID(int index) const;
	const ChunkID&		adjacentChunkID(int index) const;

	Waypoint*			adjacentWaypoint(int index) const;
	void				adjacentWaypoint(int index, Waypoint* pWaypoint);
	WaypointSet*		adjacentWaypointSet(int index) const;
	void				adjacentWaypointSet(int index, WaypointSet * pWSet);

	bool				containsPoint(float x, float y, float z) const;
	bool				isAdjacentToChunk(const ChunkID& chunkID);
	bool				isAdjacentToSet(const WaypointSet * pWSet);

	WaypointID			id() const;
	const Vector3&		centre() const;
	float				height() const;

	void				transform( const Matrix& matrix, bool heightToo );

	bool				findClosestPoint(const Vector3& dst,
							Vector3& intersection);
private:

	void				calculateCentre();

	struct Vertex
	{
		Vector2 		position_;
		WaypointID		adjacentID_;
		ChunkID			adjacentChunkID_;
		Waypoint*		adjacentWaypoint_;
		WaypointSet*	adjacentWaypointSet_;
		float			distanceToAdjacent_;
	};

	WaypointID			id_;
	float				height_;
	std::vector<Vertex>	vertexVector_;	
	Vector3				centre_;
};

#endif
