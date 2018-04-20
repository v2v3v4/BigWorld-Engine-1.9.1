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

#include "cstdmf/guard.hpp"

#include "chunk_umbra.hpp"
#include "chunk_manager.hpp"
#include "chunk_space.hpp"
#include "speedtree/speedtree_renderer.hpp"
#include "moo/dynamic_vertex_buffer.hpp"
#include "moo/dynamic_index_buffer.hpp"


#if UMBRA_ENABLE

// The library to use, to link with the debug version of umbra change it to umbrad.lib,
// that will also need the debug dll of umbra.
#pragma comment(lib, "umbra.lib")

#include "romp/line_helper.hpp"

#include "moo/material.hpp"
#include "moo/visual.hpp"
#include "moo/visual_compound.hpp"
#include "terrain/base_terrain_renderer.hpp"

#ifdef EDITOR_ENABLED
	#include "appmgr/options.hpp"
#endif

#include <umbraCommander.hpp>
#include <umbraObject.hpp>
#include <umbraLibrary.hpp>

#include "umbra_proxies.hpp"


DECLARE_DEBUG_COMPONENT2( "Chunk", 0 );

PROFILER_DECLARE( ChunkCommander_command, "ChunkCommander Command" );
PROFILER_DECLARE( ChunkCommander_occlusionStall, "ChunkCommander Occlusion Stall" );
PROFILER_DECLARE( ChunkCommander_occlusionQuery, "ChunkCommander Occlusion Query" );

static bool clipEnabled = false;

// -----------------------------------------------------------------------------
// Section: ChunkCommander definition
// -----------------------------------------------------------------------------

/*
 *	This class implements the Umbra commander interface.
 *	The commander is the callback framework from the umbra scene
 *	traversal.
 */
class ChunkCommander : public Umbra::Commander
{
public:

	ChunkCommander(ChunkUmbraServices*);
	virtual void command	(Command c);
	void repeat(void);
private:
	void flush(void);
	void enableDepthTestState(void);
	void disableDepthTestState(void);

	void drawItem(ChunkItem* pItem);
	void drawStencilModel(UmbraPortal* portal, const Matrix& worldMtx);

	Chunk*						pLastChunk_;
	std::vector< ChunkItem* >	cachedItems_;
	bool						depthStateEnabled_;
	DWORD						oldCullMode_;
	DWORD						oldAlphaTestMode_;
	DWORD						oldWriteMask1_;
	ChunkUmbraServices*			pServices_;
	int							inReflection_;
	Matrix						storedView_;
	bool						viewParametersChanged_;
};

// -----------------------------------------------------------------------------
// Section: ChunkUmbraServices definition
// -----------------------------------------------------------------------------

/*
 *	This class overrides some of the umbra services
 */
class ChunkUmbraServices : public Umbra::LibraryDefs::Services
{
public:
		virtual void error(const char* message);
		virtual void enterMutex(void);
		virtual void leaveMutex(void);
		virtual bool allocateQueryObject(int index);
		virtual void releaseQueryObject(int index);
		
		Moo::OcclusionQuery* getQuery(int index) { return queries_[index]; }
private:
		SimpleMutex mutex_;
		std::vector< Moo::OcclusionQuery* >	queries_;
};

// -----------------------------------------------------------------------------
// Section: ChunkUmbra
// -----------------------------------------------------------------------------

/**
 *	This method inits the chunkumbra instance
 */
void ChunkUmbra::init()
{
	BW_GUARD;
	IF_NOT_MF_ASSERT_DEV( s_pInstance_ == NULL )
	{
		return;
	}
	s_pInstance_ = new ChunkUmbra;
	UmbraHelper::instance().init();
}

/**
 *	This method destroys the chunkumbra instance
 */
void ChunkUmbra::fini()
{
	BW_GUARD;
	UmbraHelper::instance().fini();
	UmbraModelProxy::invalidateAll();
	UmbraObjectProxy::invalidateAll();
	if (s_pInstance_)
	{
		delete s_pInstance_;
		s_pInstance_ = NULL;
	}
}

/**
 *	Return the commander instance.
 */
Umbra::Commander* ChunkUmbra::pCommander()
{
	BW_GUARD;
	IF_NOT_MF_ASSERT_DEV( s_pInstance_ )
	{
		return NULL;
	}

	return s_pInstance_->pCommander_;
}

/*
 *	The constructor inits umbra and creates our commander
 */
