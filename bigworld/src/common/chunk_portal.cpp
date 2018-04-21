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

#include "chunk_portal.hpp"

#include "cstdmf/guard.hpp"

#include "chunk/chunk_space.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_model_obstacle.hpp"
#include "physics2/worldtri.hpp"

int ChunkPortal_token;

// -----------------------------------------------------------------------------
// Section: PortalObstacle
// -----------------------------------------------------------------------------

/**
 *	This class is the obstacle that a ChunkPortal puts in the collision scene
 */
class PortalObstacle : public ChunkObstacle
{
public:
	PortalObstacle( ChunkPortalPtr cpp );
	~PortalObstacle() {}

	virtual bool collide( const Vector3 & source, const Vector3 & extent,
		CollisionState & state ) const;
	virtual bool collide( const WorldTriangle & source, const Vector3 & extent,
		CollisionState & state ) const;

	void buildTriangles();

private:
	ChunkPortalPtr	cpp_;
	BoundingBox		bb_;

	mutable std::vector<WorldTriangle>	ltris_;
};


/**
 *	Constructor
 */
PortalObstacle::PortalObstacle( ChunkPortalPtr cpp ) :
	ChunkObstacle( cpp->chunk()->transform(), &bb_, cpp ),
	cpp_( cpp )
{
	BW_GUARD;
	// now calculate our bb. fortunately the ChunkObstacle constructor
	// doesn't do anything with it except store it.
	const ChunkBoundary::Portal * pPortal = cpp_->pPortal_;

	// extend 10cm into the chunk (the normal is always normalised)
	Vector3 ptExtra = pPortal->plane.normal() * 0.10f;

	// build up the bb from the portal points
	for (uint i = 0; i < pPortal->points.size(); i++)
	{
		Vector3 pt =
			pPortal->uAxis * pPortal->points[i][0] +
			pPortal->vAxis * pPortal->points[i][1] +
			pPortal->origin;
		if (!i)
			bb_ = BoundingBox( pt, pt );
		else
			bb_.addBounds( pt );
		bb_.addBounds( pt + ptExtra );
	}

	// and figure out the triangles (a similar process)
	this->buildTriangles();
}


/**
 *	Build the 'world' triangles to collide with
 */
void PortalObstacle::buildTriangles()
{
	BW_GUARD;
	ltris_.clear();

	const ChunkBoundary::Portal * pPortal = cpp_->pPortal_;

	// extend 5cm into the chunk
	Vector3 ptExOri = pPortal->origin + pPortal->plane.normal() * 0.05f;

	// build the wt's from the points
	Vector3 pto, pta, ptb(0.f,0.f,0.f);
	for (uint i = 0; i < pPortal->points.size(); i++)
	{
		// shuffle and find the next pt
		pta = ptb;
		ptb =
			pPortal->uAxis * pPortal->points[i][0] +
			pPortal->vAxis * pPortal->points[i][1] +
			ptExOri;

		// stop if we don't have enough for a triangle
		if (i < 2)
		{
			// start all triangles from pt 0.
			if (i == 0) pto = ptb;
			continue;
		}

		// make a triangle then
		ltris_.push_back( WorldTriangle( pto, pta, ptb ) );
	}
}


/**
 *	Collision test with an extruded point
 */
bool PortalObstacle::collide( const Vector3 & source, const Vector3 & extent,
	CollisionState & state ) const
{
	BW_GUARD;
	const ChunkBoundary::Portal * pPortal = cpp_->pPortal_;

	// see if we let anyone through
	if (pPortal->permissive) return false;

	// ok, see if they collide then
	// (chances are very high if they're in the bb!)
	Vector3 tranl = extent - source;
	for (uint i = 0; i < ltris_.size(); i++)
	{
		// see if it intersects
		float rd = 1.0f;
		if (!ltris_[i].intersects( source, tranl, rd ) ) continue;

		// see how far we really travelled (handles scaling, etc.)
		float ndist = state.sTravel_ + (state.eTravel_-state.sTravel_) * rd;

		if (state.onlyLess_ && ndist > state.dist_) continue;
		if (state.onlyMore_ && ndist < state.dist_) continue;
		state.dist_ = ndist;

		// call the callback function
		ltris_[i].flags( uint8(cpp_->triFlags_) );
		int say = state.cc_( *this, ltris_[i], state.dist_ );

		// see if any other collisions are wanted
		if (!say) return true;

		// some are wanted ... see if it's only one side
		state.onlyLess_ = !(say & 2);
		state.onlyMore_ = !(say & 1);
	}

	return false;
}

