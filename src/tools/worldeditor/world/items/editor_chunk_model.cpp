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
#include "worldeditor/world/items/editor_chunk_model.hpp"
#include "worldeditor/world/editor_chunk.hpp"
#include "worldeditor/world/world_manager.hpp"
#include "worldeditor/world/static_lighting.hpp"
#include "worldeditor/editor/item_properties.hpp"
#include "worldeditor/editor/item_editor.hpp"
#include "worldeditor/project/project_module.hpp"
#include "worldeditor/misc/cvswrapper.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "chunk/chunk_model_obstacle.hpp"
#include "resmgr/bin_section.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/xml_section.hpp"
#include "resmgr/auto_config.hpp"
#include "resmgr/string_provider.hpp"
#include "common/material_utility.hpp"
#include "common/material_properties.hpp"
#include "common/material_editor.hpp"
#include "common/dxenum.hpp"
#include "common/tools_common.hpp"
#include "model/matter.hpp"
#include "model/super_model.hpp"
#include "model/super_model_animation.hpp"
#include "model/super_model_dye.hpp"
#include "model/tint.hpp"
#include "romp/static_light_values.hpp"
#include "romp/static_light_fashion.hpp"
#include "romp/fog_controller.hpp"
#include "physics2/material_kinds.hpp"
#include "physics2/bsp.hpp"
#include "moo/visual_manager.hpp"
#include "moo/vertex_formats.hpp"
#include "moo/visual_compound.hpp"
#include "appmgr/options.hpp"
#include "appmgr/module_manager.hpp"
#include "math/colour.hpp"
#include "cstdmf/debug.hpp"
#if UMBRA_ENABLE
#include <umbraModel.hpp>
#include <umbraObject.hpp>
#include "chunk/chunk_umbra.hpp"
#endif
DECLARE_DEBUG_COMPONENT2( "Chunk", 0 )


static AutoConfigString s_notFoundModel( "system/notFoundModel" );	
StringHashMap<int> EditorChunkModel::s_materialKinds_;


std::map<std::string, std::set<EditorChunkModel*> > EditorChunkModel::editorChunkModels_;


void EditorChunkModel::add( EditorChunkModel* model, const std::string& filename )
{
	editorChunkModels_[ filename ].insert( model );
}


void EditorChunkModel::remove( EditorChunkModel* model )
{
	for( std::map<std::string, std::set<EditorChunkModel*> >::iterator iter =
		editorChunkModels_.begin(); iter != editorChunkModels_.end(); ++iter )
	{
		std::set<EditorChunkModel*>& modelSet = iter->second;
		std::set<EditorChunkModel*>::iterator siter = modelSet.find( model );
		if (siter != modelSet.end())
		{
			modelSet.erase( siter );
			if (modelSet.size())
			{
				editorChunkModels_.erase( iter );
			}
			return;
		}
	}
}


void EditorChunkModel::reload( const std::string& filename )
{
	BWResource::instance().purgeAll();
	std::set<EditorChunkModel*> modelSet =
		editorChunkModels_[ BWResource::dissolveFilename( filename ) ];
	std::string sectionName;
	std::vector<DataSectionPtr> sections;
	std::vector<ChunkPtr> chunks;

	if( !modelSet.empty() )
		sectionName = (*modelSet.begin())->sectionName();
	for( std::set<EditorChunkModel*>::iterator iter = modelSet.begin();
		iter != modelSet.end(); ++iter )
	{
		chunks.push_back( (*iter)->chunk() );
		(*iter)->toss( NULL );
		DataSectionPtr section = new XMLSection( sectionName );
		(*iter)->edSave( section );
		(*iter)->clean();
		sections.push_back( section );
	}
	std::vector<DataSectionPtr>::iterator sec_iter = sections.begin();
	std::vector<ChunkPtr>::iterator chunk_iter = chunks.begin();
	for( std::set<EditorChunkModel*>::iterator iter = modelSet.begin();
		iter != modelSet.end(); ++iter, ++sec_iter, ++chunk_iter )
	{
		// Must make the section a child of the chunk section to set the
		// pOwnSect_ to the correct state.
		DataSectionPtr ownSect;
		if( *chunk_iter )
		{
			ownSect = EditorChunkCache::instance( **chunk_iter ).
				pChunkSection()->newSection( sectionName );
			ownSect->copySections( *sec_iter );
		}
		else
		{
			ownSect = *sec_iter;
		}

		(*iter)->load( ownSect, (*iter)->chunk() );
		(*iter)->toss( *chunk_iter );

		if( !*chunk_iter )
			(*iter)->pOwnSect_ = NULL;

		if( (*iter)->chunk() )
		{
			if( (*iter)->chunk()->isOutsideChunk() )
				WorldManager::instance().markTerrainShadowsDirty( (*iter)->chunk() );
			else
				WorldManager::instance().dirtyLighting( (*iter)->chunk() );
			//TODO : either dirty shadows or dirty lighting means the thumbnail
			//is dirty.  Those methods in WorldEditor should internally then set
			//the thumbnail to dirty.  Should then be able to remove this.
			WorldManager::instance().dirtyThumbnail( (*iter)->chunk() );
		}
	}
}

void EditorChunkModel::clean()
{
	delete pSuperModel_;
	pSuperModel_ = NULL;
	pStaticLightFashion_ = NULL;
	isModelNodeless_ = true;
	firstToss_ = true;
	primGroupCount_ = 0;
	customBsp_ = false;
	standinModel_ = false;
	originalSect_ = NULL;
	outsideOnly_ = false;
	castsShadow_ = true;
	desc_.clear();
	animationNames_.clear();
	dyeTints_.clear();
	tintName_.clear();
	changedMaterials_.clear();

	pAnimation_ = NULL;
	tintMap_.clear();
	materialOverride_.clear();
	fv_.clear();
	label_.clear();
}

// -----------------------------------------------------------------------------
// Section: EditorChunkModel
// -----------------------------------------------------------------------------
uint32 EditorChunkModel::s_settingsMark_ = -16;
/**
 *	Constructor.
 */
EditorChunkModel::EditorChunkModel()
	: isModelNodeless_( true )
	, firstToss_( true )
	, primGroupCount_( 0 )
	, customBsp_( false )
	, standinModel_( false )
	, originalSect_( NULL )
	, outsideOnly_( false )
	, castsShadow_( true )
	, dxEnum_( s_dxenumPath )
	, pEditorModel_( NULL )
{
}


/**
 *	Destructor.
 */
EditorChunkModel::~EditorChunkModel()
{
	delete pEditorModel_;
	remove( this );
}

/**
 *	overridden edShouldDraw method
 */
bool EditorChunkModel::edShouldDraw()
{
	if( !ChunkModel::edShouldDraw() )
		return false;
	if( isShellModel() )
		return !Chunk::hideIndoorChunks_;
	static int renderScenery = 1;
	if (Moo::rc().frameTimestamp() != s_settingsMark_)
		renderScenery = Options::getOptionInt( "render/scenery", 1 );
	return renderScenery != 0;
}

/**
 *	overridden draw method
 */
void EditorChunkModel::draw()
{
	if( !edShouldDraw() || (Moo::rc().reflectionScene() && !reflectionVisible_) )
		return;

	static int renderMiscShadeReadOnlyAreas = 1;
	static int renderMisc = 0;
	static int renderLighting = 0;
	if (Moo::rc().frameTimestamp() != s_settingsMark_)
	{
		renderMiscShadeReadOnlyAreas =
			Options::getOptionInt( "render/misc/shadeReadOnlyAreas", 1 );
		renderMisc = Options::getOptionInt("render/misc", 0);
		renderLighting = Options::getOptionInt( "render/lighting", 0 );
		s_settingsMark_ = Moo::rc().frameTimestamp();
	}
	if (!hasPostLoaded_)
	{
		edPostLoad();
		hasPostLoaded_ = true;
	}

	if (pSuperModel_ != NULL )
	{
		Moo::rc().push();
		Moo::rc().preMultiply( transform_ );

		bool drawRed = !EditorChunkCache::instance( *chunk() ).edIsWriteable() && 
			 renderMiscShadeReadOnlyAreas != 0;
		drawRed &= !!renderMisc;
		bool projectModule = ProjectModule::currentInstance() == ModuleManager::instance().currentModule();
		if( drawRed && WorldManager::instance().drawSelection())
			return;
		if (drawRed && !projectModule)
		{
			// Set the fog to a constant red colour
			WorldManager::instance().setReadOnlyFog();
		}

		// Notify the chunk cache of how many render sets we're going to draw
		WorldManager::instance().addPrimGroupCount( chunk(), primGroupCount_ );

		bool ignoreStaticLighting = false;
		if (pStaticLightFashion_)
			ignoreStaticLighting = renderLighting != 0;

		int drawBspFlag = WorldManager::instance().drawBSP();
		bool drawBsp = drawBspFlag == 1 && !projectModule;// || (drawBspFlag == 1 && customBsp_);
		if (WorldManager::instance().drawSelection())
		{
			drawBsp = false;
			WorldManager::instance().registerDrawSelectionItem( this );
		}
		// Load up the bsp tree if needed
		if (drawBsp && verts_.size() == 0 )
		{
			// no vertices loaded yet, create some
			const BSPTree * tree = pSuperModel_->topModel(0)->decompose();

			if (tree)
			{
				Moo::Colour colour((float)rand() / (float)RAND_MAX,
									(float)rand() / (float)RAND_MAX,
									(float)rand() / (float)RAND_MAX,
									1.f);
				Moo::BSPTreeHelper::createVertexList( *tree, verts_, colour);
			}
		}

		if (drawBsp && verts_.size() > 0 )
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

			Moo::rc().setTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			Moo::rc().setTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
			Moo::rc().setTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			Moo::rc().setTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			Moo::rc().setTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );

			Moo::rc().drawPrimitiveUP( D3DPT_TRIANGLELIST, verts_.size() / 3, &verts_[0], sizeof( Moo::VertexXYZL ) );
		}
		else
		{
			bool drawEditorProxy = ( !!Options::getOptionInt("render/misc/drawEditorProxies",0) && !!Options::getOptionInt("render/proxys",0) );
			static bool s_lastDrawEditorProxy = drawEditorProxy;
			if ((drawEditorProxy != s_lastDrawEditorProxy) && (pEditorModel_))
			{
				this->ChunkModel::toss( pChunk_, drawEditorProxy ? pEditorModel_ : NULL );
				s_lastDrawEditorProxy = drawEditorProxy;
			}

			if (ignoreStaticLighting)
			{
				const FashionPtr pFashion = pStaticLightFashion_;
				MF_ASSERT( std::find( fv_.begin(), fv_.end(), pFashion ) != fv_.end() );
				FashionVector nonStaticFV = fv_;
				nonStaticFV.erase( std::find( nonStaticFV.begin(), nonStaticFV.end(), pFashion ) );

				int late = 0;
				for( std::vector<ChunkMaterialPtr>::iterator iter = materialOverride_.begin();
					iter != materialOverride_.end(); ++iter )
				{
					if( changedMaterials_.find( (*iter)->material_->identifier() ) != changedMaterials_.end() )
					{
						nonStaticFV.push_back( *iter );
						++late;
					}
				}

				pSuperModel_->draw( &nonStaticFV, late );
				if (drawEditorProxy && pEditorModel_)
				{
					pEditorModel_->draw();
				}
			}
			else
			{
				std::vector<ChunkMaterialPtr>::size_type size = materialOverride_.size();

				int late = 0;
				for( std::vector<ChunkMaterialPtr>::iterator iter = materialOverride_.begin();
					iter != materialOverride_.end(); ++iter )
				{
					if( changedMaterials_.find( (*iter)->material_->identifier() ) != changedMaterials_.end() )
					{
						fv_.push_back( *iter );
						++late;
					}
				}

				pSuperModel_->draw( &fv_, late );

				if (drawEditorProxy && pEditorModel_)
				{
					pEditorModel_->draw();
				}

				fv_.resize( fv_.size() - late );
			}
		}

		if (drawRed && !projectModule)
		{
			// Reset the fog
			FogController::instance().commitFogToDevice();
		}

		Moo::rc().pop();
	}
}

