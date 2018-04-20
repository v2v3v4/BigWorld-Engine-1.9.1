/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "Python.h"		// See http://docs.python.org/api/includes.html

#include "pch.hpp"

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Waypoint", 0 )

#include "navigator.hpp"
#include "chunk_waypoint_set.hpp"
#include "common/chunk_portal.hpp"
#include "astar.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk.hpp"
#include <sstream>


// -----------------------------------------------------------------------------
// Section: ChunkWPSetState
// -----------------------------------------------------------------------------

/**
 *	This class is a state in an A-Star search of the chunk waypoint set graph.
 */
class ChunkWPSetState
{
public:
	ChunkWPSetState();
	ChunkWPSetState( ChunkWaypointSetPtr set );
	ChunkWPSetState( const NavLoc& loc );

	typedef ChunkWaypointConns::const_iterator adjacency_iterator;

	int compare( const ChunkWPSetState & other ) const
		{ return intptr(&*set_) - intptr(&*other.set_); }

	std::string desc() const
	{
		std::stringstream ss;
		ss << '(' << position_.x << ", " <<
			position_.y << ", " <<
			position_.z << ") at " <<
			set_->chunk()->identifier();
		return ss.str();
	}

	unsigned int hash() const
	{
		return (uintptr)(&*set_);
	}

	bool isGoal( const ChunkWPSetState & goal ) const
		{ return set_ == goal.set_; }

	adjacency_iterator adjacenciesBegin() const
		{ return set_->connectionsBegin(); }
	adjacency_iterator adjacenciesEnd() const
		{ return set_->connectionsEnd(); }

	bool getAdjacency( adjacency_iterator iter, ChunkWPSetState & neigh,
		const ChunkWPSetState & goal ) const;

	float distanceFromParent() const
		{ return distanceFromParent_; }

	float distanceToGoal( const ChunkWPSetState & goal ) const
		{ return ( position_ - goal.position_ ).length(); }

	ChunkWaypointSetPtr set() const
		{ return set_; }

	void passedActivatedPortal( const bool a )
		{ passedActivatedPortal_ = a; }

	bool passedActivatedPortal() const
		{ return passedActivatedPortal_; }

	void passedShellBoundary( bool a )
		{ passedShellBoundary_ = a; }

	bool passedShellBoundary() const
		{ return passedShellBoundary_; }

	const Vector3& position() const	{ return position_; }
	static bool blockNonPermissive;

private:
	ChunkWaypointSetPtr	set_;
	float				distanceFromParent_;
	bool				passedActivatedPortal_;
	bool				passedShellBoundary_;
	Vector3				position_;
};

bool ChunkWPSetState::blockNonPermissive = true;

/**
 *	Constructor
 */
ChunkWPSetState::ChunkWPSetState() :
	distanceFromParent_( 0.f )
{
}

/**
 *	Constructor
 */
ChunkWPSetState::ChunkWPSetState( ChunkWaypointSetPtr set ) :
	set_( set ),
	distanceFromParent_( 0.f ),
	position_( set->chunk()->centre() )
{
}

/**
 *	Constructor
 */
ChunkWPSetState::ChunkWPSetState( const NavLoc& loc ) :
	set_( loc.set() ),
	distanceFromParent_( 0.f ),
	position_( loc.point() )
{
}

namespace
{

	bool isActivated( ChunkBoundary::Portal * pPortal, Chunk * pFromChunk )
	{
		// first get pointer to corresponding chunkPortal.
		ChunkPortal * pCP = NULL;
		ChunkPyCache::NamedPyObjects os = ChunkPyCache::instance( *pFromChunk ).objects();
		ChunkPyCache::NamedPyObjects::iterator i = os.begin();
		for ( ; i != os.end(); i++ )
		{
			if ( ChunkPortal::Check( &(*i->second) ) )
			{
				ChunkPortal * p = (ChunkPortal*)&*i->second;
				if ( p->pPortal() == pPortal )
				{
					pCP = p;
					break;
				}
			}
		}

		// check to see whether ChunkPortal found [probably won't exist for outdoor
		// chunks] and if so whether or not it is activated (has a door).
		if ( pCP && pCP->activated() )
			return true;

		return false;
	}

}


/**
 *	This method gets the given adjacency, if it can be traversed.
 */