/**
 *	Collision test with an extruded triangle
 */
bool PortalObstacle::collide( const WorldTriangle & source, const Vector3 & extent,
	CollisionState & state ) const
{
	BW_GUARD;
	const ChunkBoundary::Portal * pPortal = cpp_->pPortal_;

	// see if we let anyone through
	if (pPortal->permissive) return false;

	// ok, see if they collide then
	// (chances are very high if they're in the bb!)
	Vector3 tranl = extent - source.v0();
	for (uint i = 0; i < ltris_.size(); i++)
	{
		// see if it intersects
		if (!ltris_[i].intersects( source, tranl ) ) continue;

		// see how far we really travelled
		float ndist = state.sTravel_;

		if (state.onlyLess_ && ndist > state.dist_) continue;
		if (state.onlyMore_ && ndist < state.dist_) continue;
		state.dist_ = ndist;

		// call the callback function
		ltris_[i].flags( uint8(cpp_->triFlags_) );
		int say = state.cc_( *this, ltris_[i], state.dist_ );

		// see if any other collisions are wanted
		if (!say) return true;

		// some are wanted ... see if it's only one side
		state.onlyLess_ = !(say & 2);
		state.onlyMore_ = !(say & 1);
	}

	return false;
}



// -----------------------------------------------------------------------------
// Section: namespace Script
// -----------------------------------------------------------------------------

/**
 *	This is a special converter to represent a chunk pointer in python.
 *	It handles 'heaven' and 'earth' pseudo-pointers too.
 */
PyObject * Script::getData( const Chunk * pChunk )
{
	BW_GUARD;
	if (uintptr(pChunk) > ChunkBoundary::Portal::LAST_SPECIAL )
	{
		std::string fullid = pChunk->identifier() + "@" +
			pChunk->mapping()->name();
		return PyString_FromString( fullid.c_str() );
	}

	switch (uintptr(pChunk))
	{
	case 0:
		Py_Return;
	case ChunkBoundary::Portal::HEAVEN:
		return PyString_FromString( "heaven" );
	case ChunkBoundary::Portal::EARTH:
		return PyString_FromString( "earth" );
	case ChunkBoundary::Portal::INVASIVE:
		return PyString_FromString( "invasive" );
	case ChunkBoundary::Portal::EXTERN:
		return PyString_FromString( "extern" );
	}

	return PyString_FromString( "unknown_special" );
}


// -----------------------------------------------------------------------------
// Section: ChunkPortal
// -----------------------------------------------------------------------------

/*~ function ChunkPortal activate
 *  @components{ cell, client }
 *  This adds the ChunkPortal to the collision scene. Unless this is called,
 *  an entity will not collide with the ChunkPortal regardless of whether its
 *  permissive attribute is set to true.
 *  @return None
 */
/*~ attribute ChunkPortal home
 *  @components{ cell, client }
 *  The name of the chunk in which this resides.
 *  This is the opposite of the chunk attribute.
 *  @type Read-Only String.
 */
/*~ attribute ChunkPortal triFlags
 *  @components{ cell, client }
 *  The collision flags used for the triangles which make up this ChunkPortal's
 *  collision box. These flags are used by both the collision and rendering
 *  systems of BigWorld.
 *
 *
 *  Flags:
 *
 *  2  : Transparent - This dictates whether the rendering system needs to sort
 *  or alpha test these triangles. If this is true then the CursorCamera will
 *  not collide with them.
 *
 *  4  : Blended - This is read by the rendering system and informs it that the
 *  triangles are to be drawn using blended transparency. If this is true then
 *  the CursorCamera will not collide with them.
 *
 *  8  : Terrain - This indicates whether these triangles should be treated as
 *  part of the terrain.
 *
 *  16 : NoCollide - If this is set to true true, these triangles are not
 *  considered to be solid by the collision system. Note that this has no
 *  effect unless the ChunkPortal has been added to the collision scene via its
 *  activate function.
 *
 *  32 : DoubleSided - This is read by the rendering system and informs it that
 *  the triangles should be visible from both in front and behind.
 *
 *
 *  Example:
 *  @{
 *  # Set cp to the chunkportal with label "PortalLabel" from the chunk named
 *  # "ChunkName".
 *  cp = BigWorld.chunkInhabitant( "PortalLabel", "ChunkName" )
 *  # set the chunkportal's collision flags to DoubleSided
 *  cp.triFlags = ( 1 << 5 )
 *  @}
 *  @type Read-Write Integer.
 */
