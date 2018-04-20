/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// editor_chunk_substance.ipp

#include "worldeditor/world/items/editor_chunk_substance.hpp"
#include "worldeditor/project/project_module.hpp"
#include "chunk/chunk_model_obstacle.hpp"
#include "chunk/chunk_model.hpp"
#include "appmgr/module_manager.hpp"
#include "appmgr/options.hpp"
#include "romp/fog_controller.hpp"

/**
 *	@file This file contains the implementations of the template methods
 *	in EditorChunkSubstance
 */


/**
 *	Load method
 */
template <class CI> bool EditorChunkSubstance<CI>::load( DataSectionPtr pSect )
{
	pOwnSect_ = pSect;
	edCommonLoad( pSect );
	return this->CI::load( pSect );
}


/**
 *	Load method for items whose load method has a chunk pointer
 */
template <class CI> bool EditorChunkSubstance<CI>::load(
	DataSectionPtr pSect, Chunk * pChunk )
{
	pOwnSect_ = pSect;
	edCommonLoad( pSect );
	return this->CI::load( pSect, pChunk );
}


/**
 *	This method does extra stuff when this item is tossed between chunks.
 *
 *	It updates its datasection in that chunk.
 */
template <class CI> void EditorChunkSubstance<CI>::toss( Chunk * pChunk )
{
	if (pChunk_ != NULL)
	{
		ChunkModelObstacle::instance( *pChunk_ ).delObstacles( this );

		if (pOwnSect_)
		{
			EditorChunkCache::instance( *pChunk_ ).
				pChunkSection()->delChild( pOwnSect_ );
			pOwnSect_ = NULL;
		}
	}

	this->CI::toss( pChunk );

	if (pChunk_ != NULL)
	{
		if (!pOwnSect_)
		{
			pOwnSect_ = EditorChunkCache::instance( *pChunk_ ).
				pChunkSection()->newSection( this->sectName() );
			this->edSave( pOwnSect_ );
		}

		this->addAsObstacle();
	}
}

/**
 * If we want to draw or not
 */
template <class CI> bool EditorChunkSubstance<CI>::edShouldDraw()
{
	if( !CI::edShouldDraw() )
		return false;
	if (Moo::rc().frameTimestamp() != s_settingsMark_)
	{
		s_drawAlways_ = Options::getOptionBool(
			this->drawFlag(), true ) && Options::getOptionInt( "render/scenery", 1 );
		s_settingsMark_ = Moo::rc().frameTimestamp();
	}

	return s_drawAlways_;
}


/**
 *	Draw method
 */
template <class CI> void EditorChunkSubstance<CI>::draw()
{
	uint32 currentFrame = s_settingsMark_;
	this->CI::draw();
	
	if (!edShouldDraw())
		return;

	Moo::rc().push();
	Moo::rc().preMultiply( this->edTransform() );

	Model::incrementBlendCookie();

	if (WorldManager::instance().drawSelection())
	{
		WorldManager::instance().registerDrawSelectionItem(
													(EditorChunkItem*) this );
	}

	static int renderMiscShadeReadOnlyAreas = 1;
	if (Moo::rc().frameTimestamp() != currentFrame)
		renderMiscShadeReadOnlyAreas =
			Options::getOptionInt( "render/misc/shadeReadOnlyAreas", 1 );
	bool drawRed = !EditorChunkCache::instance( *pChunk_ ).edIsWriteable() && 
		 renderMiscShadeReadOnlyAreas != 0;
	if( !drawRed || !WorldManager::instance().drawSelection())
	{
		bool projectModule = ProjectModule::currentInstance() == ModuleManager::instance().currentModule();
		if( drawRed && !projectModule )
			WorldManager::instance().setReadOnlyFog();
		ModelPtr mp = this->reprModel();
		if (mp)
		{
			mp->dress();	// should really be using a supermodel...
			mp->draw( true );
		}
		if( drawRed && !projectModule )
			FogController::instance().commitFogToDevice();
	}

	Moo::rc().pop();
}


/**
 *	Get the bounding box for this substance
 */
template <class LT> void EditorChunkSubstance<LT>::edBounds(
	BoundingBox& bbRet ) const
{
	ModelPtr model = this->reprModel();
	// Get the size of the representative model
	if (model)
		bbRet = model->boundingBox();
	// If there is not representative model then we return an arbitrary box
	else
		bbRet = BoundingBox(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.1f, 0.1f, 0.1f));
}


/**
 *	Add this itme's representation model as an obstacle. Normally won't
 *	need to be overridden, but some items have special requirements.
 */
template <class LT> void EditorChunkSubstance<LT>::addAsObstacle()
{
	Matrix world( pChunk_->transform() );
	world.preMultiply( this->edTransform() );

	ModelPtr model = this->reprModel();
	if (model)
	{
		ChunkModelObstacle::instance( *pChunk_ ).addModel(
			model, world, this );
	}
}


/**
 *	Reload this item and report on success
 */
template <class LT> bool EditorChunkSubstance<LT>::reload()
{
	return this->load( pOwnSect_ );
}


template <class LT> bool EditorChunkSubstance<LT>::addYBounds( BoundingBox& bb ) const
{
	// sorry for the conversion
	bb.addYBounds( ( ( EditorChunkSubstance<LT>* ) this)->edTransform().applyToOrigin().y );
	return true;
}

template <class CI> uint32 EditorChunkSubstance<CI>::s_settingsMark_ = -16;
template <class CI> bool EditorChunkSubstance<CI>::s_drawAlways_ = true;


// editor_chunk_substance.ipp
