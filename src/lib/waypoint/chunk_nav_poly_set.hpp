/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_NAV_POLY_SET_HPP
#define CHUNK_NAV_POLY_SET_HPP

#include "chunk_waypoint_set.hpp"

/**
 *	This class is used in ChunkWaypointSet creation.
 */
class ChunkNavPolySet : public ChunkWaypointSet
{
	DECLARE_CHUNK_ITEM( ChunkNavPolySet )

public:
	ChunkNavPolySet();
	~ChunkNavPolySet();

	static ChunkItemFactory::Result navmeshFactory( Chunk * pChunk, DataSectionPtr pSection );
};

#endif // CHUNK_NAV_POLY_SET_HPP
