/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _PATH_CACHE_HEADER
#define _PATH_CACHE_HEADER

#include "cstdmf/cache.hpp"
#include "astar.hpp"

/**
 *	This class implements a cache of A* search paths.
 *	The caller can request the next node in a search, and it will either
 *	return a node from a cached search, or perform a new search if there
 *	is not a cached one.
 */ 
template<class Key, class AStarType> class PathCache
{
public:
	typedef typename AStarType::TState State;
	typedef typename AStarType::TGoalState GoalState;

	PathCache(unsigned int maxSize);

	bool search(Key key, const State& start, const GoalState& goal, State&next);

private:

	bool checkCache(Key, const State&, const GoalState&, State&);
	void addToCache(Key, const GoalState&);

	struct Path
	{
		std::vector<State>	states_;
		unsigned int		index_;
		GoalState			goal_;
	};

	Cache<Key, Path> 		cache_;
	AStarType				astar_;
};
		
#include "path_cache.ipp"
#endif