std::vector<Moo::VisualPtr> EditorChunkModel::extractVisuals()
{
	std::vector<std::string> models;
	pOwnSect_->readStrings( "resource", models );

	std::vector<Moo::VisualPtr> v;
	v.reserve( models.size() );

	for (uint i = 0; i < models.size(); i++)
	{
		DataSectionPtr modelSection = BWResource::openSection( models[i] );
		if (!modelSection)
		{
			WARNING_MSG( "Couldn't read model %s for ChunkModel\n", models.front().c_str() );
			continue;
		}

		std::string visualName = modelSection->readString( "nodelessVisual" );

		if (visualName.empty())
		{
			visualName = modelSection->readString( "nodefullVisual" );
			if (visualName.empty())
			{
				WARNING_MSG( "ChunkModel %s has a model that has no visual\n", models[i].c_str() );
			}			
			continue;
		}

		Moo::VisualPtr visual = Moo::VisualManager::instance()->get(
			visualName + ".static.visual"
			);

		if (!visual)
			visual = Moo::VisualManager::instance()->get( visualName + ".visual" );

		v.push_back( visual );
	}

	return v;
}

std::vector<std::string> EditorChunkModel::extractVisualNames() const
{
	std::vector<std::string> models;
	pOwnSect_->readStrings( "resource", models );

	std::vector<std::string> v;
	v.reserve( models.size() );

	for (uint i = 0; i < models.size(); i++)
	{
		DataSectionPtr modelSection = BWResource::openSection( models[i] );
		if (!modelSection)
		{
			WARNING_MSG( "Couldn't read model %s for ChunkModel\n", models.front().c_str() );
			continue;
		}

		std::string visualName = modelSection->readString( "nodelessVisual" );

		if (visualName.empty())
		{
			visualName = modelSection->readString( "nodefullVisual" );
			if (visualName.empty())
			{
				WARNING_MSG( "ChunkModel %s has a model that has no visual\n", models[i].c_str() );
			}
		}

		std::string fullVisualName = visualName + ".static.visual";

		Moo::VisualPtr visual = Moo::VisualManager::instance()->get(
			fullVisualName
			);

		if (!visual)
		{
			fullVisualName = visualName + ".visual";
			visual = Moo::VisualManager::instance()->get( fullVisualName );
		}

		if (visual)
			v.push_back( fullVisualName );
		else
			v.push_back( "" );
	}

	return v;
}

/**
 *	Used by load to get the names of the animations in a model file.
 */
static void addNames(std::vector<std::string>& sections, DataSectionPtr ds,
	const std::string& name)
{
	std::vector<DataSectionPtr> children;
	ds->openSections( name, children );

	std::set<std::string> names;

	std::vector<std::string>::iterator ni;
	// Build a list of names we already have
	for (ni = sections.begin(); ni != sections.end(); ++ni)
		names.insert( *ni );

	std::vector<DataSectionPtr>::iterator i;
	for (i = children.begin(); i != children.end(); ++i)
	{
		// Only add it if we don't already have a section with the same
		// name
		if (!names.count( (*i)->readString( "name" ) ))
		{
			sections.push_back( (*i)->readString( "name" ) );
		}
	}
}

/**
 *	Used by load to get the names of the dyes and tints in a model file.
 */
