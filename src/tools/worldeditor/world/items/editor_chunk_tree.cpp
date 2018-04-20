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
#include "worldeditor/world/items/editor_chunk_tree.hpp"
#include "worldeditor/project/project_module.hpp"
#include "worldeditor/world/editor_chunk.hpp"
#include "worldeditor/world/world_manager.hpp"
#include "worldeditor/world/static_lighting.hpp"
#include "worldeditor/editor/item_properties.hpp"
#include "worldeditor/editor/item_editor.hpp"
#include "worldeditor/misc/cvswrapper.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "appmgr/options.hpp"
#include "appmgr/module_manager.hpp"
#include "common/tools_common.hpp"
#include "speedtree/speedtree_config.hpp"
#include "speedtree/speedtree_collision.hpp"
#include "speedtree/billboard_optimiser.hpp"
#include "romp/fog_controller.hpp"
#include "romp/static_light_values.hpp"
#include "romp/static_light_fashion.hpp"
#include "model/super_model.hpp"
#include "physics2/bsp.hpp"
#include "resmgr/string_provider.hpp"
#include "resmgr/bin_section.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/xml_section.hpp"
#include "resmgr/auto_config.hpp"
#include "cstdmf/debug.hpp"


DECLARE_DEBUG_COMPONENT2( "Chunk", 0 )


static AutoConfigString s_notFoundModel( "system/notFoundModel" );	
static std::auto_ptr<SuperModel> s_missingTreeModel;

// -----------------------------------------------------------------------------
// Section: EditorChunkTree
// -----------------------------------------------------------------------------
uint32 EditorChunkTree::s_settingsMark_ = -16;

/**
 *	Constructor.
 */
EditorChunkTree::EditorChunkTree() :
	hasPostLoaded_( false ),
	castsShadow_( true ),
	firstToss_( true ),
	bspBB_( BoundingBox::s_insideOut_ )
{
}


/**
 *	Destructor.
 */
EditorChunkTree::~EditorChunkTree()
{
}


/**
 *	Free the allocated statics
 */
void EditorChunkTree::fini()
{
	if (s_missingTreeModel.get())
	{
		s_missingTreeModel.reset( NULL );
	}
}

/**
 *	overridden edShouldDraw method
 */
bool EditorChunkTree::edShouldDraw()
{
	if( !ChunkTree::edShouldDraw() )
		return false;
	static int renderScenery = 1;
	if (Moo::rc().frameTimestamp() != s_settingsMark_)
	{
		renderScenery = Options::getOptionInt( "render/scenery", 1 );
		s_settingsMark_ = Moo::rc().frameTimestamp();
	}
	return renderScenery != 0;
}

/**
 *	overridden draw method
 */