/*~ attribute ChunkPortal internal
 *  @components{ cell, client }
 *  This is true when this leads from an outside chunk to an inside chunk. This
 *  would occur when the home attribute referred to an outdoor chunk, and the
 *  chunk attribute referred to an indoor chunk.
 *  @type Read-Only Boolean.
 */
/*~ attribute ChunkPortal permissive
 *  @components{ cell, client }
 *  This dictates whether entities can pass through the portal.
 *  By having this set to true the ChunkPortal is opened, whereas setting it to
 *  false closes it, which allows ChunkPortal objects to function as doors that
 *  are enforcible by the game's server.
 *  Note that for a ChunkPortal to block entities it must be added to the
 *  collision scene via its activate function.
 *  @type Read-Write Boolean.
 */
/*~ attribute ChunkPortal chunk
 *  @components{ cell, client }
 *  The name of the chunk to which this leads.
 *  This is the opposite of the home attribute.
 *  @type Read-Only String.
 */
/*~ attribute ChunkPortal points
 *  @components{ cell, client }
 *  The points which make up the shape of the portal on it's plane. These are
 *  specified around the origin attribute, on axis defined by the attributes
 *  uAxis and vAxis.
 *  @type Read-Only Tuple of 2D points (represented by a tuple consisting of
 *  2 floats).
 */
/*~ attribute ChunkPortal uAxis
 *  @components{ cell, client }
 *  A vector in worldspace along the portal's plane.
 *  This, combined with attribute vAxis, provides the coordinate system within
 *  which the points which define the portal shape exist.
 *  @type Read-Only Vector3.
 */
/*~ attribute ChunkPortal vAxis
 *  @components{ cell, client }
 *  The vector in worldspace along the portal's plane which is perpendicular
 *  to uAxis.
 *  This, combined with attribute uAxis, provides the coordinate system within
 *  which the points which define the portal shape exist.
 *  @type Read-Only Vector3.
 */
/*~ attribute ChunkPortal origin
 *  @components{ cell, client }
 *  The origin of the portal's plane in local chunk space. This is the point on
 *  the plane nearest to the origin, and is equal to
 *  plane_n * plane_d / plane_n.lengthSquared().
 *  @type Read-Only Vector3.
 */
/*~ attribute ChunkPortal lcentre
 *  @components{ cell, client }
 *  The centre of the portal in local chunk space.
 *  @type Read-Only Vector3.
 */
/*~ attribute ChunkPortal centre
 *  @components{ cell, client }
 *  The origin of the portal in worldspace. This is not necessarily the centre
 *  of the portal shape.
 *  @type Read-Only Vector3.
 */
/*~ attribute ChunkPortal plane_n
 *  @components{ cell, client }
 *  The the normal to the plane on which the portal lies, in worldspace.
 *  @type Read-Only Vector3.
 */
/*~ attribute ChunkPortal plane_d
 *  @components{ cell, client }
 *  The dot product of this plane's normal with the vector to any point on the
 *  plane from the local chunk space origin. This, divided by the length of
 *  attribute plane_n is equal to the minimum distance from the plane to the
 *  local chunk space origin.
 *  @type Read-Only float.
 */
/*~ attribute ChunkPortal label
 *  @components{ cell, client }
 *  A string which can be used to identify the portal.
 *
 *  Example:
 *  @{
 *  # Returns the chunkportal with label "PortalLabel" from the chunk named
 *  "ChunkName".
 *  BigWorld.chunkInhabitant( "PortalLabel", "ChunkName" )
 *  @}
 *  @type Read-Only String.
 */

PY_TYPEOBJECT( ChunkPortal )