static void addDyeTints( std::map<std::string, std::vector<std::string> >& sections, DataSectionPtr ds )
{
	std::vector<DataSectionPtr> dyes;
	ds->openSections( "dye", dyes );

	std::vector<DataSectionPtr>::iterator i;
	for (i = dyes.begin(); i != dyes.end(); ++i)
	{
		if( sections.find( (*i)->readString( "matter" ) ) == sections.end() )
		{
			std::vector<std::string> names;
			names.push_back( L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/DEFAULT_TINT_NAME") );
			addNames( names, (*i), "tint" );
			if( names.size() > 1 )
				sections[ (*i)->readString( "matter" ) ] = names;
		}
	}
}

/**
 *	This method saves the data section pointer before calling its
 *	base class's load method
 */
bool EditorChunkModel::load( DataSectionPtr pSection, Chunk * pChunk )
{
	pStaticLightFashion_ = NULL;
	isModelNodeless_ = true;
	firstToss_ = true;
	primGroupCount_ = 0;
	customBsp_ = false;
	standinModel_ = false;
	originalSect_ = NULL;
	outsideOnly_ = false;
	castsShadow_ = true;
	desc_.clear();
	animationNames_.clear();
	dyeTints_.clear();
	tintName_.clear();
	changedMaterials_.clear();

	remove( this );
	edCommonLoad( pSection );

	pOwnSect_ = pSection;

	std::vector<std::string> models;
	
	pOwnSect_->readStrings( "resource", models );
	if( models.size() )
	{
		add( this, models[0] );
		DataSectionPtr data = BWResource::openSection( models[0] );
		if (data) 
		{
			std::string editorModel = data->readString( "editorModel", "" );
			delete pEditorModel_;
			pEditorModel_ = NULL;
			if (editorModel != "")
			{
				std::vector<std::string> editorModels;
				editorModels.push_back( editorModel );
				pEditorModel_ = new SuperModel( editorModels );
			}
		}
	}

	bool ok = this->ChunkModel::load( pSection, pChunk );
	if (!ok)
	{
		originalSect_ = new XMLSection( sectionName() );
		originalSect_->copy( pSection );

		// load in a replecement model
		DataSectionPtr pTemp = new XMLSection( sectionName() );
		pTemp->writeString( "resource", s_notFoundModel );
		pTemp->writeMatrix34( "transform", pSection->readMatrix34( "transform" ) );
		ok = this->ChunkModel::load( pTemp, pChunk );
		MF_ASSERT( ok );

		standinModel_ = true;

		// tell the user
		std::string mname = pSection->readString( "resource" );
		WorldManager::instance().addError(pChunk, this, "Model not loaded: %s", mname.c_str());

		// make sure static lighting does not get regerenated
		// (it will look at the absent model for this info)
		isModelNodeless_ = true;

		// don't look for visuals
		hasPostLoaded_ = true;
	}
	else
	{
		// for the static lighting
		detectModelType();

		if (pAnimation_)
		{
			animName_ = pSection->readString( "animation/name" );
		}

		tintName_.clear();
		std::vector<DataSectionPtr> dyes;
		pSection->openSections( "dye", dyes );
		for( std::vector<DataSectionPtr>::iterator iter = dyes.begin(); iter != dyes.end(); ++iter )
		{
			std::string dye = (*iter)->readString( "name" );
			std::string tint = (*iter)->readString( "tint" );
			if( tintMap_.find( dye ) != tintMap_.end() )
				tintName_[ dye ] = tint;
		}

		outsideOnly_ = pOwnSect()->readBool( "editorOnly/outsideOnly", outsideOnly_ );
		outsideOnly_ |= this->resourceIsOutsideOnly();

		castsShadow_ = pOwnSect()->readBool( "editorOnly/castsShadow", castsShadow_ );

		// check we're not > 100m in x or z
		BoundingBox bbox;
		edBounds(bbox);
		bbox.transformBy(edTransform());
		Vector3 boxVolume = bbox.maxBounds() - bbox.minBounds();
		static const float lengthLimit = 100.f;
		if (boxVolume.x > lengthLimit || boxVolume.z > lengthLimit)
		{
			std::string mname = pSection->readString( "resource" );
			WorldManager::instance().addError(pChunk, this,
				L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/MODEL_TOO_BIG", mname ) );
		}

		// Reset the amount of primitive groups in all the visuals
		primGroupCount_ = 0;
		hasPostLoaded_ = false;

		// Build a list of the animations in the model file
		animationNames_.push_back( std::string() );
		DataSectionPtr current = BWResource::openSection(
			pOwnSect()->readString( "resource") ) ;
		while (current)
		{
			addNames( animationNames_, current, "animation" );

			std::string parent = current->readString( "parent" );

			if (parent.empty())
				break;

			current = BWResource::openSection( parent + ".model" );
		}
		std::sort( animationNames_.begin(), animationNames_.end() );

		// Build a list of dyes in the model file
		{
			tintMap_.clear();
			DataSectionPtr current = BWResource::openSection(
				pOwnSect()->readString( "resource") ) ;

			if( current )
				addDyeTints( dyeTints_, current );
		}

		// Build a list of materials in the model file ( if it has any )
		{
			DataSectionPtr model = BWResource::openSection(
				pOwnSect()->readString( "resource") ) ;

			if( model )
			{
				std::set<std::string> existingMaterialOverrides;
				std::vector<ChunkMaterialPtr>::iterator iter = materialOverride_.begin();
				while( iter != materialOverride_.end() )
				{
					existingMaterialOverrides.insert( (*iter)->material_->identifier() );
					changedMaterials_.insert( (*iter)->material_->identifier() );
					++iter;
				}

				Moo::VisualPtr nodefullVisual;
				std::string name = model->readString( "nodefullVisual" );
				if (!name.empty())
				{
					std::string visualName = BWResource::removeExtension( name );
					visualName += ".visual";
					nodefullVisual = Moo::VisualManager::instance()->get( visualName.c_str() );
				}

				Moo::VisualPtr nodelessVisual;
				name = model->readString( "nodelessVisual" );
				if (!name.empty())
				{
					std::string visualName = BWResource::removeExtension( name );
					visualName += ".static.visual";
					nodelessVisual = Moo::VisualManager::instance()->get( visualName.c_str() );
				}

				if( !nodefullVisual && !nodelessVisual )
				{
					name = model->readString( "nodelessVisual" );
					if (!name.empty())
					{
						std::string visualName = BWResource::removeExtension( name );
						visualName += ".visual";
						nodelessVisual = Moo::VisualManager::instance()->get( visualName.c_str() );
					}
					else
					{
						nodelessVisual = NULL;
					}
				}

				std::vector<Moo::EffectMaterialPtr>	materials;

				if ( nodefullVisual )
					nodefullVisual->collateOriginalMaterials( materials );
				else if( nodelessVisual )
					nodelessVisual->collateOriginalMaterials( materials );

				for( std::vector<Moo::EffectMaterialPtr>::iterator iter = materials.begin(); iter != materials.end();
					++iter)
				{
					if( existingMaterialOverrides.find( (*iter)->identifier() ) == existingMaterialOverrides.end() )
					{
						DataSectionPtr matSec = new XMLSection( "material" );
						MaterialUtility::save( *iter, matSec );
						matSec->writeString( "identifier", (*iter)->identifier() );
						Moo::EffectMaterialPtr mat = new Moo::EffectMaterial;
						mat->load( matSec );
						materialOverride_.push_back( new ChunkMaterial( mat ) );
						existingMaterialOverrides.insert( mat->identifier() );
					}
				}
			}
		}

		// After loading,count the amount of primitive groups in all the
		// visuals.
		std::vector<std::string> visuals = extractVisualNames();
		std::vector<std::string>::iterator i = visuals.begin();
		for (; i != visuals.end(); ++i)
		{
			DataSectionPtr visualSection = BWResource::openSection( *i );
			if (!visualSection)
				continue;

			// Check for a custom bsp while we're here
			if (visualSection->readBool( "customBsp", false ))
				customBsp_ = true;

			std::vector<DataSectionPtr> renderSets;
			visualSection->openSections( "renderSet", renderSets );

			std::vector<DataSectionPtr>::iterator j = renderSets.begin();
			for (; j != renderSets.end(); ++j)
			{
				std::vector<DataSectionPtr> geoms;
				(*j)->openSections( "geometry", geoms );

				std::vector<DataSectionPtr>::iterator k = geoms.begin();
				for (; k != geoms.end(); ++k)
				{
					DataSectionPtr geom = *k;

					std::vector<DataSectionPtr> primGroups;
					(*k)->openSections( "primitiveGroup", primGroups );

					primGroupCount_ += primGroups.size();
				}
			}
		}
	}

	desc_ = pSection->readString( "resource" );
	std::string::size_type pos = desc_.find_last_of( "/" );
	if (pos != std::string::npos)
		desc_ = desc_.substr( pos + 1 );
	pos = desc_.find_last_of( "." );
	if (pos != std::string::npos)
		desc_ = desc_.substr( 0, pos );

	return ok;
}

void EditorChunkModel::loadModels( Chunk* chunk )
{
	std::vector<std::string> models;
	pOwnSect_->readStrings( "resource", models );
	if( models.size() )
	{
		SmartPointer<Model> model = Model::get( models[0] );
		if( model )
		{
			model->reload();
			load( pOwnSect_, chunk );
		}
	}
}

void EditorChunkModel::edPostLoad()
{
	// Don't put here code that might cause frame rate spikes (for example,
	// code that reads from or writes to disk).
}


void EditorChunkModel::clearLightingFashion()
{
	if (pStaticLightFashion_)
	{
		const FashionPtr pFashion = pStaticLightFashion_;
		fv_.erase( std::find( fv_.begin(), fv_.end(), pFashion ) );
		pStaticLightFashion_ = NULL;
	}

	lightingTagPrefix_.clear();
}


/**
 * We just loaded up with srcItems lighting data, create some new stuff of our
 * own
 */
void EditorChunkModel::edPostClone( EditorChunkItem* srcItem )
{
	clearLightingFashion();

	if (isModelNodeless())
		StaticLighting::markChunk( pChunk_ );

	BoundingBox bb = BoundingBox::s_insideOut_;
	edBounds( bb );
	bb.transformBy( edTransform() );
	bb.transformBy( chunk()->transform() );
	WorldManager::instance().markTerrainShadowsDirty( bb );
	this->syncInit();
}

void EditorChunkModel::edPostCreate()
{
	if (isModelNodeless())
		StaticLighting::markChunk( pChunk_ );

	BoundingBox bb = BoundingBox::s_insideOut_;
	edBounds( bb );
	bb.transformBy( edTransform() );
	bb.transformBy( chunk()->transform() );
	WorldManager::instance().markTerrainShadowsDirty( bb );
	this->syncInit();
}

/** Simple collision callback to find out if point a is visible from point b */
class VisibilityCollision : public CollisionCallback
{
public:
	VisibilityCollision() : gotone_( false ) { }

	int operator()( const ChunkObstacle & co,
		const WorldTriangle & hitTriangle, float dist )
	{
		if (!co.pItem()->pOwnSect() ||
			( co.pItem()->pOwnSect()->sectionName() != "model" &&
			 co.pItem()->pOwnSect()->sectionName() != "speedtree" &&
			co.pItem()->pOwnSect()->sectionName() != "shell" ))
		{
			return COLLIDE_ALL;
		}

		// if it's not transparent, we can stop now
		if (!hitTriangle.isTransparent() && co.pItem()->edAffectShadow()) 
		{ 
			gotone_ = true; 
			return COLLIDE_STOP; 
		}

		// otherwise we have to keep on going
		return COLLIDE_ALL;
	}

	bool gotone()			{ return gotone_; }
private:
	bool gotone_;
};


namespace
{
	D3DCOLOR combineColours( D3DCOLOR a, D3DCOLOR b )
	{
		return Colour::getUint32(Colour::getVector4( a ) + Colour::getVector4( b ));
	}

	bool isVisibleFrom( Vector3 vertex, Vector3 light )
	{
		// check the vertex is visible from the light
		VisibilityCollision v;
		ChunkManager::instance().cameraSpace()->collide( 
			vertex,
			light,
			v );

		return !v.gotone();
	}
}

bool EditorChunkModel::calculateLighting( StaticLighting::StaticLightContainer& lights,
										 StaticLightValues& values,
										 Moo::VisualPtr visual,
										 bool calculateVisibility)
{
	Moo::VertexXYZNUV* vertices;
	Moo::IndicesHolder indices;
	uint32 numVertices;
	uint32 numIndices;
	Moo::EffectMaterialPtr material;

	visual->createCopy( vertices, indices, numVertices, numIndices, material );

	MF_ASSERT( numVertices > 0 );

	StaticLightValues::ColourValueVector& colours = values.colours();

	// If there's no lights, and we're not connected to anything, just set it 
	// all to medium illumination, so you can see the shell
	if (lights.empty() && pChunk_->pbegin() == pChunk_->pend())
	{
		colours.clear();
		colours.resize( numVertices, 0x00aaaaaa );
		delete [] vertices;
		values.colours();
		return true;
	}

	// Make sure we've got the correct amount of colour entries
	colours.resize( numVertices, lights.ambient() );

	MF_ASSERT( colours.size() == numVertices );


	Matrix xform = chunk()->transform();
	xform.preMultiply( edTransform() );

	for (uint32 i = 0; i < numVertices; i++)
	{
		Vector3 vertexPos = xform.applyPoint( vertices[i].pos_ );

		colours[i] = lights.ambient();

		std::vector< Moo::OmniLightPtr >& omnis = lights.omnis();
		for (std::vector< Moo::OmniLightPtr >::iterator oi = omnis.begin();
			oi != omnis.end(); ++oi)
		{
			Moo::OmniLightPtr omni = *oi;


			Vector3 dirToLight = omni->worldPosition() - vertexPos;
			dirToLight.normalise();

			// early out for back facing tris
			float dot = dirToLight.dotProduct( xform.applyVector( vertices[i].normal_ ) );
			if (dot <= 0.f)
				continue;

			// early out for out of range vertices
			float maxRadiusSq = omni->outerRadius() * omni->outerRadius();
			if ((vertexPos - omni->worldPosition()).lengthSquared() > maxRadiusSq)
				continue;

			// Offset the vertex 50cm towards the light, so we don't collide with ourself
			// 10cm causes too many shadows on small things
			Vector3 vert = vertexPos + (dirToLight * 0.5);

			float vis = 1.f;

			if (calculateVisibility)
			{
				if (!isVisibleFrom( vert, omni->worldPosition() ))
				{

					Vector3 x(2.f, 0.f, 0.f);
					Vector3 y(0.f, 2.f, 0.f);
					Vector3 z(0.f, 0.f, 2.f);

					if (isVisibleFrom( vert, omni->worldPosition() + x ) ||
						isVisibleFrom( vert, omni->worldPosition() - x ) ||
						isVisibleFrom( vert, omni->worldPosition() + y ) ||
						isVisibleFrom( vert, omni->worldPosition() - y ) ||
						isVisibleFrom( vert, omni->worldPosition() + z ) ||
						isVisibleFrom( vert, omni->worldPosition() - z )
						)
						vis = 0.5f;
					else
						continue;
				}
			}

			// Add the colour to the vertex
			if (dot > 1.f)
				dot = 1.f;

			float dist = (vertexPos - omni->worldPosition()).length();
			if (dist < omni->innerRadius())
			{
				colours[i] = combineColours( colours[i], omni->colour() * dot * vis * omni->multiplier() );
			}
			else if (dist < omni->outerRadius())
			{
				float falloff = (dist - omni->innerRadius()) / (omni->outerRadius() - omni->innerRadius());
				colours[i] = combineColours( colours[i], omni->colour() * (1 - falloff) * dot * vis * omni->multiplier() );
			}
		}
		std::vector< Moo::SpotLightPtr >& spots = lights.spots();
		for (std::vector< Moo::SpotLightPtr >::iterator oi = spots.begin();
			oi != spots.end(); ++oi)
		{
			Moo::SpotLightPtr spot = *oi;

			Vector3 dirToLight = spot->worldPosition() - vertexPos;
			dirToLight.normalise();

			// early out for back facing tris
			float dot = (spot->worldDirection()).dotProduct( xform.applyVector( vertices[i].normal_ ) );
			if (dot <= 0.f)
				continue;

			// early out for out of range vertices
			float maxRadiusSq = spot->outerRadius() * spot->outerRadius();
			if ((vertexPos - spot->worldPosition()).lengthSquared() > maxRadiusSq)
				continue;

			float cosAngle = (spot->worldDirection()).dotProduct( dirToLight );

			float cosHalfConeAngle = cosf( acosf( spot->cosConeAngle() ) / 2.f);

			if (cosAngle <= cosHalfConeAngle)
				continue;

			// Offset the vertex 50cm towards the light, so we don't collide with ourself
			// 10cm causes too many shadows on small things
			Vector3 vert = vertexPos + (dirToLight * 0.5);

			float vis = 1.f;

			if (calculateVisibility)
			{
				if (!isVisibleFrom( vert, spot->worldPosition() ))
				{

					Vector3 x(2.f, 0.f, 0.f);
					Vector3 y(0.f, 2.f, 0.f);
					Vector3 z(0.f, 0.f, 2.f);

					if (isVisibleFrom( vert, spot->worldPosition() + x ) ||
						isVisibleFrom( vert, spot->worldPosition() - x ) ||
						isVisibleFrom( vert, spot->worldPosition() + y ) ||
						isVisibleFrom( vert, spot->worldPosition() - y ) ||
						isVisibleFrom( vert, spot->worldPosition() + z ) ||
						isVisibleFrom( vert, spot->worldPosition() - z )
						)
						vis = 0.5f;
					else
						continue;
				}
			}

			float coneFalloff = (cosAngle - cosHalfConeAngle) / (1.f - cosHalfConeAngle);


			float dist = (vertexPos - spot->worldPosition()).length();
			if (dist < spot->innerRadius())
			{
				colours[i] = combineColours( colours[i], spot->colour() * coneFalloff * dot * vis * spot->multiplier() );
			}
			else if (dist < spot->outerRadius())
			{
				float falloff = (dist - spot->innerRadius()) / (spot->outerRadius() - spot->innerRadius());
				colours[i] = combineColours( colours[i], spot->colour() * (1 - falloff) * coneFalloff * dot * vis * spot->multiplier() );
			}
		}
		WorldManager::instance().fiberPause();
		if( !WorldManager::instance().isWorkingChunk( chunk() ) )
		{
			delete [] vertices;
			return false;
		}
	}

	MF_ASSERT( !colours.empty() );

	delete [] vertices;
	values.colours();
	return true;
}

namespace
{
	std::string toString( int i )
	{
		char buf[128];
		itoa( i, buf, 10 );
		return buf;
	}
}


// this function generates a tag into the chunk cData file
std::string EditorChunkModel::generateLightingTagPrefix() const
{
	MF_ASSERT( chunk() );

	DataSectionPtr chunkSect = EditorChunkCache::instance( *chunk() ).pChunkSection();

	MF_ASSERT( chunkSect );

	std::vector<DataSectionPtr> modelSects;
	chunkSect->openSections( "model", modelSects );

	if( DataSectionPtr shellSec = chunkSect->openSection( "shell" ) )
		modelSects.push_back( shellSec );

	std::vector<std::string> usedPrefixes;
	for (std::vector<DataSectionPtr>::iterator i = modelSects.begin(); i != modelSects.end(); ++i)
		(*i)->readStrings( "lighting", usedPrefixes );

	// use model's resource name as the base (strip off the directoy and extension)
	std::string baseName = pOwnSect_->readString( "resource", "" );
	size_t lastSepIndex = baseName.find_last_of('/');
	baseName = baseName.substr(lastSepIndex + 1, baseName.length() - lastSepIndex - 7);
	baseName = "lighting/" + baseName + "-";

	int index = 0;
	std::string curName;
	do 
	{
		curName = baseName + toString( index );
		index++;
	}
	while (std::find( usedPrefixes.begin(), usedPrefixes.end(), curName ) != usedPrefixes.end());

	return curName;
}

bool EditorChunkModel::edRecalculateLighting( StaticLighting::StaticLightContainer& lights )
{
	ChunkItemPtr holder( this );

	if (!isModelNodeless())
		return true;

	if (pStaticLightFashion_)
	{
		MF_ASSERT( !lightingTagPrefix_.empty() );

		std::vector<StaticLightValues*> vals = pStaticLightFashion_->staticLightValues();
		std::vector<Moo::VisualPtr> visuals = extractVisuals();

		MF_ASSERT( !vals.empty() );
		MF_ASSERT( vals.size() == visuals.size() );

		for (uint i = 0; i < vals.size() && pChunk_; ++i)
		{
			if (vals[i])
			{
				MF_ASSERT( visuals[i] );
				if (!calculateLighting( lights, *vals[i], visuals[i], true ))
					return false;
			}
		}
	}
	else
	{
		MF_ASSERT( pOwnSect() );

		// temp values we'll use to generate the initial lighting data
		const int MAX_VALUES = 16;
		StaticLightValues values[MAX_VALUES];

		std::vector<Moo::VisualPtr> visuals = extractVisuals();
		MF_ASSERT( visuals.size() <= MAX_VALUES );

		// Generate a new set of static lighting data for each of the models
		for (uint i = 0; i < visuals.size(); ++i)
			if (visuals[i])
				if (!calculateLighting( lights, values[i], visuals[i], true ))
					return false;

		// Don't bother going ahead if we've been deleted while recalculating
		if (!pChunk_)
			return false;

		// generate a new lighting identifier for this model
		std::string lightingTagPrefix = generateLightingTagPrefix();

		size_t shortTagIndex = lightingTagPrefix.find_last_of('/');
		MF_ASSERT(shortTagIndex != std::string::npos);
		std::string shortTag = lightingTagPrefix.substr(shortTagIndex + 1);

		// create temporary data sections for the lighting data and call addStaticLighting
		// addStaticLighting needs data sections to initialise it
		BinSectionPtr modelLightingSection = 
			new BinSection( shortTag, new BinaryBlock( NULL, 0, "BinaryBlock/EditorChunkModel" ) );
		for (uint i = 0; i < visuals.size(); ++i)
			if (visuals[i])
				values[i].saveData( modelLightingSection,
									StaticLightFashion::lightingTag(
									int(i), int(visuals.size()) ) );

		addStaticLighting( chunk()->binFileName() + "/" + lightingTagPrefix, modelLightingSection );

		// Update the datasection with the lighting data
		edSave( pOwnSect() );
	}
	return true;
}


void EditorChunkModel::addStaticLighting( const std::string& resName, DataSectionPtr modelLightingSection )
{
	if ( !modelLightingSection )
		modelLightingSection = BWResource::openSection( resName );

	if ( !modelLightingSection )
	{
		clearLightingFashion();
		return;
	}

	StaticLightFashionPtr pSLF = StaticLightFashion::get( *pSuperModel_,
														modelLightingSection );

	if (!pSLF)
		return;

	// Ensure that the .lighting files aren't older than the .visuals
	std::vector<StaticLightValues*> vals = pSLF->staticLightValues();
	std::vector<Moo::VisualPtr> visuals = extractVisuals();

	MF_ASSERT( !vals.empty() );
	MF_ASSERT( vals.size() == visuals.size() );

	for (uint i = 0; i < vals.size(); ++i)
	{
		if (vals[i])
		{
			if (vals[i]->size() != visuals[i]->nVertices())
			{
				// wrong size for lighting data, bail
				INFO_MSG( "static lighting data is wrong size, ignoring\n" );
				return;
			}
		}
	}

	std::string tag = resName.substr(resName.find(".cdata") + 7);
	MF_ASSERT(tag.length() > 0);
	lightingTagPrefix_ = tag;

	fv_.push_back( pSLF );
	pStaticLightFashion_ = pSLF;
}

bool EditorChunkModel::isVisualFileNewer() const
{
	MF_ASSERT(chunk());
	MF_ASSERT(pStaticLightFashion_);

	std::vector<StaticLightValues*> vals = pStaticLightFashion_->staticLightValues();
	std::vector<std::string> visualNames = extractVisualNames();

	MF_ASSERT( !vals.empty() );
	MF_ASSERT( vals.size() == visualNames.size() );

	std::string cdataName = chunk()->binFileName();

	for (uint i = 0; i < vals.size(); ++i)
	{
		if (vals[i])
		{
			if (BWResource::isFileOlder( cdataName, visualNames[i] ))
			{
				// ok, the lighting data is out of date, bail
				INFO_MSG( "static lighting data out of date, ignoring\n" );
				return true;
			}
		}
	}

	return false;
}

/**
 *	This method does extra stuff when this item is tossed between chunks.
 *
 *	It updates its datasection in that chunk.
 */
void EditorChunkModel::toss( Chunk * pChunk )
{
	if (pChunk_ != NULL && pOwnSect_)
	{
		EditorChunkCache::instance( *pChunk_ ).
			pChunkSection()->delChild( pOwnSect_ );
		pOwnSect_ = NULL;
	}

	this->ChunkModel::toss( pChunk, ( !!Options::getOptionInt("render/misc/drawEditorProxies",0) && !!Options::getOptionInt("render/proxys",0) ) ? pEditorModel_ : NULL  );

	if (pChunk_ != NULL && !pOwnSect_)
	{
		pOwnSect_ = EditorChunkCache::instance( *pChunk_ ).
			pChunkSection()->newSection( sectionName() );
		this->edSave( pOwnSect_ );
	}

	if (firstToss_)
	{
		// check lighting files are up to date (can't do this on load as chunk() is NULL)
		if ( pChunk_ && pStaticLightFashion_ && isVisualFileNewer() )
			StaticLighting::markChunk( pChunk_ );

		firstToss_ = false;
	}

	// If we havn't got our static lighting calculated yet, mark the new
	// chuck as dirty. This will only be the case for newly created items.
	// Marking a chunk as dirty when moving chunks around is taken care of 
	// in edTransform()
	if (pChunk && !pChunk->isOutsideChunk() && !pStaticLightFashion_ && isModelNodeless())
		StaticLighting::markChunk( pChunk );
}


/**
 *	Save to the given section
 */
bool EditorChunkModel::edSave( DataSectionPtr pSection )
{
	if (!isShellModel() && !edCommonSave( pSection ))
		return false;

	if (standinModel_)
	{
		// only change its transform (want to retain knowledge of what it was)
		pSection->copy( originalSect_ );
		pSection->writeMatrix34( "transform", transform_ );
		return true;
	}

	if (pSuperModel_)
	{
		for (int i = 0; i < pSuperModel_->nModels(); i++)
		{
			pSection->writeString( "resource",
				pSuperModel_->topModel(i)->resourceID() );
		}

		if (pAnimation_)
		{
			DataSectionPtr pAnimSec = pSection->openSection( "animation", true );
			pAnimSec->writeString( "name", animName_ );
			pAnimSec->writeFloat( "frameRateMultiplier", animRateMultiplier_ );
		}
		else
		{
			pSection->delChild( "animation" );
		}

		while( pSection->findChild( "dye" ) )
			pSection->delChild( "dye" );
		for( std::map<std::string,std::string>::iterator iter = tintName_.begin(); iter != tintName_.end(); ++iter )
		{
			if( iter->second != L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/DEFAULT_TINT_NAME") )
			{
				DataSectionPtr pDyeSec = pSection->newSection( "dye" );
				pDyeSec->writeString( "name", iter->first );
				pDyeSec->writeString( "tint", iter->second );
			}
		}

		while( pSection->findChild( "material" ) )
			pSection->delChild( "material" );
		for( std::vector<ChunkMaterialPtr>::iterator iter = materialOverride_.begin(); iter != materialOverride_.end(); ++iter )
		{
			if( changedMaterials_.find( (*iter)->material_->identifier() ) != changedMaterials_.end() )
			{
				DataSectionPtr pMaterialSec = pSection->newSection( "material" );
				if( ToolsCommon::isEval() ||
					Options::getOptionInt("objects/materialOverrideMode", 0) )
					MaterialUtility::save( (*iter)->material_, pMaterialSec, false );
				else
					MaterialUtility::save( (*iter)->material_, pMaterialSec, true );
				pMaterialSec->writeString( "identifier", (*iter)->material_->identifier() );
			}
		}

		pSection->writeMatrix34( "transform", transform_ );
	}

	//Editor only data.
	if (outsideOnly_ && !resourceIsOutsideOnly())
		pSection->writeBool( "editorOnly/outsideOnly", true );
	else
		pSection->delChild( "editorOnly/outsideOnly" );

	pSection->writeBool( "editorOnly/castsShadow", castsShadow_ );

	pSection->setString( label_ );

	pSection->writeBool( "reflectionVisible", reflectionVisible_ );

	if (pStaticLightFashion_)
	{
		MF_ASSERT( !lightingTagPrefix_.empty() );

		pSection->writeString( "lighting", lightingTagPrefix_ );
	}
	else
	{
		// delete the lighting section if there is one
		// this will be the case if a model is moving from an indoor chunk
		// to an outdoor chunk
		pSection->deleteSection( "lighting" );
	}

	return true;
}

/**
 *	Called when our containing chunk is saved
 */
void EditorChunkModel::edChunkSave()
{
}

/**
 *	Called when our containing chunk is saved; save the lighting info
 */
void EditorChunkModel::edChunkSaveCData(DataSectionPtr cData)
{
	if (standinModel_)
		return;

	if (!pStaticLightFashion_)
		return;

	MF_ASSERT( !lightingTagPrefix_.empty() );

	// Save the static lighting data
	std::vector<StaticLightValues*> v = pStaticLightFashion_->staticLightValues();
	MF_ASSERT( !v.empty() );
	for (uint i = 0; i < v.size(); ++i)
	{
		if (v[i])
		{
			std::string resName = chunk()->binFileName() + "/" + lightingTagPrefix_ + 
											"/" + StaticLightFashion::lightingTag(
											int(i), int(v.size()) );

			v[i]->save( cData, resName );
		}
	}
}

/**
 *	This method sets this item's transform for the editor
 *	It takes care of moving it into the right chunk and recreating the
 *	collision scene and all that
 */
bool EditorChunkModel::edTransform( const Matrix & m, bool transient )
{
	// find out where we belong now
	BoundingBox lbb( Vector3(0.f,0.f,0.f), Vector3(1.f,1.f,1.f) );
	if (pSuperModel_) pSuperModel_->boundingBox( lbb );
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyPoint(
		(lbb.minBounds() + lbb.maxBounds()) * 0.5f ) );
	if (pNewChunk == NULL) return false; // failure, outside the space!

	// for transient transforms there are no more, checks, update the transform
    // and return
	if (transient)
	{
		transform_ = m;
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
	transform_.multiply( m, pOldChunk->transform() );
	transform_.postMultiply( pNewChunk->transformInverse() );

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
	if (isModelNodeless())
	{
		StaticLighting::markChunk( pNewChunk );
		StaticLighting::markChunk( pOldChunk );

		if (pOldChunk != pNewChunk && !lightingTagPrefix_.empty())
		{
			if (pNewChunk->isOutsideChunk())
			{
				clearLightingFashion();
			}
			else
			{
				lightingTagPrefix_ = generateLightingTagPrefix();
			}

			edSave( pOwnSect() );
		}
	}
	this->syncInit();
	return true;
}


void EditorChunkModel::edPreDelete()
{
	if (isModelNodeless())
	{
		clearLightingFashion();
		StaticLighting::markChunk( pChunk_ );
	}

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
void EditorChunkModel::edBounds( BoundingBox& bbRet ) const
{
	if (pSuperModel_)
		pSuperModel_->boundingBox( bbRet );

	BoundingBox ebb;
	if (( !!Options::getOptionInt("render/misc/drawEditorProxies",0) && !!Options::getOptionInt("render/proxys",0) ) && (pEditorModel_))
	{
		pEditorModel_->boundingBox( ebb );
		bbRet.addBounds( ebb );
	}
}


/**
 *	This method returns whether or not this model should cast a shadow.
 *
 *  @return		Returns whether or not this model should cast a shadow
 */
bool EditorChunkModel::edAffectShadow() const
{
	return castsShadow_;
}


/**
 *	Helper struct for gathering matter names
 */
struct MatterDesc
{
	std::set<std::string>	tintNames;
};
typedef std::map<std::string,MatterDesc> MatterDescs;

/// I can't believe there's not an algorithm for this...
template <class It, class S> void seq_append( It F, It L, S & s)
	{ for(; F != L; ++F) s.push_back( *F ); }


extern "C"
{
/**
 *	This property makes a dye from a matter name to one of a number of tints
 */
class ModelDyeProperty : public GeneralProperty
{
public:
	ModelDyeProperty( const std::string & name,
			const std::string & current, const MatterDesc & tints,
			EditorChunkModel * pModel ) :
		GeneralProperty( name ),
		curval_( current ),
		pModel_( pModel )
	{
		tints_.push_back( "Default" );
		seq_append( tints.tintNames.begin(), tints.tintNames.end(), tints_ );

		GENPROPERTY_MAKE_VIEWS()
	}

	virtual PyObject * EDCALL pyGet()
	{
		return PyString_FromString( curval_.c_str() );
	}

	virtual int EDCALL pySet( PyObject * value )
	{
		// try it as a string
		if (PyString_Check( value ))
		{
			const char * valStr = PyString_AsString( value );

			std::vector<std::string>::iterator found =
				std::find( tints_.begin(), tints_.end(), valStr );
			if (found == tints_.end())
			{
				std::string errStr = "GeneralEditor.";
				errStr += name_;
				errStr += " must be set to a valid tint string or an index.";
				errStr += " Valid tints are: ";
				for (uint i = 0; i < tints_.size(); i++)
				{
					if (i)
					{
						if (i+1 != tints_.size())
							errStr += ", ";
						else
							errStr += ", or ";
					}
					errStr += tints_[i];
				}

				PyErr_Format( PyExc_ValueError, errStr.c_str() );
				return -1;
			}

			curval_ = valStr;
			// TODO: Set this in the editor chunk item!
			return 0;
		}

		// try it as a number
		int idx = 0;
		if (Script::setData( value, idx ) == 0)
		{
			if (idx < 0 || idx >= (int) tints_.size())
			{
				PyErr_Format( PyExc_ValueError, "GeneralEditor.%s "
					"must be set to a string or an index under %d",
					name_, tints_.size() );
				return -1;
			}

			curval_ = tints_[ idx ];
			// TODO: Set this in the editor chunk item!
			return 0;
		}

		// give up
		PyErr_Format( PyExc_TypeError, "GeneralEditor.%s, "
			"being a dye property, must be set to a string or an index",
			name_ );
		return NULL;
	}

private:
	std::string					curval_;
	std::vector<std::string>	tints_;
	EditorChunkModel *			pModel_;

	GENPROPERTY_VIEW_FACTORY_DECLARE( ModelDyeProperty )
};
}

GENPROPERTY_VIEW_FACTORY( ModelDyeProperty )

static std::string matUIName( ComObjectWrap<ID3DXEffect> pEffect, D3DXHANDLE hParameter, const std::string& descName )
{
	std::string uiName = MaterialUtility::UIName( pEffect.pComObject(), hParameter );
	if( uiName.size() == 0 )
		uiName = descName;
	return uiName;
}

EditorChunkModel::MaterialProp EditorChunkModel::findMaterialByName( const std::string& name,
	D3DXPARAMETER_CLASS mcMin, D3DXPARAMETER_CLASS mcMax, D3DXPARAMETER_TYPE mtMin, D3DXPARAMETER_TYPE mtMax ) const
{
	std::string matName = name.substr( 0, name.find( '/' ) );
	std::string propName = name.substr( name.find( '/' ) + 1, name.npos );

	for( std::vector<ChunkMaterialPtr>::const_iterator iter = materialOverride_.begin();
		iter != materialOverride_.end(); ++iter )
	{
		Moo::EffectMaterialPtr mat = (*iter)->material_;
		
		if( mat->identifier() == matName )
		{
			if (mat->pEffect() )
			{
				ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( mat );
				if( !pEffect )
					continue;

				Moo::EffectMaterial::Properties& properties = mat->properties();
				Moo::EffectMaterial::Properties::iterator it = properties.begin();
				Moo::EffectMaterial::Properties::iterator end = properties.end();

				while ( it != end )
				{
					MF_ASSERT( it->second );
					D3DXHANDLE hParameter = it->first;
					Moo::EffectPropertyPtr& pProperty = it->second;

					if ( ToolsCommon::isEval() ||
						(Options::getOptionInt("objects/materialOverrideMode", 0)) ||
						MaterialUtility::worldBuilderEditable( &*pEffect, hParameter ) )
					{
						D3DXPARAMETER_DESC desc;
						HRESULT hr = pEffect->GetParameterDesc( hParameter, &desc );
						if( SUCCEEDED(hr) )
						{
							if( desc.Class >= mcMin && desc.Class <= mcMax &&
								desc.Type >= mtMin && desc.Type <= mtMax &&
								desc.Name == propName )
							{
								MaterialProp mp;
								mp.matName_ = matName;
								mp.effect_ = pEffect;
								mp.handle_ = it->first;
								mp.property_ = it->second;
								if( desc.Type == D3DXPT_INT )
								{
									D3DXHANDLE enumHandle = pEffect->GetAnnotationByName( hParameter, "EnumType" );
									LPCSTR enumType = NULL;
									if( enumHandle )
									{
										D3DXPARAMETER_DESC enumPara;
										if( SUCCEEDED( pEffect->GetParameterDesc( enumHandle, &enumPara ) ) &&
											enumPara.Type == D3DXPT_STRING &&
											SUCCEEDED( pEffect->GetString( enumHandle, &enumType ) ) )
										{
											if( dxEnum_.isEnum( enumType ) )
												mp.enumType_ = enumType;
										}
									}
								}
								return mp;
							}
						}
						else
						{
							ERROR_MSG( "MaterialUtility::listProperties - GetParameterDesc \
									failed with DX error code %lx\n", hr );
						}
					}

					it++;
				}
			}
		}
	}
	ERROR_MSG( "Should never arrive here : %s %d\n", __FILE__, __LINE__ );
	MF_ASSERT( 0 );
	return MaterialProp();
}

EditorChunkModel::MaterialProp EditorChunkModel::findOriginalMaterialByName( const std::string& name,
	D3DXPARAMETER_CLASS mcMin, D3DXPARAMETER_CLASS mcMax, D3DXPARAMETER_TYPE mtMin, D3DXPARAMETER_TYPE mtMax ) const
{
	std::string matName = name.substr( 0, name.find( '/' ) );
	std::vector< Moo::Visual::PrimitiveGroup * > primGroup;
	pSuperModel_->topModel(0)->gatherMaterials( matName, primGroup );

	std::string propName = name.substr( name.find( '/' ) + 1, name.npos );

	{
		Moo::EffectMaterialPtr mat = primGroup[0]->material_;
		{
			if ( mat->pEffect() )
			{
				ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( mat );
				if( !pEffect )
					return MaterialProp();

				Moo::EffectMaterial::Properties& properties = mat->properties();
				Moo::EffectMaterial::Properties::iterator it = properties.begin();
				Moo::EffectMaterial::Properties::iterator end = properties.end();

				while ( it != end )
				{
					MF_ASSERT( it->second );
					D3DXHANDLE hParameter = it->first;
					Moo::EffectPropertyPtr& pProperty = it->second;

					if ( ToolsCommon::isEval() ||
						(Options::getOptionInt("objects/materialOverrideMode", 0)) ||
						MaterialUtility::worldBuilderEditable( &*pEffect, hParameter ) )
					{
						D3DXPARAMETER_DESC desc;
						HRESULT hr = pEffect->GetParameterDesc( hParameter, &desc );
						if( SUCCEEDED(hr) )
						{
							if( desc.Class >= mcMin && desc.Class <= mcMax &&
								desc.Type >= mtMin && desc.Type <= mtMax &&
								desc.Name == propName )
							{
								MaterialProp mp;
								mp.matName_ = matName;
								mp.effect_ = pEffect;
								mp.handle_ = it->first;
								mp.property_ = it->second;
								return mp;
							}
						}
						else
						{
							ERROR_MSG( "MaterialUtility::listProperties - GetParameterDesc \
									failed with DX error code %lx\n", hr );
						}
					}

					it++;
				}
			}
		}
	}
	ERROR_MSG( "Should never arrive here : %s %d\n", __FILE__, __LINE__ );
	MF_ASSERT( 0 );
	return MaterialProp();
}

bool EditorChunkModel::getMaterialBool( const std::string& name ) const
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_SCALAR, D3DXPC_SCALAR, D3DXPT_BOOL, D3DXPT_BOOL );
	MaterialBoolProxy* proxy = (MaterialBoolProxy*)( mp.property_.getObject() );
	return proxy->get();
}

bool EditorChunkModel::setMaterialBool( const std::string& name, const bool& value )
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_SCALAR, D3DXPC_SCALAR, D3DXPT_BOOL, D3DXPT_BOOL );
	MaterialBoolProxy* proxy = (MaterialBoolProxy*)( mp.property_.getObject() );
	proxy->set( value, false );
	changedMaterials_.insert( mp.matName_ );
	return true;
}