void EditorChunkTree::draw()
{
	uint32 currentFrame = s_settingsMark_;

	if( !edShouldDraw() )
		return;

	if (!hasPostLoaded_)
	{
		edPostLoad();
		hasPostLoaded_ = true;
	}

	int drawBspFlag = WorldManager::instance().drawBSP();
	bool projectModule = ProjectModule::currentInstance() == ModuleManager::instance().currentModule();
	bool drawBsp = drawBspFlag != 0 && !projectModule;

	// Load up the bsp tree if needed
	if ((drawBsp || WorldManager::instance().drawSelection() ) && verts_.size() == 0 )
	{
		// no vertices loaded yet, create some
		const BSPTree * tree = this->bspTree();

		if (tree)
		{
			Moo::Colour colour((float)rand() / (float)RAND_MAX,
								(float)rand() / (float)RAND_MAX,
								(float)rand() / (float)RAND_MAX,
								1.f);
			Moo::BSPTreeHelper::createVertexList( *tree, verts_, colour);
		}
	}

	if ((drawBsp || WorldManager::instance().drawSelection() ) && verts_.size() > 0 )
	{
		//set the transforms
		Matrix transform;
		transform.multiply(edTransform(), chunk()->transform());
		Moo::rc().device()->SetTransform( D3DTS_WORLD, &transform );
		Moo::rc().device()->SetTransform( D3DTS_VIEW, &Moo::rc().view() );
		Moo::rc().device()->SetTransform( D3DTS_PROJECTION, &Moo::rc().projection() );

		Moo::rc().setPixelShader( NULL );
		Moo::rc().setVertexShader( NULL );
		Moo::rc().setFVF( Moo::VertexXYZL::fvf() );
		Moo::rc().setRenderState( D3DRS_ALPHATESTENABLE, FALSE );
		Moo::rc().setRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
		Moo::rc().setRenderState( D3DRS_LIGHTING, FALSE );
		Moo::rc().setRenderState( D3DRS_ZWRITEENABLE, TRUE );
		Moo::rc().setRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
		Moo::rc().setRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
		Moo::rc().fogEnabled( false );

		if (WorldManager::instance().drawSelection())
		{
			Moo::rc().setTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			Moo::rc().setTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
			Moo::rc().setTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			Moo::rc().setTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR );

			WorldManager::instance().registerDrawSelectionItem( this );
		}
		else
		{
			Moo::rc().setTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			Moo::rc().setTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
			Moo::rc().setTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		}
		Moo::rc().setTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		Moo::rc().setTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );

		Moo::rc().drawPrimitiveUP( D3DPT_TRIANGLELIST, verts_.size() / 3, &verts_[0], sizeof( Moo::VertexXYZL ) );
	}
	else
	{
		if (!this->ChunkTree::loadFailed())
		{
			static int renderMiscShadeReadOnlyAreas = 1;
			if (Moo::rc().frameTimestamp() != currentFrame)
			{
				renderMiscShadeReadOnlyAreas =
					Options::getOptionInt( "render/misc/shadeReadOnlyAreas", 1 );
			}
			bool drawRed = !EditorChunkCache::instance( *pChunk_ ).edIsWriteable() && 
				renderMiscShadeReadOnlyAreas != 0;
				
			if( drawRed && !projectModule )
			{
				WorldManager::instance().setReadOnlyFog();
			}
			else
			{
				FogController::instance().commitFogToDevice();
			}
			this->ChunkTree::draw();
			
			if( drawRed && !projectModule )
			{
				FogController::instance().commitFogToDevice();
			}
		}
		else 
		{
			// draw missing model
			if (s_missingTreeModel.get() != 0 &&
				s_missingTreeModel->nModels() > 0)
			{
				Moo::rc().push();
				Moo::rc().preMultiply(edTransform());
				s_missingTreeModel->draw();
				Moo::rc().pop();
			}
		}			
	}
}

struct ErrorCallback
{
	static void printError(
		const std::string & fileName,
		const std::string & errorMsg)
	{
		std::string msg = fileName + ":" + errorMsg;
		WorldManager::instance().addError(
			chunk, self, "%s", msg.substr(0, 255).c_str());
	}
	
	static Chunk           * chunk;
	static EditorChunkTree * self;
};

Chunk * ErrorCallback::chunk = NULL;
EditorChunkTree * ErrorCallback::self = NULL;

/**
 *	This method saves the data section pointer before calling its
 *	base class's load method
 */