PY_BEGIN_METHODS( ChunkPortal )
	PY_METHOD( activate )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( ChunkPortal )
	PY_ATTRIBUTE( home )
	PY_ATTRIBUTE( triFlags )
	PY_ATTRIBUTE( internal )
	PY_ATTRIBUTE( permissive )
	PY_ATTRIBUTE( chunk )
	PY_ATTRIBUTE( points )
	PY_ATTRIBUTE( uAxis )
	PY_ATTRIBUTE( vAxis )
	PY_ATTRIBUTE( origin )
	PY_ATTRIBUTE( lcentre )
	PY_ATTRIBUTE( centre )
	PY_ATTRIBUTE( plane_n )
	PY_ATTRIBUTE( plane_d )
	PY_ATTRIBUTE( label )
PY_END_ATTRIBUTES()



/**
 *	Constructor.
 */
ChunkPortal::ChunkPortal( ChunkBoundary::Portal * pPortal, PyTypePlus * pType ):
	PyObjectPlusWithVD( pType ),
	ChunkItem( WantFlags( 0 ) ),
	pPortal_( pPortal ),
	triFlags_( 0 ),
	activated_( false )
{
}


/**
 *	Destructor.
 */
ChunkPortal::~ChunkPortal()
{
}

/**
 *	Method to activate ourselves, i.e. make our presence felt in the
 *	collision scene
 */
void ChunkPortal::activate()
{
	BW_GUARD;
	if (activated_) return;
	activated_ = true;

	if (pChunk_ != NULL)
	{
		ChunkModelObstacle::instance( *pChunk_ ).addObstacle(
			new PortalObstacle( this ) );
	}
}


/**
 * Method to return if we are activated or not.
 */
bool ChunkPortal::activated()
{
	return activated_;
}

/**
 *	Python get attribute method
 */
PyObject * ChunkPortal::pyGetAttribute( const char * attr )
{
	BW_GUARD;
	PY_GETATTR_STD();

	return this->PyObjectPlus::pyGetAttribute( attr );
}

/**
 *	Python set attribute method
 */
int ChunkPortal::pySetAttribute( const char * attr, PyObject * value )
{
	BW_GUARD;
	PY_SETATTR_STD();

	return this->PyObjectPlus::pySetAttribute( attr, value );
}


/**
 *	Get the points that form the boundary of this chunk
 */
PyObject * ChunkPortal::pyGet_points()
{
	BW_GUARD;
	int sz = pPortal_->points.size();
	PyObject * pTuple = PyTuple_New( sz );

	for (int i = 0; i < sz; i++)
	{
		PyTuple_SetItem( pTuple, i, Script::getData( pPortal_->points[i] ) );
	}

	return pTuple;
}


#ifndef MF_SERVER
#include "moo/visual_channels.hpp"
#include "moo/render_context.hpp"

/**
 * TODO: to be documented.
 */
class PortalDrawItem : public Moo::ChannelDrawItem
{
public:
	PortalDrawItem( const Vector3* pRect, Moo::Material* pMaterial, uint32 colour )
	: pMaterial_( pMaterial ),
	  colour_( colour )
	{
		BW_GUARD;
		memcpy( rect_, pRect, sizeof(Vector3) * 4 );
		distance_ = (pRect[0].z + pRect[1].z + pRect[2].z + pRect[3].z) / 4.f;
	}
	void draw()
	{
		BW_GUARD;
		pMaterial_->set();
		Moo::rc().setFVF( Moo::VertexXYZNDS::fvf() );
		Moo::rc().setVertexShader( NULL );
		Moo::rc().device()->SetPixelShader( NULL );
		Moo::rc().setRenderState( D3DRS_LIGHTING, FALSE );
		Moo::rc().device()->SetTransform( D3DTS_PROJECTION, &Moo::rc().projection() );
		Moo::rc().device()->SetTransform( D3DTS_VIEW, &Matrix::identity );
		Moo::rc().device()->SetTransform( D3DTS_WORLD, &Matrix::identity );

		Moo::VertexXYZNDS pVerts[4];

		for (int i = 0; i < 4; i++)
		{
			pVerts[i].colour_ = colour_;
			pVerts[i].specular_ = 0xffffffff;
		}
		pVerts[0].pos_ = rect_[0];
		pVerts[1].pos_ = rect_[1];
		pVerts[2].pos_ = rect_[3];
		pVerts[3].pos_ = rect_[2];

		Moo::rc().drawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, pVerts, sizeof( pVerts[0] ) );
	}
	void fini()
	{
		delete this;
	}