std::string EditorChunkModel::getMaterialString( const std::string& name ) const
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_OBJECT, D3DXPC_OBJECT, D3DXPT_TEXTURE, D3DXPT_TEXTURECUBE );
	MaterialTextureProxy* proxy = (MaterialTextureProxy*)( mp.property_.getObject() );
	return proxy->get();
}

bool EditorChunkModel::setMaterialString( const std::string& name, const std::string& value )
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_OBJECT, D3DXPC_OBJECT, D3DXPT_TEXTURE, D3DXPT_TEXTURECUBE );
	MaterialTextureProxy* proxy = (MaterialTextureProxy*)( mp.property_.getObject() );
	proxy->set( value, false );
	changedMaterials_.insert( mp.matName_ );
	return true;
}

float EditorChunkModel::getMaterialFloat( const std::string& name ) const
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_SCALAR, D3DXPC_SCALAR, D3DXPT_FLOAT, D3DXPT_FLOAT );
	MaterialFloatProxy* proxy = (MaterialFloatProxy*)( mp.property_.getObject() );
	return proxy->get();
}

bool EditorChunkModel::setMaterialFloat( const std::string& name, const float& value )
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_SCALAR, D3DXPC_SCALAR, D3DXPT_FLOAT, D3DXPT_FLOAT );
	MaterialFloatProxy* proxy = (MaterialFloatProxy*)( mp.property_.getObject() );
	proxy->set( value, false );
	changedMaterials_.insert( mp.matName_ );
	return true;
}