bool EditorChunkTree::load( DataSectionPtr pSection, Chunk * pChunk )
{
	ErrorCallback::chunk = pChunk;
	ErrorCallback::self  = this;
	speedtree::setErrorCallback(&ErrorCallback::printError);

	edCommonLoad( pSection );

	castsShadow_ = pSection->readBool( "editorOnly/castsShadow", true );

	this->pOwnSect_ = pSection;	
	if (this->ChunkTree::load(pSection, pChunk))
	{
		this->desc_ = BWResource::getFilename(this->ChunkTree::filename());
		this->desc_ = BWResource::removeExtension(this->desc_);
		this->hasPostLoaded_ = false;	
	}
	else
	{
		this->desc_ = BWResource::getFilename(pSection->readString("spt"));
		this->desc_ = BWResource::removeExtension(this->desc_);

		if (s_missingTreeModel.get() == 0)
		{
			// load missing model (static)
			std::vector<std::string> modelName(1, s_notFoundModel.value());
			s_missingTreeModel.reset(new SuperModel(modelName));
		}
		
		// set BSP for missing model
		if (s_missingTreeModel->nModels() > 0)
		{
			Model * model = s_missingTreeModel->curModel(0);
			const BSPTree * bsp = model->decompose();
			this->BaseChunkTree::setBoundingBox(model->boundingBox());
			this->BaseChunkTree::setBSPTree(bsp);

			static const Moo::Colour colour(0.0f, 1.0f, 0.0f, 1.f);
			Moo::BSPTreeHelper::createVertexList( *bsp, verts_, colour);
		}
		
		std::string msg = L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_TREE/ERROR_LOADING_TREE",
			this->ChunkTree::lastError() );
		WorldManager::instance().addError(
			pChunk, this, "%s",  
			msg.substr(0, 255).c_str());
	
		this->hasPostLoaded_ = true;
	}
	return true;
}

void EditorChunkTree::edPostLoad()
{
}

/**
 * We just loaded up with srcItems lighting data, create some new stuff of our
 * own
 */
void EditorChunkTree::edPostClone( EditorChunkItem* srcItem )
{
	StaticLighting::markChunk( pChunk_ );

	BoundingBox bb = BoundingBox::s_insideOut_;
	edBounds( bb );
	bb.transformBy( edTransform() );
	bb.transformBy( chunk()->transform() );
	WorldManager::instance().markTerrainShadowsDirty( bb );
	this->syncInit();
}

void EditorChunkTree::edPostCreate()
{
	StaticLighting::markChunk( pChunk_ );

	BoundingBox bb = BoundingBox::s_insideOut_;
	edBounds( bb );
	bb.transformBy( edTransform() );
	bb.transformBy( chunk()->transform() );
	WorldManager::instance().markTerrainShadowsDirty( bb );
	this->syncInit();
}

/**
 *	This method does extra stuff when this item is tossed between chunks.
 *
 *	It updates its datasection in that chunk.
 */
void EditorChunkTree::toss( Chunk * pChunk )
{
	if (pChunk_ != NULL && pOwnSect_)
	{
		EditorChunkCache::instance( *pChunk_ ).
			pChunkSection()->delChild( pOwnSect_ );
		pOwnSect_ = NULL;
	}

	this->ChunkTree::toss( pChunk );

	if (pChunk_ != NULL && !pOwnSect_)
	{
		pOwnSect_ = EditorChunkCache::instance( *pChunk_ ).
			pChunkSection()->newSection( "speedtree" );
		this->edSave( pOwnSect_ );
	}

	if (firstToss_)
	{
		// check lighting files are up to date (can't do this on load as chunk() is NULL)
		StaticLighting::markChunk( pChunk_ );

		firstToss_ = false;
	}

	// If we havn't got our static lighting calculated yet, mark the new
	// chuck as dirty. This will only be the case for newly created items.
	// Marking a chunk as dirty when moving chunks around is taken care of 
	// in edTransform()
	if (pChunk && !pChunk->isOutsideChunk() )
		StaticLighting::markChunk( pChunk );
}


/**
 *	Save to the given section
 */
bool EditorChunkTree::edSave( DataSectionPtr pSection )
{
	if (!edCommonSave( pSection ))
		return false;

	pSection->writeString( "spt", this->ChunkTree::filename() );
	pSection->writeInt( "seed", this->ChunkTree::seed() );
	pSection->writeMatrix34( "transform", this->transform() );

	pSection->writeBool( "reflectionVisible", reflectionVisible_ );

	pSection->writeBool( "editorOnly/castsShadow", castsShadow_ );

	return true;
}

/**
 *	Called when our containing chunk is saved
 */
void EditorChunkTree::edChunkSave()
{
}

/**
 *	Called when our containing chunk is saved; save the lighting info
 */
void EditorChunkTree::edChunkSaveCData(DataSectionPtr cData)
{
}

/**
 *	This method sets this item's transform for the editor
 *	It takes care of moving it into the right chunk and recreating the
 *	collision scene and all that
 */