bool ChunkWPSetState::getAdjacency( ChunkWaypointConns::const_iterator iter,
		ChunkWPSetState & neigh,
		const ChunkWPSetState & goal ) const
{
	ChunkWaypointSetPtr pDestWaypointSet = iter->first;
	ChunkBoundary::Portal * pPortal = iter->second;

	Chunk * pFromChunk = set_->chunk();
	Chunk * pToChunk = pPortal->pChunk;

	/*
	DEBUG_MSG( "ChunkWPSetState::getAdjacency: "
		"considering connection from chunk %s to chunk %s\n",
		pFromChunk->identifier().c_str(),
		pToChunk->identifier().c_str() );
	*/


	// if blocking non permissive, and this non-permissive, then don't consider.
	if ( pPortal != NULL && !pPortal->permissive )
		if ( ChunkWPSetState::blockNonPermissive )
			return false;

	// find portal corresponding portal in cwc the other way.
	ChunkBoundary::Portal * pBackPortal = NULL;
	Chunk::piterator p = pToChunk->pbegin();
	for ( ; p != pToChunk->pend(); p++ )
	{
		if ( p->pChunk == pFromChunk )
		{
			pBackPortal = &(*p);
			break;
		}
	}
	if ( pBackPortal == NULL )
	{
		// TODO: Fix and change to error.
		WARNING_MSG( "ChunkWPSetState::getAdjacency: "
			"Encountered one way portal connection, assuming non passable.\n" );
		return false;
	}

	// now check if portal activated or not.
	neigh.passedShellBoundary(
		pFromChunk != pToChunk &&
		( !pFromChunk->isOutsideChunk() ||
		!pToChunk->isOutsideChunk() ) );

	if( neigh.passedShellBoundary() )
	{
		neigh.passedActivatedPortal( isActivated( pPortal, pFromChunk ) |
			isActivated( pBackPortal, pToChunk ) );
	}
	else
	{
		neigh.passedActivatedPortal( false );
	}



	neigh.set_ = pDestWaypointSet;

	if ( !neigh.set_->chunk() )
	{
		// TODO: Fix this properly. Nav system needs to be able to better deal
		// with chunks going away.
		WARNING_MSG( "ChunkWPSetState::getAdjacency: "
			"Chunk associated with neighbouring waypoint set no longer exists.\n" );
		return false;
	}

	ChunkPtr nec = neigh.set_->chunk();
	BoundingBox bb = nec->localBB();
	Vector3 start = nec->transformInverse().applyPoint( position_ );
	Vector3 end = nec->transformInverse().applyPoint( goal.position_ );

	if (bb.clip( start, end ))
	{
		neigh.position_ = nec->transform().applyPoint( start );
	}
	else
	{
		Vector2 dir( end.x - start.x, end.z - start.z );

		Vector2 min( bb.maxBounds().x, bb.maxBounds().z );

		float minDistSquared = FLT_MAX;

		for (int i = 0; i < 4; ++i)
		{
			Vector2 p;
			p.x = ( i & 1 ) ? bb.minBounds().x : bb.maxBounds().x;
			p.y = ( i & 2 ) ? bb.minBounds().z : bb.maxBounds().z;

			Vector2 minVec( p.x - start.x, p.y - start.z );
			float distSquared = minVec.crossProduct( dir );
			distSquared = distSquared * distSquared;

			if (distSquared < minDistSquared)
			{
				min = p;
				minDistSquared = distSquared;
			}
		}
		neigh.position_ = nec->transform().applyPoint( Vector3( min.x, start.y, min.y ) );
	}

	neigh.distanceFromParent_ = this->distanceToGoal( neigh );
	return true;
}


// -----------------------------------------------------------------------------
// Section: ChunkWaypointState
// -----------------------------------------------------------------------------

/**
 *	This class is a state in an A-Star search of some waypoint graph.
 */
class ChunkWaypointState
{
public:
	typedef int adjacency_iterator;

	ChunkWaypointState();
	ChunkWaypointState( ChunkWaypointSetPtr dstSet, const Vector3 & dstPoint,
		   ChunkWaypointSetPtr pSrcSet );
	ChunkWaypointState( const NavLoc & navLoc );

	int		compare( const ChunkWaypointState & other ) const
		{ return (navLoc_.set() == other.navLoc_.set()) ?
			navLoc_.waypoint() - other.navLoc_.waypoint() :
			intptr(&*navLoc_.set()) - intptr(&*other.navLoc_.set()); }

	std::string desc() const
	{
		if (!navLoc_.set())
		{
			return "no set";
		}

		if (!navLoc_.set()->chunk())
		{
			return "no chunk";
		}

		Vector3 v = navLoc_.point();
		std::stringstream ss;
		ss << navLoc_.waypoint() << " (" << v.x << ' ' << v.y << ' ' << v.z << ')' <<
			" in " << navLoc_.set()->chunk()->identifier();
		return ss.str();
	}

	unsigned int hash() const
	{
		return (uintptr)(&*navLoc_.set()) + navLoc_.waypoint();
	}

	bool	isGoal( const ChunkWaypointState & goal ) const
		{ return navLoc_.set() == goal.navLoc_.set() &&
			navLoc_.waypoint() == goal.navLoc_.waypoint(); }

	adjacency_iterator adjacenciesBegin() const
		{ return 0; }
	adjacency_iterator adjacenciesEnd() const
		{ return navLoc_.waypoint() >= 0 ?
			navLoc_.set()->waypoint( navLoc_.waypoint() ).edges_.size() : 0; }

	bool	getAdjacency( adjacency_iterator iter, ChunkWaypointState & neigh,
		const ChunkWaypointState & goal ) const;

	float	distanceFromParent() const
		{ return distanceFromParent_; }

	float	distanceToGoal( const ChunkWaypointState & goal ) const
		{ return (navLoc_.point() - goal.navLoc_.point()).length(); }

	const NavLoc & navLoc() const
		{ return navLoc_; }

private:
	NavLoc	navLoc_;
	float	distanceFromParent_;
};