ChunkUmbra::ChunkUmbra() :
	softwareMode_(false),
	clipPlaneSupport_(true)
{
	BW_GUARD;
	pServices_ = new ChunkUmbraServices();

	// Try to create an occlusion query object to see if the hardware supports them
	Moo::OcclusionQuery* testQuery = Moo::rc().createOcclusionQuery();

	// We don't support hardware occlusion queries on fixed function hardware.
	// Although D3D says it supports it, D3D crashes when a query is used.
	if ( Moo::rc().psVersion() > 0 && testQuery )
	{
		Umbra::Library::init(Umbra::LibraryDefs::COLUMN_MAJOR, Umbra::LibraryDefs::HARDWARE_OCCLUSION, pServices_);
		Moo::rc().destroyOcclusionQuery(testQuery);
		testQuery = NULL;
	}
	else
	{
		Umbra::Library::init(Umbra::LibraryDefs::COLUMN_MAJOR, Umbra::LibraryDefs::SOFTWARE_OCCLUSION, pServices_);
		softwareMode_ = true;
	}

	clipPlaneSupport_ = (Moo::rc().deviceInfo( Moo::rc().deviceIndex() ).caps_.MaxUserClipPlanes > 0);

	pCommander_ = new ChunkCommander(pServices_);
}

/*
 *	Repeat the drawing calls from the last query
 */
void ChunkUmbra::repeat()
{
	BW_GUARD;
	s_pInstance_->pCommander_->repeat();
}


bool ChunkUmbra::softwareMode()
{
	BW_GUARD;
	return s_pInstance_->softwareMode_;
}


bool ChunkUmbra::clipPlaneSupported()
{
	BW_GUARD;
	return s_pInstance_->clipPlaneSupport_;
}


/*
 *	Tick method, needs to be called once per frame
 */
void ChunkUmbra::tick()
{
	BW_GUARD;
	Umbra::Library::resetStatistics();
}


/*
 *	The destructor uninits umbra and destroys our umbra related objects.
 */
ChunkUmbra::~ChunkUmbra()
{
	BW_GUARD;
	delete pCommander_;

	Umbra::Library::exit();
	delete pServices_;
}

ChunkUmbra* ChunkUmbra::s_pInstance_ = NULL;


// -----------------------------------------------------------------------------
// Section: ChunkCommander
// -----------------------------------------------------------------------------

ChunkCommander::ChunkCommander(ChunkUmbraServices* pServices) :
	pLastChunk_(NULL),
	depthStateEnabled_(false),
	viewParametersChanged_(false),
	pServices_(pServices),
	inReflection_(0)

{
}

/**
 * Redraws the scene, used for wireframe mode
 */
void ChunkCommander::repeat(void)
{
	BW_GUARD;
	Chunk* pLastChunk = NULL;
	int count = cachedItems_.size();
	for (int i = 0; i < count; i++)
	{
		ChunkItem* pItem = cachedItems_[i];
		Chunk* pChunk = pItem->chunk();
		if (pChunk != pLastChunk)
		{
			pChunk->drawCaches();
			pLastChunk = pChunk;
		}
		Moo::rc().push();
		Moo::rc().world(pChunk->transform());
		pItem->draw();
		Moo::rc().pop();
	}
	flush();
}


/**
 * Flush delayed rendering commands for occluders
 */
void ChunkCommander::flush()
{
	BW_GUARD;
	// don't flush in reflection
	if (inReflection_ > 0)
		return;

	disableDepthTestState();

#if SPEEDTREE_SUPPORT
	if (UmbraHelper::instance().flushTrees())
	{
		speedtree::SpeedTreeRenderer::flush();
	}
#endif

	// Make sure we are using the correct view matrix
	if (viewParametersChanged_ && inReflection_ == 0)
	{
		Moo::rc().view(storedView_);
		viewParametersChanged_ = false;
	}

	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (ChunkManager::instance().cameraSpace())
	{
		Moo::LightContainerPtr pRCLC = Moo::rc().lightContainer();
		Moo::LightContainerPtr pRCSLC = Moo::rc().specularLightContainer();
		static Moo::LightContainerPtr pLC = new Moo::LightContainer;
		static Moo::LightContainerPtr pSLC = new Moo::LightContainer;

		Moo::rc().lightContainer( pSpace->lights() );

		static DogWatch s_drawTerrain("Terrain draw");

		uint32 renderState= Moo::rc().getRenderState( D3DRS_FILLMODE );

	
		Moo::rc().setRenderState( D3DRS_FILLMODE,
			( UmbraHelper::instance().wireFrameTerrain() ) ?
				D3DFILL_WIREFRAME : D3DFILL_SOLID );
	
		s_drawTerrain.start();
		Terrain::BaseTerrainRenderer::instance()->drawAll();
		s_drawTerrain.stop();

		Moo::rc().setRenderState( D3DRS_FILLMODE, renderState);

		pLC->directionals().clear();
		pLC->ambientColour( ChunkManager::instance().cameraSpace()->ambientLight() );
		if (ChunkManager::instance().cameraSpace()->sunLight())
			pLC->addDirectional( ChunkManager::instance().cameraSpace()->sunLight() );
		pSLC->directionals().clear();
		pSLC->ambientColour( ChunkManager::instance().cameraSpace()->ambientLight() );
		if (ChunkManager::instance().cameraSpace()->sunLight())
			pSLC->addDirectional( ChunkManager::instance().cameraSpace()->sunLight() );

		Moo::rc().lightContainer( pLC );
		Moo::rc().specularLightContainer( pSLC );

		Moo::VisualCompound::drawAll();
		Moo::Visual::drawBatches();

		Moo::rc().lightContainer( pRCLC );
		Moo::rc().specularLightContainer( pRCSLC );

		pLastChunk_ = NULL;
	}
}