bool EditorChunkModel::getMaterialFloatRange( const std::string& name, float& min, float& max, int& digits )
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_SCALAR, D3DXPC_SCALAR, D3DXPT_FLOAT, D3DXPT_FLOAT );
	MaterialFloatProxy* proxy = (MaterialFloatProxy*)( mp.property_.getObject() );
	return proxy->getRange( min, max, digits );
}

bool EditorChunkModel::getMaterialFloatDefault( const std::string& name, float& def )
{
	MaterialProp mp = findOriginalMaterialByName( name, D3DXPC_SCALAR, D3DXPC_SCALAR, D3DXPT_FLOAT, D3DXPT_FLOAT );
	MaterialFloatProxy* proxy = (MaterialFloatProxy*)( mp.property_.getObject() );
	def = proxy->get();
	return true;
}

void EditorChunkModel::setMaterialFloatToDefault( const std::string& name )
{
	float def;
	if( getMaterialFloatDefault( name, def ) )
	{
		MaterialProp mp = findMaterialByName( name, D3DXPC_SCALAR, D3DXPC_SCALAR, D3DXPT_FLOAT, D3DXPT_FLOAT );
		MaterialFloatProxy* proxy = (MaterialFloatProxy*)( mp.property_.getObject() );
		proxy->set( def, false );
		// TODO: remove it from the list
	}
}