/**
 *	Constructor
 */
ChunkWaypointState::ChunkWaypointState() :
	distanceFromParent_( 0.f )
{
}


/**
 *	Constructor.
 *
 *	@param pDstSet A pointer to the destination ChunkWaypointSet.
 *	@param dstPoint The destination point in the coordinates of the destination
 *		ChunkWaypointSet.
 *	@param pSrcSet The source ChunkWaypointSet that this ChunkWaypointState will
 *		be used with. (This is needed to convert dstPoint to the correct
 *		coordinate system.
 */
ChunkWaypointState::ChunkWaypointState( ChunkWaypointSetPtr pDstSet,
		const Vector3 & dstPoint,
		ChunkWaypointSetPtr	pSrcSet ) :
	distanceFromParent_( 0.f )
{
	navLoc_.set_ = pDstSet;
	navLoc_.waypoint_ = -1;

	navLoc_.point_ = dstPoint;

	navLoc_.clip();
}


/**
 *	Constructor
 */
ChunkWaypointState::ChunkWaypointState( const NavLoc & navLoc ) :
	navLoc_( navLoc ),
	distanceFromParent_( 0.f )
{
}

/**
 *  This method gets the given adjacency, if it can be traversed
 */
bool ChunkWaypointState::getAdjacency( int index, ChunkWaypointState & neigh,
	const ChunkWaypointState & goal ) const
{
	const ChunkWaypoint & cw = navLoc_.set()->waypoint( navLoc_.waypoint() );
	const ChunkWaypoint::Edge & cwe = cw.edges_[ index ];

	int waypoint = cwe.neighbouringWaypoint();
	bool waypointAdjToChunk = cwe.adjacentToChunk();
	if (waypoint >= 0)
	{
		neigh.navLoc_.set_ = navLoc_.set();
		neigh.navLoc_.waypoint_ = waypoint;
	}
	else if (waypointAdjToChunk)
	{
		neigh.navLoc_.set_ =
			navLoc_.set()->connectionWaypoint( cwe );
		neigh.navLoc_.waypoint_ = -1;
	}
	else
	{
		return false;
	}

	Vector2 src( navLoc_.point().x, navLoc_.point().z );
	Vector2 dst( goal.navLoc_.point().x, goal.navLoc_.point().z );
	Vector2 way;
	Vector2 del = dst - src;
	Vector2 p1 = cwe.start_;
	Vector2 p2 = cw.edges_[ (index+1) % cw.edges_.size() ].start_;

	float cp1 = del.crossProduct( p1 - src );
	float cp2 = del.crossProduct( p2 - src );
	// see if our path goes through this edge
	if (cp1 > 0.f && cp2 < 0.f)
	{

		// calculate the intersection of the line (src->dst) and (p1->p2).
		// Brief description of how this works. cp1 and cp2 are the areas of
		// the parallelograms formed by the intervals of the cross product.
		// We want the ratio that the intersection point divides p1->p2.
		// This is the same as the ratio of the perpendicular heights of p1
		// and p2 to del. This is also the same as the ratio between the
		// area of the parallelograms (divide by len(del)).
		// Trust me, this works.
		way = p1 + (cp1/(cp1-cp2)) * (p2-p1);

		/*
		// yep, use the intersection
		LineEq moveLine( src, dst, true );
		LineEq edgeLine( p1, p2, true );
		way = moveLine.param( moveLine.intersect( edgeLine ) );
		*/
	}
	else
	{
		way = (fabs(cp1) < fabs(cp2)) ? p1 : p2;
	}

	if (neigh.navLoc_.waypoint_ == -1)
	{
		del.normalise();
		way += del * 0.2f;
	}

	neigh.navLoc_.point_.set( way.x, cw.maxHeight_, way.y );

	if (waypointAdjToChunk)
	{
		if (neigh.navLoc_.set() && neigh.navLoc_.set()->chunk())
		{
			BoundingBox bb = neigh.navLoc_.set()->chunk()->boundingBox();
			static const float inABit = 0.01f;
			neigh.navLoc_.point_.x = Math::clamp( bb.minBounds().x + inABit,
				neigh.navLoc_.point_.x, bb.maxBounds().x - inABit );
			neigh.navLoc_.point_.z = Math::clamp( bb.minBounds().z + inABit,
				neigh.navLoc_.point_.z, bb.maxBounds().z - inABit );
		}
		else
		{
			return false;
		}
	}

	neigh.navLoc_.clip();
	neigh.distanceFromParent_ =
		(neigh.navLoc_.point() - navLoc_.point()).length();

	//DEBUG_MSG( "AStar: Considering adjacency from %d to %d "
	//	"dest (%f,%f,%f) dist %f\n",
	//	navLoc_.waypoint(), neigh.navLoc_.waypoint(),
	//	way.x, cw.maxHeight_, way.y, neigh.distanceFromParent_ );

	return true;
}


// -----------------------------------------------------------------------------
// Section: NavigatorCache
// -----------------------------------------------------------------------------

/**
 *  This class caches data from recent searches. It is purposefully not
 *  defined in the header file so that our users need not know its contents.
 */