private:
	Vector3 rect_[4];
	Moo::Material* pMaterial_;
	uint32 colour_;
};


/**
 *	Draw method to debug portal states
 */
void ChunkPortal::draw()
{
	BW_GUARD;
	// get out if we haven't been activated
	if (!activated_) return;

	static DogWatch drawWatch( "ChunkPortal" );
	ScopedDogWatch watcher( drawWatch );

	// get the material
	static Moo::Material * mym = NULL;
	static bool shouldDrawChunkPortals = false;
	if (mym == NULL)
	{
		mym = new Moo::Material();
		mym->load( "materials/addvertex.mfm" );

		MF_WATCH( "Client Settings/drawChunkPortals", shouldDrawChunkPortals );
	}

	// get out if we don't want to draw them
	if (!shouldDrawChunkPortals) return;

	// get the transformation matrix
	Matrix tran;
	tran.multiply( Moo::rc().world(), Moo::rc().view() );

	// transform all the points
	Vector3 prect[4];
	for (int i = 0; i < 4; i++)
	{
		// project the point straight into clip space
		tran.applyPoint( prect[i], Vector3(
			pPortal_->uAxis * pPortal_->points[i][0] +
			pPortal_->vAxis * pPortal_->points[i][1] +
			pPortal_->origin ) );

	}

	Moo::SortedChannel::addDrawItem( new PortalDrawItem( prect, mym, pPortal_->permissive ? 0xff003300 : 0xff550000 ) );
}
#else
void ChunkPortal::draw()
{
}
#endif

/**
 *	Toss method. Not that this should be called dynamically.
 */
void ChunkPortal::toss( Chunk * pChunk )
{
	BW_GUARD;
	std::string label = pPortal_->label;
	if (label.empty())
	{
		char buf[32];
		bw_snprintf( buf, sizeof(buf), "portal_%p", pPortal_ );
		label = buf;
	}

	if (pChunk_ != NULL)
	{
		ChunkPyCache::instance( *pChunk_ ).del( label );
		if (activated_)
			ChunkModelObstacle::instance( *pChunk_ ).delObstacles( this );
	}

	this->ChunkItem::toss( pChunk );

	if (pChunk_ != NULL)
	{
		ChunkPyCache::instance( *pChunk_ ).add( label, this );
		if (activated_)
			ChunkModelObstacle::instance( *pChunk_ ).addObstacle(
				new PortalObstacle( this ) );
	}
}


// -----------------------------------------------------------------------------
// Section: ChunkPyCache
// -----------------------------------------------------------------------------


/**
 *	Constructor
 */
ChunkPyCache::ChunkPyCache( Chunk & chunk ) :
	chunk_( chunk ),
	bound_( false )
{
}

/**
 *	Destructor
 */
ChunkPyCache::~ChunkPyCache()
{
}


/**
 *	Add this python object to our list of exposed items for this chunk
 */
void ChunkPyCache::add( const std::string & name, PyObject * pObject )
{
	BW_GUARD;
	// this is safe when overwriting
	exposed_[ name ] = pObject;
}

/**
 *	Remove this python object from our list of exposed items for this chunk
 */
void ChunkPyCache::del( const std::string & name )
{
	BW_GUARD;
	// this is safe when absent
	exposed_.erase( name );
}


/**
 *	Get the python object with the given name from this chunk
 */
SmartPointer<PyObject> ChunkPyCache::get( const std::string & name )
{
	BW_GUARD;
	NamedPyObjects::iterator found = exposed_.find( name );
	return found != exposed_.end() ? found->second : (PyObject *)NULL;
}


/**
 *	Static helper method to get chunk from info describing it
 */
Chunk * lookupChunk( const std::string & chunkNMapping, SpaceID spaceID,
	const char * methodName )
{
	BW_GUARD;
	// look up the space
	ChunkSpacePtr pSpace;

	ChunkManager & cm = ChunkManager::instance();
	if (spaceID == 0)
	{
		pSpace = cm.cameraSpace();
	}
	else
	{
		pSpace = cm.space( spaceID, false );
	}

	if (!pSpace)
	{
		PyErr_Format( PyExc_ValueError,
			"%s: space ID %d not found", methodName, int(spaceID) );
		return NULL;
	}

	// look up the chunk
	std::string chunkOnly = chunkNMapping;
	std::string mappingOnly;
	uint firstAt = chunkNMapping.find_first_of( '@' );
	if (firstAt < chunkNMapping.size())
	{
		chunkOnly = chunkNMapping.substr( 0, firstAt );
		mappingOnly = chunkNMapping.substr( firstAt+1 );
	}
	Chunk * pChunk = pSpace->findChunk( chunkOnly, mappingOnly );
	if (pChunk == NULL)
	{
		PyErr_Format( PyExc_ValueError,
			"%s: chunk '%s' not found", methodName, chunkNMapping.c_str() );
		return NULL;
	}

	return pChunk;
}

