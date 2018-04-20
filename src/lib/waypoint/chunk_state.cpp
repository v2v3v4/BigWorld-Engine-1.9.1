/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "chunk_state.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT(0);

/**
 *	This is the constructor.
 */ 
ChunkState::ChunkState() :
	pChunk_(NULL),
	distanceFromParent_(0.0f)
{
}
	
/**
 *	This method sets the chunk for this state.
 */ 
void ChunkState::setChunk(const WaypointChunk* pChunk)
{
	pChunk_ = pChunk;
}

/**
 *	This method sets the position for this state.
 */ 
void ChunkState::setPosition(const Vector3& position)
{
	position_ = position;
}

/**
 *	This method compares this state with another one.
 *	Another ChunkState will be considered equivalent
 *	if its chunkID is the same. The position is not taken
 *	into account.
 *
 *	@return 	Negative if this is less than other
 *				Positive if this is greater than other
 *				Zero if this equals other
 */ 
int ChunkState::compare(const ChunkState& other) const
{
	return strcmp(pChunk_->chunkID().c_str(), 
			other.pChunk_->chunkID().c_str());
}

/**
 *	This method returns true if this state matches the
 *	given goal state.
 *
 *	@param	The goal state to check against
 *	@return True if this state matches the goal.
 */
bool ChunkState::isGoal(const ChunkState& goal) const
{
	return pChunk_ == goal.pChunk_;
}

/**
 *	This method returns the number of adjacencies for this state.
 */ 
int ChunkState::getAdjacencyCount() const
{
	return pChunk_->getAdjacencyCount();
}

/**
 *	This method returns an adjacent state.
 *
 *	@param index	Index of the state to return.
 *	@param newState	The new state is returned here. 
 *	@param goal		The goal state is passed in here.
 *	@return True if there is an adjacent state with this index.
 */ 
bool ChunkState::getAdjacency(int index, ChunkState& newState, 
		const ChunkState&) const
{
	Vector2 centre;
	newState.pChunk_ = pChunk_->getAdjacentChunk(index);
	centre = newState.pChunk_->centre();

	newState.position_.x = centre.x;
	newState.position_.y = position_.y;
	newState.position_.z = centre.y;
	newState.distanceFromParent_ = (newState.position_ - position_).length();

	return true;
}

/**
 *	This method returns the distance from the parent state.
 */ 
float ChunkState::distanceFromParent() const
{
	return distanceFromParent_;
}

/**
 *	This method returns the distance to the given goal state.
 */ 
float ChunkState::distanceToGoal(const ChunkState& goal) const
{
	return (goal.position_ - position_).length();
}
