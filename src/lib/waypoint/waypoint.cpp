/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include <float.h>
#include "waypoint.hpp"
#include "math/lineeq.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2("Waypoint", 0);

/**
 *	This is the constructor.
 */
Waypoint::Waypoint() :
	id_(0),
	height_(0.0f)
{
}

/**
 *	This method reads the contents of a waypoint from a datasection.
 *
 *	@param pSection		The section from which to read the waypoint.
 *	@param chunkID		The chunkID that contains this waypoint.
 */
void Waypoint::readFromSection(DataSectionPtr pSection, const ChunkID& chunkID)
{
	DataSection::iterator it;

	id_ = pSection->asLong();
	height_ = pSection->readFloat("height");
	vertexVector_.clear();

	for(it = pSection->begin(); it != pSection->end(); it++)
	{
		DataSectionPtr pDS = *it;
		if(pDS->sectionName() == "vertex")
		{
			Vertex vertex;
			Vector3 vPos = pDS->asVector3();
			vertex.position_[0] = vPos[0];
			vertex.position_[1] = vPos[1];
			vertex.adjacentID_ = WaypointID(int(vPos[2]));
			vertex.adjacentChunkID_ = pDS->readString("adjacentChunk");
			vertex.adjacentWaypoint_ = NULL;
			vertex.adjacentWaypointSet_ = NULL;
			vertex.distanceToAdjacent_ = 0.0f;

			std::string & acid = vertex.adjacentChunkID_;
			if (acid.empty())
			{
				if (int(vertex.adjacentID_) > 0)
				{
					// If there is an adjacent waypoint ID, but no adjacent
					// chunkID, it is assumed that we are talking about a
					// waypoint in the current chunk.
					acid = chunkID;
				}
			}
			else
			{
				// Ensure chunk id is in canonical format
				for (unsigned int i = 0; i < acid.size() - 1; i++)
					acid[i] = toupper(acid[i]);
			}

			vertexVector_.push_back(vertex);
		}
	}

	this->calculateCentre();
}

/**
 *	This method writes the contents of a waypoint to a datasection.
 *
 *	@param pSection		The section to write the waypoint to.
 *	@param chunkID		The id of the chunk that the waypoint is in.
 */
void Waypoint::writeToSection(DataSectionPtr pSection, const ChunkID& chunkID)
{
	std::vector<Vertex>::iterator it;
	pSection->setLong(id_);
	pSection->writeFloat("height", height_);

	for(it = vertexVector_.begin(); it != vertexVector_.end(); it++)
	{
		DataSectionPtr pVertex = pSection->openSection("vertex", true);
		pVertex->setVector3( Vector3(
			it->position_[0], it->position_[1], float( it->adjacentID_ ) ) );
		if (!it->adjacentChunkID_.empty() && it->adjacentChunkID_ != chunkID)
			pVertex->writeString("adjacentChunk", it->adjacentChunkID_);
	}
}

/**
 *	This method reads the contents of a waypoint to a memory stream.
 *
 *	@param stream		The stream from which to read the waypoint.
 */
void Waypoint::readFromStream(BinaryIStream& stream)
{
	unsigned int i, count;
	Vertex v;

	vertexVector_.clear();
	stream >> id_ >> height_ >> centre_ >> count;

	for(i = 0; i < count; i++)
	{
		stream >> v.position_ >> v.adjacentID_ >> v.adjacentChunkID_;
		v.adjacentWaypoint_ = NULL;
		v.adjacentWaypointSet_ = NULL;
		v.distanceToAdjacent_ = 0.0f;
		vertexVector_.push_back(v);
	}
}

/**
 *	This method writes the contents of a waypoint to a memory stream.
 *
 *	@param stream		The stream to write the waypoint to.
 */
void Waypoint::writeToStream(BinaryOStream& stream)
{
	stream << id_ << height_ << centre_ << vertexVector_.size();

	for(unsigned int i = 0; i < vertexVector_.size(); i++)
	{
		stream << vertexVector_[i].position_;
		stream << vertexVector_[i].adjacentID_;
		stream << vertexVector_[i].adjacentChunkID_;
	}
}