class NavigatorCache : public ReferenceCount
{
public:
	const ChunkWaypointState * saveWayPath( AStar<ChunkWaypointState> & astar );

	const ChunkWaypointState * findWayPath(
		const ChunkWaypointState & src, const ChunkWaypointState & dst );

	const ChunkWPSetState * saveWaySetPath( AStar<ChunkWPSetState> & astar );

	const ChunkWPSetState * findWaySetPath(
		const ChunkWPSetState & src, const ChunkWPSetState & dst );

	int getWaySetPathSize() const;

	// this may be necessary because different results will be obtained depending
	// on whether ChunkWPSetState::blockNonPermissive is true or false.
	void clearWPSetCache()
		{ waySetPath_.clear(); }

	void clearWPCache()
		{ wayPath_.clear(); }

	const std::vector<ChunkWaypointState>& wayPath() const
		{ return wayPath_; }

	void passedShellBoundary( bool a )
		{ passedShellBoundary_ = a; }

	bool passedShellBoundary() const
		{ return passedShellBoundary_; }
private:
	std::vector<ChunkWaypointState>		wayPath_;
	std::vector<ChunkWPSetState>		waySetPath_;
	bool								passedShellBoundary_;
};

/**
 *  This method saves a waypoint path.
 *
 *  Both the source and destination (goal) are extracted from the
 *  search result in the given AStar object. (i.e. must be unspoilt)
 */
const ChunkWaypointState * NavigatorCache::saveWayPath(
	AStar<ChunkWaypointState> & astar )
{
	std::vector<const ChunkWaypointState *>		fwdPath;

	// get out all the pointers
	const ChunkWaypointState * as = astar.first();
	bool first = true;
	const ChunkWaypointState* last = as;

	while (as != NULL)
	{
		if (!almostZero( as->distanceFromParent() ) || first)
		{
			fwdPath.push_back( as );
			first = false;
		}

		as = astar.next();

		if (as)
		{
			last = as;
		}
	}

	if (fwdPath.size() < 2)
	{
		// make sure that fwdPath has at least 2 nodes
		fwdPath.push_back( last );
	}

	// and store the path in reverse order (for pop_back)
	wayPath_.clear();
	for (int i = fwdPath.size()-1; i >= 0; i--)
	{
		wayPath_.push_back( *fwdPath[i] );
	}

	MF_ASSERT_DEBUG( wayPath_.size() >= 2 );
	return &wayPath_[wayPath_.size()-2];
}

/**
 *  This method finds a waypoint path
 */
const ChunkWaypointState * NavigatorCache::findWayPath(
	const ChunkWaypointState & src, const ChunkWaypointState & dst )
{
	// see if we have a path
	if (wayPath_.size() < 2) return NULL;

	// see if the path goes to the same waypoint
	// (we intentionally ignore lpoint here ... we would not be
	// called if src and dst were the same waypoint, since our
	// user can take care of itself once that is achieved)
	if (dst.navLoc().set() != wayPath_.front().navLoc().set() ||
		dst.navLoc().waypoint() != wayPath_.front().navLoc().waypoint())
			return NULL;

	// ok, this path leads to the right place.
	// now make sure it starts from the right place too.
	if (src.navLoc().set() == wayPath_.back().navLoc().set() &&
		src.navLoc().waypoint() == wayPath_.back().navLoc().waypoint())
	{
		// looks good.
		return &wayPath_[wayPath_.size()-2];
	}

	// we might have progressed to the next node, so move on to it
	wayPath_.pop_back();

	// but make sure we didn't run out of nodes
	// (no point checking if src is dst, that is known false)
	if (wayPath_.size() < 2) return NULL;

	// and see if it suits our purpose
	if (src.navLoc().set() == wayPath_.back().navLoc().set() &&
		src.navLoc().waypoint() == wayPath_.back().navLoc().waypoint())
	{
		return &wayPath_[wayPath_.size()-2];
	}

	// nope, no good. clear it now just for sanity
	wayPath_.clear();
	return NULL;
}


/**
 *  This method saves a waypoint set path.
 *
 *  Both the source and destination (goal) are extracted from the
 *  search result in the given AStar object. (i.e. must be unspoilt)
 */
const ChunkWPSetState * NavigatorCache::saveWaySetPath(
	AStar<ChunkWPSetState> & astar )
{
	std::vector<const ChunkWPSetState *>		fwdPath;

	// get out all the pointers
	passedShellBoundary_ = false;
	const ChunkWPSetState * as = astar.first();
	while (as != NULL)
	{
		passedShellBoundary_ = passedShellBoundary_ || as->passedShellBoundary();
		fwdPath.push_back( as );
		as = astar.next();
	}

	// and store the path in reverse order (for pop_back)
	waySetPath_.clear();
	for (int i = fwdPath.size()-1; i >= 0; i--)
	{
		waySetPath_.push_back( *fwdPath[i] );
	}

	MF_ASSERT_DEBUG( waySetPath_.size() >= 2 );
	return &waySetPath_[waySetPath_.size()-2];
}

/**
 *  This method finds a waypoint path
 */