/**
 * 
 */
void ChunkCommander::enableDepthTestState(void)
{
	BW_GUARD;
	if (depthStateEnabled_)
		return;

	DX::Device* dev = Moo::rc().device();

	Moo::rc().push();

	oldCullMode_ = Moo::rc().getRenderState( D3DRS_CULLMODE );
	oldAlphaTestMode_ = Moo::rc().getRenderState( D3DRS_ALPHATESTENABLE );
	oldWriteMask1_ = Moo::rc().getRenderState( D3DRS_COLORWRITEENABLE1 );

	// Set up state and draw.

	Moo::rc().setFVF( Moo::VertexXYZ::fvf() );
	Moo::rc().setVertexShader( NULL );
	Moo::rc().setPixelShader( NULL );

	Moo::rc().setRenderState( D3DRS_ALPHATESTENABLE, D3DZB_FALSE );
	Moo::rc().setRenderState( D3DRS_ZWRITEENABLE, D3DZB_FALSE );
	Moo::rc().setRenderState( D3DRS_ZENABLE, inReflection_ > 0 ? D3DZB_FALSE : D3DZB_TRUE );
	Moo::rc().setRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE, 0 );
	Moo::rc().setWriteMask( 1, 0 );

	Moo::rc().setRenderState( D3DRS_CULLMODE, D3DCULL_NONE  );

	// we need to disable reflection surface clipping plane on the query boxes
	if (Moo::rc().reflectionScene() || clipEnabled)
		Moo::rc().setRenderState(D3DRS_CLIPPLANEENABLE, 0);

	dev->SetTransform( D3DTS_WORLD, &Matrix::identity );
	dev->SetTransform( D3DTS_PROJECTION, &Moo::rc().projection() );

	depthStateEnabled_ = true;
}

/**
 *
 */
void ChunkCommander::disableDepthTestState(void)
{
	BW_GUARD;
	if (!depthStateEnabled_)
		return;

	DX::Device* dev = Moo::rc().device();

	// re-enable clipping plane 
	if (ChunkUmbra::clipPlaneSupported() && (Moo::rc().reflectionScene() || clipEnabled))
		Moo::rc().setRenderState(D3DRS_CLIPPLANEENABLE, 1);

	// restore state
	Moo::rc().setRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
	Moo::rc().setRenderState(D3DRS_ZWRITEENABLE, D3DZB_TRUE);
	Moo::rc().setRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
	Moo::rc().setWriteMask( 1, oldWriteMask1_ );
	Moo::rc().setRenderState( D3DRS_ALPHATESTENABLE, oldAlphaTestMode_ );
	Moo::rc().setRenderState( D3DRS_CULLMODE, oldCullMode_  );

	Moo::rc().pop();

	depthStateEnabled_ = false;
}

/**
 * This method is for debugging purposes only..
 * It flushes the rendering queue.
 */

static void sync()
{
	BW_GUARD;
	IDirect3DQuery9* pEventQuery;
	Moo::rc().device()->CreateQuery(D3DQUERYTYPE_EVENT, &pEventQuery);

	// Add an end marker to the command buffer queue.
	pEventQuery->Issue(D3DISSUE_END);

	// Force the driver to execute the commands from the command buffer.
	// Empty the command buffer and wait until the GPU is idle.
	while(S_FALSE == pEventQuery->GetData( NULL, 0, D3DGETDATA_FLUSH ))
		;

	pEventQuery->Release();
}

/**
 *
 */

void ChunkCommander::drawStencilModel(UmbraPortal* portal, const Matrix& worldMtx)
{
	BW_GUARD;
	// Set up state and draw.


	Moo::rc().setFVF( Moo::VertexXYZ::fvf() );

	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE, 0 );
	Moo::rc().setRenderState( D3DRS_CULLMODE, D3DCULL_NONE  );

	Moo::rc().push();
	Moo::rc().world(Matrix::identity);

	Moo::rc().device()->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 
										0, 
										portal->vertexCount(), 
										portal->triangleCount(), 
										portal->triangles(), 
										D3DFMT_INDEX16, 
										portal->vertices(), 
										sizeof(Vector3));

	Moo::rc().pop();
}

void setDepthRange(float n, float f)
{
	BW_GUARD;
	DX::Viewport vp;
	Moo::rc().getViewport(&vp);
	vp.MinZ = n;
	vp.MaxZ = f;
	Moo::rc().setViewport(&vp);
}