/**
 *	This method returns the number of vertices in the polygon.
 */
int Waypoint::vertexCount() const
{
	return vertexVector_.size();
}

/**
 *	This method returns the position of a vertex in the polygon.
 */
const Vector2& Waypoint::vertexPosition(int index) const
{
	return vertexVector_[index].position_;
}

/**
 *	This method returns the flags of the given edge.
 *	Note that the edge is defined along the line
 *	segment from vertex[index] to vertex[index+1].
 *
 *	@param index	Index of the edge to return.
 *
 *	@return The flags of the edge
 */
uint32 Waypoint::edgeFlags(int index) const
{
	int aid = int( vertexVector_[index].adjacentID_ );
	return aid < 0 ? uint32( -aid ) : 0;
}

/**
 *	This method returns the waypoint ID of an adjacent waypoint.
 *	If this ID is > zero, there is no adjacency along this edge.
 *	Note that the adjacent waypoint is reachable along the line
 *	segment from vertex[index] to vertex[index+1].
 *
 *	@param index	Index of the adjacency to return.
 *
 *	@return The ID of the adjacent waypoint.
 */
WaypointID Waypoint::adjacentID(int index) const
{
	int aid = int( vertexVector_[index].adjacentID_ );
	return aid > 0 ? WaypointID( aid ) : 0;
}

/**
 *	This method returns the chunk ID of an adjacent waypoint.
 *	If the waypoint ID is zero, this will be an empty string
 *	and should be ignored.
 *
 *	@param index	Index of the adjacency to return.
 *	@return The chunk ID of the adjacent waypoint.
 */
const ChunkID& Waypoint::adjacentChunkID(int index) const
{
	return vertexVector_[index].adjacentChunkID_;
}

/**
 *	This method returns a pointer to an adjacent waypoint.
 *	Traversing a waypoint graph by following pointers is
 *	faster than searching for a waypoint by id.
 *	The waypoints must be linked together by whoever reads
 *	them.
 *
 *	@param index	Index of the adjacency to return.
 *	@return A pointer to the adjacent waypoint.
 */
Waypoint* Waypoint::adjacentWaypoint(int index) const
{
	return vertexVector_[index].adjacentWaypoint_;
}

/**
 *	This method is called to link the waypoint to an adjacency
 *	by specifying a pointer to it.
 *
 *	@param index		Index of the adjacency to set.
 *	@param pWaypoint	Pointer to the adjacent waypoint.
 */
void Waypoint::adjacentWaypoint(int index, Waypoint* pWaypoint)
{
	vertexVector_[index].adjacentWaypoint_ = pWaypoint;
	vertexVector_[index].distanceToAdjacent_ =
		(pWaypoint->centre() - this->centre()).length();
}

/**
 *	This method returns a pointer to an adjacent waypoint set,
 *	if we have one (must have been set by the mutator, when looked
 *	up from our adjacent chunk id).
 *
 *	@param index	Index of the adjacency to return.
 *	@return A pointer to the adjacent waypoint set.
 */
WaypointSet* Waypoint::adjacentWaypointSet(int index) const
{
	return vertexVector_[index].adjacentWaypointSet_;
}

/**
 *	This method is called to link the waypoint to an adjacent
 *	waypoint set by specifying a pointer to it.
 *
 *	@param index		Index of the adjacency to set.
 *	@param pWSet		Pointer to the adjacent waypoint set.
 */
void Waypoint::adjacentWaypointSet(int index, WaypointSet* pWSet)
{
	vertexVector_[index].adjacentWaypointSet_ = pWSet;
}

/**
 *	This method returns true if the waypoint contains the given
 *	point.
 *
 *	@param x	The x position of the point.
 *	@param y	The y position of the point.
 *	@param z	The z position of the point.
 */
