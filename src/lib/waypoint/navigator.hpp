/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef NAVIGATOR_HPP
#define NAVIGATOR_HPP

#include "math/vector3.hpp"
#include "cstdmf/smartpointer.hpp"
#include "cstdmf/stdmf.hpp"


class ChunkSpace;
class Chunk;
class ChunkWaypointSet;
typedef SmartPointer<ChunkWaypointSet> ChunkWaypointSetPtr;

#include "chunk_waypoint_set.hpp"


/**
 *	This class is a location in the navigation mesh
 */
class NavLoc
{
public:
	NavLoc();
	NavLoc( ChunkSpace * pSpace, const Vector3 & point, float girth );
	NavLoc( Chunk * pChunk, const Vector3 & point, float girth );
	NavLoc( const NavLoc & guess, const Vector3 & point );
	~NavLoc();

	bool valid() const						{ return set_ && set_->chunk(); }

	ChunkWaypointSetPtr set() const			{ return set_; }
	int					waypoint() const	{ return waypoint_; }
	Vector3				point() const	{ return point_; }

	bool isWithinWP() const;
	void clip();
	void clip( Vector3& point ) const;
	std::string desc() const;

private:
	ChunkWaypointSetPtr	set_;
	int					waypoint_;
	Vector3				point_;

	friend class ChunkWaypointState;
	friend class Navigator;
};


class NavigatorCache;
typedef SmartPointer<NavigatorCache> NavigatorCachePtr;

/**
 *	This class guides vessels through the treacherous domain of chunk
 *	space navigation. Each instance caches recent data so similar searches
 *	can reuse previous effort.
 */
class Navigator
{
public:
	Navigator();
	~Navigator();

	bool findPath( const NavLoc & src, const NavLoc & dst, float maxDistance,
		NavLoc & way, bool blockNonPermissive, bool & passedActivatedPortal );

	bool findSituationAhead( uint32 situation, const NavLoc & src,
		float radius, const Vector3 & tgt, Vector3 & dst );

	int getWaySetPathSize() const;

	void clearWPSetCache();
	void clearWPCache();

	bool infiniteLoopProblem;

	void getWaypointPath( const NavLoc& srcLoc, std::vector<Vector3>& wppath );

	static void astarSearchTimeLimit( float seconds );
	static float astarSearchTimeLimit();

private:
	Navigator( const Navigator & other );
	Navigator & operator = ( const Navigator & other );

	NavigatorCachePtr	pCache_;
};

#endif // NAVIGATOR_HPP
