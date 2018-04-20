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
#include "chunk_water.hpp"

#include "cstdmf/guard.hpp"

#include "moo/render_context.hpp"
#if UMBRA_ENABLE
#include "terrain/base_terrain_block.hpp"
#endif

#include "romp/geometrics.hpp"

#include "chunk.hpp"
#include "chunk_obstacle.hpp"
#include "chunk_space.hpp"
#include "chunk_manager.hpp"

#if UMBRA_ENABLE
#include "chunk_terrain.hpp"
#include "chunk_umbra.hpp"
#include <umbraModel.hpp>
#include <umbraObject.hpp>
#include "umbra_proxies.hpp"
#endif

// -----------------------------------------------------------------------------
// Section: ChunkWater
// -----------------------------------------------------------------------------

int ChunkWater_token;

/**
 *	Constructor.
 */
ChunkWater::ChunkWater( std::string uid ) :
	VeryLargeObject( uid, "water" ),
	pWater_( NULL ),
	pChunk_( NULL )
{
}


/**
 *	Constructor.
 */
ChunkWater::ChunkWater( ) :
	pWater_( NULL )	
{
	uid_ = "";
	type_ = "water";
}


/**
 *	Destructor.
 */
ChunkWater::~ChunkWater()
{
	if (pWater_ != NULL)
	{
		Water::deleteWater(pWater_);
		pWater_ = NULL;
	}
}


/**
 *	Load method
 */
