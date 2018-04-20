/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _ADJACENT_CHUNK_SET_HEADER
#define _ADJACENT_CHUNK_SET_HEADER


#include "waypoint.hpp"
#include "math/planeeq.hpp"
#include "math/vector3.hpp"
#include <vector>


/**
 *	This class maintains a list of adjacent chunks.
 */
class AdjacentChunkSet
{
public:
	AdjacentChunkSet();

	bool	read(DataSectionPtr pChunkDir, const ChunkID& chunkID);

	const ChunkID&	startChunk() const;  

	bool	hasChunk(const ChunkID& chunkID) const;
	void	addChunk(const ChunkID& chunkID);
	void	addPlane(const PlaneEq& planeEq);

	bool	test(const Vector3& position, ChunkID& chunkID) const;

private:
	bool	readChunk(DataSectionPtr pAdjChunk);
	bool 	testChunk(const Vector3& position, int i) const;

	struct ChunkDef
	{
		ChunkID	chunkID;
		std::vector<PlaneEq> planes;
	};

	std::vector<ChunkDef> chunks_;
	ChunkID startChunk_;
};

#endif // _ADJACENT_CHUNK_SET_HEADER
