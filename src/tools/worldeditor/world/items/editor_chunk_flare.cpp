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
#include "worldeditor/world/items/editor_chunk_flare.hpp"
#include "worldeditor/world/items/editor_chunk_substance.ipp"
#include "worldeditor/world/world_manager.hpp"
#include "worldeditor/world/editor_chunk.hpp"
#include "worldeditor/editor/item_editor.hpp"
#include "worldeditor/editor/item_properties.hpp"
#include "appmgr/options.hpp"
#include "model/super_model.hpp"
#include "romp/geometrics.hpp"
#include "chunk/chunk_model.hpp"
#include "chunk/chunk_manager.hpp"
#include "resmgr/string_provider.hpp"

#if UMBRA_ENABLE
#include <umbraModel.hpp>
#include <umbraObject.hpp>
#include "chunk/chunk_umbra.hpp"
#endif

namespace
{
	ModelPtr	s_flareModel; //Large Icon
	ModelPtr	s_flareModelSmall; //Small Icon
	bool		s_triedLoadOnce	= false;
}


// -----------------------------------------------------------------------------
// Section: EditorChunkFlare
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
EditorChunkFlare::EditorChunkFlare()
{
}


/**
 *	Destructor.
 */
EditorChunkFlare::~EditorChunkFlare()
{
}


/**
 *	This method saves the data section pointer before calling its
 *	base class's load method
 */
bool EditorChunkFlare::load( DataSectionPtr pSection, Chunk* pChunk, std::string* errorString )
{
	bool ok = this->EditorChunkSubstance<ChunkFlare>::load( pSection, pChunk );
	if (ok)
	{
		flareRes_ = pSection->readString( "resource" );
	}
	else if ( errorString )
	{
		*errorString = "Failed to load flare '" + pSection->readString( "resource" ) + "'";
	}
	return ok;
}



/**
 *	Save any property changes to this data section
 */
bool EditorChunkFlare::edSave( DataSectionPtr pSection )
{
	if (!edCommonSave( pSection ))
		return false;

	pSection->writeString( "resource", flareRes_ );
	pSection->writeVector3( "position", position_ );
	pSection->delChild( "colour" );
	if (colourApplied_) pSection->writeVector3( "colour", colour_ );

	return true;
}


/**
 *	Get the current transform
 */
const Matrix & EditorChunkFlare::edTransform()
{
	transform_.setIdentity();
	transform_.translation( position_ );

	return transform_;
}


/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkFlare::edTransform( const Matrix & m, bool transient )
{
	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyToOrigin() );
	if (pNewChunk == NULL) return false;

	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		transform_ = m;
		position_ = transform_.applyToOrigin();
		this->syncInit();
		return true;
	}

	// make sure the chunks aren't readonly
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() 
		|| !EditorChunkCache::instance( *pNewChunk ).edIsWriteable())
		return false;

	// ok, accept the transform change then
	transform_.multiply( m, pOldChunk->transform() );
	transform_.postMultiply( pNewChunk->transformInverse() );
	position_ = transform_.applyToOrigin();

	// note that both affected chunks have seen changes
	WorldManager::instance().changedChunk( pOldChunk );
	WorldManager::instance().changedChunk( pNewChunk );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );
	this->syncInit();
	return true;
}




/**
 *	This class checkes whether or not a data section is a suitable flare
 *	resource.
 */
class FlareResourceChecker : public ResourceProperty::Checker
{
public:
	virtual bool check( DataSectionPtr pRoot ) const
	{
		return !!pRoot->openSection( "Flare" );
	}

	static FlareResourceChecker instance;
};

FlareResourceChecker FlareResourceChecker::instance;


/**
 *	This helper class wraps up a flare's colour property
 */
class FlareColourWrapper : public UndoableDataProxy<ColourProxy>
{
public:
	explicit FlareColourWrapper( EditorChunkFlarePtr pItem ) :
		pItem_( pItem )
	{
	}

	virtual Moo::Colour EDCALL get() const
	{
		return pItem_->colour();
	}

	virtual void EDCALL setTransient( Moo::Colour v )
	{
		pItem_->colour( v );
	}

	virtual bool EDCALL setPermanent( Moo::Colour v )
	{
		// make it valid
		if (v.r < 0.f) v.r = 0.f;
		if (v.r > 1.f) v.r = 1.f;
		if (v.g < 0.f) v.g = 0.f;
		if (v.g > 1.f) v.g = 1.f;
		if (v.b < 0.f) v.b = 0.f;
		if (v.b > 1.f) v.b = 1.f;
		v.a = 1.f;

		// set it
		this->setTransient( v );

		// flag the chunk as having changed
		WorldManager::instance().changedChunk( pItem_->chunk() );

		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		return true;
	}

	virtual std::string EDCALL opName()
	{
		return L( "WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_FLARE/SET_COLOUR", pItem_->edDescription() );
	}

private:
	EditorChunkFlarePtr	pItem_;
};


/**
 *	Add the properties of this flare to the given editor
 */