bool ChunkWater::load( DataSectionPtr pSection, Chunk * pChunk )
{
	BW_GUARD;
	if (pChunk==NULL)
		return false;

	// clear existing water if present
	if (pWater_ != NULL)
		shouldRebuild(true);

	// load new settings (water created on first draw)
	DataSectionPtr pReqSec;

	pReqSec = pSection->openSection( "position" );
	if (!pReqSec)
		config_.position_ = pChunk->boundingBox().centre();
	else
		config_.position_ = pReqSec->asVector3();

	float lori = pSection->readFloat( "orientation", 0.f );
	Vector3 oriVec( sinf( lori ), 0.f, cosf( lori ) );
	config_.orientation_ = atan2f( oriVec.x, oriVec.z );

	pReqSec = pSection->openSection( "size" );
	if (!pReqSec) return false;

	Vector3 sizeV3 = pReqSec->asVector3();
	config_.size_ = Vector2( sizeV3.x, sizeV3.z );

	config_.fresnelConstant_ = pSection->readFloat( "fresnelConstant", 0.3f );
	config_.fresnelExponent_ = pSection->readFloat( "fresnelExponent", 5.f );

	config_.reflectionTint_ = pSection->readVector4( "reflectionTint", Vector4(1,1,1,1) );
	config_.reflectionScale_ = pSection->readFloat( "reflectionStrength", 0.04f );

	config_.refractionTint_ = pSection->readVector4( "refractionTint", Vector4(1,1,1,1) );
	config_.refractionScale_ = pSection->readFloat( "refractionStrength", 0.04f );

	config_.tessellation_ = pSection->readFloat( "tessellation", 10.f );
	config_.consistency_ = pSection->readFloat( "consistency", 0.95f );

	config_.textureTessellation_ = pSection->readFloat( "textureTessellation", config_.tessellation_);
	

	float oldX = pSection->readFloat( "scrollSpeedX", -1.f );	
	float oldY = pSection->readFloat( "scrollSpeedY", 1.f );

	config_.scrollSpeed1_ = pSection->readVector2( "scrollSpeed1", Vector2(oldX,0.5f) );	
	config_.scrollSpeed2_ = pSection->readVector2( "scrollSpeed2", Vector2(oldY,0.f) );	
	config_.waveScale_ = pSection->readVector2( "waveScale", Vector2(1.f,0.75f) );

	//config_.windDirection_ = pSection->readFloat( "windDirection", 0.0f );	
	config_.windVelocity_ = pSection->readFloat( "windVelocity", 0.02f );

	config_.sunPower_ = pSection->readFloat( "sunPower", 32.f );
	config_.sunScale_ = pSection->readFloat( "sunScale", 1.0f );

	config_.waveTexture_ = pSection->readString( "waveTexture", "system/maps/waves2.dds" );

	config_.simCellSize_ = pSection->readFloat( "cellsize", 100.f );
	config_.smoothness_ = pSection->readFloat( "smoothness", 0.f );

	config_.foamTexture_ = pSection->readString( "foamTexture", "system/maps/water_foam2.dds" );	
	
	config_.reflectionTexture_ = pSection->readString( "reflectionTexture", "system/maps/cloudyhillscubemap2.dds" );

	config_.deepColour_ = pSection->readVector4( "deepColour", Vector4(0.f,0.20f,0.33f,1.f) );

	config_.depth_ = pSection->readFloat( "depth", 10.f );
	config_.fadeDepth_ = pSection->readFloat( "fadeDepth", 0.f );

	config_.foamIntersection_ = pSection->readFloat( "foamIntersection", 0.25f );
	config_.foamMultiplier_ = pSection->readFloat( "foamMultiplier", 0.75f );
	config_.foamTiling_ = pSection->readFloat( "foamTiling", 1.f );	

	config_.useEdgeAlpha_ = pSection->readBool( "useEdgeAlpha", true );
	
	config_.useCubeMap_ = pSection->readBool( "useCubeMap", false );	
	
	config_.useSimulation_ = pSection->readBool( "useSimulation", true );

	config_.visibility_ = pSection->readInt( "visibility", Water::ALWAYS_VISIBLE );
	if ( config_.visibility_ != Water::ALWAYS_VISIBLE &&
		config_.visibility_ != Water::INSIDE_ONLY &&
		config_.visibility_ != Water::OUTSIDE_ONLY )
	{
		config_.visibility_ = Water::ALWAYS_VISIBLE;
	}


	config_.transparencyTable_ = pChunk->mapping()->path() + uid_ + ".odata";

	pChunk_ = pChunk;
	return true;
}
#if UMBRA_ENABLE
class ChunkMirror : public ReferenceCount
{
public:
	ChunkMirror(const std::vector<Vector3>& vertices, const std::vector<uint32>& triangles, Chunk* pChunk) :
		pChunk_(pChunk),
		portal_(NULL)
	{
		BW_GUARD;
		Umbra::MeshModel* model = Umbra::MeshModel::create((Umbra::Vector3*)&vertices[0], (Umbra::Vector3i*)&triangles[0], vertices.size(), triangles.size()/3);
		model->autoRelease();
		model->set(Umbra::Model::BACKFACE_CULLABLE, true);

		Umbra::VirtualPortal* umbraPortalA = Umbra::VirtualPortal::create(model, NULL);
		Umbra::VirtualPortal* umbraPortalB = Umbra::VirtualPortal::create(model, umbraPortalA);

		umbraPortalA->setCell(pChunk->getUmbraCell());
		umbraPortalB->setCell(pChunk->getUmbraCell());

		umbraPortalA->setTargetPortal(umbraPortalB);
		umbraPortalB->set(Umbra::Object::ENABLED, false);

		umbraPortalA->set(Umbra::Object::INFORM_PORTAL_ENTER, true);
		umbraPortalA->set(Umbra::Object::INFORM_PORTAL_EXIT, true);

		umbraPortalA->setStencilModel(model);
		umbraPortalA->set(Umbra::Object::FLOATING_PORTAL, true);

		portal_ = new UmbraPortal(vertices, triangles, pChunk_);
		portal_->reflectionPortal_ = true;

		umbraPortalA->setUserPointer(portal_);

		Vector3 p = vertices[triangles[0]];
		Vector3 da = (vertices[triangles[1]]-vertices[triangles[0]]);
		Vector3 db = (vertices[triangles[2]]-vertices[triangles[0]]);
		Vector3 normal;
		normal.crossProduct(da, db);

		da.normalise();
		db.normalise();
		normal.normalise();

		Matrix warp;
		warp.setIdentity();
		warp.setTranslate(p);
		warp[0] = da;
		warp[1] = db;
		warp[2] = normal;

		umbraPortalA->setWarpMatrix((Umbra::Matrix4x4&)warp);
		
		warp[2] = -warp[2];
		umbraPortalB->setWarpMatrix((Umbra::Matrix4x4&)warp);
		
		umbraPortalA_ = UmbraObjectProxy::get( umbraPortalA );
		umbraPortalB_ = UmbraObjectProxy::get( umbraPortalB );
	}