Vector4 EditorChunkModel::getMaterialVector4( const std::string& name ) const
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_VECTOR, D3DXPC_VECTOR, D3DXPT_FLOAT, D3DXPT_FLOAT );
	MaterialVector4Proxy* proxy = (MaterialVector4Proxy*)( mp.property_.getObject() );
	return proxy->get();
}

bool EditorChunkModel::setMaterialVector4( const std::string& name, const Vector4& value )
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_VECTOR, D3DXPC_VECTOR, D3DXPT_FLOAT, D3DXPT_FLOAT );
	MaterialVector4Proxy* proxy = (MaterialVector4Proxy*)( mp.property_.getObject() );
	proxy->set( value, false );
	changedMaterials_.insert( mp.matName_ );
	return true;
}

Matrix EditorChunkModel::getMaterialMatrix( const std::string& name ) const
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_MATRIX_ROWS, D3DXPC_MATRIX_COLUMNS, D3DXPT_FLOAT, D3DXPT_FLOAT );
	MaterialMatrixProxy* proxy = (MaterialMatrixProxy*)( mp.property_.getObject() );
	Matrix m;
	proxy->getMatrix( m, true );
	return m;
}

bool EditorChunkModel::setMaterialMatrix( const std::string& name, const Matrix& value )
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_MATRIX_ROWS, D3DXPC_MATRIX_COLUMNS, D3DXPT_FLOAT, D3DXPT_FLOAT );
	MaterialMatrixProxy* proxy = (MaterialMatrixProxy*)( mp.property_.getObject() );
	proxy->setMatrix( value );
	changedMaterials_.insert( mp.matName_ );
	return true;
}

uint32 EditorChunkModel::getMaterialInt( const std::string& name ) const
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_SCALAR, D3DXPC_SCALAR, D3DXPT_INT, D3DXPT_INT );
	MaterialIntProxy* proxy = (MaterialIntProxy*)( mp.property_.getObject() );
	return proxy->get();
}

bool EditorChunkModel::setMaterialInt( const std::string& name, const uint32& value )
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_SCALAR, D3DXPC_SCALAR, D3DXPT_INT, D3DXPT_INT );
	MaterialIntProxy* proxy = (MaterialIntProxy*)( mp.property_.getObject() );
	proxy->set( value, false );
	changedMaterials_.insert( mp.matName_ );
	return true;
}

bool EditorChunkModel::getMaterialIntRange( const std::string& name, uint32& min, uint32& max, int& digits )
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_SCALAR, D3DXPC_SCALAR, D3DXPT_INT, D3DXPT_INT );
	MaterialIntProxy* proxy = (MaterialIntProxy*)( mp.property_.getObject() );
	return proxy->getRange( (int&)min, (int&)max );// hack, avoid problems in exisitng code
}

Moo::EffectMaterialPtr EditorChunkModel::findMaterialByName( const std::string& name ) const
{
	std::string matName = name.substr( 0, name.find( '/' ) );
	std::string propName = name.substr( name.find( '/' ) + 1, name.npos );

	for( std::vector<ChunkMaterialPtr>::const_iterator iter = materialOverride_.begin();
		iter != materialOverride_.end(); ++iter )
	{
		Moo::EffectMaterialPtr mat = (*iter)->material_;
		if( mat->identifier() == matName )
			return mat;
	}
	ERROR_MSG( "Should never arrive here : %s %d\n", __FILE__, __LINE__ );
	MF_ASSERT( 0 );
	return NULL;
}

std::string EditorChunkModel::getMaterialCollision( const std::string& name ) const
{
	Moo::EffectMaterialPtr mat = findMaterialByName( name );
	for( StringHashMap<int>::const_iterator iter = collisionFlags_.begin();
		iter != collisionFlags_.end(); ++iter )
	{
		if( iter->second == mat->collisionFlags() )
			return iter->first;
	}
	for( StringHashMap<int>::const_iterator iter = collisionFlags_.begin();
		iter != collisionFlags_.end(); ++iter )
	{
		if( iter->second == 0 )
			return iter->first;
	}
	return "";
}

bool EditorChunkModel::setMaterialCollision( const std::string& name, const std::string& collisionType )
{
	Moo::EffectMaterialPtr mat = findMaterialByName( name );
	mat->collisionFlags( collisionFlags_.find( collisionType )->second );
	mat->bspModified_ = true;
	changedMaterials_.insert( mat->identifier() );
	return true;
}

std::string EditorChunkModel::getMaterialKind( const std::string& name ) const
{
	Moo::EffectMaterialPtr mat = findMaterialByName( name );
	for( StringHashMap<int>::const_iterator iter = s_materialKinds_.begin();
		iter != s_materialKinds_.end(); ++iter )
	{
		if( iter->second == mat->materialKind() )
			return iter->first;
	}
	for( StringHashMap<int>::const_iterator iter = s_materialKinds_.begin();
		iter != s_materialKinds_.end(); ++iter )
	{
		if( iter->second == 0 )
			return iter->first;
	}
	return "";
}

bool EditorChunkModel::setMaterialKind( const std::string& name, const std::string& collisionType )
{
	Moo::EffectMaterialPtr mat = findMaterialByName( name );
	mat->materialKind( s_materialKinds_.find( collisionType )->second );
	changedMaterials_.insert( mat->identifier() );
	return true;
}

std::string EditorChunkModel::getMaterialEnum( const std::string& name ) const
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_SCALAR, D3DXPC_SCALAR, D3DXPT_INT, D3DXPT_INT );
	MaterialIntProxy* proxy = (MaterialIntProxy*)( mp.property_.getObject() );
	return dxEnum_.name( mp.enumType_, proxy->get() );
}

bool EditorChunkModel::setMaterialEnum( const std::string& name, const std::string& enumValue )
{
	MaterialProp mp = findMaterialByName( name, D3DXPC_SCALAR, D3DXPC_SCALAR, D3DXPT_INT, D3DXPT_INT );
	MaterialIntProxy* proxy = (MaterialIntProxy*)( mp.property_.getObject() );
	proxy->set( dxEnum_.value( mp.enumType_, enumValue ), false );
	changedMaterials_.insert( mp.matName_ );
	return true;
}