bool Waypoint::containsPoint(float x, float y, float z) const
{
	if (y > height_+2.f) return false;
	// TODO: really want a min height too...

	float u, v, xd, zd, c;
	unsigned int i, j;

	i = vertexVector_.size() - 1;
	for(j = 0; j < vertexVector_.size(); j++)
	{
		u = vertexVector_[j].position_[0] - vertexVector_[i].position_[0];
		v = vertexVector_[j].position_[1] - vertexVector_[i].position_[1];

		/*
		DEBUG_MSG("(%f, %f) - (%f, %f)\n",
			vertexVector_[i].position_[0],
			vertexVector_[i].position_[1],
			vertexVector_[j].position_[0],
			vertexVector_[j].position_[1]);
			*/

		xd = x - vertexVector_[i].position_[0];
		zd = z - vertexVector_[i].position_[1];

		c = xd * v - zd * u;

		if(c < 0)
			return false;

		i = j;
	}

	return true;
}

/**
 *	This method returns true if a given chunk is adjacent to this waypoint.
 */
bool Waypoint::isAdjacentToChunk(const ChunkID& chunkID)
{
	for(unsigned int i = 0; i < vertexVector_.size(); i++)
		if(vertexVector_[i].adjacentChunkID_ == chunkID)
			return true;
	return false;
}

/**
 *	This method returns true if the given waypoint set is adjacent to this
 *	waypoint.
 */
bool Waypoint::isAdjacentToSet(const WaypointSet * pWSet)
{
	for(unsigned int i = 0; i < vertexVector_.size(); i++)
		if(vertexVector_[i].adjacentWaypointSet_ == pWSet)
			return true;
	return false;
}



/**
 *	This method calculates the centre of the waypoint.
 */
void Waypoint::calculateCentre()
{
	Vector2 newCen( 0.f, 0.f );

	for(unsigned int i = 0; i < vertexVector_.size(); i++)
		newCen += vertexVector_[i].position_;

	uint sz = vertexVector_.size();
	if (sz != 0) newCen /= float(sz);

	centre_.set( newCen.x, height_, newCen.y );
}

/**
 *	This method returns the ID of this waypoint.
 */
WaypointID Waypoint::id() const
{
	return id_;
}

/**
 *	This method returns the centre of this waypoint.
 */
const Vector3& Waypoint::centre() const
{
	return centre_;
}

/**
 *	This method returns the height of this waypoint.
 */
float Waypoint::height() const
{
	return height_;
}

/**
 *	This method transforms all points in this waypoint by the given transform.
 */
void Waypoint::transform( const Matrix& matrix, bool heightToo )
{
	Vector3 v;

	if (heightToo)
	{
		v.x = 0;
		v.y = height_;
		v.z = 0;

		v = matrix.applyPoint( v );
		height_ = v.y;
	}


	for (unsigned int i = 0; i < vertexVector_.size(); i++)
	{
		v.x = vertexVector_[i].position_[0];
		v.y = 0.f;
		v.z = vertexVector_[i].position_[1];

		v = matrix.applyPoint( v );
		vertexVector_[i].position_.set( v.x, v.z );
	}

	this->calculateCentre();
}

/**
 *	This method find the intersection with a line segment starting at
 *	the centre of the waypoint, and ending at dst outside the waypoint.
 *
 *	@param dst3	A point outside the waypoint
 *	@param intersection The intersection on waypoint border is returned here
 *	@return True if successful.
 */
bool Waypoint::findClosestPoint(const Vector3& dst3, Vector3& intersection)
{
	Vector3 cen = this->centre();
	Vector2 src( cen.x, cen.z );
	Vector2 dst( dst3.x, dst3.z );
	Vector2 movementVector = dst - src;
	LineEq movementLine(src, dst, true);

	for (unsigned int i = 0; i < vertexVector_.size(); i++)
	{
		Vector2 p1 = vertexVector_[i].position_;
		Vector2 p2 = vertexVector_[(i + 1) % vertexVector_.size()].position_;

		float cp1 = movementVector.crossProduct(p1 - src);
		float cp2 = movementVector.crossProduct(p2 - src);

		// If our desired path takes us through this line segment,
		// find the intersection and use it.

		if(cp1 > 0.0f && cp2 < 0.0f)
		{
			LineEq edgeLine(p1, p2, true);
			float p = movementLine.intersect(edgeLine);
			Vector2 interv2 = movementLine.param(p);
			intersection.set( interv2.x, cen.y + 0.1f, interv2.y );
			return true;
		}
	}

	return false;
}

// waypoint.cpp