static bool colorPass = false;

/**
 *	This method implements our callback from the umbra scene traversal
 */
void ChunkCommander::command(Command c)
{
	BW_GUARD_PROFILER( ChunkCommander_command );
	static DogWatch s_commander( "Commander callback" );
	s_commander.start();
	switch (c)
	{
		// Begin traversal
		case QUERY_BEGIN:
		{
			storedView_ = Moo::rc().view();
			viewParametersChanged_ = false;

			disableDepthTestState();
			colorPass = false;
			Moo::rc().setRenderState(D3DRS_STENCILENABLE, FALSE);

			cachedItems_.clear();

			pLastChunk_ = NULL;

			ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
			if (pSpace)
				Moo::rc().lightContainer( pSpace->lights() );

			inReflection_ = 0;

			break;
		}

		// We have finished traversal
		case QUERY_END:
		{
			Moo::rc().view(storedView_);
			Moo::rc().setRenderState(D3DRS_STENCILENABLE, FALSE); 

			LineHelper::instance().purge();

			flush();

			MF_ASSERT_DEV(inReflection_ == 0);
			break;
		}
	
		// We have entered a portal
		case PORTAL_ENTER:
		{
			disableDepthTestState();
			// Get the chunk object for this portal
			const Instance* instance = getInstance();
			Umbra::Object*	object = instance->getObject();

			UmbraPortal* portal = (UmbraPortal*)object->getUserPointer();

			if (!portal)
				break;

			if (portal->reflectionPortal_)
			{
				++inReflection_;
			}

			break;
		}

		// We are leaving a portal
		case PORTAL_EXIT:
		{
			const Instance* instance = getInstance();
			Umbra::Object*	object = instance->getObject();
			UmbraPortal* portal = (UmbraPortal*)object->getUserPointer();
			if (!portal)
				break;
			if (portal->reflectionPortal_)
			{
				--inReflection_;
			}

			break;
		}

		case FLUSH_DEPTH:
		{
			flush();
			break;
		}

		case OCCLUSION_QUERY_BEGIN:
		{
			OcclusionQuery* query = getOcclusionQuery();
			Moo::OcclusionQuery* pOcclusionQuery = pServices_->getQuery( query->getIndex() );
			Moo::rc().beginQuery( pOcclusionQuery );

			break;
		};

		case OCCLUSION_QUERY_END:
		{
			OcclusionQuery* query = getOcclusionQuery();
			Moo::OcclusionQuery* pOcclusionQuery = pServices_->getQuery( query->getIndex() );
			Moo::rc().endQuery( pOcclusionQuery );

			Moo::rc().depthOnly( false );

			break;
		};

		case OCCLUSION_QUERY_GET_RESULT:
		{
			BW_GUARD_PROFILER( ChunkCommander_occlusionStall );
			static DogWatch s_boxQuery( "GetOcclusionResult" );
			s_boxQuery.start();
		
			OcclusionQuery* query = getOcclusionQuery();
			Moo::OcclusionQuery* pOcclusionQuery = pServices_->getQuery( query->getIndex() );

			int visiblePixels = 0;

			bool available = Moo::rc().getQueryResult(visiblePixels, pOcclusionQuery, query->getWaitForResult());
			
			query->setResult(available, visiblePixels);

			s_boxQuery.stop();

			break;
		}

		// An object is visible
		case OCCLUSION_QUERY_DRAW_TEST_DEPTH:
		{
			BW_GUARD_PROFILER( ChunkCommander_occlusionQuery );
			static DogWatch s_renderTestDepth( "IssueOcclusionQuery" );
			s_renderTestDepth.start();

			enableDepthTestState();

			OcclusionQuery* query = getOcclusionQuery();

			Umbra::Matrix4x4 obbToCamera;
			query->getToCameraMatrix((Umbra::Matrix4x4&)obbToCamera);

			DX::Device* dev = Moo::rc().device();

			dev->SetTransform( D3DTS_VIEW, (const D3DMATRIX*)&obbToCamera );

			uint32 vertexBase=0;
			bool locked = Moo::DynamicVertexBuffer<Moo::VertexXYZ>::instance().lockAndLoad( 
				(const Moo::VertexXYZ*)query->getVertices(), query->getVertexCount(), vertexBase );

			uint32 MAX_16_BIT_INDEX = 0xffff; 

	        // Umbra uses 32 bit indices, if 32 bit indices are supported,  
 	        // use umbras indices, otherwise we convert to 16 bit indices  
 		    // before rendering 
 	        if (locked && Moo::rc().maxVertexIndex() > MAX_16_BIT_INDEX)
 	        { 
 		        Moo::DynamicIndexBufferBase& dynamicIndexBuffer = Moo::rc().dynamicIndexBufferInterface().get( D3DFMT_INDEX32 ); 
 	
	            Moo::IndicesReference indices = dynamicIndexBuffer.lock2(  
 					query->getTriangleCount() * 3 ); 
 	            if (indices.valid()) 
 	            { 
 		            indices.fill( query->getTriangles(), 3 * query->getTriangleCount() ); 
 	 
 	                dynamicIndexBuffer.unlock(); 
 		 
 	                dynamicIndexBuffer.indexBuffer().set(); 
 	 
 	                Moo::DynamicVertexBuffer<Moo::VertexXYZ>::instance().set(); 
 	                Moo::rc().drawIndexedPrimitive( 
 		                D3DPT_TRIANGLELIST,  
 	                    vertexBase,  
 	                    0,  
 	                    query->getVertexCount(),   
 	                    dynamicIndexBuffer.lockIndex(), 
 	                    query->getTriangleCount() ); 
 	                } 
 	           } 
 	           // If there are more indices in the query than the maximum allowed 
 	           // skip this query 
 	        else if (uint32(query->getVertexCount()) <= Moo::rc().maxVertexIndex()) 
 	        { 
				Moo::DynamicIndexBufferBase& dynamicIndexBuffer = Moo::rc().dynamicIndexBufferInterface().get( D3DFMT_INDEX16 ); 
 		 
 	            Moo::IndicesReference indices = dynamicIndexBuffer.lock2(  
 					query->getTriangleCount() * 3 ); 
 		        if (indices.valid()) 
 	            { 
 		            static std::vector<uint16> indicesCopy; 
 	                indicesCopy.clear(); 
 	                indicesCopy.reserve(3 * query->getTriangleCount()); 
 	 
 	 
					const Umbra::Vector3i* pTriangles = query->getTriangles(); 
 	                uint32 triangleCount = query->getTriangleCount(); 
 	 
 	                for (uint32 i = 0; i < triangleCount; i++) 
 	                { 
 		                indicesCopy.push_back( uint16(pTriangles->i) ); 
 	                    indicesCopy.push_back( uint16(pTriangles->j) ); 
 	                    indicesCopy.push_back( uint16(pTriangles->k) ); 
 	                    ++pTriangles; 
 	                } 
 	 
 		 
 	                indices.fill( &indicesCopy.front(), 3 * query->getTriangleCount() ); 
 	 
 	                dynamicIndexBuffer.unlock(); 
 		 
 	                dynamicIndexBuffer.indexBuffer().set(); 
 		 
 	                Moo::DynamicVertexBuffer<Moo::VertexXYZ>::instance().set(); 
 	                Moo::rc().drawIndexedPrimitive( 
 		                D3DPT_TRIANGLELIST,  
 	                    vertexBase,  
 	                    0,  
 	                    query->getVertexCount(),   
 	                    dynamicIndexBuffer.lockIndex(), 
 	                    query->getTriangleCount() ); 
 		        } 
			} 
 	        else 
 	        { 
 				ERROR_MSG( "ChunkCommander::command: Umbra occlusion query " 
 					"draw request failed, too many vertices were requested " 
 		            "only %d supported\n", 
 	            Moo::rc().maxVertexIndex()); 
			}

			s_renderTestDepth.stop();
			break;
		}

		case INSTANCE_DRAW_DEPTH:
		{
			if (inReflection_ > 0)
				break;

			// Get the umbra object that is visible
			const Instance* instance = getInstance();
			Umbra::Object*	object = instance->getObject();

			// Get the chunkitem
			ChunkItem* pItem = (ChunkItem*)object->getUserPointer();
			if (pItem == NULL || pItem->chunk() == NULL)
				break;

			// TODO: make sure only actual occluders have the occlusion flag set

			// Set depth only mode if enabled
			if ( UmbraHelper::instance().depthOnlyPass() && !Moo::rc().reflectionScene() )
				Moo::rc().depthOnly( true );

			drawItem(pItem);

			break;
		}

		// An object is visible
		case INSTANCE_VISIBLE:
		{
			if ( !colorPass )
				Moo::rc().setRenderState(D3DRS_STENCILENABLE, FALSE);

			// Get the umbra object that is visible
			const Instance* instance = getInstance();
			Umbra::Object*	object = instance->getObject();

			// Get the chunkitem
			ChunkItem* pItem = (ChunkItem*)object->getUserPointer();

			// Break out if there object does not exist, it does not belong to a chunk
			// or it has already been drawn.
			if (pItem == NULL || 
				pItem->chunk() == NULL)
				break;

			if (inReflection_ > 0)
			{
				Chunk* pChunk = pItem->chunk();
				if (pChunk->reflectionMark() != Chunk::s_nextMark_)
				{
					pChunk->reflectionMark(Chunk::s_nextMark_);
					ChunkManager::instance().addToCache(pItem->chunk());
				}
				break;
			}

			colorPass = true;

			// This is only needed for wireframe mode
			cachedItems_.push_back(pItem);

			// when occlusion culling objects are rendered in OCCLUSION_QUERY_RENDER_INSTANCE_DEPTH
			// TODO objects that are not occluders should still be rendered here
			// TODO cache these until the end of resolvevisibility to improve parallelism
			//if (!UmbraHelper::instance().occlusionCulling() || !object->test(Umbra::Object::OCCLUDER))
			Moo::rc().depthOnly( false );
			drawItem(pItem);
			break;
		}

		case VIEW_PARAMETERS_CHANGED:
		{
			disableDepthTestState();
			flush();
			
			LineHelper::instance().purge();

			const Viewer* viewer = getViewer();

			Matrix view;
		
			viewer->getCameraToWorldMatrix((Umbra::Matrix4x4&)view);
			view.invert();
			Moo::rc().view(view);
			viewParametersChanged_ = true;

			if (viewer->isMirrored())
			{
				Moo::rc().setRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
				Moo::rc().mirroredTransform(true);
			}
			else
			{
				Moo::rc().setRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
				Moo::rc().mirroredTransform(false);
			}

			if (ChunkUmbra::clipPlaneSupported() && viewer->getFrustumPlaneCount() == 7)
			{
				// additional clipping plane for virtual portals
				Matrix proj = Moo::rc().projection();

				proj.invert();
				proj.transpose();
				Vector4 plane;
				viewer->getFrustumPlane(6, (Umbra::Vector4&)plane);
				D3DXPlaneTransform((D3DXPLANE*)&plane.x, (D3DXPLANE*)&plane.x, &proj);

				Moo::rc().setRenderState(D3DRS_CLIPPLANEENABLE, D3DCLIPPLANE0);
				clipEnabled = true;

				Moo::rc().device()->SetClipPlane(0, &plane.x);
			}
			else
			{
				Moo::rc().setRenderState(D3DRS_CLIPPLANEENABLE, 0);
				clipEnabled = false;
			}
			break;
		}

		case STENCIL_MASK:
		{	
			if (colorPass)
				break;

			disableDepthTestState();

			Umbra::Matrix4x4 objectToCamera;
			const Umbra::Commander::Instance* instance = getInstance();

			instance->getObjectToCameraMatrix(objectToCamera);
			const Umbra::Object* portalObject = instance->getObject();

			UmbraPortal* portal = (UmbraPortal*)portalObject->getUserPointer();

			Moo::Material::setVertexColour();

			//---------------------------------------------------
			// Increment/Decrement stencil buffer values
			//---------------------------------------------------

			int test,write;
			getStencilValues(test, write);
 			bool increment = (write>test);

			Moo::rc().setRenderState(D3DRS_STENCILENABLE, TRUE);
			Moo::rc().setRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
			Moo::rc().setRenderState(D3DRS_STENCILREF, test);
			Moo::rc().setRenderState(D3DRS_STENCILMASK, 0x3f);

			if(increment)
			{
				Moo::rc().setRenderState(D3DRS_STENCILFAIL , D3DSTENCILOP_KEEP);
				Moo::rc().setRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
				Moo::rc().setRenderState(D3DRS_STENCILPASS , D3DSTENCILOP_INCR);
			}
			else
			{
				Moo::rc().setRenderState(D3DRS_STENCILFAIL , D3DSTENCILOP_KEEP);
				Moo::rc().setRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_DECR);
				Moo::rc().setRenderState(D3DRS_STENCILPASS , D3DSTENCILOP_DECR);
			}

			Moo::rc().setRenderState(D3DRS_ZWRITEENABLE, D3DZB_FALSE);

			DWORD oldWriteMask1 = Moo::rc().getRenderState( D3DRS_COLORWRITEENABLE1 );
			Moo::rc().setWriteMask( 1, 0 );
			drawStencilModel(portal, (Matrix&)objectToCamera);

			//---------------------------------------------------
			// Restore state
			//---------------------------------------------------

			setDepthRange	(0.0, 1.0);

			Moo::rc().setRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

			Moo::rc().setRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
			Moo::rc().setWriteMask( 1, oldWriteMask1 );
			Moo::rc().setRenderState(D3DRS_ZWRITEENABLE, D3DZB_TRUE);

			//---------------------------------------------------
			// Set stencil variables to correct state for subsequent normal objects
			//---------------------------------------------------

			Moo::rc().setRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
			Moo::rc().setRenderState(D3DRS_STENCILREF, write);
			Moo::rc().setRenderState(D3DRS_STENCILMASK, 0x3f);

			Moo::rc().setRenderState(D3DRS_STENCILFAIL , D3DSTENCILOP_KEEP);
			Moo::rc().setRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
			Moo::rc().setRenderState(D3DRS_STENCILPASS , D3DSTENCILOP_KEEP);

			break;
		};

		// Umbra wants us to draw a 2d debug line
		case DRAW_LINE_2D:
		{
			disableDepthTestState();

			static Vector4 begin(0,0,0,1);
			static Vector4 end(0,0,0,1);
			static Moo::Colour colour;

			// Get a linehelper from umbra
			getLine2D((Umbra::Vector2&)begin, (Umbra::Vector2&)end, (Umbra::Vector4&)colour);

			// Add line to the linehelper
			LineHelper::instance().drawLineScreenSpace( begin, end, colour );
			break;
		}

		// Umbra wants us to draw a 3d debug line
		case DRAW_LINE_3D:
		{
			disableDepthTestState();

			Vector3 begin;
			Vector3 end;
			Moo::Colour colour;

			// Get a linehelper from umbra
			getLine3D((Umbra::Vector3&)begin, (Umbra::Vector3&)end, (Umbra::Vector4&)colour);

			// Add line to the linehelper
			LineHelper::instance().drawLine( begin, end, colour );
			break;
		}
	};
	s_commander.stop();
}