/*~ function BigWorld chunkInhabitant
 *  @components{ client, cell }
 *  This function gives access to a chunk inhabitant via its label, or to all
 *  inhabitants of a chunk. A chunk inhabitant is a static object which resides
 *  in a chunk, such as a PyChunkModel or PyChunkLight.
 *  This throws a ValueError if it its label argument is not ""
 *  and none of the inhabitants of the searched chunk have a matching label.
 *  A ValueError is also thrown if no chunks are found in the searched
 *  space that match argument chunkNMapping or if the space argument is
 *  specified and a corresponding space is not found.
 *  @param label label is a string containing the label of the desired chunk
 *  inhabitant, or an empty string if all chunk inhabitants are desired.
 *  @param chunkNMapping chunkNMapping is a string containing the name of the
 *  chunk to search.
 *  @param spaceID spaceID is an optional id being the id of the space in which
 *  the chunk resides. This defaults to the space currently inhabited by the
 *  camera if omitted.
 *  @return If label is "", this returns a tuple containing all inhabitants of
 *  the specified chunk. Otherwise this returns an inhabitant of the specified
 *  chunk with a label that matches the label argument.
 */
/**
 *	Static method to get the given chunk inhabitant
 */
PyObject * ChunkPyCache::chunkInhabitant( const std::string & label,
	const std::string & chunkNMapping, SpaceID spaceID )
{
	BW_GUARD;
	Chunk * pChunk = lookupChunk( chunkNMapping, spaceID,
		"BigWorld.chunkInhabitant()" );
	if (!pChunk) return NULL;

	// return all inhabitants if desired
	if (label.empty())
	{
		NamedPyObjects & npo = ChunkPyCache::instance( *pChunk ).exposed_;
		NamedPyObjects::iterator it = npo.begin();
		PyObject * pTuple = PyTuple_New( npo.size() );
		for (uint i = 0; i < npo.size(); i++, it++)
		{
			PyObject * pPyObj = it->second.getObject();
			Py_INCREF( pPyObj );
			PyTuple_SetItem( pTuple, i, pPyObj );
		}
		return pTuple;
	}

	// look up the inhabitant
	SmartPointer<PyObject> spPyObj =
		ChunkPyCache::instance( *pChunk ).get( label );
	if (!spPyObj)
	{
		PyErr_Format( PyExc_ValueError, "BigWorld.chunkInhabitant(): "
			"no inhabitant with label '%s' found in chunk '%s'",
			label.c_str(), chunkNMapping.c_str() );
		return NULL;
	}

	// and return it!
	PyObject * pPyObj = spPyObj.getObject();
	Py_INCREF( pPyObj );
	return pPyObj;
}
PY_MODULE_STATIC_METHOD( ChunkPyCache, chunkInhabitant, BigWorld )

/*~ function BigWorld findChunkFromPoint
 *  @components{ client, cell }
 *  findChunkFromPoint is used to identify the chunk which surrounds a given
 *  location. It throws a ValueError if no chunk is found at the given
 *  location.
 *  @param point point is a Vector3 describing the location to search for a
 *  chunk, in world space.
 *  @param spaceID spaceID is an optional id being the name of the space to
 *  search. This defaults to the space currently inhabited by the camera if
 *  omitted.
 *  @return The name of the chunk found, as a string.
 */
/**
 *	Static method to find a chunk from a point
 */