const ChunkWPSetState * NavigatorCache::findWaySetPath(
	const ChunkWPSetState & src, const ChunkWPSetState & dst )
{
	if( passedShellBoundary() )
	{
		waySetPath_.clear();
		return NULL;
	}

	// see if we have a path
	if (waySetPath_.size() < 2) return NULL;

	// see if the path goes to the same waypoint set
	// (we assume that src is no the same as dest, or why call us)
	if (dst.set() != waySetPath_.front().set())
		return NULL;

	// ok, this path leads to the right place.
	// now make sure it starts from the right place too.
	if (src.set() == waySetPath_.back().set())
	{
		// looks good.
		return &waySetPath_[waySetPath_.size()-2];
	}

	// we might have progressed to the next node, so move on to it
	waySetPath_.pop_back();

	// but make sure we didn't run out of nodes
	// (no point checking if src is dst, that is known false)
	if (waySetPath_.size() < 2) return NULL;

	// and see if it suits our purpose
	if (src.set() == waySetPath_.back().set())
	{
		return &waySetPath_[waySetPath_.size()-2];
	}

	// nope, no good. clear it now just for sanity
	waySetPath_.clear();
	return NULL;
}

/**
 *  This method finds a waypoint path
 */
int NavigatorCache::getWaySetPathSize() const
{
	return waySetPath_.size();
}

// -----------------------------------------------------------------------------
// Section: NavLoc
// -----------------------------------------------------------------------------

/**
 *	Default constructor (result always invalid)
 */
NavLoc::NavLoc()
{
}

/**
 *	Constructor from a space and a point in that space's world coords.
 */
NavLoc::NavLoc( ChunkSpace * pSpace, const Vector3 & point, float girth )
{
	// To ensure that we always get the correct chunk because
	// sometimes a shell is on a chunk exactly, which causes
	// y conflicts.
	Vector3 p( point );
	p.y += 0.01f;
	Chunk * pChunk = pSpace->findChunkFromPoint( p );
	//DEBUG_MSG( "NavLoc::NavLoc: from Space: chunk %s\n",
	//	pChunk ? pChunk->identifier().c_str() : "no chunk" );
	if (pChunk != NULL)
	{
		point_ = point;
		ChunkNavigator::Result res;
		if (ChunkNavigator::instance( *pChunk ).find( point_, girth, res ))
		{
			//DEBUG_MSG( "NavLoc::NavLoc: waypoint %d\n", res.waypoint_ );
			set_ = res.pSet_;
			waypoint_ = res.waypoint_;
		}
		//DEBUG_MSG( "NavLoc::NavLoc: point (%f,%f,%f)\n",
		//	point_.x, point_.y, point_.z );
	}
}

/**
 *	Constructor from a chunk and a point in that chunk's local coords.
 */
NavLoc::NavLoc( Chunk * pChunk, const Vector3 & point, float girth ) :
	point_( point )
{
	MF_ASSERT_DEBUG( pChunk != NULL );

	//DEBUG_MSG( "NavLoc::NavLoc: from Chunk: chunk %s\n",
	//	pChunk ? pChunk->identifier().c_str() : "no chunk" );
	ChunkNavigator::Result res;
	if (ChunkNavigator::instance( *pChunk ).find( point_, girth, res ))
	{
		set_ = res.pSet_;
		waypoint_ = res.waypoint_;
	}
	//DEBUG_MSG( "NavLoc::NavLoc: point (%f,%f,%f)\n",
	//	point_.x, point_.y, point_.z );
}

/**
 *	Constructor from a similar navloc and a point in world coords.
 *
 *	First it tries the same waypoint, then the same waypoint set,
 *	and if that fails then it resorts to the full world point search.
 */
NavLoc::NavLoc( const NavLoc & guess, const Vector3 & point )
	: point_( point )
{
	MF_ASSERT_DEBUG( guess.valid() );

	//DEBUG_MSG( "NavLoc::NavLoc: Guessing...\n");
	ChunkNavigator::Result res;
	//DEBUG_MSG( "NavLoc::NavLoc: guessing point (%f,%f,%f)\n",
	//		point_.x, point_.y, point_.z );

	//DEBUG_MSG( "NavLoc::NavLoc: guess NavLoc (%f,%f,%f) in %s id %d\n",
	//	guess.point().x, guess.point().y, guess.point().z,
	//	guess.set()->chunk()->identifier().c_str(), guess.waypoint());

	if (guess.waypoint() == -1)
		waypoint_ = guess.set()->find( point_ );
	else if ((point_ - guess.point()).lengthSquared() < 0.00001f)
		waypoint_ = guess.waypoint();
	else if (guess.set()->waypoint( guess.waypoint() ).contains( point_ ))
		waypoint_ = guess.waypoint();
	else
		waypoint_ = guess.set()->find( point_ );
	set_ = guess.set();

	if (waypoint_ < 0)
		*this = NavLoc( set_->chunk()->space(), point, set_->girth() );

	//DEBUG_MSG( "NavLoc::NavLoc: point (%f,%f,%f)\n",
	//	point_.x, point_.y, point_.z );
}


/**
 *	Destructor
 */
NavLoc::~NavLoc()
{
}

/**
 * This method returns whether or not the lpoint is within the waypoint
 */