void ChunkCommander::drawItem(ChunkItem* pItem)
{
	BW_GUARD;
	if (pItem->drawMark() != Chunk::s_nextMark_)
	{
		// Make sure we are using the correct view matrix
		if (viewParametersChanged_ && inReflection_ == 0)
		{
			Moo::rc().view(storedView_);
			viewParametersChanged_ = false;
		}

		disableDepthTestState();

		// Get the chunk, set up the chunk transform and draw
		// the item
		Chunk* pChunk = pItem->chunk();
		
		// If we have moved into a new chunk, we should set up
		// its caches, ie lights.
		if (pChunk != pLastChunk_)
		{
			pLastChunk_ = pChunk;
			pChunk->drawCaches();
			pChunk->drawMark(Chunk::s_nextMark_ );
		}

		// TODO: only set the chunk transform when we move to a new chunk
		Moo::rc().push();
		Moo::rc().world(pChunk->transform());
		pItem->draw();
		Moo::rc().pop();

		// Don't mark if we don't render into colour
		// Which happens if we can can avoid it and are trying to avoid it
		if ( !( ( pItem->typeFlags() & ChunkItemBase::TYPE_DEPTH_ONLY ) &&
				Moo::rc().depthOnly() ) )
			pItem->drawMark( Chunk::s_nextMark_ );
	}
}

