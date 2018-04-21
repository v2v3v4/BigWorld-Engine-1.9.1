/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_PORTAL_HPP
#define CHUNK_PORTAL_HPP

#include "Python.h"

#include "chunk/chunk.hpp"
#include "chunk/chunk_item.hpp"
#include "chunk/chunk_boundary.hpp"

#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"

#include "network/basictypes.hpp"

#include <map>
#include <string>


namespace Script
{
	PyObject * getData( const Chunk * pChunk );
};

/*~ class NoModule.ChunkPortal
 *  @components{ cell, client }
 *	This class is the chunk item created to represent a portal to or from an
 *  indoor chunk. A chunk item is a static object which resides inside a chunk,
 *  such as a PyChunkModel or PyChunkLight. BigWorld creates a ChunkPortal
 *  object inside each chunk for each chunk that it is attached to, where at
 *  least one of the two is an indoor chunk. Instances of these can be accessed
 *  by name via the BigWorld.chunkInhabitant function.
 */

/**
 *	This class is the chunk item created to represent a portal when
 *	it has a special name and can thus be referenced (and fiddled with)
 *	by scripts.
 */
class ChunkPortal : public PyObjectPlusWithVD, public ChunkItem
{
	Py_Header( ChunkPortal, PyObjectPlusWithVD )

public:
	ChunkPortal( ChunkBoundary::Portal * pPortal,
		PyTypePlus * pType = & s_type_ );
	~ChunkPortal();

	// Python stuff
	PyObject * pyGetAttribute( const char * attr );
	int pySetAttribute( const char * attr, PyObject * value );

	void activate();
	bool activated();

	PY_AUTO_METHOD_DECLARE( RETVOID, activate, END )

	PY_RO_ATTRIBUTE_DECLARE( pChunk_, home )
	PY_RW_ATTRIBUTE_DECLARE( triFlags_, triFlags )

	PY_RO_ATTRIBUTE_DECLARE( pPortal_->internal, internal )
	PY_RW_ATTRIBUTE_DECLARE( pPortal_->permissive, permissive )
	PY_RO_ATTRIBUTE_DECLARE( pPortal_->pChunk, chunk )

	PyObject * pyGet_points();
	PY_RO_ATTRIBUTE_SET( points )

	PY_RO_ATTRIBUTE_DECLARE( pPortal_->uAxis, uAxis )
	PY_RO_ATTRIBUTE_DECLARE( pPortal_->vAxis, vAxis )
	PY_RO_ATTRIBUTE_DECLARE( pPortal_->origin, origin )
	PY_RO_ATTRIBUTE_DECLARE( pPortal_->lcentre, lcentre )
	PY_RO_ATTRIBUTE_DECLARE( pPortal_->centre, centre )
	PY_RO_ATTRIBUTE_DECLARE( pPortal_->plane.normal(), plane_n )
	PY_RO_ATTRIBUTE_DECLARE( pPortal_->plane.d(), plane_d )
	PY_RO_ATTRIBUTE_DECLARE( pPortal_->label, label )


	// ChunkItem overrides
	virtual void toss( Chunk * pChunk );
	virtual void draw();	// for debugging

	virtual void incRef() const		{ this->PyObjectPlus::incRef(); }
	virtual void decRef() const		{ this->PyObjectPlus::decRef(); }
	virtual int refCount() const	{ return this->PyObjectPlus::refCount(); }

	ChunkBoundary::Portal * pPortal() const	{ return pPortal_; }
	uint32 & triFlags()						{ return triFlags_; }

private:
	ChunkPortal( const ChunkPortal& );
	ChunkPortal& operator=( const ChunkPortal& );

	ChunkBoundary::Portal * pPortal_;
	uint32					triFlags_;

	bool					activated_;

	friend class PortalObstacle;
};

typedef SmartPointer<ChunkPortal> ChunkPortalPtr;

/**
 *	This class keeps track of all python-accessible objects in a chunk.
 *	It also takes care of creating the ChunkPortal items when a chunk
 *	loads (actually when it binds, for threading reasons)
 */
class ChunkPyCache : public ChunkCache
{
	typedef ChunkPyCache This;

public:
	ChunkPyCache( Chunk & chunk );
	~ChunkPyCache();

	void add( const std::string & name, PyObject * pObject );
	void del( const std::string & name );

	SmartPointer<PyObject> get( const std::string & name );

	static PyObject * chunkInhabitant( const std::string & label,
		const std::string & chunkNMapping, SpaceID spaceID );
	PY_AUTO_MODULE_STATIC_METHOD_DECLARE( RETOWN, chunkInhabitant,
		ARG( std::string, ARG( std::string, OPTARG( SpaceID, 0, END ) ) ) )

	static PyObject * findChunkFromPoint( const Vector3 & point,
		SpaceID spaceID );
	PY_AUTO_MODULE_STATIC_METHOD_DECLARE( RETOWN, findChunkFromPoint,
		ARG( Vector3, OPTARG( SpaceID, 0, END ) ) )

	virtual void bind( bool looseNotBind );
	static void touch( Chunk & chunk );

	static Instance<ChunkPyCache>	instance;

	typedef std::map< std::string, SmartPointer<PyObject> > NamedPyObjects;

	const NamedPyObjects & objects()
		{ return exposed_; }

private:
	Chunk & chunk_;
	NamedPyObjects	exposed_;
	bool bound_;
};

#endif // CHUNK_PORTAL_HPP