void EditorChunkModel::edit( Moo::EffectMaterialPtr material, ChunkItemEditor & editor )
{
	ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
	if ( !pEffect )
		return;
	GeneralProperty* pProp;
#define NO_DEFAULT_MATERIAL_ATTRIBUTES
#ifndef NO_DEFAULT_MATERIAL_ATTRIBUTES
	//Add the two default properties; material kind and collision flags
	if( collisionFlags_.empty() )
	{
		DataSectionPtr pFile = BWResource::openSection( "resources/flags.xml" );
		DataSectionPtr pSect = pFile->openSection( "collisionFlags" );
		MF_ASSERT( pSect.hasObject() );

		for (DataSectionIterator it = pSect->begin(); it != pSect->end(); ++it )
		{
			std::string name = pSect->unsanitise((*it)->sectionName());
			if( collisionFlags_.find( name ) == collisionFlags_.end() )
			{
				collisionFlags_[ name ] = ( *it )->asInt();
				collisionFlagNames_.push_back( name );
			}
		}
	}

	pProp = new ListTextProperty( "Collision Flags",
		new AccessorDataProxyWithName< EditorChunkModel, StringProxy >(
			this, material->identifier() + "/" + "Collision Flags",
			getMaterialCollision, setMaterialCollision ), collisionFlagNames_ );
	pProp->setGroup( std::string( "Material/" ) + material->identifier() );
	editor.addProperty( pProp );

	// load the material kinds
	if( s_materialKinds_.empty() )
	{
		s_materialKinds_[ "(Use Visual's)" ] = 0;
		MaterialKinds::instance().createDescriptionMap( s_materialKinds_ );		
	}

	pProp = new ListTextProperty( "Material Kind",
		new AccessorDataProxyWithName< EditorChunkModel, StringProxy >(
			this, material->identifier() + "/" + "Material Kind",
			getMaterialKind, setMaterialKind ), materialKindNames_ );
	pProp->setGroup( std::string( "Material/" ) + material->identifier() );
	editor.addProperty( pProp );
#endif// NO_DEFAULT_MATERIAL_ATTRIBUTES
	//Now add the material's own properties.
	material->replaceDefaults();

	std::vector<Moo::EffectPropertyPtr> existingProps;

	if ( material->pEffect() )
	{
		ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
		if ( !pEffect )
			return;

		Moo::EffectMaterial::Properties& properties = material->properties();
		Moo::EffectMaterial::Properties::iterator it = properties.begin();
		Moo::EffectMaterial::Properties::iterator end = properties.end();

		while ( it != end )
		{
			MF_ASSERT( it->second );
			D3DXHANDLE hParameter = it->first;
			Moo::EffectPropertyPtr& pProperty = it->second;

			//Skip over properties that we have already added.  This can occur
			//when using multi-layer effects - there will most likely be
			//shared properties referenced by both effects.
			std::vector<Moo::EffectPropertyPtr>::iterator fit =
				std::find(existingProps.begin(),existingProps.end(),pProperty);
			if ( fit != existingProps.end() )
			{
				it++;
				continue;
			}

			existingProps.push_back(pProperty);

			if ( ToolsCommon::isEval() ||
				(Options::getOptionInt("objects/materialOverrideMode", 0)) ||
				MaterialUtility::worldBuilderEditable( &*pEffect, hParameter ) )
			{
				D3DXPARAMETER_DESC desc;
				HRESULT hr = pEffect->GetParameterDesc( hParameter, &desc );
				if ( SUCCEEDED(hr) )
				{
					if( desc.Class == D3DXPC_SCALAR && desc.Type == D3DXPT_BOOL )
					{
						pProp = new GenBoolProperty( matUIName( pEffect, hParameter, desc.Name ),
							new AccessorDataProxyWithName< EditorChunkModel, BoolProxy >(
								this, material->identifier() + "/" + desc.Name, 
								&EditorChunkModel::getMaterialBool, 
								&EditorChunkModel::setMaterialBool ) );
					}
					else if( desc.Class == D3DXPC_OBJECT &&
						( desc.Type == D3DXPT_TEXTURE || desc.Type == D3DXPT_TEXTURE1D ||
						desc.Type == D3DXPT_TEXTURE2D || desc.Type == D3DXPT_TEXTURE3D ||
						desc.Type == D3DXPT_TEXTURECUBE ) )
					{
						pProp = new TextProperty( matUIName( pEffect, hParameter, desc.Name ),
							new AccessorDataProxyWithName< EditorChunkModel, StringProxy >(
								this, material->identifier() + "/" + desc.Name, 
								&EditorChunkModel::getMaterialString, 
								&EditorChunkModel::setMaterialString ) );
						((TextProperty*)pProp)->fileFilter( "Texture files(*.jpg;*.tga;*.bmp)|*.jpg;*.tga;*.bmp||" );
						((TextProperty*)pProp)->canTextureFeed( false );
					}
					else if( desc.Class == D3DXPC_SCALAR && desc.Type == D3DXPT_FLOAT )
					{
						pProp = new GenFloatProperty( matUIName( pEffect, hParameter, desc.Name ),
							new AccessorDataProxyWithName< EditorChunkModel, FloatProxy >(
								this, material->identifier() + "/" + desc.Name, 
								&EditorChunkModel::getMaterialFloat, 
								&EditorChunkModel::setMaterialFloat, 
								&EditorChunkModel::getMaterialFloatRange,
								&EditorChunkModel::getMaterialFloatDefault, 
								&EditorChunkModel::setMaterialFloatToDefault ) );
					}
					else if( desc.Class == D3DXPC_VECTOR && desc.Type == D3DXPT_FLOAT )
					{
						std::string UIWidget = MaterialUtility::UIWidget( pEffect.pComObject(), hParameter );

						if ((UIWidget == "Color") || (UIWidget == "Colour"))
						{
							pProp = new ColourProperty( matUIName( pEffect, hParameter, desc.Name ),
								new AccessorDataProxyWithName< EditorChunkModel, Vector4Proxy >(
									this, material->identifier() + "/" + desc.Name, 
									&EditorChunkModel::getMaterialVector4, 
									&EditorChunkModel::setMaterialVector4 ) );
						}
						else // Must be a vector
						{
							pProp = new Vector4Property( matUIName( pEffect, hParameter, desc.Name ),
								new AccessorDataProxyWithName< EditorChunkModel, Vector4Proxy >(
									this, material->identifier() + "/" + desc.Name, 
									&EditorChunkModel::getMaterialVector4, 
									&EditorChunkModel::setMaterialVector4 ) );
						}
					}
					else if( desc.Class == D3DXPC_SCALAR && desc.Type == D3DXPT_INT )
					{
						D3DXHANDLE enumHandle = pEffect->GetAnnotationByName( hParameter, "EnumType" );
						LPCSTR enumType = NULL;
						if( enumHandle )
						{
							D3DXPARAMETER_DESC enumPara;
							if( SUCCEEDED( pEffect->GetParameterDesc( enumHandle, &enumPara ) ) &&
								enumPara.Type == D3DXPT_STRING &&
								SUCCEEDED( pEffect->GetString( enumHandle, &enumType ) ) )
							{
								if( dxEnum_.isEnum( enumType ) )
								{
									std::vector<std::string> materialEnumNames;
									for( DXEnum::size_type i = 0; i < dxEnum_.size( enumType ); ++i )
										materialEnumNames.push_back( dxEnum_.entry( enumType, i ) );
									pProp = new ListTextProperty(
										matUIName( pEffect, hParameter, desc.Name ),
										new AccessorDataProxyWithName< EditorChunkModel, StringProxy >(
											this, material->identifier() + "/" + desc.Name,
											&EditorChunkModel::getMaterialEnum, 
											&EditorChunkModel::setMaterialEnum ), 
										materialEnumNames );
								}
								else
									enumType = NULL;
							}
						}
						if( !enumType )
							pProp = new GenIntProperty( matUIName( pEffect, hParameter, desc.Name ),
								new AccessorDataProxyWithName< EditorChunkModel, IntProxy >(
									this, material->identifier() + "/" + desc.Name, 
									&EditorChunkModel::getMaterialInt, 
									&EditorChunkModel::setMaterialInt, 
									&EditorChunkModel::getMaterialIntRange ) );
					}
					else if( ( desc.Class == D3DXPC_MATRIX_ROWS && desc.Type == D3DXPT_FLOAT ) ||
						( desc.Class == D3DXPC_MATRIX_COLUMNS && desc.Type == D3DXPT_FLOAT ) )
					{
						pProp = new GenMatrixProperty( matUIName( pEffect, hParameter, desc.Name ),
							new AccessorDataProxyWithName< EditorChunkModel, MatrixProxy >(
								this, material->identifier() + "/" + desc.Name, 
								&EditorChunkModel::getMaterialMatrix, 
								&EditorChunkModel::setMaterialMatrix ) );
					}
					pProp->UIDesc( MaterialUtility::UIDesc( pEffect.pComObject(), hParameter ) );
					pProp->canExposeToScript( false );
					pProp->setGroup( std::string( "Material/" ) + material->identifier() );
					editor.addProperty( pProp );
				}
				else
				{
					ERROR_MSG( "MaterialUtility::listProperties - GetParameterDesc \
							   failed with DX error code %lx\n", hr );
				}
			}

			it++;
		}
	}
}


/**
 *	This method adds this item's properties to the given editor
 */
