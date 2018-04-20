/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"

#include "chunk_boundary.hpp"


#ifndef MF_SERVER
#include "moo/render_context.hpp"
#include "romp/geometrics.hpp"
#include "chunk_manager.hpp"
#endif

#include "resmgr/bwresource.hpp"
#include "resmgr/xml_section.hpp"
#include "math/portal2d.hpp"
#include "chunk.hpp"
#include "chunk_space.hpp"

#include "physics2/worldpoly.hpp"	// a bit unfortunate...

#if UMBRA_ENABLE
#include <umbraModel.hpp>
#include <umbraObject.hpp>
#include "chunk_umbra.hpp"
#endif

#include "cstdmf/config.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/guard.hpp"

DECLARE_DEBUG_COMPONENT2( "Chunk", 0 );

// Static initialiser.
bool ChunkBoundary::Portal::drawPortals_ = false;


// ----------------------------------------------------------------------------
// Section: ChunkBoundary
// ----------------------------------------------------------------------------


/**
 *	Constructor and loader
 *	@param	pSection	the DataSection contains the data defining the boundary
 *	@param	pMapping	the chunk mapping of the chunk holding this boundary
 *	@param	ownerChunkName	the identifier of the chunk holding this boundary
 */
ChunkBoundary::ChunkBoundary( DataSectionPtr pSection,
							 ChunkDirMapping * pMapping,
							 const std::string & ownerChunkName )
{
	BW_GUARD;
	// make sure something's there
	if (!pSection)
	{
		plane_ = PlaneEq( Vector3(0,0,0), 0 );
		return;
	}

	// read the plane
	Vector3 normal = pSection->readVector3( "normal" );
	float d = pSection->readFloat( "d" );
	float normalLen = normal.length();
	normal /= normalLen;
	d /= normalLen;
	plane_ = PlaneEq( normal, d );

	// prepare for some error checking
	bool someInternal = false;
	bool someExternal = false;

	// read any portals
	DataSectionIterator end = pSection->end();
	for (DataSectionIterator it = pSection->begin(); it != end; it++)
	{
		if ((*it)->sectionName() != "portal") continue;

		Portal * newPortal = new Portal( *it, plane_,pMapping,ownerChunkName );

		if (newPortal->isHeaven() || newPortal->isEarth())
			boundPortals_.push_back( newPortal );
		else
			unboundPortals_.push_back( newPortal );

		if (newPortal->internal)
			someInternal = true;
		else
			someExternal = true;
	}

	// make sure no-one stuffed up
	MF_ASSERT_DEV(!(someInternal && someExternal))
}

/**
 *	Destructor
 */
ChunkBoundary::~ChunkBoundary()
{
	BW_GUARD;
	for (Portals::iterator pit = boundPortals_.begin();
		pit != boundPortals_.end();
		pit++)
	{
		delete *pit;
	}

	for (Portals::iterator pit = unboundPortals_.begin();
		pit != unboundPortals_.end();
		pit++)
	{
		delete *pit;
	}
}

/**
 *	Move the given portal to the bound list
 *	@param	unboundIndex	the index of the unbound portal
 */
void ChunkBoundary::bindPortal( uint32 unboundIndex )
{
	BW_GUARD;
	Portal * pPortal = unboundPortals_[unboundIndex];
	boundPortals_.push_back( pPortal );
	unboundPortals_.erase( unboundPortals_.begin() + unboundIndex );
}

/**
 *	Move the given portal to the unbound list
 *	@param	boundIndex	the index of the bound portal
 */
void ChunkBoundary::loosePortal( uint32 boundIndex )
{
	BW_GUARD;
	Portal * pPortal = boundPortals_[boundIndex];
	unboundPortals_.push_back( pPortal );
	boundPortals_.erase( boundPortals_.begin() + boundIndex );
}

/**
 *	This method adds a new invasive portal to the list of unbound portals
 *	in this boundary. Used only by editors.
 *	@param	pPortal	the portal to be added
 */
void ChunkBoundary::addInvasivePortal( Portal * pPortal )
{
	BW_GUARD;
	unboundPortals_.push_back( pPortal );
}

/**
 *	This method splits the identified invasive portal if necessary,
 *	i.e. if it overlaps chunks other than the one it points to.
 *	Also used only by editors. Note that this function is called on
 *	the inside chunk that already has an invasive portal, wheres the
 *	@see addInvasivePortal function is called on the invaded outside chunk.
 *	@param	pChunk	the reference chunk to split the portal
 *	@param	i	the index of the unbound portal
 */