bool EditorChunkTree::edTransform( const Matrix & m, bool transient )
{
	// it's permanent, so find out where we belong now
	BoundingBox lbb( Vector3(0.f,0.f,0.f), Vector3(1.f,1.f,1.f) );
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyPoint(
		(lbb.minBounds() + lbb.maxBounds()) * 0.5f ) );
	if (pNewChunk == NULL) return false;

	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		this->setTransform( m );
		this->syncInit();
		return true;
	}

	// make sure the chunks aren't readonly
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() 
		|| !EditorChunkCache::instance( *pNewChunk ).edIsWriteable())
		return false;


	// Calc the old world space BB, so we can update the terrain shadows
	BoundingBox oldBB = BoundingBox::s_insideOut_;
	edBounds( oldBB );
	oldBB.transformBy( edTransform() );
	oldBB.transformBy( chunk()->transform() );

	// ok, accept the transform change then
	//transform_ = m;
	Matrix transform = this->transform();
	transform.multiply( m, pOldChunk->transform() );
	transform.postMultiply( pNewChunk->transformInverse() );
	this->setTransform( transform );

	// Calc the new world space BB, so we can update the terrain shadows
	BoundingBox newBB = BoundingBox::s_insideOut_;
	edBounds( newBB );
	newBB.transformBy( edTransform() );
	newBB.transformBy( pNewChunk->transform() );

	// note that both affected chunks have seen changes
	WorldManager::instance().changedChunk( pOldChunk );
	WorldManager::instance().changedChunk( pNewChunk );

	WorldManager::instance().markTerrainShadowsDirty( oldBB );
	WorldManager::instance().markTerrainShadowsDirty( newBB );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );

	// Recalculate static lighting in the old and new chunks
	StaticLighting::markChunk( pNewChunk );
	StaticLighting::markChunk( pOldChunk );

	if (pOldChunk != pNewChunk )
	{
		edSave( pOwnSect() );
	}
	this->syncInit();
	return true;
}


void EditorChunkTree::edPreDelete()
{
	StaticLighting::markChunk( pChunk_ );

	BoundingBox bb = BoundingBox::s_insideOut_;
	edBounds( bb );
	bb.transformBy( edTransform() );
	bb.transformBy( chunk()->transform() );
	WorldManager::instance().markTerrainShadowsDirty( bb );
	EditorChunkItem::edPreDelete();
}


/**
 *	Get the bounding box
 */
void EditorChunkTree::edBounds( BoundingBox& bbRet ) const
{
	bbRet = this->boundingBox();
}


/**                                                                             
 *  Get the bounding box used for showing the selection                         
 */                                                                             
void EditorChunkTree::edSelectedBox( BoundingBox& bbRet ) const                 
{                                                                               
    if ( bspTree() && bspTree()->size() > 0 &&                                  
        Options::getOptionInt( "bspBoundingBox", 1 ) )                          
    {                                                                           
        if ( bspBB_ == BoundingBox::s_insideOut_ )                              
        {                                                                       
            // bsp bounding box uninitialised, so initialise.                   
            const RealWTriangleSet& tris = bspTree()->triangles();              
            for ( RealWTriangleSet::const_iterator i = tris.begin();            
                i != tris.end(); ++i )                                          
            {                                                                   
                bspBB_.addBounds( (*i).v0() );                                  
                bspBB_.addBounds( (*i).v1() );                                  
                bspBB_.addBounds( (*i).v2() );                                  
            }                                                                   
        }                                                                       
        bbRet = bspBB_;                                                         
    }                                                                           
    else                                                                        
    {                                                                           
        this->edBounds( bbRet );                                                
    }                                                                           
}                                                                               


/**
 *	This method returns whether or not this tree should cast a shadow.
 *
 *  @return		Returns whether or not this tree should cast a shadow
 */
bool EditorChunkTree::edAffectShadow() const
{
	return castsShadow_;
}


/**
 *	This method adds this item's properties to the given editor
 */