// -----------------------------------------------------------------------------
// Section: ChunkUmbraServices
// -----------------------------------------------------------------------------

/*
 *	This method outputs a umbra error
 */
void ChunkUmbraServices::error(const char* message)
{
	BW_GUARD;
	CRITICAL_MSG( message );
}

namespace Moo { extern THREADLOCAL( bool ) g_renderThread; }

/*
 * This is the entermutex method for umbra.
 * As we only allow access to umbra from the render thread in BigWorld, 
 * this is not implemented as a mutex, but triggers an assert if it is accessed
 * from any other thread.
 * We only allow umbra access from the render thread as umbra can hold on to the
 * mutex for an extended period of time, and as such we do not want
 * the loading thread to block for this amount of time.
 */
void ChunkUmbraServices::enterMutex(void)
{
	BW_GUARD;
	MF_ASSERT(Moo::g_renderThread);
}

/*
 *	This is the leavemutex method for umbra.
 *	See the comment for ChunkUmbraServices::enterMutex above for the reasoning 
 *  behind this being an assert only.
 */
void ChunkUmbraServices::leaveMutex(void)
{
	BW_GUARD;
	MF_ASSERT( Moo::g_renderThread );
}

bool ChunkUmbraServices::allocateQueryObject(int index)
{
	BW_GUARD;
	queries_.resize(index+1);
	queries_[index] = Moo::rc().createOcclusionQuery();
	return queries_[index] != NULL;
}