void ChunkBoundary::splitInvasivePortal( Chunk * pChunk, uint i )
{
	BW_GUARD;
	return;
	Portal & p = *unboundPortals_[i];
	Chunk * pDest = p.pChunk;

	// get our plane in world coordinates
	const PlaneEq & srcLocalPlane = plane_;
	Vector3 ndtr = pChunk->transform().applyPoint(
		srcLocalPlane.normal() * srcLocalPlane.d() );
	Vector3 ntr = pChunk->transform().applyVector( srcLocalPlane.normal() );
	PlaneEq srcWorldPlane( ntr, ntr.dotProduct( ndtr ) );

	// make matrices to convert from portal space to world space and back again
	Matrix portalToWorld = Matrix::identity;
	portalToWorld[0] = p.uAxis;
	portalToWorld[1] = p.vAxis;
	portalToWorld[2] = p.plane.normal();
	portalToWorld[3] = p.origin;
	portalToWorld.postMultiply( pChunk->transform() );
	Matrix worldToPortal;
	worldToPortal.invert( portalToWorld );

	WorldPolygon srcPortalPoly;
	bool srcPortalPolyValid = false;

	// slice off a new portal for every boundary that intersects us
	for (ChunkBoundaries::iterator bit = pDest->bounds().begin();
		bit != pDest->bounds().end();
		++bit)
	{
		// get other plane in world coordinates
		const PlaneEq & dstLocalPlane = (*bit)->plane_;
		ndtr = pDest->transform().applyPoint(
			dstLocalPlane.normal() * dstLocalPlane.d() );
		ntr = pDest->transform().applyVector( dstLocalPlane.normal() );
		PlaneEq dstWorldPlane( ntr, ntr.dotProduct( ndtr ) );

		// if parallel or almost so then ignore it
		if (fabsf( dstWorldPlane.normal().dotProduct(
			srcWorldPlane.normal() ) ) > 0.99f)
				continue;

		Portal * pNewPortal = NULL;

		// see which points lie outside this boundary, and add them
		// to newPortal (under pChunk's transform) if we found any...
		// ... remove them from 'p' while we're at it.
		// binary/linear splits are fine - we can split either side again
		// if we have to (either later in this loop, or when this function is
		// called for pNewPortal)

		// we're going to use WorldPolygon to do our dirty work for us,
		// so first turn our current portal into a world coords if necessary
		if (!srcPortalPolyValid)
		{
			srcPortalPoly.clear();
			Vector3 worldPoint;
			for (uint j = 0; j < p.points.size(); ++j)
			{
				//pChunk->transform().applyPoint(
				//	worldPoint, p.origin +
				//		p.uAxis * p.points[j][0] + p.vAxis * (p.points[j][1] );
				Vector3 ptAug( p.points[j][0], p.points[j][1], 0.f );
				portalToWorld.applyPoint( worldPoint, ptAug );
				srcPortalPoly.push_back( worldPoint );
			}
			srcPortalPolyValid = true;
		}

		// ask the WorldPolygon to cleave itself in twain
		WorldPolygon insidePoly, outsidePoly;
		srcPortalPoly.split( dstWorldPlane, insidePoly, outsidePoly );

		// create the new portal and update the old
		if (!outsidePoly.empty())
		{
			MF_ASSERT_DEV( !insidePoly.empty() );

			Vector3 ptAug;
			Vector2 ptAvg;

			// new portal
			pNewPortal = new Portal( p );	// use copy constructor
			pNewPortal->points.resize( outsidePoly.size() );
			ptAvg.set( 0.f, 0.f );
			for (uint j = 0; j < outsidePoly.size(); ++j)
			{
				worldToPortal.applyPoint( ptAug, outsidePoly[j] );
				pNewPortal->points[j] = Vector2( ptAug[0], ptAug[1] );
				ptAvg += pNewPortal->points[j];
			}
			ptAvg /= float(pNewPortal->points.size());
			pNewPortal->lcentre = pNewPortal->uAxis * ptAvg.x +
				pNewPortal->vAxis * ptAvg.y + pNewPortal->origin;
			pChunk->transform().applyPoint(
				pNewPortal->centre, pNewPortal->lcentre );

			// old portal
			p.points.resize( insidePoly.size() );
			ptAvg.set( 0.f, 0.f );
			for (uint j = 0; j < insidePoly.size(); ++j)
			{
				worldToPortal.applyPoint( ptAug, insidePoly[j] );
				p.points[j] = Vector2( ptAug[0], ptAug[1] );
				ptAvg += p.points[j];
			}
			ptAvg /= float(p.points.size());
			p.lcentre = p.uAxis * ptAvg.x + p.vAxis * ptAvg.y + p.origin;
			pChunk->transform().applyPoint( p.centre, p.lcentre );
			srcPortalPolyValid = false;

			// don't really like these double-transformations, hmm
			// maybe better to bring plane into portal space and
			// pretend it's world space... still would need to
			// augment p.points to Vector3s .. 'tho it'd be trivial now...
		}

		// if we made a portal then add it
		if (pNewPortal != NULL)
		{
			DEBUG_MSG( "ChunkBoundary::splitInvasivePortal: "
				"Split portal in %s since it extends outside first hit %s\n",
				pChunk->identifier().c_str(), pDest->identifier().c_str() );

			pNewPortal->pChunk = (Chunk*)Portal::INVASIVE;
			unboundPortals_.push_back( pNewPortal );
		}
	}
}



// ----------------------------------------------------------------------------
// Section: ChunkBoundary::Portal
// ----------------------------------------------------------------------------

/**
 *	Constructor and loader
 *	@param	pSection	the DataSection contains the data defining the portal
 *	@param	iplane	the plane equation contains the data defining the portal
 *	@param	pMapping	the chunk mapping of the chunk holding this portal
 *	@param	ownerChunkName	the identifier of the chunk holding this portal
 */
ChunkBoundary::Portal::Portal( DataSectionPtr pSection,
							  const PlaneEq & iplane,
							  ChunkDirMapping * pMapping,
							  const std::string & ownerChunkName ) :
	internal( false ),
	permissive( true ),
	pChunk( NULL ),
	plane( iplane )
#if UMBRA_ENABLE
	,
	pUmbraPortal_( NULL )
#endif
{
	BW_GUARD;
	// make sure there's something there
	if (!pSection) return;

	// set the label if it's got one
	label = pSection->asString();

	// read in our flags
	internal = pSection->readBool( "internal", internal );
	permissive = pSection->readBool( "permissive", permissive );

	// find out what to set pChunk to
	std::string	chunkName = pSection->readString( "chunk" );
	if (chunkName == "")
	{
		pChunk = NULL;
	}
	else if (chunkName == "heaven")
	{
		pChunk = (Chunk*)HEAVEN;
	}
	else if (chunkName == "earth")
	{
		pChunk = (Chunk*)EARTH;
	}
	else if (chunkName == "invasive")
	{
		pChunk = (Chunk*)INVASIVE;
	}
	else if (chunkName == "extern")
	{
		pChunk = (Chunk*)EXTERN;
	}
	else
	{
		pChunk = new Chunk( chunkName, pMapping );
	}


	// read in the polygon points
	Vector2	avg(0,0);

	std::vector<Vector3>	v3points;
	pSection->readVector3s( "point", v3points );
	for (uint i=0; i<v3points.size(); i++)
	{
		Vector2 next( v3points[i].x, v3points[i].y );

		avg += next;

		points.push_back( next );
		// TODO: These probably have to be in some sort of order...
	}

	// read in the axes
	uAxis = pSection->readVector3( "uAxis" );
	vAxis = plane.normal().crossProduct( uAxis );
	origin = plane.normal() * plane.d() / plane.normal().lengthSquared();

#ifdef EDITOR_ENABLED
	const float EPSILON = 0.5;
	if( !chunkName.empty() && *chunkName.rbegin() == 'o' &&
		!ownerChunkName.empty() && *ownerChunkName.rbegin() == 'o' &&
		points.size() == 4 )
	{
		if( uAxis[ 1 ] >= EPSILON || uAxis[ 1 ] <= -EPSILON )
		{
			for( int i = 0; i < 4; ++i )
			{
				if( points[ i ][ 0 ] > EPSILON )
					points[ i ][ 0 ] = MAX_CHUNK_HEIGHT;
				else if( points[ i ][ 0 ] < -EPSILON )
					points[ i ][ 0 ] = MIN_CHUNK_HEIGHT;
			}
		}
		else
		{
			for( int i = 0; i < 4; ++i )
			{
				if( points[ i ][ 1 ] > EPSILON )
					points[ i ][ 1 ] = MAX_CHUNK_HEIGHT;
				else if( points[ i ][ 1 ] < -EPSILON )
					points[ i ][ 1 ] = MIN_CHUNK_HEIGHT;
			}
		}
	}
#endif //EDITOR_ENABLED

	// figure out the centre of our portal (local coords)
	avg /= float(points.size());
	lcentre = uAxis * avg.x + vAxis * avg.y + origin;
	centre = lcentre;

	PlaneEq testPlane(
		points[0][0] * uAxis + points[0][1] * vAxis + origin,
		points[1][0] * uAxis + points[1][1] * vAxis + origin,
		points[2][0] * uAxis + points[2][1] * vAxis + origin );
	Vector3 n1 = plane.normal();
	Vector3 n2 = testPlane.normal();
	n1.normalise();	n2.normalise();//TODO: revise the code here to use dot product
	if ((n1 + n2).length() < 1.f)	// should be 2 if equal
	{
		std::reverse( points.begin(), points.end() );
	}
}