bool NavLoc::isWithinWP() const
{
	if ( set_ && waypoint_ >= 0 )
		return set_->waypoint( waypoint_ ).contains( point_ );
	else
		return 0;
}

/**
 * This method clips the lpoint so that it is within the waypoint.
 */
void NavLoc::clip()
{
	clip( point_ );
}


/**
 * This method clips the point so that it is within the waypoint.
 */
void NavLoc::clip( Vector3& point ) const
{
	if ( set_ && waypoint_ >= 0 )
		set_->waypoint( waypoint_ ).clip( set_->chunk(), point );
}


/**
 * This method gives a description of the current NavLoc
 */
std::string NavLoc::desc() const
{
	std::stringstream ss;

	if (set_ && set_->chunk())
	{
		ss << set_->chunk()->identifier() << ':' << waypoint() << ' ';

		ss << point_;

		if (waypoint_ != -1)
		{
			ss << " - ";

			ChunkWaypoint& wp = set_->waypoint( waypoint_ );

			for (int i = 0; i < wp.edgeCount_; ++i)
			{
				ss << '(' << wp.edges_[ i ].start_.x << ", "
					<< wp.edges_[ i ].start_.y << ')';

				if (i != wp.edgeCount_ - 1)
				{
					ss << ", ";
				}
			}
		}
	}
	else
	{
		ss << point_;
	}

	return ss.str();
}

// -----------------------------------------------------------------------------
// Section: Navigator
// -----------------------------------------------------------------------------

/**
 *	Constructor
 */
Navigator::Navigator()
	: pCache_(NULL)
{
}

/**
 *	Destructor
 */
Navigator::~Navigator()
{
}

void Navigator::clearWPSetCache()
{
	if ( pCache_ )
	{
		pCache_->clearWPSetCache();
	}
}

void Navigator::clearWPCache()
{
	if (pCache_)
	{
		pCache_->clearWPCache();
	}
}

/**
 *	Find the path between the given NavLocs. They must be valid and distinct.
 *
 *	@note The NavLoc returned in 'way' should be handled with care,
 *	as it may only be semi-valid. Do not pass it into any methods
 *	without first verifying it using the 'guess' NavInfo constructor.
 *	For example, do not pass it back into findPath without doing this.
 *	Also, the 'lpoint' in that NavLoc should always be interpreted in the
 *	coordinate system of the src NavLoc, not its own.
 */
bool Navigator::findPath( const NavLoc & src, const NavLoc & dst, float maxDistance,
	NavLoc & way, bool blockNonPermissive, bool & passedActivatedPortal )
{
	// it makes no sense to check for maxDistance inside
	// navpoly set if it is greater than GRID_RESOLUTION.
	float maxDistanceInSet = maxDistance > GRID_RESOLUTION ? -1.f : maxDistance;

	passedActivatedPortal = false;

	this->infiniteLoopProblem = false;

	MF_ASSERT_DEBUG( src.valid() && dst.valid() );

	if (!pCache_) pCache_ = new NavigatorCache();

	// see if they are in the same waypoint set
	if (src.set() == dst.set())
	{
		// we need to find a path amongst the waypoints in this set
		ChunkWaypointState srcState( src );
		ChunkWaypointState dstState( dst );

		// check if it's in our cache
		const ChunkWaypointState * pWayState =
			pCache_->findWayPath( srcState, dstState );
		if (pWayState == NULL)
		{
			// do an A-Star search amongst the waypoints then
			AStar<ChunkWaypointState> astar;

			if (astar.search( srcState, dstState, maxDistanceInSet ))
			{
				pWayState = pCache_->saveWayPath( astar );
				//DEBUG_MSG( "Navigator::findPath: "
				//		"Next waypoint %d found through a new search\n",
				//	pWayState->navLoc().waypoint() );
			}
			if ( astar.infiniteLoopProblem )
			{
				ERROR_MSG( "Navigator::findPath: Infinite Loop problem "
					"from waypoint %s to %s\n",
					src.desc().c_str(), dst.desc().c_str() );
				this->infiniteLoopProblem = true;
			}
		}
		//else
		//{
		//	DEBUG_MSG( "Next waypoint %d found in cache\n",
		//		pWayState->navLoc().waypoint() );
		//}

		if (pWayState != NULL)
		{
			way = pWayState->navLoc();
			return true;
		}

		//DEBUG_MSG( "Navigator::findPath: "
		//		"No path amongst waypoints of same set\n" );
	}
	// src and dst are in different sets
	else
	{
		// we need to find a path amongst the waypoint sets
		ChunkWPSetState srcSetState( src );
		ChunkWPSetState dstSetState( dst );

		// check if it's in the cache
		const ChunkWPSetState * pWaySetState =
			pCache_->findWaySetPath( srcSetState, dstSetState );

		if (pWaySetState == NULL)
		{
			// do an A-Star search amongst the waypoint sets then
			ChunkWPSetState::blockNonPermissive = blockNonPermissive;
			AStar<ChunkWPSetState> astarSet;
			if (astarSet.search( srcSetState, dstSetState, maxDistance ))
			{
				pWaySetState = pCache_->saveWaySetPath( astarSet );
				//DEBUG_MSG( "Next waypoint set 0x%08X found through "
				//	"a new search\n", &*pWaySetState->set() );
			}
			if ( astarSet.infiniteLoopProblem )
			{
				ERROR_MSG( "Navigator::findPath: Infinite Loop problem "
					"from waypoint %d to %d\n", src.waypoint(), dst.waypoint() );
				this->infiniteLoopProblem = true;
			}
		}
		//else
		//{
		//	DEBUG_MSG( "Next waypoint set 0x%08X found through cache\n",
		//		&*pWaySetState->set() );
		//}
		if (pWaySetState != NULL)
		{
			// now search amongst the waypoints
			ChunkWaypointState srcState( src );
			ChunkWaypointState dstState( pWaySetState->set(), dst.point(),
				src.set() );
			// check if it's in our cache
			const ChunkWaypointState * pWayState =
				pCache_->findWayPath( srcState, dstState );
			if (pWayState == NULL)
			{
				// do the A-Star waypoint search then
				AStar<ChunkWaypointState> astar;
				if (astar.search( srcState, dstState, maxDistanceInSet ))
				{
					pWayState = pCache_->saveWayPath( astar );
					//DEBUG_MSG( "Next ulterior waypoint %d found through "
					//	"a new search\n", pWayState->navLoc().waypoint() );
				}
				if ( astar.infiniteLoopProblem )
				{
					ERROR_MSG( "Navigator::findPath: Infinite Loop problem "
						"from waypoint %d to %d\n", src.waypoint(), dst.waypoint() );
					this->infiniteLoopProblem = true;
				}

			}
			//else
			//	DEBUG_MSG( "Next ulterior waypoint %d found in cache\n",
			//		pWayState->navLoc().waypoint() );
			if (pWayState != NULL)
			{
				way = pWayState->navLoc();
				if ( (way.set() != src.set()) && pWaySetState->passedActivatedPortal() )
				{
					passedActivatedPortal = true;
				}
				return true;
			}

			//DEBUG_MSG( "Navigator::findPath: "
			//	"No path amongst waypoints to foreign set frontier\n" );
		}
		//DEBUG_MSG( "Navigator::findPath: "
		//	"No path amongst waypoint sets\n" );
	}

	// if anything falls through to here then we failed
	return false;
}


