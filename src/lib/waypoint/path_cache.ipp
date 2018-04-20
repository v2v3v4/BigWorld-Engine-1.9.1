/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *	This file contains the implementation of the PathCache class.
 */ 

/**
 *	This is the constructor.
 */ 
template<class Key, class AStarType>
PathCache<Key, AStarType>::PathCache(unsigned int maxSize) : 
	cache_(maxSize)
{
}

/**
 *	This method checks for a cached search result.
 *
 *	@param key 		Search key
 *	@param start	Start state
 *	@param goal		Goal state
 *	@param next		Next state is returned here
 *
 *	@return True if successful.
 */
template<class Key, class AStarType>
bool PathCache<Key, AStarType>::checkCache(Key key, const State& start, 
		const GoalState& goal, State& next)
{
	Path* pPath = cache_.find(key);

	if(pPath)
	{
		// If the goal for the cached path is different,
		// it is of no use.

		if(pPath->goal_.compare(goal) != 0)
			return false;
		
		// Find a state in the path that matches our current
		// state. Then, move to the next state. We don't advance
		// the index_ at this time, since it is not guaranteed that
		// we will move all the way to the next state. 
		
		while(pPath->index_ < pPath->states_.size() - 1)
		{
			if(pPath->states_[pPath->index_].compare(start) == 0)
			{
				next = pPath->states_[pPath->index_ + 1];
				return true;
			}

			// It is ok to advance the index now, since we have
			// definately passed this state.

			pPath->index_++;
		}
	}

	return false;
}

/**
 *	This method takes the current path from the astar search object,
 *	and adds it to the cache with the given key and goal. We can't
 *	assume that the last node in the path is the goal, since some
 *	searches may be limited, and not actually reach the goal.
 *
 *	@param key	Key to associate with the current path
 *	@param goal	Goal to associate with the current path
 */
template<class Key, class AStarType>
void PathCache<Key, AStarType>::addToCache(Key key, const GoalState& goal)
{
	Path path;
	const State* pState;

	path.goal_ = goal;
	path.index_ = 0;

	pState = astar_.first();

	while(pState)
	{
		path.states_.push_back(*pState);
		pState = astar_.next();
	}

	cache_.insert(key, path);
}

/**
 *	This method attempts to find the next state in a search path from
 *	start to goal.
 *
 *	@param key		Search key
 *	@param start	The start state
 *	@param goal		The goal state
 *	@param next		The next state is returned here
 *
 * 	@return True if successful. 
 */
template<class Key, class AStarType>
bool PathCache<Key, AStarType>::search(Key key, const State& start, 
		const GoalState& goal, State& next)
{
	// First try the cache
	if(this->checkCache(key, start, goal, next))
	{
		return true;
	}
	
	astar_.reset();

	if(astar_.search(start, goal))
	{
		this->addToCache(key, goal);
		return this->checkCache(key, start, goal, next);
	}

	return false;
}