/**
 *	Destructor
 */
ChunkBoundary::Portal::~Portal()
{
#if UMBRA_ENABLE
	releaseUmbraPortal();
#endif
}

/**
 *	This method saves a description of this portal into the given data section.
 *	@param	pSection	DataSection used to save the chunk boundary to
 *	@param	pOwnMapping	the mapping of the chunk containing the boundary
 */
void ChunkBoundary::Portal::save( DataSectionPtr pSection,
	ChunkDirMapping * pOwnMapping ) const
{
	BW_GUARD;
	if (!pSection) return;

	DataSectionPtr pPS = pSection->newSection( "portal" );
	if (!label.empty()) pPS->setString( label );
	if (internal) pPS->writeBool( "internal", true );
	if (pChunk != NULL)
	{
		pPS->writeString( "chunk",
			isHeaven() ? "heaven" :
			isEarth() ? "earth" :
			isInvasive() ? "invasive" :
			isExtern() ? "extern" :
			pChunk->mapping() != pOwnMapping ? "extern" :
			pChunk->identifier() );
	}
	pPS->writeVector3( "uAxis", uAxis );
	for (uint i = 0; i < points.size(); i++)
	{
		pPS->newSection( "point" )->setVector3(
			Vector3( points[i][0], points[i][1], 0 ) );
	}

}

/**
 *	This method attempts to resolve an extern portal to find the chunk that
 *	it should be connected to - regardless of what mapping it is in.
 *	Note: if an appropriate chunk is found, it is returned holding a
 *	reference to its ChunkDirMapping.
 *	@param	pOwnChunk	the chunk containing the boundary
 *	@return	true is successfully resolved, otherwise false
 */
bool ChunkBoundary::Portal::resolveExtern( Chunk * pOwnChunk )
{
	BW_GUARD;
	Vector3 conPt = pOwnChunk->transform().applyPoint(
		lcentre + plane.normal() * -0.1f );
	Chunk * pExternChunk = pOwnChunk->space()->guessChunk( conPt, true );
	if (pExternChunk != NULL)
	{
		if (pExternChunk->mapping() != pOwnChunk->mapping())
		{
			pChunk = pExternChunk;
			return true;
		}
		else
		{
			// we don't want it because it's not extern
			// (although technically it should be allowed...)
			delete pExternChunk;
			pOwnChunk->mapping()->decRef();
		}
	}

	return false;
}


// -----------------------------------------------------------------------------
// Section: Client only
// -----------------------------------------------------------------------------

#ifndef MF_SERVER

/**
 *	This class stores up ready-made portals
 */
class Portal2DStore
{
public:
	/**
	 *	This method returns an empty Portal2DRef object from the pool
	 *	@return an empty Portal2DRef object
	 */
	static Portal2DRef grab()
	{
		BW_GUARD;
		if (s_portalStore_.empty())
		{
			Portal2D * pPortal = new Portal2D();
			for (int i = 0; i < 8; i++) pPortal->addPoint( Vector2::zero() );
			s_portalStore_.push_back( pPortal );
		}

		Portal2DRef pref;
		pref.pVal_ = s_portalStore_.back();
		s_portalStore_.pop_back();
		Portal2DStore::grab( pref.pVal_ );
		pref->erasePoints();
		return pref;
	}

	static void fini()
	{
		BW_GUARD;
		for (uint i=0; i<s_portalStore_.size(); i++)
		{
			delete s_portalStore_[i];
		}
		s_portalStore_.clear();
	}

private:
	/**
	 *	This method increase reference count of a Portal2D object
	 *	@param	pPortal	pointer to Portal2D object whose reference count will be increased
	 */
	static void grab( Portal2D * pPortal )
	{
		BW_GUARD;
		pPortal->refs( pPortal->refs() + 1 );
	}

	/**
	 *	This method decrease reference count of a Portal2D object,
	 *	if the reference count is zero, it put the object back to pool
	 *	@param	pPortal	pointer to Portal2D object whose reference count will be decreased
	 */
	static void give( Portal2D * pPortal )
	{
		BW_GUARD;
		if (pPortal == NULL) return;
		pPortal->refs( pPortal->refs() - 1 );
		if (pPortal->refs() == 0)
		{
			s_portalStore_.push_back( pPortal );
		}
	}

	static VectorNoDestructor<Portal2D*>		s_portalStore_;

	friend class Portal2DRef;
};

VectorNoDestructor<Portal2D*> Portal2DStore::s_portalStore_;

/**
 *	Constructor
 *	@param	valid	if the object to be created is valid
 */
Portal2DRef::Portal2DRef( bool valid ) :
	pVal_( valid ? NULL : (Portal2D*)-1 )
{
}

/**
 *	Copy constructor
 *	@param	other	the object to be copied
 */
Portal2DRef::Portal2DRef( const Portal2DRef & other ) :
	pVal_( other.pVal_ )
{
	BW_GUARD;
	if (uint(pVal_)+1 > 1)
		Portal2DStore::grab( pVal_ );
}

/**
 *	Assignment Operator
 *	@param	other	the object to be copied
 */
Portal2DRef & Portal2DRef::operator =( const Portal2DRef & other )
{
	BW_GUARD;
	if (uint(pVal_)+1 > 1)
		Portal2DStore::give( pVal_ );
	pVal_ = other.pVal_;
	if (uint(pVal_)+1 > 1)
		Portal2DStore::grab( pVal_ );
	return *this;
}

/**
 *	Destructor
 */
Portal2DRef::~Portal2DRef()
{
	BW_GUARD;
	if (uint(pVal_)+1 > 1)
		Portal2DStore::give( pVal_ );
}

// E3'04 option to turn off clipping outside to portals
// ... needed until it works better! (portal aggregation)
/**
 * This class adds a watcher named 'Render/clipOutsideToPortal' so the user
 * can enable or disable the clipping of an outside chunk to a portal.
 */