bool EditorChunkModel::edEdit( ChunkItemEditor & editor )
{
	// can only move this model if it's not in the shells directory
	if (pSuperModel_ == NULL || pSuperModel_->nModels() != 1 ||
		_stricmp( "shells", pSuperModel_->topModel(0)->
			resourceID().substr(8,6).c_str() ) != 0)
	{
		MatrixProxy * pMP = new ChunkItemMatrix( this );
		editor.addProperty( new ChunkItemPositionProperty(
			L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/POSITION"), pMP, this ) );
		editor.addProperty( new GenRotationProperty(
			L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/ROTATION"), pMP ) );
		editor.addProperty( new GenScaleProperty(
			L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/SCALE"), pMP ) );

		// can affect shadow?
		editor.addProperty( new GenBoolProperty(
			L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/CASTS_SHADOW"),
			new AccessorDataProxy< EditorChunkModel, BoolProxy >(
				this, "castsShadow", 
				&EditorChunkModel::getCastsShadow, 
				&EditorChunkModel::setCastsShadow ) ) );

		// can flag models as outside-only
		editor.addProperty( new GenBoolProperty(
			L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/OUTSIDE_ONLY"),
			new AccessorDataProxy< EditorChunkModel, BoolProxy >(
				this, "outsideOnly", 
				&EditorChunkModel::getOutsideOnly, 
				&EditorChunkModel::setOutsideOnly ) ) );
	}
	// Xiaoming Shi : Following code is commented out,
	// users can no longer put a shell via model tab.
	// Xiaoming Shi : show mercy to shell, let them move, for bug 4754{
/*	else
	{
		MatrixProxy * pMP = new ChunkItemMatrix( this );
		editor.addProperty( new GenPositionProperty( "position", pMP ) );
	}*/
	// Xiaoming Shi : show mercy to shell, let them move, for bug 4754}


	editor.addProperty( new GenBoolProperty(
		L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/REFLECTION_VISIBLE"),
		new AccessorDataProxy< EditorChunkModel, BoolProxy >(
			this, "reflectionVisible",
			&EditorChunkModel::getReflectionVis,
			&EditorChunkModel::setReflectionVis ) ) );

	editor.addProperty( new ListTextProperty(
		L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/ANIMATION"),
		new AccessorDataProxy< EditorChunkModel, StringProxy >(
			this, "animation", 
			&EditorChunkModel::getAnimation, 
			&EditorChunkModel::setAnimation ), animationNames_ ) );

	for( std::map<std::string, std::vector<std::string> >::iterator iter = dyeTints_.begin();
		iter != dyeTints_.end(); ++iter )
	{
		ListTextProperty* ltProperty = 
			new ListTextProperty( std::string( iter->first ),
				new AccessorDataProxyWithName< EditorChunkModel, StringProxy >(
					this, std::string( iter->first ), 
					&EditorChunkModel::getDyeTints, 
					&EditorChunkModel::setDyeTints ), 
					iter->second );
		ltProperty->setGroup( "dye" );
		editor.addProperty( ltProperty );
	}

	for( std::vector<ChunkMaterialPtr>::iterator iter = materialOverride_.begin();
		iter != materialOverride_.end(); ++iter )
	{
		edit( (*iter)->material_, editor );
	}

	editor.addProperty( new GenFloatProperty(
		L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/ANIMATION_SPEED"),
			new AccessorDataProxy< EditorChunkModel, FloatProxy >(
				this, "animation speed", 
				&EditorChunkModel::getAnimRateMultiplier, 
				&EditorChunkModel::setAnimRateMultiplier ) ) );



	std::string modelNames = "";
	if (pSuperModel_ != NULL)
	{
		for (int i = 0; i < pSuperModel_->nModels(); i++)
		{
			if (i) modelNames += ", ";
			modelNames += pSuperModel_->topModel(i)->resourceID();
		} 
	}

	editor.addProperty( new StaticTextProperty( pSuperModel_->nModels() == 1 ?
		L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/MODEL_NAME") :
		L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/MODEL_NAMES"), new
		ConstantDataProxy<StringProxy>( modelNames ) ) );

	if (pSuperModel_ != NULL)
	{
		MatterDescs	mds;

		// for each model
		for (int i = 0; i < pSuperModel_->nModels(); i++)
		{
			ModelPtr pTop = pSuperModel_->topModel(i);

			// get each matter element pointer
			for (int j = 0; ; j++)
			{
				const Matter * pmit = pTop->lookupLocalMatter( j );
				if (pmit == NULL)
					break;

				// and collect its tints it has
				const Matter::Tints & mts = pmit->tints_;
				for (Matter::Tints::const_iterator tit = mts.begin()+1;
					tit != mts.end();	// except default...
					tit++)
				{
					mds[ pmit->name_ ].tintNames.insert( (*tit)->name_ );
				}
			}
		}

		// now add them all as properties
		for (MatterDescs::iterator it = mds.begin(); it != mds.end(); it++)
		{
			editor.addProperty( new ModelDyeProperty(
				it->first, "Default", it->second, this ) );
		}
	}

	return true;
}


/**
 *	Find the drop chunk for this item
 */
Chunk * EditorChunkModel::edDropChunk( const Vector3 & lpos )
{
	Vector3 npos = pChunk_->transform().applyPoint( lpos );

	Chunk * pNewChunk = NULL;

	if ( !this->outsideOnly_ )
		pNewChunk = pChunk_->space()->findChunkFromPoint( npos );
	else
		pNewChunk = EditorChunk::findOutsideChunk( npos );

	if (pNewChunk == NULL)
	{
		ERROR_MSG( "Cannot move %s to (%f,%f,%f) "
			"because it is not in any loaded chunk!\n",
			this->edDescription().c_str(), npos.x, npos.y, npos.z );
		return NULL;
	}

	return pNewChunk;
}



/**
 * Are we the interior mesh for the chunk?
 *
 * We check by seeing if the model is in the shells directory
 */
bool EditorChunkModel::isShellModel() 
{
	return ChunkModel::isShellModel( pOwnSect() );
}

/**
 * Which section name shall we use when saving?
 */
const char* EditorChunkModel::sectionName()
{
	return isShellModel() ? "shell" : "model";
}

/**
 * Look in the .model file to see if it's nodeless or nodefull
 */
void EditorChunkModel::detectModelType()
{
	isModelNodeless_ = true;

	std::vector<std::string> models;
	pOwnSect_->readStrings( "resource", models );

	std::vector<Moo::VisualPtr> v;
	v.reserve( models.size() );

	for (uint i = 0; i < models.size(); i++)
	{
		DataSectionPtr modelSection = BWResource::openSection( models[i] );
		if (!modelSection)
			continue;

		std::string visualName = modelSection->readString( "nodelessVisual" );

		if (visualName.empty())
		{
			isModelNodeless_ = false;
			return;
		}
	}
}

std::string EditorChunkModel::edDescription()
{
	if (isShellModel())
	{
		if (pChunk_)
		{
			return L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/ED_DESCRIPTION", pChunk_->identifier() );
		}
		else
		{
			return L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/UNKNOWN_CHUNK");
		}
	}
	else
	{
		//return ChunkModel::edDescription();
		return desc_;
	}
}

std::vector<std::string> EditorChunkModel::edCommand( const std::string& path ) const
{
	std::vector<std::string> commands;
	std::vector<std::string> models;
	pOwnSect_->readStrings( "resource", models );
	if( !models.empty() )
	{
		commands.push_back( L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/EDIT_IN_MODEL_EDITOR") );
/*		if( animationNames_.size() > 1 )
		{
			commands.push_back( "animation: (default)" );
			for( std::vector<std::string>::const_iterator iter = animationNames_.begin() + 1;
				iter != animationNames_.end(); ++iter )
				commands.insert( commands.end(), std::string( "animation: " ) + *iter );
		}*/
	}
	return commands;
}

bool EditorChunkModel::edExecuteCommand( const std::string& path, std::vector<std::string>::size_type index )
{
	if( path.empty() && index == 0 )
	{
		std::vector<std::string> models;
		pOwnSect_->readStrings( "resource", models );

		char exe[ 1024 ];
		GetModuleFileName( NULL, exe, sizeof( exe ) );
		if( !models.empty() && std::count( exe, exe + strlen( exe ), '\\' ) > 2 )
		{
			*strrchr( exe, '\\' ) = 0;
			*strrchr( exe, '\\' ) = 0;
			std::string path = exe;
			path += "\\modeleditor\\";

			if( ToolsCommon::isEval() )
				strcat( exe, "\\modeleditor\\modeleditor_eval.exe" );
			else
				strcat( exe, "\\modeleditor\\modeleditor.exe" );

			std::string commandLine = exe;
			commandLine += " -o ";
			commandLine += "\"" + BWResource::resolveFilename( models[0] ) + "\"";
			std::replace( commandLine.begin(), commandLine.end(), '/', '\\' );

			commandLine += ' ';
			commandLine += BWResource::getPathAsCommandLine();

			PROCESS_INFORMATION pi;
			STARTUPINFO si;
			GetStartupInfo( &si );

			if( CreateProcess( exe, (LPSTR)commandLine.c_str(), NULL, NULL, FALSE, 0, NULL, path.c_str(),
				&si, &pi ) )
			{
				CloseHandle( pi.hThread );
				CloseHandle( pi.hProcess );
			}
		}
		return true;
	}
	else if( path.empty() && animationNames_.size() > 1 && index - 1 < animationNames_.size() )
	{
		setAnimation( animationNames_[ index - 1 ] );
	}

	return false;
}

Vector3 EditorChunkModel::edMovementDeltaSnaps()
{
	if (isShellModel())
	{
		return Options::getOptionVector3( "shellSnaps/movement", Vector3( 0.f, 0.f, 0.f ) );
	}
	else
	{
		return EditorChunkItem::edMovementDeltaSnaps();
	}
}

float EditorChunkModel::edAngleSnaps()
{
	if (isShellModel())
	{
		return Options::getOptionFloat( "shellSnaps/angle", 0.f );
	}
	else
	{
		return EditorChunkItem::edAngleSnaps();
	}
}

bool EditorChunkModel::setAnimation(const std::string & newAnimationName)
{
	if (newAnimationName.empty())
	{
		animName_ = "";
		if (pAnimation_)
		{
			const FashionPtr pFashion = pAnimation_;
			fv_.erase( std::find( fv_.begin(), fv_.end(), pFashion ) );
		}

		pAnimation_ = 0;

		return true;
	}
	else
	{
		SuperModelAnimationPtr newAnimation = pSuperModel_->getAnimation(
			newAnimationName );

		if (!newAnimation)
			return false;

		if (newAnimation->pSource( *pSuperModel_ ) == NULL)
			return false;

		newAnimation->time = 0.f;
		newAnimation->blendRatio = 1.0f;

		if (pAnimation_)
		{
			const FashionPtr pFashion = pAnimation_;
			fv_.erase( std::find( fv_.begin(), fv_.end(), pFashion ) );
		}

		pAnimation_ = newAnimation;
		fv_.push_back( pAnimation_ );

		animName_ = newAnimationName;

		return true;
	}
}

std::string EditorChunkModel::getDyeTints( const std::string& dye ) const
{
	if( tintName_.find( dye ) == tintName_.end() )
		return L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_MODEL/DEFAULT_TINT_NAME");
	return tintName_.find( dye )->second;
}

bool EditorChunkModel::setDyeTints( const std::string& dye, const std::string& tint )
{
	SuperModelDyePtr newDye = pSuperModel_->getDye( dye, tint );

	if( !newDye )
		return false;

	if( tintMap_[ dye ] )
	{
		const FashionPtr pFashion = tintMap_[ dye ];
		fv_.erase( std::find( fv_.begin(), fv_.end(), pFashion ) );
	}

	tintMap_[ dye ] = newDye;
	fv_.push_back( tintMap_[ dye ] );

	tintName_[ dye ] = tint;

	return true;
}

bool EditorChunkModel::setAnimRateMultiplier( const float& f )
{
	if ( f < 0.f )
		return false;

	// limit animation preview speed to 100x the original speed
	float mult = f;
	if ( mult > 100.0f )
		mult = 100.0f;

	animRateMultiplier_ = mult;

	return true;
}


bool EditorChunkModel::resourceIsOutsideOnly() const
{
	if ( !pOwnSect_ )
		return false;

	DataSectionPtr modelResource = BWResource::openSection(
		pOwnSect_->readString( "resource") ) ;
	if ( modelResource )
	{
		return modelResource->readBool( "editorOnly/outsideOnly", false );
	}

	return false;
}


bool EditorChunkModel::setOutsideOnly( const bool& outsideOnly )
{
	if ( outsideOnly_ != outsideOnly )
	{
		//We cannot turn off outsideOnly, if the model resource
		//specifies it to be true.
		if ( !outsideOnly && this->resourceIsOutsideOnly() )
		{
			ERROR_MSG( "Cannot turn off outsideOnly because the .model file overrides the chunk entry\n" );
			return false;
		}

		outsideOnly_ = outsideOnly;
		if (!edTransform( transform_, false ))
		{
			ERROR_MSG( "Changed outsideOnly flag, but could not change the chunk for this model\n" );
		}
		return true;
	}

	return false;
}


bool EditorChunkModel::setCastsShadow( const bool& castsShadow )
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
		if (isModelNodeless())
			StaticLighting::markChunk( pChunk_ );

		return true;
	}

	return false;
}


/// Write the factory statics stuff
/// Write the factory statics stuff
#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk)
IMPLEMENT_CHUNK_ITEM( EditorChunkModel, model, 1 )
IMPLEMENT_CHUNK_ITEM_ALIAS( EditorChunkModel, shell, 1 )

// editor_chunk_model.cpp
