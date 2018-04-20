/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _CHUNK_STATE_HEADER
#define _CHUNK_STATE_HEADER

#include "waypoint_chunk.hpp"

/**
 *	This class represents the search state on a graph of Chunks.
 */ 
class ChunkState
{
public:
	ChunkState();

	void	setChunk(const WaypointChunk* pChunk);
	void	setPosition(const Vector3& position);

	const ChunkID& chunkID() const	{ return pChunk_->chunkID(); }
	
	int 	compare(const ChunkState& other) const;
	bool	isGoal(const ChunkState& goal) const;
	int		getAdjacencyCount() const;
	bool	getAdjacency(int index, ChunkState&, const ChunkState&) const;
	float	distanceFromParent() const;
	float	distanceToGoal(const ChunkState& goal) const;
	
private:
	const WaypointChunk*	pChunk_;
	float					distanceFromParent_;
	Vector3					position_;
};	

#endif