void ChunkUmbraServices::releaseQueryObject(int index)
{
	BW_GUARD;
	Moo::rc().destroyOcclusionQuery( queries_[index] );
	queries_[index] = NULL;
}

// -----------------------------------------------------------------------------
// Section: UmbraHelper
// -----------------------------------------------------------------------------

/*
 *	Helper class to contain one umbra statistic
 */
class UmbraHelper::Statistic
{
public:
	void set( Umbra::LibraryDefs::Statistic statistic )
	{
		statistic_ = statistic;
	}
	float get() const
	{
		return Umbra::Library::getStatistic( statistic_ );
	}
private:
	Umbra::LibraryDefs::Statistic statistic_;
};

/*
 *	This method inits the umbra helper
 */
void UmbraHelper::init()
{
	BW_GUARD;
	static Statistic stats[Umbra::LibraryDefs::STAT_MAX];

	// Register our debug watchers
	MF_WATCH( "Render/Umbra/occlusionCulling",	*this, MF_ACCESSORS( bool, UmbraHelper, occlusionCulling ),
		"Enable/disable umbra occlusion culling, this still uses umbra for frustum culling" );
	MF_WATCH( "Render/Umbra/enabled",		   *this, MF_ACCESSORS( bool, UmbraHelper, umbraEnabled ),
		"Enable/disable umbra, this causes the rendering to bypass umbra and use the BigWorld scene traversal" );
	MF_WATCH( "Render/Umbra/drawTestModels",	*this, MF_ACCESSORS( bool, UmbraHelper, drawTestModels ),
		"Draw the umbra test models" );
	MF_WATCH( "Render/Umbra/drawWriteModels",	*this, MF_ACCESSORS( bool, UmbraHelper, drawWriteModels ),
		"Draw the umbra writemodels" );
	MF_WATCH( "Render/Umbra/drawObjectBounds",	*this, MF_ACCESSORS( bool, UmbraHelper, drawObjectBounds ),
		"Draw the umbra object bounds" );
	MF_WATCH( "Render/Umbra/drawVoxels",		*this, MF_ACCESSORS( bool, UmbraHelper, drawVoxels ),
		"Draw the umbra voxels" );
	MF_WATCH( "Render/Umbra/drawSilhouettes",	*this, MF_ACCESSORS( bool, UmbraHelper, drawSilhouettes ),
		"Draw the umbra object silhouettes (software mode only)" );
	MF_WATCH( "Render/Umbra/drawQueries",		*this, MF_ACCESSORS( bool, UmbraHelper, drawQueries ),
		"Draw the umbra occlusion queries (hardware mode only)" );
	MF_WATCH( "Render/Umbra/flushTrees",		*this, MF_ACCESSORS( bool, UmbraHelper, flushTrees ),
		"Flush speedtrees as part of umbra, if this is set to false, all speedtrees are flushed after "
		"the occlusion queries have been issued. "
		"Which means that speedtrees do not act as occluders" );
	MF_WATCH( "Render/Umbra/depthOnlyPass",		*this, MF_ACCESSORS( bool, UmbraHelper, depthOnlyPass ),
		"Do seperate depth and colour passes as requested by Umbra" );
	
	// Set up the watchers for the statistics
	for (uint32 i = 0; i < (uint32)Umbra::LibraryDefs::STAT_MAX; i++)
	{
		std::string statName(Umbra::Library::getStatisticName( (Umbra::LibraryDefs::Statistic)i ));
		std::string statNameBegin = statName;

		stats[i].set( (Umbra::LibraryDefs::Statistic)i );

		MF_WATCH( (std::string( "Render/Umbra/Statistics/" ) + statNameBegin + "/" + statName).c_str(), 
			stats[i], &UmbraHelper::Statistic::get );
	}
}