bool EditorChunkFlare::edEdit( class ChunkItemEditor & editor )
{
	editor.addProperty( new ResourceProperty(
		L( "WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_FLARE/FLARE" ),
		new SlowPropReloadingProxy<EditorChunkFlare,StringProxy>(
			this, "flare resource", 
			&EditorChunkFlare::flareResGet, 
			&EditorChunkFlare::flareResSet ),
		".xml",
		FlareResourceChecker::instance ) );

	MatrixProxy * pMP = new ChunkItemMatrix( this );
	editor.addProperty( new GenPositionProperty( L( "WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_FLARE/POSITION" ), pMP ) );

	editor.addProperty( new ColourProperty( L( "WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_FLARE/COLOUR" ),
		new FlareColourWrapper( this ) ) );

	// colourApplied_ is set to (colour_ != 'white')

	return true;
}


#if UMBRA_ENABLE
void EditorChunkFlare::tick( float /*dTime*/ )
{
	ModelPtr model = reprModel();
	if (currentUmbraModel_ != model)
	{
		currentUmbraModel_ = model;
		this->syncInit();
	}
}
#endif


void EditorChunkFlare::draw()
{
	if (!edShouldDraw())
		return;

	ModelPtr model = reprModel();
	
	if( WorldManager::instance().drawSelection() && model)
	{
		// draw a some points near the centre of the reprModel, so the system
		// can be selected from the distance where the repr model might be
		// smaller than a pixel and fail to draw.
		Moo::rc().push();
		Moo::rc().world( chunk()->transform() );
		Moo::rc().preMultiply( edTransform() );
		// bias of half the size of the representation model's bounding box in
		// the vertical axis, because the object might be snapped to terrain
		// or another object, so the centre might be below something else.
		float bias = model->boundingBox().width() / 2.0f;
		Vector3 points[3];
		points[0] = Vector3( 0.0f, -bias, 0.0f );
		points[1] = Vector3( 0.0f, 0.0f, 0.0f );
		points[2] = Vector3( 0.0f, bias, 0.0f );
		Geometrics::drawPoints( points, 3, 3.0f, (DWORD)this );
		Moo::rc().pop();
	}

	EditorChunkSubstance<ChunkFlare>::draw();
}


/**
 *	This method gets our colour as a moo colour
 */
Moo::Colour EditorChunkFlare::colour() const
{
	Vector4 v4col = colourApplied_ ?
		Vector4( colour_ / 255.f, 1.f ) :
		Vector4( 1.f, 1.f, 1.f, 1.f );

	return Moo::Colour( v4col );
}

/**
 *	This method sets our colour (and colourApplied flag) from a moo colour
 */
void EditorChunkFlare::colour( const Moo::Colour & c )
{
	colour_ = Vector3( c.r, c.g, c.b ) * 255.f;
	colourApplied_ = !(c.r == 1.f && c.g == 1.f && c.b == 1.f);
}


/**
 *	This cleans up some one-off internally used memory.
 */
/*static*/ void EditorChunkFlare::fini()
{
	s_flareModel = NULL;
	s_flareModelSmall = NULL;
}


/**
 *	Return a modelptr that is the representation of this chunk item
 */
ModelPtr EditorChunkFlare::reprModel() const
{
	int renderProxys = Options::getOptionInt( "render/proxys", 1 );
	int renderLightProxy = Options::getOptionInt( "render/proxys/lightProxys", 1 );
	int renderFlareProxy = Options::getOptionInt( "render/proxys/flareProxys", 1 );
	int renderLargeProxy = Options::getOptionInt( "render/proxys/flareProxyLarge", 1 );

	if (!s_flareModel && !s_flareModelSmall && !s_triedLoadOnce)
	{
		s_flareModel = Model::get( "resources/models/flare.model" );
		s_flareModelSmall = Model::get( "resources/models/flare_small.model" );
		s_triedLoadOnce = true;
	}
	if ( renderLargeProxy && renderFlareProxy && renderLightProxy && renderProxys ) 
	{
		return s_flareModel;
	}
	else if ( renderFlareProxy && renderLightProxy && renderProxys  )
	{
		return s_flareModelSmall;
	}

	return NULL;
}


void EditorChunkFlare::syncInit()
{
	// Grab the visibility bounding box
	#if UMBRA_ENABLE
	pUmbraModel_ = NULL;
	pUmbraObject_ = NULL;	
	if (currentUmbraModel_ == NULL)
	{
		return;
	}	
	
	BoundingBox bb = BoundingBox::s_insideOut_;
	bb = currentUmbraModel_->boundingBox();
	if (!pUmbraObject_.hasObject())
	{
		pUmbraModel_ = UmbraModelProxy::getObbModel( bb.minBounds(), bb.maxBounds() );
		pUmbraObject_ = UmbraObjectProxy::get( pUmbraModel_ );
	}
	
	// Set the user pointer up to point at this chunk item
	pUmbraObject_->object()->setUserPointer( (void*)this );

	// Set up object transforms
	Matrix m = pChunk_->transform();
	m.preMultiply( transform_ );
	pUmbraObject_->object()->setObjectToCellMatrix( (Umbra::Matrix4x4&)m );
	pUmbraObject_->object()->setCell( pChunk_->getUmbraCell() );
	#endif
}
/// Write the factory statics stuff
#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk, &errorString)
IMPLEMENT_CHUNK_ITEM( EditorChunkFlare, flare, 1 )


// editor_chunk_flare.cpp