	~ChunkMirror()
	{
		BW_GUARD;
		umbraPortalA_ = NULL;
		umbraPortalB_ = NULL;
		if (portal_)
			delete portal_;
	}

private:

	Chunk* pChunk_;
	UmbraObjectProxyPtr	umbraPortalA_;
	UmbraObjectProxyPtr	umbraPortalB_;
	UmbraPortal*		portal_;
};
#endif

void ChunkWater::syncInit(ChunkVLO* pVLO)
{
	BW_GUARD;
#if UMBRA_ENABLE
	if (pVLO && pVLO->chunk())
	{
		// create umbra mirror portal 

		Vector2 xy = config_.size_ * 0.5f;

		std::vector<Vector3> v(4);

		v[0].set( -xy.x, 0.f, -xy.y );
		v[1].set( -xy.x, 0.f,  xy.y );
		v[2].set(  xy.x, 0.f,  xy.y );
		v[3].set(  xy.x, 0.f, -xy.y );

		Matrix m;
		m.setRotateY( config_.orientation_ );
		m.postTranslateBy( config_.position_ );

		for (int i = 0; i < 4; i++)
			v[i] = m.applyPoint(v[i]);

		std::vector<uint32> tris(3*2);
		tris[0] = 0;
		tris[1] = 1;
		tris[2] = 2;

		tris[3] = 0;
		tris[4] = 2;	
		tris[5] = 3;

		mirrorA_ = new ChunkMirror(v, tris, pVLO->chunk() );


		tris[0] = 0;
		tris[1] = 2;
		tris[2] = 1;

		tris[3] = 0;
		tris[4] = 3;	
		tris[5] = 2;

		mirrorB_ = new ChunkMirror(v, tris, pVLO->chunk());
	}
	else
	{
		mirrorA_ = NULL;
		mirrorB_ = NULL;
	}
#endif
}

BoundingBox ChunkWater::chunkBB( Chunk* pChunk )
{
	BW_GUARD;
	BoundingBox bb = BoundingBox::s_insideOut_;
	BoundingBox cbb = pChunk->boundingBox();

	Vector3 size( config_.size_.x * 0.5f, 0, config_.size_.y * 0.5f );
	BoundingBox wbb( -size, size );

	Matrix m;
	m.setRotateY( config_.orientation_ );
	m.postTranslateBy( config_.position_ );

	wbb.transformBy( m );
    
	if (wbb.intersects( cbb ))
	{
		bb.setBounds( 
			Vector3(	std::max( wbb.minBounds().x, cbb.minBounds().x ),
						std::max( wbb.minBounds().y, cbb.minBounds().y ),
						std::max( wbb.minBounds().z, cbb.minBounds().z ) ),
			Vector3(	std::min( wbb.maxBounds().x, cbb.maxBounds().x ),
						std::min( wbb.maxBounds().y, cbb.maxBounds().y ),
						std::min( wbb.maxBounds().z, cbb.maxBounds().z ) ) );
		bb.transformBy( pChunk->transformInverse() );
	}

	return bb;
}


bool ChunkWater::addYBounds( BoundingBox& bb ) const
{
	BW_GUARD;
	bb.addYBounds( config_.position_.y );

	return true;
}

/**
 *	Draw (and update) this body of water
 */
void ChunkWater::draw( ChunkSpace* pSpace )
{
	BW_GUARD;
	static DogWatch drawWatch( "ChunkWater" );
	ScopedDogWatch watcher( drawWatch );

	// create the water if this is the first time
	if (pWater_ == NULL)
	{
#ifdef EDITOR_ENABLED
		//if (pChunk_ == NULL) return;
		
		//ChunkSpace * pSpace = pChunk_->space();

		// but make sure there's some (focussed, outside) chunks
		//  in all of the columns that we intersect, so there's
		//  some terrain to collide with.
		//for (int i = 0; i < 4; i++)
		//{
		//	Vector3 corner;
		//	corner.x = config_.position_.x + ((i & 1) ? config_.size_.x*0.5f : -config_.size_.x*0.5f);
		//	corner.y = config_.position_.y;
		//	corner.z = config_.position_.z + ((i & 2) ? config_.size_.y*0.5f : -config_.size_.y*0.5f);

		//	ChunkSpace::Column * pCol = pSpace->column( corner, false );
		//	if (pCol == NULL || pCol->pOutsideChunk() == NULL) return;
		//}

#endif //EDITOR_ENABLED

		pWater_ = new Water( config_, new ChunkRompTerrainCollider( /*pSpace*/ ) );	// TODO: Uncomment pSpace!

		this->objectCreated();
	}
	else if ( shouldRebuild() )
	{
		pWater_->rebuild( config_ );
		shouldRebuild(false);
		this->objectCreated();
	}

	// and remember to draw it after the rest of the solid scene
	if ( !s_simpleDraw )
	{
		Waters::addToDrawList( pWater_ );
	}
	//RA: turned off simple draw for now....TODO: FIX
}