void UmbraHelper::fini()
{
}

/**
 *	This method returns the umbra helper instance
 */
UmbraHelper& UmbraHelper::instance()
{
	static UmbraHelper s_instance;
	return s_instance;
}

/*
 *	macro to help implement the umbra flag switches
 */
#define IMPLEMENT_Umbra_HELPER_FLAG( method, flag )\
bool UmbraHelper::method() const\
{\
	return (Umbra::Library::getFlags(Umbra::LibraryDefs::LINEDRAW) & Umbra::LibraryDefs::flag) != 0;\
}\
void UmbraHelper::method(bool b) \
{ \
	if (b)\
		Umbra::Library::setFlags(Umbra::LibraryDefs::LINEDRAW, Umbra::LibraryDefs::flag); \
	else\
		Umbra::Library::clearFlags(Umbra::LibraryDefs::LINEDRAW, Umbra::LibraryDefs::flag); \
}

IMPLEMENT_Umbra_HELPER_FLAG( drawTestModels, LINE_OBJECT_TEST_MODEL )
IMPLEMENT_Umbra_HELPER_FLAG( drawWriteModels, LINE_OBJECT_WRITE_MODEL )
IMPLEMENT_Umbra_HELPER_FLAG( drawObjectBounds, LINE_OBJECT_BOUNDS )
IMPLEMENT_Umbra_HELPER_FLAG( drawVoxels, LINE_VOXELS )
IMPLEMENT_Umbra_HELPER_FLAG( drawSilhouettes, LINE_SILHOUETTES )
IMPLEMENT_Umbra_HELPER_FLAG( drawQueries, LINE_OCCLUSION_QUERIES )

UmbraHelper::UmbraHelper() :
	occlusionCulling_(true),
#ifdef EDITOR_ENABLED
	/* Read from options.xml on startup
	   Default to on.
	*/
	umbraEnabled_(Options::getOptionInt( "render/useUmbra", 1) == 1),
#else
	umbraEnabled_(true),
	flushTrees_(true),
#endif
	depthOnlyPass_(true),
	wireFrameTerrain_( false )
{
}

UmbraHelper::~UmbraHelper()
{
}

#endif