PyObject * ChunkPyCache::findChunkFromPoint( const Vector3 & point,
	SpaceID spaceID )
{
	BW_GUARD;
	// look up the space
	ChunkSpacePtr pSpace = NULL;
	ChunkManager & cm = ChunkManager::instance();
	if (spaceID == 0)
	{
		pSpace = cm.cameraSpace();
	}
	else
	{
		pSpace = cm.space( spaceID, false );
	}

	if (!pSpace)
	{
		PyErr_Format( PyExc_ValueError,
			"BigWorld.findChunkFromPoint: space ID %d not found", int(spaceID) );
		return NULL;
	}

	// ask it to find the chunk
	Chunk * pChunk = pSpace->findChunkFromPoint( point );
	if (!pChunk)
	{
		char sillybuf[256];
		bw_snprintf( sillybuf, sizeof(sillybuf), "BigWorld.findChunkFromPoint(): "
			"chunk at (%f,%f,%f) not found", point.x, point.y, point.z );
		PyErr_SetString( PyExc_ValueError, sillybuf );
		return NULL;
	}

	// return the chunk identifier
	return Script::getData(
		pChunk->identifier() + "@" + pChunk->mapping()->name() );
}
PY_MODULE_STATIC_METHOD( ChunkPyCache, findChunkFromPoint, BigWorld )

/*~ function BigWorld chunkTransform
 *  @components{ client, cell }
 *  This gives access to a chunk's transform.
 *  It throws a ValueError if no chunks are found in the searched space that
 *  match argument chunkNMapping, or if the space argument is specified and a
 *  corresponding space is not found.
 *  @param chunkNMapping chunkNMapping is a string containing the name of the
 *  chunk whose transform is to be returned.
 *  @param spaceID spaceID is an optional id being the id of the space in which
 *  the chunk resides. This defaults to the space currently inhabited by the
 *  camera if omitted.
 *  @return A Matrix which describes the chunk's transform.
 */
/**
 *	Method to let python get a chunk's transform
 */
static PyObject * chunkTransform( const std::string & chunkNMapping,
	SpaceID spaceID )
{
	BW_GUARD;
	Chunk * pChunk = lookupChunk( chunkNMapping, spaceID,
		"BigWorld.chunkTransform()" );
	if (!pChunk) return NULL;

	return Script::getData( pChunk->transform() );
}
PY_AUTO_MODULE_FUNCTION( RETOWN, chunkTransform,
	ARG( std::string, OPTARG( SpaceID, 0, END ) ), BigWorld )


/**
 *	Bind method
 */
void ChunkPyCache::bind( bool looseNotBind )
{
	BW_GUARD;
	// only do this once
	if (bound_) return;
	bound_ = true;

	// go through all portals and create items for named ones,
	// whether bound or not.
	ChunkBoundaries::iterator			bit;
	ChunkBoundary::Portals::iterator	pit;

	// go through all our joint boundaries
	for (bit = chunk_.joints().begin(); bit != chunk_.joints().end(); bit++)
	{
		// go through all their bound portals
		for (pit = (*bit)->boundPortals_.begin();
			pit != (*bit)->boundPortals_.end();
			pit++)
		{
			//if (!(*pit)->label.empty())
			bool outside = (*pit)->points.size() == 4 &&
				((*pit)->points[0] - (*pit)->points[2]).
					lengthSquared() > 100.f*100.f;
			if ((*pit)->hasChunk() && (!(*pit)->label.empty() || !outside))
			{
				ChunkPortal* pCP = new ChunkPortal( *pit );
				chunk_.addStaticItem( pCP );
				Py_DECREF( pCP );
			}
		}

		// go through all their unbound portals too
		for (pit = (*bit)->unboundPortals_.begin();
			pit != (*bit)->unboundPortals_.end();
			pit++)
		{
			//if (!(*pit)->label.empty())
			bool outside = (*pit)->points.size() == 4 &&
				((*pit)->points[0] - (*pit)->points[2]).
					lengthSquared() > 100.f*100.f;
			if ((*pit)->hasChunk() && (!(*pit)->label.empty() || !outside))
			{
				ChunkPortal* pCP = new ChunkPortal( *pit );
				chunk_.addStaticItem( pCP );
				Py_DECREF( pCP );
			}
		}
	}
}

/**
 *	Static touch method
 */
void ChunkPyCache::touch( Chunk & chunk )
{
	BW_GUARD;
	// make us exist in this chunk
	ChunkPyCache::instance( chunk );
}

/// Static instance accessor initialiser
ChunkCache::Instance<ChunkPyCache> ChunkPyCache::instance;