static struct ClipOutsideToPortal
{
	/**
	 *	Constructor
	 */
	ClipOutsideToPortal() : b_( false )
	{ 
		MF_WATCH( "Render/clipOutsideToPortal", b_, Watcher::WT_READ_WRITE,
			"Clip outdoor chunks to indoor portals" ); 
	}
	/**
	 *	conversion operator to bool
	 *	@return	bool	true if clip outside chunk to portal, otherwise false
	 */
	operator bool() { return b_; }
	bool b_;
} s_clipOutsideToPortal;


/*static*/ void ChunkBoundary::fini()
{
	BW_GUARD;
	Portal2DStore::fini();

}


BoundingBox ChunkBoundary::Portal::bbFrustum_;

void ChunkBoundary::Portal::updateFrustumBB()
{
	BW_GUARD;
	static std::vector<Vector4> clipSpaceFrustum;
	if( !clipSpaceFrustum.size() )
	{
		clipSpaceFrustum.push_back( Vector4( -1.f, -1.f, 0.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( +1.f, -1.f, 0.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( -1.f, +1.f, 0.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( +1.f, +1.f, 0.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( -1.f, -1.f, 1.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( +1.f, -1.f, 1.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( -1.f, +1.f, 1.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( +1.f, +1.f, 1.f, 1.f ) );
	}
	std::vector<Vector4> worldSpaceFrustum( clipSpaceFrustum.size() );

	Matrix m;
	m.invert( Moo::rc().viewProjection() );
	for( std::vector<Vector4>::size_type i = 0; i < clipSpaceFrustum.size(); ++i )
	{
		m.applyPoint( worldSpaceFrustum[i], clipSpaceFrustum[i] );
		if( worldSpaceFrustum[i].w < 0.0000001f && worldSpaceFrustum[i].w > -0.0000001f )
			worldSpaceFrustum[i].x = 0.0000001f;
		worldSpaceFrustum[i] = worldSpaceFrustum[i] / worldSpaceFrustum[i].w;
	}

	bbFrustum_ = BoundingBox( Vector3( worldSpaceFrustum[0].x, worldSpaceFrustum[0].y, worldSpaceFrustum[0].z ),
		 Vector3( worldSpaceFrustum[0].x, worldSpaceFrustum[0].y, worldSpaceFrustum[0].z ) );
	for( std::vector<Vector4>::size_type i = 1; i < worldSpaceFrustum.size(); ++i )
	{
		bbFrustum_.addBounds( Vector3( worldSpaceFrustum[i].x, worldSpaceFrustum[i].y, worldSpaceFrustum[i].z ) );
	}
}

/**
 *	This method checks if the point is inside the portal
 *	@param point the point to check, assumed to already be on the portal plane
 *	@return true if the point is inside the portal
 */
bool ChunkBoundary::Portal::inside( const Vector3& point ) const
{
	BW_GUARD;
	Vector3 rel = point - origin;
	Vector2 p( rel.dotProduct( uAxis ), rel.dotProduct( vAxis ) );

	for (uint32 i = 0; i < points.size(); i++)
	{
		const Vector2& p1 = points[i];
		const Vector2& p2 = points[(i+1) % points.size()];

		Vector2 diff(p1.y - p2.y, p2.x - p1.x);

		if (diff.dotProduct( p - p1 ) < 0.f)
		{
			return false;
		}
	}
	return true;
}

/**
 *	This method calculates the object-space point of our portal points.
 *	Our portal points are defined along a plane and thus are not necessarily
 *	useful for those needing to project them into the world.
 *	@param	i		index of the point to transform
 *	@param	ret		return vector, is set with a true object space position.
 */
void ChunkBoundary::Portal::objectSpacePoint( int i, Vector3& ret )
{
	ret = uAxis * points[i][0] + vAxis * points[i][1] + origin;
}

/**
 *	This method calculates the object-space point of our portal points.
 *	Our portal points are defined along a plane and thus are not necessarily
 *	useful for those needing to project them into the world.
 *	@param	i		index of the point to transform
 *	@param	ret		return vector, is set with a true object space position.
 */
void ChunkBoundary::Portal::objectSpacePoint( int i, Vector4& ret )
{
	ret = Vector4(uAxis * points[i][0] + vAxis * points[i][1] + origin,1);
}


/**
 *	This method calculates which edges on a portal a point is outside
 *	useful for calculating if a number of points are inside a portal.
 *	@param point the point to check
 *	@return the outcode mask for the point each bit represents one edge
 *	if the bit is set, the point is outside that edge
 */
uint32 ChunkBoundary::Portal::outcode( const Vector3& point ) const
{
	BW_GUARD;
	uint32 res = 0;
	Vector3 rel = point - origin;
	Vector2 p( rel.dotProduct( uAxis ), rel.dotProduct( vAxis ) );

	for (uint32 i = 0; i < points.size(); i++)
	{
		const Vector2& p1 = points[i];
		const Vector2& p2 = points[(i+1) % points.size()];

		Vector2 diff(p1.y - p2.y, p2.x - p1.x);

		if (diff.dotProduct( p - p1 ) < 0.f)
		{
			res |= 1 << i;
		}
	}
	return res;
}

#if UMBRA_ENABLE
/**
 *	This method creates a portal pointing from the current chunk to a target chunk
 *	@param pOwner the owner of this portal
 */
void ChunkBoundary::Portal::createUmbraPortal(Chunk* pOwner)
{
	BW_GUARD;
	// Make sure we release the current umbra portal
	releaseUmbraPortal();

	// If the target chunk and the owner exist in the same cell
	// there is no point to this portal
	if (pOwner->getUmbraCell() == pChunk->getUmbraCell())
		return;

	// Create the portal model
	Umbra::Model* model = createUmbraPortalModel();
	model->autoRelease();

	// Create the portal and set up it's data
	pUmbraPortal_ = Umbra::PhysicalPortal::create( model, pChunk->getUmbraCell() );
	pUmbraPortal_->set(Umbra::Object::INFORM_PORTAL_ENTER, true);
	pUmbraPortal_->set(Umbra::Object::INFORM_PORTAL_EXIT, true);
	UmbraPortal* umbraPortal = new UmbraPortal(pOwner);
	pUmbraPortal_->setUserPointer( (void*)umbraPortal );
	pUmbraPortal_->setObjectToCellMatrix( (Umbra::Matrix4x4&)pOwner->transform() );
    pUmbraPortal_->setCell( pOwner->getUmbraCell() );
}

/**
 *	This method releases the umbra portal and associated objects
 */
void ChunkBoundary::Portal::releaseUmbraPortal()
{
	BW_GUARD;
	if (pUmbraPortal_)
	{
		UmbraPortal* up = (UmbraPortal*)pUmbraPortal_->getUserPointer();
		delete up;
		pUmbraPortal_->release();
		pUmbraPortal_ = NULL;
	}
}

/**
 *	This method creates the umbra portal model for a portal
 *	@return the umbra portal model for this portal
 */
Umbra::Model* ChunkBoundary::Portal::createUmbraPortalModel()
{
	BW_GUARD;
	// Collect the vertices for the portal
	uint32 nVertices = points.size();

	std::vector<Vector3> vertices;
	vertices.reserve(nVertices);

	for( uint32 i = 0; i < nVertices; i++ )
	{
		vertices.push_back(uAxis * points[i][0] + vAxis * points[i][1] + origin);
	}

	// Set up the triangles for the portal model
	int nTriangles = nVertices - 2;

	std::vector<uint32> triangles;
	triangles.reserve(nTriangles*3);

	MF_ASSERT_DEV(nTriangles > 0);

	for (uint32 c = 2; c < nVertices; c++)
	{
		triangles.push_back( 0 );
		triangles.push_back( c-1 );
		triangles.push_back( c );
	}

	// Create the umbra model and set it up to be backface cullable so that we can only see
	// through one end of the portal
	Umbra::Model* model = Umbra::MeshModel::create((Umbra::Vector3*)&vertices.front(), (Umbra::Vector3i*)&triangles.front(), nVertices, nTriangles);
	model->set(Umbra::Model::BACKFACE_CULLABLE, true);
	return model;
}
#endif

/**
 *	This method traverses this portal and draws the chunk it's connected to, if
 *	the portal is visible. Assumes that 'hasChunk' would return true.
 *	It returns the portal to be used to traverse any portals inside that chunk.
 *	An invlaid Portal2DRef is returned if the chunk was not drawn.
 *	@param	transform	world transform of the portal
 *	@param	transformInverse	inverse world transform of the portal
 *	@param	pClipPortal	portal used to clip the traverse, we traverse through
 *			this portal to the current chunk. Could be NULL, which means no clip.
 *	@return	the portal to be used to traverse any portals inside that chunk
 */
//#define CHUNK_TRAVERSE_OPTIMISATION_OFF
#ifdef CHUNK_TRAVERSE_OPTIMISATION_OFF
Portal2DRef ChunkBoundary::Portal::traverse(
	const Matrix & transform,
	const Matrix & transformInverse,
	Portal2DRef pClipPortal ) const
{
	BW_GUARD;
	// find the matrix to transform from local space to clip space
	Matrix		objectToClip;
	objectToClip.multiply( transform, Moo::rc().viewProjection() );

	// outside stuff ... see if this is a big portal between outside chunks
	bool outside = (points.size() == 4 &&
		(points[0] - points[2]).lengthSquared() > 100.f*100.f);

	// find the matrix to transform from camera space to local space
	Matrix		cameraToObject;
	cameraToObject.multiply( Moo::rc().invView(), transformInverse );

	// find the camera position on the near plane in local space. we use
	// this position to determine whether or not this portal is visible.
	Vector3 cameraOnNearPlane = cameraToObject.applyPoint(
		Vector3( 0, 0, Moo::rc().camera().nearPlane() ) );

	// our plane is in object space, so now we can find the distance
	// of the camera from it
	if (plane.distanceTo( cameraOnNearPlane ) <= (outside ? -100.f : -2.0f) )
	{									// -100.f : -0.1f
		ChunkManager::drawTreeBranch( pChunk, " - CAMERA" );
		ChunkManager::drawTreeReturn();
		return Portal2DRef( false );
	}
	// if the polygon crosses the near plane inside the view volume,
	// then we really want to intersect it with the near plane...

	// yay, we can see it. or could if there were no other portals

	// set up two outcode variables. new outcodes are accumulated into
	// outProduct by 'and' operations, and into outSum by 'or' operations.
	Outcode outProduct = OUTCODE_MASK;
	Outcode outSum = 0;	// Remember circuit analysis: SoP and PoS?

	Portal2DRef ourClipPortal = Portal2DStore::grab();

	Vector4 clipPoint;

	// check all the points in our portal - if any lie
	// inside the view volume, add them to ourClipPortal
	for( uint32 i = 0; i < points.size(); i++ )
	{
		// project the point into clip space
#ifndef EDITOR_ENABLED
		objectToClip.applyPoint( clipPoint, Vector4(
			uAxis * points[i][0] + vAxis * points[i][1] + origin, 1 ) );
#else
		Vector4 objectPoint(
			uAxis * points[i][0] + vAxis * points[i][1] + origin, 1 );
		// raise up outside portals to at least the height of the camera
		if (outside && objectPoint.y > 0.f && 
			objectPoint.y < cameraToObject[3].y)
				objectPoint.y = cameraToObject[3].y + 5.f;
		objectToClip.applyPoint( clipPoint, objectPoint );
#endif

		// see where it lies
		Outcode ocPoint = clipPoint.calculateOutcode();

		// if it's not too close add it to our clip portal
		if (( ocPoint & OUTCODE_NEAR ) == 0)
		{
			float oow = 1.f/clipPoint.w;
			ourClipPortal->addPoint(
				Vector2( clipPoint.x*oow, clipPoint.y*oow ) );
		}

		outSum |= ocPoint;

		// it's not near as long as w isn't too negative
		//  (why I don't know - copied from rhino's portal.cpp)
		if (clipPoint.w >= -1)
			ocPoint &= ~OUTCODE_NEAR;

		outProduct &= ocPoint;
	}

	// if all the points have at least one outcode bit in common,
	// then the whole portal must be out of the volume, so ignore it.
	if (outProduct != 0)
	{
		ChunkManager::drawTreeBranch( pChunk, " - BBOX" );
		ChunkManager::drawTreeReturn();
		return Portal2DRef( false );
	}

	// if we're going to the outside, do something different here
	if (outside)
	{
		// if we're looking through a smaller portal, make sure there's
		// something worth drawing.
		// E3'04: don't clip the outside to just one inside portal...
		// far too error prone in somewhere with multiple heavenly portals
		// like say the city - we really need portal aggregation for this.
		if (&*pClipPortal != NULL && !(outSum & OUTCODE_NEAR) &&
			s_clipOutsideToPortal)
		{
			Portal2DRef combinedP2D = Portal2DStore::grab();
			if (!combinedP2D->combine( &*pClipPortal, &*ourClipPortal ))
			{
				ChunkManager::drawTreeBranch( pChunk, " - COMBINED" );
				ChunkManager::drawTreeReturn();
				return Portal2DRef( false );
			}
		}

		// ok, either we're outside looking outside, or we're
		// inside looking outside and we can see this portal.
		// either way, we want to draw the chunk ... so draw it!
		ChunkManager::drawTreeBranch( pChunk, " + outside" );
		//pChunk->draw( pClipPortal );
		pChunk->drawBeg();
		return pClipPortal;
	}
	else
	{
		// ok, we're not going to the outside.

		// so, are any of our clip points near?
		if (outSum & OUTCODE_NEAR)
		{
			// ok, at least one was before the volume, so don't attempt to
			// do any fancy polygon intersection and just draw it with
			// the same Portal2D that we got given.
			ChunkManager::drawTreeBranch( pChunk, " + close" );
			//pChunk->draw( pClipPortal );
			pChunk->drawBeg();
			return pClipPortal;
		}
		else
		{
			// ok, let's combine them Portal2D's
			Portal2DRef combinedP2D = ourClipPortal;
			/*
			if (&*pClipPortal == NULL)
			{
				static Portal2D defP2D;
				if (defP2D.points().empty()) defP2D.setDefaults();

				combinedP2D->combine( &defP2D, &*ourClipPortal );

				// TODO: Why don't we just use ourClipPortal
				// straight off here? Instead of combining it
				// with the default one?
			}
			*/
			if (&*pClipPortal != NULL)
			{
				combinedP2D = Portal2DStore::grab();
				if (!combinedP2D->combine( &*pClipPortal, &*ourClipPortal ))
				{
					ChunkManager::drawTreeBranch( pChunk, " - COMBINED" );
					ChunkManager::drawTreeReturn();
					return Portal2DRef( false );
				}
			}

			ChunkManager::drawTreeBranch( pChunk, " + combined" );
			//pChunk->draw( &combinedP2D );
			pChunk->drawBeg();

			if (drawPortals_)
			{
				#if ENABLE_DRAW_PORTALS
					Geometrics::drawLinesInClip(
						(Vector2*)&*combinedP2D->points().begin(),
						combinedP2D->points().size() );
				#endif
			}
			return combinedP2D;
		}
	}
}
#else//CHUNK_TRAVERSE_OPTIMISATION_OFF
Portal2DRef ChunkBoundary::Portal::traverse(
	const Matrix & transform,
	const Matrix & transformInverse,
	Portal2DRef pClipPortal ) const
{
	BW_GUARD;
	float distance = pChunk->boundingBox().distance( Moo::rc().invView()[3] );
	if( distance < 0.f )
		distance = -distance;
	if( distance > Moo::rc().camera().farPlane() +
		( pChunk->boundingBox().minBounds() - pChunk->boundingBox().maxBounds() ).length() / 2 )
	{
		ChunkManager::drawTreeBranch( pChunk, " - PORTAL TOO FAR" );
		ChunkManager::drawTreeReturn();
		return Portal2DRef( false );
	}

	// find the matrix to transform from local space to clip space
	Matrix		objectToClip;
	objectToClip.multiply( transform, Moo::rc().viewProjection() );

	// outside stuff ... see if this is a big portal between outside chunks
	bool outside = (points.size() == 4 &&
		(points[0] - points[2]).lengthSquared() > 100.f*100.f);

	// find the matrix to transform from camera space to local space
	Matrix		cameraToObject;
	cameraToObject.multiply( Moo::rc().invView(), transformInverse );

	// find the camera position on the near plane in local space. we use
	// this position to determine whether or not this portal is visible.
	Vector3 cameraOnNearPlane = cameraToObject.applyPoint(
		Vector3( 0, 0, Moo::rc().camera().nearPlane() ) );

	// our plane is in object space, so now we can find the distance
	// of the camera from it
	if (plane.distanceTo( cameraOnNearPlane ) <= (outside ? -100.f : -2.0f) )
	{									// -100.f : -0.1f
		ChunkManager::drawTreeBranch( pChunk, " - CAMERA" );
		ChunkManager::drawTreeReturn();
		return Portal2DRef( false );
	}
	// if the polygon crosses the near plane inside the view volume,
	// then we really want to intersect it with the near plane...

	// yay, we can see it. or could if there were no other portals

	// set up two outcode variables. new outcodes are accumulated into
	// outProduct by 'and' operations, and into outSum by 'or' operations.
	Outcode outProduct = OUTCODE_MASK;
	Outcode outSum = 0;	// Remember circuit analysis: SoP and PoS?

	Portal2DRef ourClipPortal = Portal2DStore::grab();

	Vector4 clipPoint;

	BoundingBox bbPortal;

	// check all the points in our portal - if any lie
	// inside the view volume, add them to ourClipPortal
	for( uint32 i = 0; i < points.size(); i++ )
	{
		Vector4 worldPoint;
		// project the point into clip space

		Vector4 objectPoint( uAxis * points[i][0] + vAxis * points[i][1] + origin, 1 );		
		// raise up outside portals to at least the height of the camera
		if (outside && objectPoint.y > 0.f && 
			objectPoint.y < 1000000.f)
				objectPoint.y = 1000000.f + 5.f;
		objectToClip.applyPoint( clipPoint, objectPoint );
		transform.applyPoint( worldPoint, objectPoint );

		worldPoint = worldPoint / worldPoint.w;
		Vector3 worldPoint3( worldPoint.x, worldPoint.y, worldPoint.z );
		if( i == 0 )
			bbPortal = BoundingBox( worldPoint3, worldPoint3 );
		else
			bbPortal.addBounds( worldPoint3 );

		// see where it lies
		Outcode ocPoint = clipPoint.calculateOutcode();

		// if it's not too close add it to our clip portal
		if (( ocPoint & OUTCODE_NEAR ) == 0)
		{
			float oow = 1.f/clipPoint.w;
			ourClipPortal->addPoint(
				Vector2( clipPoint.x*oow, clipPoint.y*oow ) );
		}

		outSum |= ocPoint;

		// it's not near as long as w isn't too negative
		//  (why I don't know - copied from rhino's portal.cpp)
		if (clipPoint.w >= -1)
			ocPoint &= ~OUTCODE_NEAR;

		outProduct &= ocPoint;
	}

	// if all the points have at least one outcode bit in common,
	// then the whole portal must be out of the volume, so ignore it.
	if (outProduct != 0)
	{
		ChunkManager::drawTreeBranch( pChunk, " - BBOX" );
		ChunkManager::drawTreeReturn();
		return Portal2DRef( false );
	}
	pChunk->boundingBox().calculateOutcode( Moo::rc().viewProjection() );
	if( pChunk->boundingBox().combinedOutcode() != 0 )
	{
		ChunkManager::drawTreeBranch( pChunk, " - BBOX" );
		ChunkManager::drawTreeReturn();
		return pClipPortal;
	}
	if( !pChunk->boundingBox().intersects( bbFrustum_ ) )
	{
		ChunkManager::drawTreeBranch( pChunk, " - BOUNDINGBOX NOT INTERSECTED" );
		ChunkManager::drawTreeReturn();
		return pClipPortal;
	}
	// if we're going to the outside, do something different here
	if (outside)
	{
		// if we're looking through a smaller portal, make sure there's
		// something worth drawing.
		// E3'04: don't clip the outside to just one inside portal...
		// far too error prone in somewhere with multiple heavenly portals
		// like say the city - we really need portal aggregation for this.
		if (&*pClipPortal != NULL && !(outSum & OUTCODE_NEAR) &&
			s_clipOutsideToPortal)
		{
			Portal2DRef combinedP2D = Portal2DStore::grab();
			if (!combinedP2D->combine( &*pClipPortal, &*ourClipPortal ))
			{
				ChunkManager::drawTreeBranch( pChunk, " - COMBINED" );
				ChunkManager::drawTreeReturn();
				return Portal2DRef( false );
			}
		}

		// ok, either we're outside looking outside, or we're
		// inside looking outside and we can see this portal.
		// either way, we want to draw the chunk ... so draw it!
		ChunkManager::drawTreeBranch( pChunk, " + outside" );
		//pChunk->draw( pClipPortal );
		pChunk->drawBeg();
		return pClipPortal;
	}
	else
	{
		// ok, we're not going to the outside.

		// so, are any of our clip points near?
		if (outSum & OUTCODE_NEAR)
		{
			// ok, at least one was before the volume, so don't attempt to
			// do any fancy polygon intersection and just draw it with
			// the same Portal2D that we got given.
			ChunkManager::drawTreeBranch( pChunk, " + close" );
			//pChunk->draw( pClipPortal );
			pChunk->drawBeg();
			return pClipPortal;
		}
		else
		{
			// ok, let's combine them Portal2D's
			Portal2DRef combinedP2D = ourClipPortal;
			/*
			if (&*pClipPortal == NULL)
			{
				static Portal2D defP2D;
				if (defP2D.points().empty()) defP2D.setDefaults();

				combinedP2D->combine( &defP2D, &*ourClipPortal );

				// TODO: Why don't we just use ourClipPortal
				// straight off here? Instead of combining it
				// with the default one?
			}
			*/
			if (&*pClipPortal != NULL)
			{
				combinedP2D = Portal2DStore::grab();
				if (!combinedP2D->combine( &*pClipPortal, &*ourClipPortal ))
				{
					ChunkManager::drawTreeBranch( pChunk, " - COMBINED" );
					ChunkManager::drawTreeReturn();
					return Portal2DRef( false );
				}
			}

			ChunkManager::drawTreeBranch( pChunk, " + combined" );
			//pChunk->draw( &combinedP2D );

			pChunk->drawBeg();

			if (drawPortals_)
			{
				#if ENABLE_DRAW_PORTALS
					Geometrics::drawLinesInClip(
						(Vector2*)&*combinedP2D->points().begin(),
						combinedP2D->points().size() );
				#endif
			}
			return combinedP2D;
		}
	}
}
#endif// CHUNK_TRAVERSE_OPTIMISATION_OFF

/**
 *	Debugging method for displaying a portal. It is drawn in purple if the
 *	camera is on the inside of the portal plane, and green if it's outside.
 *	
 */
void ChunkBoundary::Portal::display( const Matrix & transform,
	const Matrix & transformInverse, float inset ) const
{
	BW_GUARD;
	Vector3 worldPoint[16];

	// find the centre
	Vector2 avgPt(0.f,0.f);
	for( uint32 i = 0; i < points.size(); i++ ) avgPt += points[i];
	avgPt /= float(points.size());

	// transform all the points
	for( uint32 i = 0; i < points.size(); i++ )
	{
		// project the point into clip space
		transform.applyPoint( worldPoint[i], Vector3(
			uAxis * (points[i][0] + (points[i][0] < avgPt[0] ? inset : -inset)) +
			vAxis * (points[i][1] + (points[i][1] < avgPt[1] ? inset : -inset)) +
			origin ) );
	}

	// set the colour based on which side of the portal the camera is on
	uint32 colour = plane.distanceTo( transformInverse.applyPoint(
		Moo::rc().invView().applyToOrigin() ) ) < 0.f ? 0x0000ff00 : 0x00ff00ff;

	// draw the lines
	for( uint32 i = 0; i < points.size(); i++ )
	{
		Geometrics::drawLine( worldPoint[i], worldPoint[(i+1)%points.size()], colour );
	}
}
#endif // !MF_SERVER

/**
 *	This method calculates the object-space point of our portal points.
 *	Our portal points are defined along a plane and thus are not necessarily
 *	useful for those needing to project them into the world.
 *	@param	i		index of the point to transform
 *	@return			return vector, is set with a true object space position.
 */
Vector3 ChunkBoundary::Portal::objectSpacePoint( int i )
{
	return uAxis * points[i][0] + vAxis * points[i][1] + origin;
}

/**
 *	add points into the portal
 *	@param	pd	the points to be added
 */
void PrivPortal::addPoints( const std::vector<Vector3> & pd )
{
	BW_GUARD;
	const float IN_PORTAL_PLANE			= 0.2f;

	points_.assign( pd.begin(), pd.end() );

	std::vector<Vector3>::size_type i = 0;

	// find a plane
	for (i = 0; i < points_.size() - 2; ++i)
	{
		planeEquation_.init( points_[ i ], points_[ i + 1 ], points_[ i + 2 ] );

		if (! almostEqual( planeEquation_.normal(), Vector3( 0.f, 0.f, 0.f ) ) )
		{
			break;
		}
	}

	// remove points not on the plane
	bool foundOne = true;

	while (foundOne)
	{
		for (i = 0; i < points_.size() && foundOne; ++i)
		{
			foundOne = false;

			if (fabs( planeEquation_.distanceTo( points_[ i ] ) ) > IN_PORTAL_PLANE)
			{
				ERROR_MSG( "PrivPortal::addPoints: found a point that is not on the portal plane.\n" );
				points_.erase( points_.begin() + i );
				foundOne = true;
				break;
			}
		}
	}

	// remove points on the same line
	foundOne = true;

	while (foundOne)
	{
		foundOne = false;

		for (i = 0; i < points_.size(); ++i)
		{
			Vector3 v0 = points_[ ( i + points_.size() ) % points_.size() ];
			Vector3 v1 = points_[ ( i + 1 + points_.size() ) % points_.size() ];
			Vector3 v2 = points_[ ( i + 2 + points_.size() ) % points_.size() ];
			Vector3 n1 = v0 - v1;
			Vector3 n2 = v1 - v2;
			n1.normalise();
			n2.normalise();

			if (almostEqual( v0, v1 ) || // v0 == v1
				almostEqual( v1, v2 ) || // v1 == v2
				almostEqual( n1, n2 )) // v0, v1, v2 colinear
			{
				points_.erase( points_.begin() + i + 1 );
				foundOne = true;
				break;
			}
		}
	}
}

/**
 *	set the flag of the portal
 *	@param	flag	the flag to be set
 */
void PrivPortal::flag( const std::string& flag )
{
	flags_ |= int( flag == "heaven" ) << 1;
	flags_ |= int( flag == "earth" ) << 2;
	flags_ |= int( flag == "invasive" ) << 3;
}

/**
 *	This method applies the given transform to this portal
 *	@param	transform	the transform to be applied
 */
void PrivPortal::transform( const class Matrix& transform )
{
	BW_GUARD;
	Vector3 pos = transform.applyPoint( planeEquation_.normal() * planeEquation_.d() );
	Vector3 norm = transform.applyVector( planeEquation_.normal() );
	norm.normalise();
	planeEquation_ = PlaneEq( norm, pos.dotProduct( norm ) );

	for( uint32 i = 0; i < points_.size(); i++ )
	{
		points_[ i ] = transform.applyPoint( points_[ i ] );
	}
}

/**
 *	This method creates the boundary sections from a visual data section
 *	@param	pVis	the visual data section
 *	@param	wantWorld	world transform
 *	@return	Smart pointer to DataSection object contains the boundary sections
 */
DataSectionPtr createBoundarySections( DataSectionPtr pVis, const Matrix & wantWorld )
{
	BW_GUARD;
	// set up some matrices
	Matrix parentWorld = Matrix::identity;
	Matrix wantWorldInv; wantWorldInv.invert( wantWorld );

	// get the (world) bounding box
	BoundingBox bb;
	bb.setBounds( pVis->readVector3( "boundingBox/min" ),
		pVis->readVector3( "boundingBox/max" ) );

	bb.transformBy( wantWorld );
	Vector3 bbMin = bb.minBounds();
	Vector3 bbMax = bb.maxBounds();

	std::vector<PrivPortal *>		portals[6];

	std::vector<DataSectionPtr> visualPortals;
	pVis->openSections( "portal", visualPortals );

	// now look at all our portals, and assign them to a boundary
	for (uint i = 0; i < visualPortals.size(); i++)
	{
		std::vector<Vector3> portalPoints;
		visualPortals[i]->readVector3s( "point", portalPoints );
		if( !portalPoints.empty() )
		{
			PrivPortal* bpNew = new PrivPortal();
			bpNew->addPoints( portalPoints );
			bpNew->flag( visualPortals[i]->asString() );
			PrivPortal & bp = *bpNew;
			bp.transform( parentWorld );

			const PlaneEq & peq = bp.getPlaneEquation();
			Vector3 normal = peq.normal();
			Vector3 point = normal * peq.d();

			// figure out which side it's on
			int side = 0;
			Vector3 anormal( fabsf(normal[0]), fabsf(normal[1]), fabsf(normal[2]) );
			if (anormal[0] > anormal[1] && anormal[0] > anormal[2])
			{	// on yz plane (l or r)
				side = 0 + int(fabs(point[0] - bbMin[0]) > fabsf(point[0] - bbMax[0]));
			}
			else if (anormal[1] > anormal[0] && anormal[1] > anormal[2])
			{	// on xz plane (d or u)
				side = 2 + int(fabs(point[1] - bbMin[1]) > fabsf(point[1] - bbMax[1]));
			}
			else
			{	// on xy plane (f or b)
				side = 4 + int(fabs(point[2] - bbMin[2]) > fabsf(point[2] - bbMax[2]));
			}

			// add it to that side's list
			portals[side].push_back( &bp );
		}
	}

	DataSectionPtr result = new XMLSection( "root" );
	// now write out the boundary
	for (int b = 0; b < 6; b++)
	{
		DataSectionPtr pBoundary = result->newSection( "boundary" );

		// figure out the boundary plane in world coordinates
		int sign = 1 - (b&1)*2;
		int axis = b>>1;
		Vector3 normal(
			sign * ((axis==0)?1.f:0.f),
			sign * ((axis==1)?1.f:0.f),
			sign * ((axis==2)?1.f:0.f) );
		float d = sign > 0 ? bbMin[axis] : -bbMax[axis];

		Vector3 localCentre = wantWorldInv.applyPoint( normal * d );
		Vector3 localNormal = wantWorldInv.applyVector( normal );
		localNormal.normalise();
		PlaneEq localPlane( localNormal, localNormal.dotProduct( localCentre ) );

		pBoundary->writeVector3( "normal", localPlane.normal() );
		pBoundary->writeFloat( "d", localPlane.d() );

		for (uint i = 0; i < portals[b].size(); i++)
		{
			PrivPortal  & bp = *portals[b][i];

			// write out the link
			DataSectionPtr pPortal = pBoundary->newSection( "portal" );

			if (bp.isHeaven())
				pPortal->writeString( "chunk", "heaven" );
			else if (bp.isEarth())
				pPortal->writeString( "chunk", "earth" );
			else if (bp.isInvasive())
				pPortal->writeString( "chunk", "invasive" );

			// figure out a uAxis and a vAxis ... for calculation purposes,
			// make (uAxis, vAxis, normal) a basis in world space.
			Vector3	uAxis(
				(axis==1)?1.f:0.f,
				(axis==2)?1.f:0.f,
				(axis==0)?1.f:0.f );
			Vector3 vAxis = normal.crossProduct( uAxis );

			// but write out a uAxis that turns the 2D points into local space
			pPortal->writeVector3( "uAxis", wantWorldInv.applyVector( uAxis ) );

			// now transform and write out the points
			Matrix basis;	// (actually using matrix for ordinary maths!)
			basis[0] = uAxis;
			basis[1] = vAxis;
			basis[2] = normal;		// so error from plane is in the z.
			basis.translation( normal * d + wantWorld.applyToOrigin() );
			Matrix invBasis; invBasis.invert( basis );

			for (uint j = 0; j < bp.getNPoints(); j++)
			{
				pPortal->newSection( "point" )->setVector3(
					invBasis.applyPoint( bp.getPoint(j) ) );
			}

			delete portals[b][i];
		}
	}

	return result;
}

// chunk_boundary.cpp