/**
 *  This is a helper un-refcounted waypoint reference
 */
struct TempWayRef
{
	TempWayRef() {}
	TempWayRef( const NavLoc & nl ) :
		pSet_( &*nl.set() ),
		waypoint_( nl.waypoint() )
	{}
	TempWayRef( ChunkWaypointSet * pSet, int waypoint ) :
		pSet_( pSet ),
		waypoint_( waypoint )
	{}

	ChunkWaypointSet	* pSet_;
	int					waypoint_;
};

/**
 *	Finds a point where the given situation is ahead, i.e. in the
 *	direction of a target, constrained to a radius from the source.
 *	The source NavLoc must be valid.
 */
bool Navigator::findSituationAhead( uint32 situation, const NavLoc & src,
	float radius, const Vector3 & tgt, Vector3 & dst )
{
	MF_ASSERT( 0 && "deprecated and haven't been converted" );
	MF_ASSERT( src.valid() );

	// Set up some variables
	const float sweepCrossDist = 30.f;
	float radiusSquared = radius * radius;
	float bestDistSquared = -1;
	ChunkWaypoint::Edges::const_iterator eit;
	int neigh;

	// calculate local and world versions of our points
	ChunkWaypointSet * srcLPSet = &*src.set();
	Vector3 srcLPoint;
	Vector3 srcWPoint = src.point();
	Vector3 tgtLPoint = srcLPSet->chunk()->transformInverse().applyPoint( tgt );

	// We traverse the waypoint graph starting from src, since that lets
	// us into other chunks and possibly saves us some effort.
	uint16 mark = ChunkWaypoint::s_nextMark_++;
	src.set()->waypoint( src.waypoint() ).mark_ = mark;

	// Set up the traversal stack
	static VectorNoDestructor<TempWayRef> stack;
	stack.clear();
	stack.push_back( TempWayRef( src ) );
	while (!stack.empty())
	{
		// get the next waypoint on the stack
		TempWayRef tway = stack.back();
		stack.pop_back();

		// get our src point into the relevant local coords
		if (tway.pSet_ != srcLPSet)
		{
			srcLPSet = tway.pSet_;
			srcLPoint = srcLPSet->chunk()->transformInverse().
				applyPoint( srcWPoint );
			tgtLPoint = srcLPSet->chunk()->transformInverse().
				applyPoint( tgt );
		}

		// look at what's adjacent to every edge of the waypoint
		const ChunkWaypoint & waypoint = tway.pSet_->waypoint( tway.waypoint_ );
		for (eit = waypoint.edges_.begin(); eit != waypoint.edges_.end(); eit++)
		{
			if ((neigh = eit->neighbouringWaypoint()) >= 0)
			{
				// if it's another waypoint, consider processing it
				const ChunkWaypoint & nwp = tway.pSet_->waypoint( neigh );
				if (nwp.mark_ == mark) continue;
				nwp.mark_ = mark;
				if (nwp.distanceSquared( tway.pSet_->chunk(), srcLPoint ) < radiusSquared)
					stack.push_back( TempWayRef( tway.pSet_, neigh ) );
			}
			else if (eit->adjacentToChunk())
			{
				// if it's an edge into another set, find the waypoint
				// in the other set, and consider processing it.
				ChunkWaypointSetPtr pOtherSet =
					tway.pSet_->connectionWaypoint( *eit );
				if (!pOtherSet)
				{
					WARNING_MSG( "Navigator::findSituationAhead: "
						"no waypoint set for edge adjacent to chunk\n" );
					continue;
				}
				if (!pOtherSet->chunk())
				{
					WARNING_MSG( "Navigator::findSituationAhead: "
						"adjacent waypoint set has no chunk\n" );
					continue;
				}
				Vector3 otherLPoint = pOtherSet->chunk()->transformInverse().
					applyPoint( srcWPoint );
				int otherWP = pOtherSet->find( otherLPoint );
				if (otherWP < 0) continue;

				const ChunkWaypoint & nwp = pOtherSet->waypoint( otherWP );
				if (nwp.mark_ == mark) continue;
				nwp.mark_ = mark;
				if (nwp.distanceSquared( pOtherSet->chunk(), otherLPoint ) < radiusSquared)
				{
					stack.push_back( TempWayRef( pOtherSet.getObject(),
						otherWP ) );
				}
			}
			else
			{
				// ok, it's not passable, so examine the view
				uint32 vista = eit->neighbouringVista();

				const Vector2 & p1 = eit->start_;
				const Vector2 & p2 = ((eit+1 != waypoint.edges_.end()) ?
					*(eit+1) : waypoint.edges_.front()).start_;

				// see if it is on the correct side of the edge
				LineEq eline( p1, p2 );
				const Vector2 tgtFlat( tgtLPoint.x, tgtLPoint.z );
				if (!eline.isInFrontOf( tgtFlat )) continue;

				// see if the flags match for any of the wedges
				for (int w = 0; w < 3; w++)
				{
					if ( ((vista >> (w<<2)) & 0xF) != situation) continue;

					float t;

					// ok, see if the target is in this wedge then
					const Vector2 & normal = eline.normal();
					Vector2 ortho(p2.x-p1.x, p2.y-p1.y);
					ortho.normalise();
					ortho = ortho * 0.5f;
					Vector2 dir( normal.y, -normal.x );
					if (w == 0)			// front
					{
						LineEq lline( p2, p2 + normal + ortho );
						if (!lline.isInFrontOf( tgtFlat )) continue;
						LineEq rline( p1 + normal - ortho, p1 );
						if (!rline.isInFrontOf( tgtFlat )) continue;
						t = 0.5f;
					}
					else if (w == 1)	// left
					{
						LineEq lline( p2, p2 + normal + dir );
						if (!lline.isInFrontOf( tgtFlat )) continue;
						LineEq rline( p2 + normal*sweepCrossDist, (p1+p2)*0.5 );
						if (!rline.isInFrontOf( tgtFlat )) continue;
						t = 0.25f;
					}
					else				// right
					{
						LineEq lline( p1 + normal + dir, p1 );
						if (!lline.isInFrontOf( tgtFlat )) continue;
						LineEq rline( (p1+p2)*0.5, p1 + normal*sweepCrossDist );
						if (!rline.isInFrontOf( tgtFlat )) continue;
						t = 0.75f;
					}

					// ok we're on

					// Should make sure there aren't any other holes in the way.
					// Do this by walking around the outside of this hold until
					// we intersect with the line from it to the target, and
					// then make sure there's only waypoints all the way to the
					// target. But for now we'll just assume it'll be ok :)

					// TODO: also should randomise t along the available portion
					// of the edge.

					// get our point along the edge
					Vector2 candPoint = p1 * (1.f-t) + p2 * t - normal * 0.25f;
						// moved in along the normal a little

					// and if it's better than the best, keep it!
					float candDistSquared = (candPoint - Vector2(
						srcLPoint.x, srcLPoint.z )).lengthSquared();
					if (candDistSquared < bestDistSquared ||
						bestDistSquared < 0.f)
					{
						bestDistSquared = candDistSquared;
						dst = srcLPSet->chunk()->transform().applyPoint(
							Vector3( candPoint.x, waypoint.maxHeight_,
								candPoint.y ) );
					}
				}
			}
		}
	}


	return bestDistSquared >= 0.f;
}

/**
 *  This method finds a waypoint path
 */
int Navigator::getWaySetPathSize() const
{
	if (pCache_)
		return pCache_->getWaySetPathSize();
	else
		return 0;
}

void Navigator::getWaypointPath( const NavLoc& srcLoc,std::vector<Vector3>& wppath )
{
	wppath.clear();

	if (pCache_)
	{
		const std::vector<ChunkWaypointState>& path = pCache_->wayPath();
		std::vector<ChunkWaypointState>::const_reverse_iterator iter =
			path.rbegin();

		while (iter != path.rend())
		{
			wppath.push_back( iter->navLoc().point() );
			++iter;
		}
	}
}

// navigator.cpp