#if UMBRA_ENABLE
/**
 * Using the lending system + the vlo system to get all the terrain chunk items
 * intersecting the body of water. (only used in the UMBRA calculations)
 */
void ChunkWater::lend( Chunk * pChunk )
{
	BW_GUARD;
	if (!pWater_)
	{
		pWater_ = new Water( config_, new ChunkRompTerrainCollider( /*pSpace*/ ) );	// TODO: Uncomment pSpace!
		this->objectCreated();
	}

	if (pChunk != NULL)
	{
		//find the terrain block
		ChunkTerrain * pChunkTerrain =
			ChunkTerrainCache::instance( *pChunk ).pTerrain();

		if (pChunkTerrain != NULL)
			pWater_->addTerrainItem( pChunkTerrain );
	}
}


/**
 * Called when a vlo reference object gets tossed out
 */
void ChunkWater::unlend( Chunk * pChunk )
{
	BW_GUARD;
	if (pWater_)
	{
		if (pChunk != NULL)
		{
			//find the terrain block
			ChunkTerrain * pChunkTerrain =
				ChunkTerrainCache::instance( *pChunk ).pTerrain();

			if (pChunkTerrain != NULL)
				pWater_->eraseTerrainItem( pChunkTerrain );
		}
	}
}

#endif // UMBRA_ENABLE


/**
 *	Apply a disturbance to this body of water
 */
void ChunkWater::sway( const Vector3 & src, const Vector3 & dst, const float diameter )
{
	BW_GUARD;
	if (pWater_ != NULL)
	{
		pWater_->addMovement( src, dst, diameter );
	}
}


#ifdef EDITOR_ENABLED
/**
 *	This method regenerates the water ... later
 */
void ChunkWater::dirty()
{
	BW_GUARD;
	if (pWater_ != NULL)
		shouldRebuild(true);
}
#endif //EDITOR_ENABLED

/**
 *	This static method creates a body of water from the input section and adds
 *	it to the given chunk.
 */
bool ChunkWater::create( Chunk * pChunk, DataSectionPtr pSection, std::string uid )
{
	BW_GUARD;
	//TODO: check it isnt already created?
	ChunkWater * pItem = new ChunkWater( uid );	
	if (pItem->load( pSection, pChunk ))
		return true;
	delete pItem;
	return false;
}


void ChunkWater::simpleDraw( bool state )
{
	ChunkWater::s_simpleDraw = state;
}


/// Static factory initialiser
VLOFactory ChunkWater::factory_( "water", 0, ChunkWater::create );
/// This variable is set to true if we would like to draw cheaply
/// ( e.g. during picture-in-picture )
bool ChunkWater::s_simpleDraw = false;
//
//
//bool ChunkWater::oldCreate( Chunk * pChunk, DataSectionPtr pSection )
//{
//	bool converted = pSection->readBool("deprecated",false);
//	if (converted)
//		return false;
//
//	//TODO: generalise the want flags...
//	ChunkVLO * pVLO = new ChunkVLO( 5 );	
//	if (!pVLO->legacyLoad( pSection, pChunk, std::string("water") ))
//	{
//		delete pVLO;
//		return false;
//	}
//	else
//	{
//		pChunk->addStaticItem( pVLO );
//		return true;
//	}
//	return false;
//}
//
///// Static factory initialiser
//ChunkItemFactory ChunkWater::oldWaterFactory_( "water", 0, ChunkWater::oldCreate );

// chunk_water.cpp