bool EditorChunkTree::edEdit( ChunkItemEditor & editor )
{
	MatrixProxy * pMP = new ChunkItemMatrix( this );
	editor.addProperty( new ChunkItemPositionProperty(
		L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_TREE/POSITION"), pMP, this ) );
	editor.addProperty( new GenRotationProperty(
		L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_TREE/ROTATION"), pMP ) );
	editor.addProperty( new GenScaleProperty(
		L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_TREE/SCALE"), pMP ) );

	// can affect shadow?
	editor.addProperty( new GenBoolProperty(
		L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_TREE/CASTS_SHADOW"),
		new AccessorDataProxy< EditorChunkTree, BoolProxy >(
			this, "castsShadow", 
			&EditorChunkTree::getCastsShadow, 
			&EditorChunkTree::setCastsShadow ) ) );

	StaticTextProperty * pProp = new StaticTextProperty(
		L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_TREE/FILENAME"),
		new ConstantDataProxy< StringProxy >( this->ChunkTree::filename() ) );
	editor.addProperty(pProp);
		
	std::stringstream seed;
	seed << EditorChunkTree::getSeed() <<
		L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_TREE/CHANGE_IN_SPT_CAD");
	editor.addProperty( new StaticTextProperty(
		"seed", new ConstantDataProxy<StringProxy>(seed.str()) ) );

	editor.addProperty( new GenBoolProperty(
		L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_TREE/REFLECTION_VISIBLE"),
		new AccessorDataProxy< EditorChunkTree, BoolProxy >(
			this, "reflectionVisible",
			&EditorChunkTree::getReflectionVis,
			&EditorChunkTree::setReflectionVis ) ) );

	return true;
}


/**
 *	Find the drop chunk for this item
 */
Chunk * EditorChunkTree::edDropChunk( const Vector3 & lpos )
{
	Vector3 npos = pChunk_->transform().applyPoint( lpos );

	Chunk * pNewChunk = NULL;

	pNewChunk = pChunk_->space()->findChunkFromPoint( npos );

	if (pNewChunk == NULL)
	{
		ERROR_MSG( "Cannot move %s to (%f,%f,%f) "
			"because it is not in any loaded chunk!\n",
			this->edDescription().c_str(), npos.x, npos.y, npos.z );
		return NULL;
	}

	return pNewChunk;
}

std::string EditorChunkTree::edDescription()
{
	return desc_;
}

Vector3 EditorChunkTree::edMovementDeltaSnaps()
{
	return EditorChunkItem::edMovementDeltaSnaps();
}

float EditorChunkTree::edAngleSnaps()
{
	return EditorChunkItem::edAngleSnaps();
}

unsigned long EditorChunkTree::getSeed() const
{
	return this->ChunkTree::seed();
}

bool EditorChunkTree::setSeed( const unsigned long & seed )
{
	bool success = this->ChunkTree::seed(seed);
	if (!success)
	{
		std::string msg = L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_TREE/COULD_NOT_CHANGE_TREE_SEED",
			this->ChunkTree::lastError() );
		WorldManager::instance().addCommentaryMsg(msg.substr(0,255));
	}	
	return success;
}

bool EditorChunkTree::getCastsShadow() const
{
	return castsShadow_;
}

bool EditorChunkTree::setCastsShadow( const bool & castsShadow )
{
	if ( castsShadow_ != castsShadow )
	{
		castsShadow_ = castsShadow;

		MF_ASSERT( pChunk_ != NULL );

		WorldManager::instance().changedChunk( pChunk_ );

		BoundingBox bb = BoundingBox::s_insideOut_;
		edBounds( bb );
		bb.transformBy( edTransform() );
		bb.transformBy( chunk()->transform() );
		WorldManager::instance().markTerrainShadowsDirty( bb );
		if (!pChunk_->isOutsideChunk())
		{
			StaticLighting::markChunk( pChunk_ );
		}
	}	
	return true;
}

/// Write the factory statics stuff
/// Write the factory statics stuff
#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk)
IMPLEMENT_CHUNK_ITEM( EditorChunkTree, speedtree, 1 )
