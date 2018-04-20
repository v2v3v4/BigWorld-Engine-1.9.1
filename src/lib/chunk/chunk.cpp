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

#include "cstdmf/debug.hpp"
#include "cstdmf/guard.hpp"
#include "cstdmf/main_loop_task.hpp"

#include "physics2/hulltree.hpp"

#include "resmgr/bwresource.hpp"

#if UMBRA_ENABLE
#include <umbraCell.hpp>
#include <umbraObject.hpp>
#include "chunk_umbra.hpp"
#endif

// #include "moo/moo_math_helper.hpp"

#ifdef _WIN32
#ifndef MF_SERVER
#include "moo/render_context.hpp"
#include "moo/effect_visual_context.hpp"

#include "romp/geometrics.hpp"
#endif // MF_SERVER
#endif // _WIN32

#include "chunk.hpp"
#ifdef MF_SERVER
#include "server_chunk_model.hpp"
#else//MF_SERVER
#include "chunk_model.hpp"
#endif//MF_SERVER
#include "chunk_boundary.hpp"
#include "chunk_space.hpp"
#include "chunk_exit_portal.hpp"

#ifdef EDITOR_ENABLED
#include "chunk_item_amortise_delete.hpp"
#endif//EDITOR_ENABLED

#ifndef MF_SERVER
#include "chunk_manager.hpp"
#endif // MF_SERVER


#include <set>

DECLARE_DEBUG_COMPONENT2( "Chunk", 0 )

#ifndef MF_SERVER

PROFILER_DECLARE( Chunk_tick, "Chunk Tick" );
PROFILER_DECLARE( Chunk_tick2, "Chunk Tick 2" );

namespace { // anonymous

bool s_cullDebugEnable = false;

#if ENABLE_CULLING_HUD
	float s_cullHUDDist = 2500;

	typedef std::avector< std::pair<Matrix, BoundingBox> > BBoxVector;
	BBoxVector s_traversedChunks;
	BBoxVector s_visibleChunks;
	BBoxVector s_fringeChunks;
	BBoxVector s_reflectedChunks;

	typedef std::map< Chunk *, BoundingBox > BBoxMap;
	BBoxMap s_debugBoxes;

	void Chunks_drawCullingHUD_Priv();
#endif // ENABLE_CULLING_HUD

// The mainlook Task
class CullDebugTask : public MainLoopTask
{
	virtual void draw()
	{
		Chunks_drawCullingHUD();
	}
};
std::auto_ptr<CullDebugTask> s_cullDebugInstance;

} // namespace anonymous
#endif // MF_SERVER

// Static initialisers
uint32	Chunk::s_nextMark_           = 0; // not that this matters
uint32	Chunk::s_nextVisibilityMark_ = 0;
Chunk::OverlapperFinder Chunk::overlapperFinder_ = NULL;
Chunk::Factories * Chunk::pFactories_ = NULL;
int Chunk::nextCacheID_ = 0;
uint32	Chunk::s_instanceCount_ = 0;
uint32	Chunk::s_instanceCountPeak_ = 0;

/**
 *	Constructor.
 */
Chunk::Chunk( const std::string & identifier, ChunkDirMapping * pMapping ) :
	identifier_( identifier ),
	pMapping_( pMapping ),
	pSpace_( &*pMapping->pSpace() ),
	isOutsideChunk_( *(identifier.end()-1) == 'o' ),
	hasInternalChunks_( false ),
	ratified_( false ),
	loading_( false ),
	loaded_( false ),
	online_( false ),
	focusCount_( 0 ),
	transform_( Matrix::identity ),
	transformInverse_( Matrix::identity ),
#ifndef MF_SERVER
	visibilityBox_( BoundingBox::s_insideOut_ ),
	visibilityBoxCache_( BoundingBox::s_insideOut_ ),
	visibilityBoxMark_( s_nextMark_ - 128 ), // i.e. 'a while ago'
#endif // MF_SERVER
	drawMark_( s_nextMark_ - 128 ),
	traverseMark_( s_nextMark_ - 128 ),
	reflectionMark_( s_nextMark_ - 128 ),
	caches_( new ChunkCache *[ Chunk::nextCacheID_ ] ),
	fringeNext_( NULL ),
	fringePrev_( NULL ),
	inTick_( false ),
	removable_( true )
#if UMBRA_ENABLE
	,
	pUmbraCell_( NULL )
#endif
{
	BW_GUARD;
	for (int i = 0; i < Chunk::nextCacheID_; i++) caches_[i] = NULL;

	if( isOutsideChunk() )
	{
		pMapping->gridFromChunkName( this->identifier(), x_, z_ );

		float xf = float(x_) * GRID_RESOLUTION;
		float zf = float(z_) * GRID_RESOLUTION;

		localBB_ = BoundingBox( Vector3( 0.f, MIN_CHUNK_HEIGHT, 0.f ),
			Vector3( GRID_RESOLUTION, MAX_CHUNK_HEIGHT, GRID_RESOLUTION ) );

		boundingBox_ = BoundingBox( Vector3( xf, MIN_CHUNK_HEIGHT, zf ),
			Vector3( xf + GRID_RESOLUTION, MAX_CHUNK_HEIGHT, zf + GRID_RESOLUTION ) );

		transform_.setTranslate( xf, 0.f, zf );
		transform_.postMultiply( pMapping->mapper() );
		transformInverse_.invert( transform_ );

		Vector3 min = this->localBB_.minBounds();
		Vector3 max = this->localBB_.maxBounds();
		min.y = +std::numeric_limits<float>::max();
		max.y = -std::numeric_limits<float>::max();

		centre_ = boundingBox_.centre();

#ifndef MF_SERVER
		this->visibilityBox_.setBounds(min, max);
#endif // MF_SERVER
	}

	s_instanceCount_++;
	if ( s_instanceCount_ > s_instanceCountPeak_ )
		s_instanceCountPeak_ = s_instanceCount_;
}


/// destructor
Chunk::~Chunk()
{
	BW_GUARD;
	// loose ourselves if we are bound
	if (this->online()) this->loose( false );

	// unload ourselves if we are loaded
	if (this->loaded()) this->eject();

	// delete the caches if they are here just in case
	//  (some eager users create caches on unloaded chunks)
	for (int i = 0; i < Chunk::nextCacheID_; i++)
	{
		if (caches_[i] != NULL)
		{
			delete caches_[i];
			caches_[i] = NULL;
		}
	}
	delete [] caches_;

	// and remove ourselves from our space if we're in it
	if (this->ratified()) pSpace_->delChunk( this );

	s_instanceCount_--;
}


/**
 *	This method lets this chunk know that it has been ratified by its
 *	ChunkSpace and is now a full member of it.
 */
void Chunk::ratify()
{
	ratified_ = true;
}


void Chunk::init()
{
	BW_GUARD;	
#ifndef MF_SERVER
#if ENABLE_CULLING_HUD
#if !UMBRA_ENABLE
	MF_WATCH( "Chunks/Chunk Culling HUD", s_cullDebugEnable,
		Watcher::WT_READ_WRITE, "Toggles the chunks culling debug HUD" );

	MF_WATCH( "Chunks/Culling HUD Far Distance", s_cullHUDDist,
		Watcher::WT_READ_WRITE, "Sets the scale of the chunks culling debug HUD" );

	s_cullDebugInstance.reset(new CullDebugTask);
	MainLoopTasks::root().add(
		s_cullDebugInstance.get(),
		"World/Debug Chunk Culling", ">App", NULL );
#endif // !UMBRA_ENABLE
#endif // ENABLE_CULLING_HUD

	MF_WATCH( "Chunks/Loaded Chunks", s_instanceCount_, Watcher::WT_READ_ONLY, "Number of loaded chunks" );
#endif // MF_SERVER
}


void Chunk::fini()
{
	BW_GUARD;
	delete pFactories_;
	pFactories_ = NULL;
}


// helper function to read a moo matrix called 'transform', with
// identity as the default
void readMooMatrix( DataSectionPtr pSection, const std::string & tag,
	Matrix &result )
{
	BW_GUARD;
	result = pSection->readMatrix34( tag, Matrix::identity );
}


/// general load method, called by the ChunkLoader
bool Chunk::load( DataSectionPtr pSection )
{
	BW_GUARD;	
// Editor will call this when it's already loaded to recreate the chunk
#ifndef EDITOR_ENABLED
	MF_ASSERT_DEV( !loaded_ );
#endif

	// clear some variables in case we are unloaded then reloaded
	hasInternalChunks_ = false;

	// load but complain if the section is missing
	if (!pSection)	// not sure if line above is a good idea...
	{
#ifdef EDITOR_ENABLED
		ERROR_MSG( "Chunk::load: DataSection for %s is NULL (FNF)\n",
			identifier_.c_str() );
#else //EDITOR_ENABLED
		WARNING_MSG( "Chunk::load: DataSection for %s is NULL (FNF)\n",
			identifier_.c_str() );
#endif//EDITOR_ENABLED
		loaded_ = true;
		return false;
	}

	bool good = true;
	bool skipBoundaryAndIncludes = false;

	// first set our label (if present)
	label_ = pSection->asString();

	if( !isOutsideChunk() )
	{
		readMooMatrix( pSection, "transform", transform_ );
		transform_.postMultiply( pMapping_->mapper() );
		transformInverse_.invert( transform_ );

		DataSectionPtr shellSection = pSection->openSection( "shell" );
		if( !shellSection )// old style chunk, with first model as shell
			shellSection = pSection->openSection( "model" );
		if( !shellSection )
		{
			good = false;
		}
		else
		{
			good &= bool( this->loadItem( shellSection ) );
		}
		if( !good )
		{
			localBB_ = BoundingBox( Vector3( 0.f, 0.f, 0.f ), Vector3( 1.f, 1.f, 1.f ) );
			boundingBox_ = localBB_;
#ifndef MF_SERVER
			visibilityBox_ = localBB_;
#endif//MF_SERVER
			boundingBox_.transformBy( transform_ );

			ERROR_MSG( "Chunk::load: Failed to load shell model for chunk %s\n", identifier_.c_str() );
			skipBoundaryAndIncludes = true;
		}
	}

	if (!skipBoundaryAndIncludes)
	{
		// and the boundaries (call this before loading lights)
		if ( !this->formBoundaries( pSection ) )
		{
			good = false;
			ERROR_MSG( "Chunk::load: Failed to load chunk %s boundaries\n", identifier_.c_str() );
		}

		// now read it in as if it were an include
		std::string errorStr;
		if ( !this->loadInclude( pSection, Matrix::identity, &errorStr ) )
		{
			good = false;
			ERROR_MSG( "Chunk::load: Failed to load chunk %s: %s\n", identifier_.c_str(), errorStr.c_str() );
		}
	}

	// prime anything which caches world transforms
	this->transform( transform_ );

	// let any current caches know that loading is finished
	for (int i = 0; i < Chunk::nextCacheID_; i++)
	{
		// first touch this cache type
		(*Chunk::touchType()[i])( *this );

		// now if it exists then load it
		ChunkCache * cc = caches_[i];
		if (cc != NULL) {
			if ( !cc->load( pSection ) )
			{
				good = false;
				ERROR_MSG( "Chunk::load: Failed to load cache %d for chunk %s\n", i, identifier_.c_str() );
			}
		}
	}

	loaded_ = true;
	return good;
}


/**
 *	This method loads the given section assuming it is a chunk item
 */
ChunkItemFactory::Result Chunk::loadItem( DataSectionPtr pSection )
{
	BW_GUARD;
	IF_NOT_MF_ASSERT_DEV( pFactories_ != NULL )
	{
		return ChunkItemFactory::SucceededWithoutItem();
	}

	Factories::iterator found = pFactories_->find( pSection->sectionName() );
	if (found != pFactories_->end())
		return found->second->create( this, pSection );

	return ChunkItemFactory::SucceededWithoutItem();	// we ignore unknown section names
}


/**
 *	Helper function to load an included file
 */
bool Chunk::loadInclude( DataSectionPtr pSection, const Matrix & flatten, std::string* errorStr )
{
	BW_GUARD;
	if (!pSection) return false;

	bool good = true;
	bool lgood;
	int nincludes = 0;

	// ok, iterate over all its sections
	DataSectionIterator end = pSection->end();
	bool needShell = !isOutsideChunk() && !pSection->openSection( "shell" );
	bool gotFirstModel = false;
	for (DataSectionIterator it = pSection->begin(); it != end; it++ )
	{
		const std::string stype = (*it)->sectionName();

		if( stype == "shell" )
			continue;

		if( needShell && stype == "model" && !gotFirstModel )
		{
			gotFirstModel = true;
			continue;
		}

		std::string itemError;
		// could do this with a dispatch table but really
		// I couldn't be bothered

		if (stype == "include")
		{
			// read its transform
			Matrix mlevel;
			readMooMatrix( *it, "transform", mlevel );

			// accumulate it with flatten
			mlevel.postMultiply( flatten );

			// and parse it
			lgood = this->loadInclude(
				BWResource::openSection( (*it)->readString( "resource" ) ),
				mlevel,
				errorStr );
			good &= lgood;
			if (!lgood && errorStr)
			{
				std::stringstream ss;
				ss << "bad include section index " << nincludes;
				itemError += ss.str();
			}

			nincludes++;
		}
		else
		{
			//uint64 loadTime = timestamp();
			ChunkItemFactory::Result res = this->loadItem( *it );
			good &= bool(res);
			if (!bool(res) && errorStr)
			{
				if ( !res.errorString().empty() )
				{
					itemError = res.errorString();
				}
				else
				{
					itemError = "unknown error in item '" + (*it)->sectionName() + "'";
				}
			}
			//loadTime = timestamp() - loadTime;
			//DEBUG_MSG( "Loading %s took %f ms\n", stype.c_str(),
			//	float(double(int64(loadTime)) /
			//		stampsPerSecondD()) * 1000.f );
		}
		if ( !itemError.empty() && errorStr )
		{
			if ( !errorStr->empty() )
			{
				*errorStr += ", ";
			}
			*errorStr += itemError;
		}
	}

	return good;
}

void createPortal( DataSectionPtr boundary, const std::string& toChunk,
	const Vector3& uAxis, const Vector3& pt1, const Vector3& pt2,
	const Vector3& pt3, const Vector3& pt4 )
{
	BW_GUARD;
	DataSectionPtr portal = boundary->newSection( "portal" );
	portal->writeString( "chunk", toChunk );
	portal->writeVector3( "uAxis", uAxis );
	portal->newSection( "point" )->setVector3( pt1 );
	portal->newSection( "point" )->setVector3( pt2 );
	portal->newSection( "point" )->setVector3( pt3 );
	portal->newSection( "point" )->setVector3( pt4 );
}

static DataSectionPtr createBoundary( DataSectionPtr pBoundarySection, const Vector3& normal, float d )
{
	BW_GUARD;
	DataSectionPtr boundary = pBoundarySection->newSection( "boundary" );
	boundary->writeVector3( "normal", normal );
	boundary->writeFloat( "d", d );
	return boundary;
}

/**
 *	Helper function to recreate boundaries of a chunk
 */
static void createBoundary( DataSectionPtr chunkSection, ChunkDirMapping* pMapping, 
							std::vector<DataSectionPtr>& bsects )
{	
	BW_GUARD;
	DataSectionPtr pTempBoundSect = new XMLSection( "root" );

	// "xxxxxxxx[i|o].chunk"
	IF_NOT_MF_ASSERT_DEV( chunkSection->sectionName().size() >= 15 )
	{
		return;
	}

	if( chunkSection->sectionName()[ chunkSection->sectionName().size() - 7 ] == 'o' )
	{
		std::string chunkName = chunkSection->sectionName();
		chunkName = chunkName.substr( 0, chunkName.size() - 6 );
		int16 x, z;
		pMapping->gridFromChunkName( chunkName, x, z );
		for (int i = 0; i < 6; ++i)
		{
			float minYf = float( MIN_CHUNK_HEIGHT );
			float maxYf = float( MAX_CHUNK_HEIGHT );

			if( i == 0 )
			{
				// right
				DataSectionPtr b = createBoundary( pTempBoundSect, Vector3( 1.f, 0.f, 0.f ), 0.f );
				if (x != pMapping->minGridX() )
					createPortal( b, pMapping->outsideChunkIdentifier( x - 1, z ), Vector3( 0.f, 1.f, 0.f ),
						Vector3( minYf, 0.f, 0.f ),
						Vector3( maxYf, 0.f, 0.f ),
						Vector3( maxYf, GRID_RESOLUTION, 0.f ),
						Vector3( minYf, GRID_RESOLUTION, 0.f ) );
			}
			else if (i == 1)
			{
				// left
				DataSectionPtr b = createBoundary( pTempBoundSect, Vector3( -1.f, 0.f, 0.f ), -GRID_RESOLUTION );
				if (x != pMapping->maxGridX() )
					createPortal( b, pMapping->outsideChunkIdentifier( x + 1, z ), Vector3( 0.f, 0.f, 1.f ),
						Vector3( 0.f, minYf, 0.f ),
						Vector3( GRID_RESOLUTION, minYf, 0.f ),
						Vector3( GRID_RESOLUTION, maxYf, 0.f ),
						Vector3( 0.f, maxYf, 0.f ) );

			}
			else if (i == 2)
			{
				// bottom
				DataSectionPtr b = createBoundary( pTempBoundSect, Vector3( 0.f, 1.f, 0.f ), minYf );
				createPortal( b, "earth", Vector3( 0.f, 0.f, 1.f ),
					Vector3( 0.f, 0.f, 0.f ),
					Vector3( GRID_RESOLUTION, 0.f, 0.f ),
					Vector3( GRID_RESOLUTION, GRID_RESOLUTION, 0.f ),
					Vector3( 0.f, GRID_RESOLUTION, 0.f ) );
			}
			else if (i == 3)
			{
				// top
				DataSectionPtr b = createBoundary( pTempBoundSect, Vector3( 0.f, -1.f, 0.f ), -maxYf );
				createPortal( b, "heaven", Vector3( 1.f, 0.f, 0.f ),
					Vector3( 0.f, 0.f, 0.f ),
					Vector3( GRID_RESOLUTION, 0.f, 0.f ),
					Vector3( GRID_RESOLUTION, GRID_RESOLUTION, 0.f ),
					Vector3( 0.f, GRID_RESOLUTION, 0.f ) );
			}
			else if (i == 4)
			{
				// back
				DataSectionPtr b = createBoundary( pTempBoundSect, Vector3( 0.f, 0.f, 1.f ), 0.f );
				if( z != pMapping->minGridY() )
					createPortal( b, pMapping->outsideChunkIdentifier( x, z - 1 ), Vector3( 1.f, 0.f, 0.f ),
						Vector3( 0.f, minYf, 0.f ),
						Vector3( GRID_RESOLUTION, minYf, 0.f ),
						Vector3( GRID_RESOLUTION, maxYf, 0.f ),
						Vector3( 0.f, maxYf, 0.f ) );
			}
			else if (i == 5)
			{
				// front
				DataSectionPtr b = createBoundary( pTempBoundSect, Vector3( 0.f, 0.f, -1.f ), -GRID_RESOLUTION );
				if( z != pMapping->maxGridY() )
					createPortal( b, pMapping->outsideChunkIdentifier( x, z + 1 ), Vector3( 0.f, 1.f, 0.f ),
						Vector3( minYf, 0.f, 0.f ),
						Vector3( maxYf, 0.f, 0.f ),
						Vector3( maxYf, GRID_RESOLUTION, 0.f ),
						Vector3( minYf, GRID_RESOLUTION, 0.f ) );
			}
		}
	}
	else
	{
		DataSectionPtr modelSection = chunkSection->openSection( "shell" );
		if( !modelSection )
			modelSection = chunkSection->openSection( "model" );
		if( modelSection )
		{
			std::string resource = modelSection->readString( "resource" );
			if( !resource.empty() )
			{
				resource = BWResource::changeExtension( resource, ".visual" );
				DataSectionPtr visualSection = BWResource::openSection( resource );
				if( !visualSection )
				{
					resource = BWResource::changeExtension( resource, ".static.visual" );
					visualSection = BWResource::openSection( resource );
				}
				if( visualSection )
				{
					std::vector<DataSectionPtr> boundarySections;
					visualSection->openSections( "boundary", boundarySections );
					if( boundarySections.empty() )
						visualSection = createBoundarySections( visualSection, Matrix::identity );
					pTempBoundSect->copySections( visualSection, "boundary" );
				}
			}
		}
	}
	pTempBoundSect->openSections( "boundary", bsects );
}

/**
 *	Helper function to load a chunk's boundary
 */
bool Chunk::formBoundaries( DataSectionPtr pSection )
{
	BW_GUARD;
	std::vector<DataSectionPtr> bsects;
	createBoundary( pSection, pMapping_, bsects );
	bool good = true;

	for (uint i=0; i<bsects.size(); i++)
	{
		ChunkBoundary * pCB = new ChunkBoundary(bsects[i],
												pMapping_,
												this->identifier() );

		if (pCB->plane().normal().length() == 0.f)
		{
			delete pCB;
			good = false;
			continue;
		}

		bool isaBound = false;
		bool isaJoint = false;
		if (pCB->unboundPortals_.size())
		{
			isaJoint = true;
			if (!pCB->unboundPortals_[0]->internal)
			{
				// we only need to check the first portal
				// because if there are any non-internal
				// portals then the ChunkBoundary must
				// be a bound, (because chunks are convex),
				// and the portal should be internal.
				isaBound = true;
			}
		}
		else
		{
			// the only portals bound at this time are those
			// connecting to heaven or earth.
			if (pCB->boundPortals_.size())
			{
				isaJoint = true;
			}
			isaBound = true;
		}

		if (isaBound) bounds_.push_back( pCB );
		if (isaJoint) joints_.push_back( pCB );
	}

	return good && bsects.size() >= 4;
}


/**
 *	This method unloads this chunk and returns it to its unloaded state.
 */
void Chunk::eject()
{
	BW_GUARD;
	// make sure we're not online
	if (this->online())
	{
		ERROR_MSG( "Chunk::eject: "
			"Tried to eject a chunk while still online\n" );
		return;
	}

	// if we're not loaded, then there's nothing to do
	if (!this->loaded()) return;

	// ok, get rid of all our items, boundaries and caches then!

	// first the items
	for (int i = dynoItems_.size()-1; i >= 0; i--)
	{
		ChunkItemPtr pItem = dynoItems_[i];
		this->delDynamicItem( pItem );
		pSpace_->addHomelessItem( pItem.getObject() );
	}
	{
		MatrixMutexHolder lock( this );
		for (int i = selfItems_.size()-1; i >= 0; i--)
		{
			ChunkItemPtr pItem = selfItems_[i];

#ifdef EDITOR_ENABLED
			// Add the chunk item to the amortise chunk item delete manager
			AmortiseChunkItemDelete::instance().add( pItem );
#endif//EDITOR_ENABLED

			this->delStaticItem( pItem );
			if (pItem->wantsNest())
			{
				pSpace_->addHomelessItem( pItem.getObject() );
			}
		}

		// clear them all here just in case
		selfItems_.clear();
	}
	dynoItems_.clear();
	swayItems_.clear();

	lenders_.clear();
	borrowers_.clear();

	// now the boundaries
	bounds_.clear();
	joints_.clear();

	// and finally the caches
	for (int i = 0; i < Chunk::nextCacheID_; i++)
	{
		if (caches_[i] != NULL)
		{
			delete caches_[i];
			caches_[i] = NULL;
		}
	}		// let's hope caches don't refer to each other...

#if UMBRA_ENABLE
	// Release the umbra cell
	if (pUmbraCell_)
	{
		pUmbraCell_->release();
		pUmbraCell_ = NULL;
	}
#endif

	// so we are now unloaded!
	loaded_ = false;
}


/**
 *	General bind method, called by the ChunkManager after loading.
 *
 *	If the form argument is true, then connections are formed between
 *	unconnected portals and the surrounding chunks.
 */
void Chunk::bind( bool form )
{
	BW_GUARD;
	this->syncInit();

	bindPortals( form );

	this->notifyCachesOfBind( false );

	online_ = true;

	// let the chunk space know we can now be focussed
	pSpace_->noticeChunk( this );
}

/**
 *	try to bind all unbound portals
 */
void Chunk::bindPortals( bool form )
{
	BW_GUARD;
	// go through all our boundaries
	for (ChunkBoundaries::iterator bit = joints_.begin();
		bit != joints_.end();
		bit++)
	{
		// go through all their unbound portals
		for (uint i=0; i < (*bit)->unboundPortals_.size(); i++)
		{
			// get the portal
			ChunkBoundary::Portal *& pPortal = (*bit)->unboundPortals_[i];
			ChunkBoundary::Portal & p = *pPortal;

			// deal with mapping race conditions and extern portals
			if (p.hasChunk() && p.pChunk->mapping()->condemned())
			{
				ChunkDirMapping * pOthMapping = p.pChunk->mapping();
				MF_ASSERT_DEV( pOthMapping != pMapping_ );
				MF_ASSERT_DEV( !p.pChunk->ratified() );	// since condemned

				delete p.pChunk;
				pOthMapping->decRef();

				// try to resolve it again for the changed world
				p.pChunk = (Chunk*)ChunkBoundary::Portal::EXTERN;
			}
			if (p.isExtern())
			{
				// TODO: Only do this if we set it above or if a new mapping
				// was recently added - or else it is a huge waste of time.
				// (because we already tried resolveExtern and found nothing)
				p.resolveExtern( this );
			}

			// does it have a chunk?
			if (!p.hasChunk())
			{
				if (!form) continue;
				if (p.pChunk != NULL && !p.isInvasive()) continue;

				// ok, we want to give it one then
				Vector3 conPt = transform_.applyPoint(
					p.lcentre + p.plane.normal() * -0.001f );

				// look at point 10cm away from centre of portal
				Chunk * pFound = NULL;
				ChunkSpace::Column * pCol = pSpace_->column( conPt, false );
				if (pCol != NULL)
					pFound = pCol->findChunkExcluding( conPt, this );

				if (pFound == NULL)
					continue;

				// see if it wants to form a boundary with us
				if (!pFound->formPortal( this, p ))
					continue;

				// this is the chunk for us then
				p.pChunk = pFound;

				// split it if it extends beyond just this chunk
				(*bit)->splitInvasivePortal( this, i );
				// (the function above may modify unboundPortals_, but that
				// OK as it is a vector of pointers; 'p' is not clobbered)
				// if portals were appended we'll get to them in a later cycle
			}
			else
			{
				// see if we are holding a mapping ref through an extern portal
				bool holdingMappingRef =	// (that we hasn't been decref'd)
					(p.pChunk->mapping() != pMapping_) && !p.pChunk->ratified();

				// find the chunk it refers to in its space's map
				p.pChunk = p.pChunk->space()->findOrAddChunk( p.pChunk );

				// release any mapping ref now that chunk is in the space's list
				if (holdingMappingRef) p.pChunk->mapping()->decRef();
			}

			// create a chunk exit portal item, mainly for rain but who knows
			// what else this will be used for..
			if (!this->isOutsideChunk_ && p.pChunk->isOutsideChunk())
			{
				this->addStaticItem( new ChunkExitPortal(p) );
			}

			// if it's already bound, then get it to bind to this portal too
			if (p.pChunk->online())
			{
				// save chunk pointer before invalidating reference...
				Chunk * pOnlineChunk = p.pChunk;

				// move it to the bound portals list
				(*bit)->bindPortal( i-- );

				// and let it know we're online
				pOnlineChunk->bind( this );

#if UMBRA_ENABLE
				// Create umbra portal
				p.createUmbraPortal( this );
#endif
			}
		}
	}
}
/**
 *	General loose method, to reverse the effect of 'bind'. It sorts out
 *	all the portals so that if it is unloaded then it can be reloaded
 *	and rebound successfully.
 *
 *	A call to this method should be followed by a call to either the
 *	bind or eject methods, or else the ChunkManager may try to load a
 *	new chunk on top of what's here (since it's not bound, but it's
 *	not in its list of loading chunks). So heed this advice.
 *
 *	Also, the space that is chunk is in must be refocussed before anything
 *	robust can access the focus grid (some bits may be missing). This is
 *	done from the 'camera' method in the chunk manager.
 */
void Chunk::loose( bool cut )
{
	BW_GUARD;
	// ok, remove ourselves from the focus grid then
	//  (can't tell if we are partially focussed or totally unfocussed,
	//	so we always have to do this)
	pSpace_->ignoreChunk( this );
	focusCount_ = 0;

	// get rid of any items lent out
	Borrowers::iterator brit;
	for (brit = borrowers_.begin(); brit != borrowers_.end(); brit++)
	{
		bool foundSelfAsLender = false;

		for (Lenders::iterator lit = (*brit)->lenders_.begin();
			lit != (*brit)->lenders_.end();
			lit++)
		{
			if ((*lit)->pLender_ == this)
			{
				(*brit)->lenders_.erase( lit );
				foundSelfAsLender = true;
				break;
			}
		}

		if (!foundSelfAsLender)
		{
			CRITICAL_MSG( "Chunk::loose: "
				"%s could not find itself as a lender in %s\n",
				identifier_.c_str(), (*brit)->identifier_.c_str() );
		}
		else
		{
			/*TRACE_MSG( "Chunk::loose: %s cut ties with borrower %s\n",
				identifier_.c_str(), (*brit)->identifier_.c_str() );*/
		}
	}
	borrowers_.clear();

	// get rid of any items borrowed
	Lenders::iterator lit;
	for (lit = lenders_.begin(); lit != lenders_.end(); lit++)
	{
		Chunk * pLender = (*lit)->pLender_;
		Borrowers::iterator brit = std::find(
			pLender->borrowers_.begin(), pLender->borrowers_.end(), this );

		bool foundSelfAsBorrower = (brit != pLender->borrowers_.end());
		if (foundSelfAsBorrower)
			pLender->borrowers_.erase( brit );

		if (!foundSelfAsBorrower)
		{
			CRITICAL_MSG( "Chunk::loose: "
				"%s could not find itself as a borrower in %s\n",
				identifier_.c_str(), pLender->identifier_.c_str() );
		}
		else
		{
			/*TRACE_MSG( "Chunk::loose: %s cut ties with lender %s\n",
				identifier_.c_str(), pLender->identifier_.c_str() );*/
		}
	}
	lenders_.clear();

	// go through all our boundaries
	for (ChunkBoundaries::iterator bit = joints_.begin();
		bit != joints_.end();
		bit++)
	{
		// go through all their bound portals
		for (uint i=0; i < (*bit)->boundPortals_.size(); i++)
		{
			// get the portal
			ChunkBoundary::Portal *& pPortal = (*bit)->boundPortals_[i];
			ChunkBoundary::Portal & p = *pPortal;

			// don't unbind it if it's not a chunk
			if (!p.hasChunk()) continue;

			// save chunk pointer before invalidating reference...
			Chunk * pOnlineChunk = p.pChunk;

			// clear the chunk if we're cutting it off
			if (cut)
			{
				if (!this->isOutsideChunk() && p.pChunk->isOutsideChunk())
					p.pChunk = (Chunk*)ChunkBoundary::Portal::INVASIVE;
				else
					p.pChunk = NULL;
			}

			// move it to the unbound portals list
			(*bit)->loosePortal( i-- );

			// and let it know we're offline
			if (this->isOutsideChunk() && !pOnlineChunk->isOutsideChunk())
				pOnlineChunk->loose( this, true );// always cut off an exit portal
			else
				pOnlineChunk->loose( this, cut );
		}
	}

	// tell the caches about it (bit of a misnomer I know)
	this->notifyCachesOfBind( true );	// looseNotBind

	// and now we are offline
	online_ = false;
}


//extern uint64 g_hullTreeAddTime, g_hullTreePlaneTime, g_hullTreeMarkTime;

typedef std::set< ChunkSpace::Column * > ColumnSet;

/**
 *	This method is called when the chunk is brought into the focus of
 *	the chunk space. Various services are only available when a chunk
 *	is focused in this way (such as being part of the collision scene,
 *	and being found by the point test routine). Chunks must be bound
 *	before they are focussed, but not all online chunks are focussed,
 *	as they may have been unfocussed then cached for reuse. There is no
 *	corresponding 'blur' method, because the focus count is automatically
 *	reduced when the chunk's holdings in the focus grid go away - it's
 *	like a reference count. A chunk may not be unbound or unloaded
 *	until its focus count has reached zero of its own accord.
 */
void Chunk::focus()
{
	BW_GUARD;
	//g_hullTreeAddTime = 0;
	//g_hullTreePlaneTime = 0;
	//g_hullTreeMarkTime = 0;
	//uint64	ftime = timestamp();

	//ChunkSpace::Column::cacheControl( true );

	// figure out the border
	HullBorder	border;

	for (uint i = 0; i < bounds_.size(); i++)
	{
		const PlaneEq & peq = bounds_[i]->plane();
		// we need to apply our transform to the plane
		Vector3 ndtr = transform_.applyPoint( peq.normal() * peq.d() );
		Vector3 ntr = transform_.applyVector( peq.normal() );
		border.push_back( PlaneEq( ntr, ntr.dotProduct( ndtr ) ) );
	}

	// find what columns we need to add to (z is needless I know)
	ColumnSet columns;
	if (*(this->identifier().end()-1) == 'o')
	{
		// the following will create the column in pSpace if it is needed.
		columns.insert( pSpace_->column( centre_ ) );

		// this is more to prevent unwanted overlaps than for speed
	}
	else
	{
		const Vector3 & mb = boundingBox_.minBounds();
		const Vector3 & Mb = boundingBox_.maxBounds();
		for (int i = 0; i < 8; i++)
		{
			Vector3 pt(	(i&1) ? Mb.x : mb.x,
						(i&2) ? Mb.y : mb.y,
						(i&4) ? Mb.z : mb.z );

			ChunkSpace::Column* pColumn = pSpace_->column( pt );
			if (pColumn)
			{
				columns.insert( pColumn );
			}
		}
	}

	// and add it to all of them
	for (ColumnSet::iterator it = columns.begin(); it != columns.end(); it++)
	{
		MF_ASSERT_DEV( &**it );	// make sure we can reach all those we need to!
		
		if( &**it )
			(*it)->addChunk( border, this );
	}

	//TRACE_MSG( "Chunk::focus: Adding hull of %s (ncols %d)\n",
	//	identifier_.c_str(), columns.size());

	// focus any current caches
	for (int i = 0; i < Chunk::nextCacheID_; i++)
	//for (int i = Chunk::nextCacheID_-1; i >= 0; i--)
	{
		ChunkCache * cc = caches_[i];
		if (cc != NULL) focusCount_ += cc->focus();
	}

	// and set our focus count to one (new meaning - should revert to focus_)
	focusCount_ = 1;
	//TRACE_MSG( "Chunk::focus: %s is now focussed (fc %d)\n",
	//	identifier_.c_str(), focusCount_ );

	//ChunkSpace::Column::cacheControl( false );

	/*
	ftime = timestamp() - ftime;
	char sbuf[256];
	std::strstream ss( sbuf, sizeof(sbuf) );
	ss << "Focus time for chunk " << identifier_ << ": " << NiceTime(ftime);
	//ss << " with hull add " << NiceTime( g_hullTreeAddTime );
	//ss << " and plane add " << NiceTime( g_hullTreePlaneTime );
	ss << std::ends;
	DEBUG_MSG( "%s\n", ss.str() );
	*/
}


/**
 *	This method reduces the chunk's focus count by one, re-adding the
 *	chunk to its space's unfocussed chunks list if the count is not
 *	already zero.
 */
void Chunk::smudge()
{
	BW_GUARD;
	if (focusCount_ != 0)
	{
		//TRACE_MSG( "Chunk::smudge: %s is now blurred (fc %d)\n",
		//	identifier_.c_str(), focusCount_ );
		focusCount_ = 0;
		pSpace_->blurredChunk( this );
	}
}


/**
 *	This method resolves any extern portals that have not yet been resolved.
 *	Most of them are resolved at load time. This method is only called
 *	when a mapping is added to or deleted from our space.
 *
 *	If pDeadMapping is not NULL then we only look at portals that are
 *	current connected to chunks in that mapping, otherwise we consider
 *	all unresolved extern portals.
 */
void Chunk::resolveExterns( ChunkDirMapping * pDeadMapping )
{
	BW_GUARD;
	IF_NOT_MF_ASSERT_DEV( online_ )
	{
		return;
	}

	ChunkBoundaries::iterator bit;
	for (bit = joints_.begin(); bit != joints_.end(); bit++)
	{
		// Whether pDeadMapping is NULL or not, we are only interested
		// in unbound portals. If is is not NULL, then the chunks in that
		// mapping have just been unloaded, so they will have reverted to
		// being unbound. If it is NULL, then the mappings we're looking
		// for are all currently extern so they can't be in the bound list.

		// TODO: Should ensure there are no one-way extern portals or
		// else they will not get re-resolved here.
		for (uint i=0; i < (*bit)->unboundPortals_.size(); i++)
		{
			ChunkBoundary::Portal & p = *(*bit)->unboundPortals_[i];

			// see if this portal is worth a look
			if (pDeadMapping != NULL)
			{
				// we're only interested in existing portals to a dead mapping
				if (!p.hasChunk() || p.pChunk->mapping() != pDeadMapping)
					continue;

				// set this portal back to extern
				p.pChunk = (Chunk*)ChunkBoundary::Portal::EXTERN;
			}
			else
			{
				// we're only interested in portals that are currently extern
				if (!p.isExtern()) continue;
			}

			// see if it now binds elsewhere
			if (p.resolveExtern( this ))
			{
				p.pChunk = pSpace_->findOrAddChunk( p.pChunk );
				p.pChunk->mapping()->decRef();
				if (p.pChunk->online())
				{
					Chunk * pOnlineChunk = p.pChunk;

					// move it to the bound portals list
					(*bit)->bindPortal( i-- );

					// and let it know we're online
					pOnlineChunk->bind( this );
				}
			}
		}
	}
}


/**
 *	Private bind method for late reverse bindings
 */
void Chunk::bind( Chunk * pChunk )
{
	BW_GUARD;
	// go through all our boundaries
	for (ChunkBoundaries::iterator bit = joints_.begin();
		bit != joints_.end();
		bit++)
	{
		// go through all their unbound portals
		for (ChunkBoundary::Portals::iterator pit =
				(*bit)->unboundPortals_.begin();
			pit != (*bit)->unboundPortals_.end(); pit++)
		{
			// see if this is the one
			if ((*pit)->pChunk == pChunk)
			{
#if UMBRA_ENABLE
				// Create umbra portal
				(*pit)->createUmbraPortal( this );
#endif
				(*bit)->bindPortal( pit - (*bit)->unboundPortals_.begin() );

				this->notifyCachesOfBind( false );

				// we return here - if there is more than one
				// portal from that chunk then we'll get another
				// bind call when it finds the other one :)
				return;
			}
		}
	}

	// so, we didn't find a portal. that's bad.
	ERROR_MSG( "Chunk::bind: Chunk %s didn't find reverse portal to %s!\n",
		identifier_.c_str(), pChunk->identifier().c_str() );
}

namespace // anonymous
{
	/**
	 * If portalA and portalB can be bound togeather
	 */
	bool canBind( ChunkBoundary::Portal & portalA, ChunkBoundary::Portal & portalB,
		Chunk* chunkA, Chunk* chunkB )
	{
		BW_GUARD;
		IF_NOT_MF_ASSERT_DEV( chunkA != chunkB )
		{
			return false;
		}

		// ensure both the portals are available (ie, not heaven, earth, or invasive)
		if ((portalA.pChunk != NULL && !portalA.hasChunk()) ||
			 (portalB.pChunk != NULL && !portalB.hasChunk()) )
		{
			return false;
		}

		if (portalA.points.size() != portalB.points.size())
		{
			return false;
		}

		if (! almostEqual( ( portalA.centre - portalB.centre ).lengthSquared(), 0.f ))
		{
			return false;
		}

		Vector3 n1 = chunkA->transform().applyVector(portalA.plane.normal());
		Vector3 n2 = chunkB->transform().applyVector(portalB.plane.normal());

		// Check normals are opposite
		if (! almostEqual( ( n1 + n2 ).length(), 0.f ))
		{
			return false;
		}

		std::vector< Vector3 > points;

		for (unsigned int i = 0; i < portalA.points.size(); ++i)
		{
			Vector3 v = chunkA->transform().applyPoint( portalA.objectSpacePoint( i ) );
			points.push_back( v );
		}

		for (unsigned int i = 0; i < portalA.points.size(); ++i)
		{
			Vector3 v = chunkB->transform().applyPoint( portalB.objectSpacePoint( i ) );
			std::vector< Vector3 >::iterator iter = points.begin();

			for (; iter != points.end(); ++iter)
			{
				if (almostEqual( v, *iter ))
				{
					break;
				}
			}

			if (iter == points.end())
			{
				return false;
			}
		}

		return true;
	}
} // namespace anonymous

/**
 *	Private unbound portal formation method
 */
bool Chunk::formPortal( Chunk * pChunk, ChunkBoundary::Portal & oportal )
{
	BW_GUARD;
	// first see if we already have a portal that fits the bill

	// go through all our boundaries
	// we won't snap non-invasive shell portal to an outdoor chunk
	if (oportal.isInvasive() || ( !oportal.isInvasive() && !this->isOutsideChunk()))
	{
		for (ChunkBoundaries::iterator bit = joints_.begin();
			bit != joints_.end();
			bit++)
		{
			// go through all their unbound portals
			for (ChunkBoundary::Portals::iterator pit =
					(*bit)->unboundPortals_.begin();
				pit != (*bit)->unboundPortals_.end(); pit++)
			{
				if ( canBind( oportal, **pit, pChunk, this ) )
				{
					(*pit)->pChunk = pChunk;

					// ok that's it. we leave it unbound for now as
					// it will soon be bound by an ordinary 'bind' call.
					return true;
				}

				// we could recalculate centres, but we may as well use
				//  the existing cached ones
			}
		}
	}

	// ok we didn't find anything to connect to.
	// if the other chunk's portal isn't invasive, or if
	//  we don't want to be invaded, then no connection is made.
	if (!oportal.isInvasive() || !this->isOutsideChunk()) return false;

	// we'd better form that portal then
	const PlaneEq & fplane = oportal.plane;
	const Vector3 & fnormal = fplane.normal();
	Vector3 wnormal = pChunk->transform_.applyVector( fnormal ) * -1.f;
	Vector3 wcentre = oportal.centre;				// facing other way
//	PlaneEq wplane( wcentre, wnormal );
	Vector3 lnormal = transformInverse_.applyVector( wnormal );
	Vector3 lcentre = transformInverse_.applyPoint( wcentre );
	PlaneEq lplane( lcentre, lnormal );

	// see if any existing planes fit
	bool isInternal = false;
	ChunkBoundaries::iterator bit;
	/*
	for (bit = bounds_.begin(); bit != joints_.end(); bit++)
	{
		PlaneEq & oplane = (*bit)->plane_;
		if ((oplane.normal() - lplane.normal()).lengthSquared() < 0.0001f &&
			fabs(oplane.d() - lplane.d()) < 0.2f)	// 20cm and    1% off
		{
			break;
		}

		if (bit == bounds_.end()-1)
		{		// always has >=4 bounds, so this is safe
			isInternal = true;
			bit = joints_.begin()-1;
		}
	}
	*/
	bit = joints_.end();

	// ok, make a new one then
	if (bit == joints_.end())
	{
		isInternal = true;

		ChunkBoundary * pCB = new ChunkBoundary( NULL, pMapping_ );
		pCB->plane_ = lplane;
		joints_.push_back( pCB );
		bit = joints_.end() - 1;
	}

	// make up the portal on it
	ChunkBoundary::Portal * portal =
		new ChunkBoundary::Portal( NULL, (*bit)->plane_, pMapping_ );
	portal->internal = isInternal;
	portal->pChunk = pChunk;

	// Figure out the basis for the polygon in this chunk's local space

	// 1) Find the cartesian axis that is most perpendicular to the lnormal
	// vector.
	// 1.a) Take the dot product of the lnormal vector with each axis
	float NdotX = lnormal.dotProduct( Vector3(1.0f, 0.0f, 0.0f) );
	float NdotY = lnormal.dotProduct( Vector3(0.0f, 1.0f, 0.0f) );
	float NdotZ = lnormal.dotProduct( Vector3(0.0f, 0.0f, 1.0f) );

	// 1.b) The value which is closest to zero represents the cartesian
	// axis that is the most perpendicular to the lnormal vector
	Vector3 cartesianAxis;

	// First test X against Y
	if ( fabsf(NdotX) < fabsf(NdotY) )
		// If here, test X against Z
		if ( fabsf(NdotX) < fabsf(NdotZ) )
			// X most perpendicular
			cartesianAxis = Vector3(1.0f, 0.0f, 0.0f);
		else
			// Z most perpendicular
			cartesianAxis = Vector3(0.0f, 0.0f, 1.0f);
	else
		// If here, test Y against Z
		if ( fabsf(NdotY) < fabsf(NdotZ) )
			// Y most perpendicular
			cartesianAxis = Vector3(0.0f, 1.0f, 0.0f);
		else
			// Z most perpendicular
			cartesianAxis = Vector3(0.0f, 0.0f, 1.0f);

	// 2) Now that the most perpendicular axis has been found, it can
	// be used to find the tangent vector, luAxis
	Vector3 luAxis = lnormal.crossProduct( cartesianAxis );

	// 3) The normal and the tangent vectors can now be used to find the
	// binormal (remember cartesianAxis was only the closest perpendicular
	// axis, it probably isn't going to be perpendicular)
	Vector3 lvAxis = lnormal.crossProduct( luAxis );

	// turn it into a matrix (actually using matrix for ordinary maths!)
	Matrix basis = Matrix::identity;
	memcpy( basis[0], luAxis, sizeof(Vector3) );
	memcpy( basis[1], lvAxis, sizeof(Vector3) );
	memcpy( basis[2], lnormal, sizeof(Vector3) );
		// error from plane is in the z.
	basis.translation( lnormal * lplane.d() / lnormal.lengthSquared() );
	Matrix invBasis; invBasis.invert( basis );

	// use it to convert the world coordinates of the points into local space
	for (uint i = 0; i < oportal.points.size(); i++)
	{
		// point starts in form portal's space
		Vector3 fpt =
			oportal.uAxis * oportal.points[i][0] +
			oportal.vAxis * oportal.points[i][1] +
			oportal.origin;
		// now in form chunk's space
		Vector3 wpt = pChunk->transform_.applyPoint( fpt );
		// now in world space
		Vector3 lpt = transformInverse_.applyPoint( wpt );
		// now in our chunk's space
		Vector3 ppt = invBasis.applyPoint( lpt );
		// and finally in our portal's space
		portal->points.push_back( Vector2( ppt.x, ppt.y ) );
	}
	portal->uAxis = basis.applyToUnitAxisVector(0); //luAxis;
	portal->vAxis = basis.applyToUnitAxisVector(1); //lvAxis;
	portal->origin = basis.applyToOrigin();
	portal->lcentre = transformInverse_.applyPoint( wcentre );
	portal->centre = wcentre;

	// now do the dodgy reverse portal hack thing, from ChunkBoundary::Portal
	// hack for 4-sided polygons - reverse order of two middle points
	// if plane from first three points points the wrong way.
	if (portal->points.size() == 4)
	{
		PlaneEq testPlane(
			portal->points[0][0] * portal->uAxis + portal->points[0][1] * portal->vAxis + portal->origin,
			portal->points[1][0] * portal->uAxis + portal->points[1][1] * portal->vAxis + portal->origin,
			portal->points[2][0] * portal->uAxis + portal->points[2][1] * portal->vAxis + portal->origin );
		Vector3 n1 = (*bit)->plane_.normal();
		Vector3 n2 = testPlane.normal();
		n1.normalise();	n2.normalise();
		if ((n1 + n2).length() < 1.f)	// should be 2 if equal
		{
			Vector2 tpt = portal->points[1];
			portal->points[1] = portal->points[3];
			portal->points[3] = tpt;
		}
	}

	// and add it as an unbound portal
	(*bit)->addInvasivePortal( portal );

	// let the caches know things have changed
	this->notifyCachesOfBind( false );

	// and record if we now have internal chunks
	hasInternalChunks_ |= isInternal;

	return true;
}


/**
 *	Private method to undo a binding from one chunk
 */
void Chunk::loose( Chunk * pChunk, bool cut )
{
	BW_GUARD;
	// go through all our boundaries
	for (ChunkBoundaries::iterator bit = joints_.begin();
		bit != joints_.end();
		bit++)
	{
		// go through all their bound portals
		for (ChunkBoundary::Portals::iterator ppit =
				(*bit)->boundPortals_.begin();
			ppit != (*bit)->boundPortals_.end(); ppit++)
		{
			ChunkBoundary::Portal * pit = *ppit;
			if (pit->pChunk == pChunk)
			{
				// clear the link if we're cutting it out
				if (cut)
				{
					if( !isOutsideChunk() && pChunk->isOutsideChunk() )
						pit->pChunk = (Chunk*)ChunkBoundary::Portal::INVASIVE;
					else
						pit->pChunk = NULL;	// note: bounds_ not updated

					// and get rid of the whole boundary if this was
					//  an internal portal on a non-bounding plane
					if (pit->internal)
					{
						// TODO: check there aren't other internal portals
						//  on the same plane! (or do they all get their own?)

						joints_.erase( bit );

						this->notifyCachesOfBind( true );

						// TODO: set hasInternalChunks_ appropriately

						return;
					}
				}

				(*bit)->loosePortal( ppit - (*bit)->boundPortals_.begin() );

				this->notifyCachesOfBind( true );

				// we return here - just like in 'bind' above.
				return;
			}
		}
	}

	ERROR_MSG( "Chunk::loose: Chunk %s didn't find reverse portal to %s!\n",
		identifier_.c_str(), pChunk->identifier().c_str() );
}



void Chunk::syncInit()
{
	BW_GUARD;
	#if UMBRA_ENABLE
		// Create umbra cell
	if (ChunkUmbra::softwareMode() && !isOutsideChunk_)
	{
		pUmbraCell_ = Umbra::Cell::create();
	}
	#endif

	MatrixMutexHolder lock( this );
	Items::iterator it;
	for (it = selfItems_.begin(); it != selfItems_.end(); it++)
	{
		(*it)->syncInit();
	}
}

/**
 *	Private method to notify any caches we have that our bindings have changed
 */
void Chunk::notifyCachesOfBind( bool looseNotBind )
{
	BW_GUARD;
	// let the caches know things have changed
	for (int i = 0; i < Chunk::nextCacheID_; i++)
	{
		ChunkCache * cc = caches_[i];
		if (cc != NULL) cc->bind( looseNotBind );
	}

	// and see if we want to lend any of our items anywhere,
	// as long as this really was due to a bind
	if (!looseNotBind)
	{
		Items::iterator it;
		{
			MatrixMutexHolder lock( this );
			for (it = selfItems_.begin(); it != selfItems_.end(); it++)
			{
				(*it)->lend( this );
			}
		}
		Lenders::iterator lit;
		for (lit = lenders_.begin(); lit != lenders_.end(); lit++)
		{
			MatrixMutexHolder lock( (*lit).getObject() );
			for (it = (*lit)->items_.begin(); it != (*lit)->items_.end(); it++)
				(*it)->lend( this );
		}

		// (no point doing it when loosed as we might lend them
		// back to the chunk that's just trying to get rid of them!)
	}
}

/**
 *	Add this static item to our list
 */
void Chunk::updateBoundingBoxes( ChunkItemPtr pItem )
{
	BW_GUARD;
	if( pItem->addYBounds( localBB_ ) )
	{
		boundingBox_ = localBB_;
		boundingBox_.transformBy( transform() );
	}

#ifndef MF_SERVER
	pItem->addYBounds(visibilityBox_);
#endif // MF_SERVER
}

/**
 *	Add this static item to our list
 */
void Chunk::addStaticItem( ChunkItemPtr pItem )
{
	{
		MatrixMutexHolder lock( this );

		if( !isOutsideChunk() && localBB_.insideOut() )
		{// this is the first item of a shell chunk, which should be the shell model
#ifdef MF_SERVER
				localBB_ = ( (ServerChunkModel*)pItem.getObject() )->localBB();
				boundingBox_ = localBB_;
#else//MF_SERVER
				localBB_ = ( (ChunkModel*)pItem.getObject() )->localBB();
				visibilityBox_ = localBB_;
				boundingBox_ = localBB_;
#endif//MF_SERVER
				boundingBox_.transformBy( transform_ );
		}

		updateBoundingBoxes( pItem );

		// add it to our lists
		selfItems_.push_back( pItem );
	}
	if (pItem->wantsSway()) swayItems_.push_back( pItem );

	// tell it where it belongs
	pItem->toss( this );

	// and lent it around if we're online
	if (this->online())
	{
		pItem->lend( this );
	}
}

/**
 *	Remove this static item from our list
 */
void Chunk::delStaticItem( ChunkItemPtr pItem )
{
	BW_GUARD;
	// make sure we have it
	MatrixMutexHolder lock( this );
	Items::iterator found = std::find(
		selfItems_.begin(), selfItems_.end(), pItem );
	if (found == selfItems_.end()) return;

	// recall it if we're online
	if (this->online())
	{
		uint bris = borrowers_.size();
		for (uint bri = 0; bri < bris; bri++)
		{
			borrowers_[bri]->delLoanItem( pItem );

			// see if the borrower was removed, which happens
			// when this was the last item lent to it
			uint newBris = borrowers_.size();
			if (bris != newBris)
			{
				bri--;
				bris = newBris;
			}
		}
	}

	// remove it
	selfItems_.erase( found );

	// also remove it from sway
	if (pItem->wantsSway())
	{
		found = std::find( swayItems_.begin(), swayItems_.end(), pItem );
		if (found != swayItems_.end()) swayItems_.erase( found );
	}

	// and tell it it's no longer in a chunk
	pItem->toss( NULL );
}


/**
 *	Add this dynamic item to our list
 */
void Chunk::addDynamicItem( ChunkItemPtr pItem )
{
	BW_GUARD;
	dynoItems_.push_back( pItem );
	pItem->toss( this );
}


/**
 *	Push this dynamic item around until it's in the right chunk
 *
 *	@return true on success, false if no chunk could be found
 */
bool Chunk::modDynamicItem( ChunkItemPtr pItem,
	const Vector3 & oldPos, const Vector3 & newPos, const float diameter,
	bool bUseDynamicLending )
{
	BW_GUARD;
	// tell any sway items about it
	for (Items::iterator it = swayItems_.begin(); it != swayItems_.end(); it++)
	{
		(*it)->sway( oldPos, newPos, diameter );
	}

	// find out what column it is in
	ChunkSpace::Column * pCol = pSpace_->column( newPos, false );
	float radius = diameter > 1.f ? diameter*0.5f : 0.f;

	// see if it's still within our boundary
	if (!hasInternalChunks_ &&
		(!isOutsideChunk_ || pCol == NULL || !pCol->hasInsideChunks()) &&
		this->contains( newPos, radius ))
	{
		// can only optimise like this if we don't have internal chunks,
		//  and we're an inside chunk or we're an outside chunk but the
		//  column we're the outside chunk for doesn't have any inside chunks
		return true;
	}

	// find the chunk that it is in then
	// (not checking portals / space changes for now)
	Chunk * pDest = pCol != NULL ? pCol->findChunk( newPos ) : NULL;

	if ( bUseDynamicLending && radius > 0.f )
	{
#ifndef MF_SERVER
		static DogWatch dWatch("DynamicLending");
		dWatch.start();
#endif

		static std::vector<ChunkPtr> nearbyChunks;
		Chunk::piterator pend = this->pend();
		for (Chunk::piterator pit = this->pbegin(); pit != pend; pit++)
		{
			// loop through the valid portals, checking for the previously lent chunks
			// and removing the link.
			if (!pit->hasChunk()) continue;
			Chunk * pConsider = pit->pChunk;

			// Remove old lending data
			pConsider->delLoanItem( pItem, true );

			// Store if it's close to the new position.
			if ( pConsider->boundingBox().distance(newPos) <= radius )
				nearbyChunks.push_back(pConsider);
		}

		// check for chunk changes
		if (pDest != this)
		{
			nearbyChunks.clear();
			// move it around
			this->delDynamicItem( pItem, false );
			if (pDest != NULL)
			{
				pDest->addDynamicItem( pItem );
			}
			else
			{
				pSpace_->addHomelessItem( pItem.getObject() );
#ifndef MF_SERVER
				dWatch.stop();
#endif
				return false;
			}
		}

		// Used the cached chunk list if available.
		if (nearbyChunks.size())
		{
			std::vector<ChunkPtr>::iterator cit=nearbyChunks.begin();
			while(cit != nearbyChunks.end())
			{
				// in this branch it's safe to assume that if the first fails,
				// the rest will already have been added.
				if (!(*cit)->addLoanItem( pItem ))
					break;
				cit++;
			}
			nearbyChunks.clear();
		}
		else
		{
			Chunk::piterator pend2 = pDest->pend();
			for (Chunk::piterator pit = pDest->pbegin(); pit != pend2; pit++)
			{
				// loop through the portals of the destination, checking for chunks to lend
				// this item to.
				if (!pit->hasChunk()) continue;

				Chunk * pConsider = pit->pChunk;
				// don't lend to the destination chunk.
				if (pConsider == pDest ||
					pConsider->boundingBox().distance(newPos) > (radius)) continue;

				pConsider->addLoanItem( pItem );
			}
		}
#ifndef MF_SERVER
		dWatch.stop();
#endif
	}
	else if (pDest != this) // and move it around (without worrying about the radius)
	{
		this->delDynamicItem( pItem, false );
		if (pDest != NULL)
		{
			pDest->addDynamicItem( pItem );
		}
		else
		{
			pSpace_->addHomelessItem( pItem.getObject() );
			return false;
		}
	}

	return true;
}


/**
 *	Remove this dynamic item from our list
 */
void Chunk::delDynamicItem( ChunkItemPtr pItem, bool bUseDynamicLending /* =true */ )
{
	BW_GUARD;
	if (bUseDynamicLending)
	{
		// Remove lent items.
		Chunk::piterator pend = this->pend();
		for (Chunk::piterator pit = this->pbegin(); pit != pend; pit++)
		{
			// loop through the valid portals, checking for the previously lent chunks
			// and removing the link.
			if (!pit->hasChunk()) continue;

			Chunk * pConsider = pit->pChunk;
			pConsider->delLoanItem( pItem, true );
		}
	}

	Items::iterator found = std::find(
		dynoItems_.begin(), dynoItems_.end(), pItem );

	if (found != dynoItems_.end())
	{
		dynoItems_.erase( found );
		pItem->toss( NULL );
	}
}


/**
 *	Jog all our foreign items and see if they fall into a different
 *	chunk now (after a chunk has been added to our column)
 */
void Chunk::jogForeignItems()
{
	BW_GUARD;
	// assume all dynamic items are foreign
	int diSize = dynoItems_.size();
	for (int i = 0; i < diSize; i++)
	{
		Items::iterator it = dynoItems_.begin() + i;

		// see if it wants to move to a smaller chunk <sob>
		ChunkItemPtr cip = (*it);	// this iterator can be invalidated 
									// in nest()
		cip->nest( pSpace_ );

		// adjust if item removed
		int niSize = dynoItems_.size();
		i -= diSize - niSize;
		diSize = niSize;
	}

	// only items that want to nest could be foreign
	MatrixMutexHolder lock( this );
	int siSize = selfItems_.size();
	for (int i = 0; i < siSize; i++)
	{
		Items::iterator it = selfItems_.begin() + i;
		if (!(*it)->wantsNest()) continue;

		// see if it wants to move to a smaller chunk <sob>
		ChunkItemPtr cip = (*it);	// this iterator can be invalidated 
									// in nest()
		cip->nest( pSpace_ );

		// adjust if item removed
		int niSize = selfItems_.size();
		i -= siSize - niSize;
		siSize = niSize;
	}
}


/**
 *	Lends this item to this chunk. If this item is already in
 *	this chunk (lent or owned) then the call is ignored, otherwise
 *	it is added to this chunk and its lend method is called
 *	again from this chunk.
 */
bool Chunk::addLoanItem( ChunkItemPtr pItem )
{
	BW_GUARD;
	// see if it's our own item
	Chunk * pSourceChunk = pItem->chunk();
	if (pSourceChunk == this) return false;

	// see if we've seen its chunk before
	Lenders::iterator lit;
	for (lit = lenders_.begin(); lit != lenders_.end(); lit++)
	{
		if ((*lit)->pLender_ == pSourceChunk) break;
	}
	if (lit != lenders_.end())
	{
		// see if we've already got its item
		Items::iterator found = std::find(
			(*lit)->items_.begin(), (*lit)->items_.end(), pItem );
		if (found != (*lit)->items_.end()) return false;
	}
	else
	{
		// never seen this chunk before, so introduce each other
		lenders_.push_back( new Lender() );
		lit = lenders_.end() - 1;
		(*lit)->pLender_ = pSourceChunk;
		pSourceChunk->borrowers_.push_back( this );

		/*TRACE_MSG( "Chunk::addLoanItem: "
			"%s formed relationship with lender %s\n",
			identifier_.c_str(), pSourceChunk->identifier_.c_str() );*/
	}

	// ok, add the item on loan then
	(*lit)->items_.push_back( pItem );

#if UMBRA_ENABLE
	// The cells for the chunks are different add a umbra lender
	// the reason for this is that all outdoor chunks use the same cell
	// and as such do not have any lending problems
	if (pItem->chunk()->getUmbraCell() != this->getUmbraCell())
	{
		// Get the new item transform, the item transform is the transform
		// of the object in the current cell. Outside chunks use the identity
		// transform as they are all one umbra cell
		Matrix lenderChunkTransform = Matrix::identity;
		Matrix invBorrowerChunkTransform = Matrix::identity;
		invBorrowerChunkTransform.invert();
		
		Matrix itemTransform;
		UmbraObjectProxyPtr pChunkObject = pItem->pUmbraObject();
		if (pChunkObject && pChunkObject->object())
		{
			pChunkObject->object()->getObjectToCellMatrix((Umbra::Matrix4x4&)itemTransform);
			itemTransform.postMultiply( lenderChunkTransform );
			itemTransform.postMultiply( invBorrowerChunkTransform );

			// Set up the umbra object
			UmbraObjectProxyPtr pLenderObject = UmbraObjectProxy::get( pChunkObject->pModelProxy() );
			pLenderObject->object()->setUserPointer( pChunkObject->object()->getUserPointer() );
			pLenderObject->object()->setCell( this->getUmbraCell() );
			pLenderObject->object()->setObjectToCellMatrix( (Umbra::Matrix4x4&)itemTransform );
			
			// Add the umbra object to the lent item list
			(*lit)->umbraItems_.insert(std::make_pair(pItem.getObject(), pLenderObject));
		}
	}
#endif

	// loan items can also be sway items
	if (pItem->wantsSway()) swayItems_.push_back( pItem );

	// and push it around again from our point of view
	pItem->lend( this );

    return true;
}


/**
 *	Recalls this item from this chunk. The item may not be in the chunk,
 *	but the caller has no way of knowing that.
 *	This method is called automatically when a static item is removed
 *	from its home chunk.
 */
bool Chunk::delLoanItem( ChunkItemPtr pItem, bool bCanFail )
{
	BW_GUARD;
	Chunk * pSourceChunk = pItem->chunk();

	// find our lender record
	Lenders::iterator lit;
	for (lit = lenders_.begin(); lit != lenders_.end(); lit++)
	{
		if ((*lit)->pLender_ == pSourceChunk) break;
	}
	if (lit == lenders_.end())
	{
		// Added bCanFail to avoid error messages with the dynamic lending.
		if (!bCanFail)
			ERROR_MSG( "Chunk::delLoanItem: "
				"No lender entry in %s for borrower entry in %s!\n",
				identifier_.c_str(), pSourceChunk->identifier_.c_str() );
		return false;
	}
#if UMBRA_ENABLE
	// remove the umbra object from the list if it is there
	UmbraItems::iterator dit = (*lit)->umbraItems_.find( pItem.getObject() );
	if (dit != (*lit)->umbraItems_.end())
	{
		(*lit)->umbraItems_.erase(dit);
	}
#endif

	// see if we know about the item
	Items::iterator found = std::find(
		(*lit)->items_.begin(), (*lit)->items_.end(), pItem );
	if (found == (*lit)->items_.end()) return false;

	// get rid of it then
	(*lit)->items_.erase( found );

	// and see if we're not talking any more
	if ((*lit)->items_.empty())
	{
		lenders_.erase( lit );

		Borrowers::iterator brit;
		brit = std::find( pSourceChunk->borrowers_.begin(),
			pSourceChunk->borrowers_.end(), this );
		if (brit == pSourceChunk->borrowers_.end())
		{
			CRITICAL_MSG( "Chunk::delLoanItem: "
				"No borrower entry in %s for lender entry in %s!\n",
				pSourceChunk->identifier_.c_str(), identifier_.c_str() );
			return false;
		}
		pSourceChunk->borrowers_.erase( brit );

		/*TRACE_MSG( "Chunk::delLoanItem: "
			"%s ended relationship with lender %s\n",
			identifier_.c_str(), pSourceChunk->identifier_.c_str() );*/
	}

    return true;
}


/**
 *  Checks whether pItem has been loaned to this chunk.
 */
bool Chunk::isLoanItem( ChunkItemPtr pItem ) const
{
    BW_GUARD;
	Chunk *pSourceChunk = pItem->chunk();

	// find our lender record
	Lenders::const_iterator lit;
	for (lit = lenders_.begin(); lit != lenders_.end(); lit++)
	{
		if ((*lit)->pLender_ == pSourceChunk) break;
	}
	if (lit == lenders_.end())
	{
		return false;
	}

	// see if we know about the item
	Items::iterator found = std::find(
		(*lit)->items_.begin(), (*lit)->items_.end(), pItem );
	return found != (*lit)->items_.end();
}


#ifndef MF_SERVER

/**
 *	Commence drawing of this chunk.
 */
void Chunk::drawBeg()
{
	BW_GUARD;
	if (drawMark() == s_nextMark_)
	{
		return;
	}

	++ChunkManager::s_chunksTraversed;

	bool drawSelf = this->drawSelf();
	if (drawSelf)
	{
		// and make sure our space won't
		// draw us due to lent items
		if (fringePrev_ != NULL)
		{
			ChunkManager::instance().delFringe( this );
		}

		// we've rendered this chunk
		++ChunkManager::s_chunksVisible;

		#if ENABLE_CULLING_HUD
			BoundingBox contractBox = this->visibilityBox();
			float offset = -10.0f * std::min(7, ChunkManager::s_drawPass);
			contractBox.expandSymmetrically(offset, 0, offset);
			s_visibleChunks.push_back(
				std::make_pair(
					this->transform(),
					contractBox));
		#endif // ENABLE_CULLING_HUD
	}
	else
	{
		#if ENABLE_CULLING_HUD
			s_traversedChunks.push_back(
				std::make_pair(
					this->transform(),
					this->visibilityBox()));
		#endif // ENABLE_CULLING_HUD
	}

	if (drawSelf)
	{
		// make sure we don't
		// come back here again
		this->drawMark( s_nextMark_ );
	}

	if (!Moo::rc().reflectionScene() && this->reflectionMark() != s_nextMark_)
	{
		// we may want to render for reflection
		ChunkManager::instance().addToCache(this, false);
		this->reflectionMark( s_nextMark_ );
	}
}

/**
 *	Complete drawing of the chunk.
 */
void Chunk::drawEnd()
{
	BW_GUARD;
	// Only draw fringe chunks if the chunk has actually been drawn.
	// This is as the traversal calls the drawEnd method regardless
	// of the chunk having been drawn or not.
	if (drawMark() == s_nextMark_)
	{
		// now go through all the chunks that have lent us items, and make sure
		//  they get drawn even if the traversal doesn't reach them
		for (Lenders::iterator lit = lenders_.begin(); lit != lenders_.end(); lit++)
		{
			if ((*lit)->pLender_->drawMark() != s_nextMark_)
			{
				Chunk * pLender = (*lit)->pLender_;
				MF_ASSERT(lentItemLists_.size() == 0);
				pLender->lentItemLists_.push_back( &(*lit)->items_ );

				if ((*lit)->pLender_->fringePrev() == NULL)
					ChunkManager::instance().addFringe( pLender );
			}
		}
	}
}


void Chunk::drawCaches()
{
	BW_GUARD;
	// put our world transform on the render context
	Moo::rc().push();
	Moo::rc().world( transform_ );

	// now 'draw' all the caches
	for (int i = 0; i < nextCacheID_; i++)
	{
		ChunkCache * cc = caches_[i];
		if (cc != NULL) cc->draw();
	}
	Moo::rc().pop();
}

/**
 *	Draw this chunk
 */
#ifdef EDITOR_ENABLED
bool Chunk::hideIndoorChunks_ = false;
#endif

bool Chunk::drawSelf( bool lentOnly )
{
	BW_GUARD;
	IF_NOT_MF_ASSERT_DEV( this->online() )
	{
		return false;
	}

	bool result = false;
	bool isOutside = this->isOutsideChunk();

	BoundingBox vbox = this->visibilityBox();
	vbox.calculateOutcode( Moo::rc().viewProjection() );

	if (lentOnly                                       ||
		this == ChunkManager::instance().cameraChunk() ||
		!ChunkManager::s_enableChunkCulling            ||
		vbox.combinedOutcode() == 0
#ifdef EDITOR_ENABLED
		&& (isOutside || !hideIndoorChunks_)
#endif // EDITOR_ENABLED
		)
	{
		// Render bounding box
		if (ChunkManager::s_drawVisibilityBBoxes)
		{
			Moo::Material::setVertexColour();
			Geometrics::wireBox(
				this->visibilityBox(),
				Moo::Colour(1.0, 0.0, 0.0, 0.0));
		}


		Moo::EffectVisualContext::instance().isOutside( isOutside );

		// put our world transform on the render context
		Moo::rc().push();
		Moo::rc().world( transform_ );

		// now 'draw' all the caches
		for (int i = 0; i < nextCacheID_; i++)
		{
			ChunkCache * cc = caches_[i];
			if (cc != NULL) cc->draw();
		}

		// and draw our subjects
		Items::iterator it;
		if (!lentOnly)
		{
			// normal draw
			MatrixMutexHolder lock( this );
			for (it = selfItems_.begin(); it != selfItems_.end(); it++)
			{
				++ChunkManager::s_visibleCount;
				(*it)->draw();
				(*it)->drawMark(s_nextMark_);
			}

			for (it = dynoItems_.begin(); it != dynoItems_.end(); it++)
			{
				++ChunkManager::s_visibleCount;
				(*it)->draw();
				(*it)->drawMark(s_nextMark_);
			}
		}
		else
		{
			// lent items only
			uint lils = lentItemLists_.size();
			for (uint i = 0; i < lils; i++)
			{
				for (it = lentItemLists_[i]->begin();
					it != lentItemLists_[i]->end();
					it++)
				{
					ChunkItem * pCI = &**it;
					if (pCI->drawMark() != s_nextMark_)
					{
						++ChunkManager::s_visibleCount;
						pCI->drawMark( s_nextMark_ );
						pCI->draw();
					}
				}
			}

			#if ENABLE_CULLING_HUD
				BoundingBox contractBox = this->visibilityBox();
				float offset = -10.0f * std::min(7, ChunkManager::s_drawPass);
				contractBox.expandSymmetrically(offset, 0, offset);
				s_fringeChunks.push_back(
					std::make_pair(
						this->transform(),
						contractBox));
			#endif // ENABLE_CULLING_HUD

		}

		if (Moo::rc().reflectionScene())
		{
			// add to culling HUD
			++ChunkManager::s_chunksReflected;
			#if ENABLE_CULLING_HUD
				BoundingBox refectedtBox = vbox;
				float offset = -10.0f * std::min(7, ChunkManager::s_drawPass);
				refectedtBox.expandSymmetrically(offset, 0, offset);
				s_reflectedChunks.push_back(std::make_pair(this->transform(), refectedtBox));
			#endif // ENABLE_CULLING_HUD
		}

		Moo::rc().pop();
		result = true;

		// clear the lent items lists
		lentItemLists_.clear();
	}

	/**
	//		... Portal debugging ...
	float inset = uint(identifier_.find( 'i' )) < identifier_.length() ? 0.2f : 0.f;
	// temporarily draw all portals
	int count = 0;
	for (piterator it = this->pbegin(); it != this->pend(); it++)
	{
		it->display( transform_, transformInverse_, inset );
		count++;
		if (count > 6)
		{
			DEBUG_MSG( "Chunk %s portal %d (@ 0x%08X) : centre (%f,%f,%f)\n",
				identifier_.c_str(), count-1, uint32(&*it),
				it->centre[0], it->centre[1], it->centre[2] );
		}
	}
	**/

	return result;
}


/**
 *	Tick this chunk
 */
void Chunk::tick( float dTime )
{
	// tick our subjects
	BW_GUARD_PROFILER( Chunk_tick );
	MatrixMutexHolder lock( this );

	PROFILER_BEGIN( Chunk_tick2 );
	for (Items::iterator it = selfItems_.begin(); it != selfItems_.end(); it++)
	{
		(*it)->tick( dTime );
	}
	for (Items::iterator it = dynoItems_.begin(); it != dynoItems_.end(); it++)
	{
		(*it)->tick( dTime );
	}
	PROFILER_END();
}

/** Chunk debug helper. Was useful when debugging
 *  chunk link culling problems. May be useful again
 *  in the future. That's why I am leaving it here.
 * /
void Chunk::viewVisibilityBBox(bool visible)
{
	if (visible)
	{
		s_debugBoxes[this] = this->visibilityBox();
	}
	else
	{
		BBoxMap::iterator chunkIt = s_debugBoxes.find(this);
		if (chunkIt != s_debugBoxes.end())
		{
			s_debugBoxes.erase(chunkIt);
		}
	}
}
*/

#endif // MF_SERVER





/**
 *	Helper function used by ChunkManager's blindpanic method.
 *
 *	Calculates the closest unloaded chunk to the given point.
 *	Since the chunk isn't loaded, we can't of course use its transform,
 *	instead we approximate it by the centre of the portal to
 *	that chunk.
 */
Chunk *	Chunk::findClosestUnloadedChunkTo( const Vector3 & point,
	float * pDist )
{
	BW_GUARD;
	Chunk * pClosest = NULL;
	float dist = 0.f;

	// go through all our boundaries
	for (ChunkBoundaries::iterator bit = joints_.begin();
		bit != joints_.end();
		bit++)
	{
		// go through all their unbound portals
		for (ChunkBoundary::Portals::iterator ppit =
				(*bit)->unboundPortals_.begin();
			ppit != (*bit)->unboundPortals_.end(); ppit++)
		{
			ChunkBoundary::Portal * pit = *ppit;
			if (!pit->hasChunk()) continue;

			float tdist = (pit->centre - point).length();
			if (!pClosest || tdist < dist)
			{
				pClosest = pit->pChunk;
				dist = tdist;
			}
		}
	}

	*pDist = dist;
	return pClosest;
}



/**
 *	This method changes this chunk's transform and updates anything
 *	that has stuff cached in world co-ordinates and wants to move with
 *	the chunk. It can only be done when the chunk is not bound.
 */
void Chunk::transform( const Matrix & transform )
{
	BW_GUARD;
	IF_NOT_MF_ASSERT_DEV( !this->online() )
	{
		return;
	}

	Matrix oldXFormInv = transformInverse_;

	// set the transform
	transform_ = transform;
	transformInverse_.invert( transform );

	// move the bounding box
	boundingBox_ = localBB_;
	boundingBox_.transformBy( transform );

	// set the centre point
	centre_ = boundingBox_.centre();

	// go through all our boundaries
	for (ChunkBoundaries::iterator bit = joints_.begin();
		bit != joints_.end();
		bit++)
	{
		// go through all their bound portals
		for (ChunkBoundary::Portals::iterator pit =
				(*bit)->boundPortals_.begin();
			pit != (*bit)->boundPortals_.end(); pit++)
		{
			(*pit)->centre = transform.applyPoint( (*pit)->lcentre );
		}

		// go through all their unbound portals
		for (ChunkBoundary::Portals::iterator pit =
				(*bit)->unboundPortals_.begin();
			pit != (*bit)->unboundPortals_.end(); pit++)
		{
			(*pit)->centre = transform.applyPoint( (*pit)->lcentre );

			// if we are not online then also resolve extern portals here
			// (now that the portal knows its centre)
			if ((*pit)->isExtern() && !this->online())
				(*pit)->resolveExtern( this );
		}
	}

	// if we've not yet loaded, this is all we have to do
	if (!this->loaded()) return;

	// let our static items know, by tossing them to ourselves
	MatrixMutexHolder lock( this );
	for (Items::iterator it = selfItems_.begin(); it != selfItems_.end(); it++)
	{
		(*it)->toss( this );
	}

	// our dynamic items will get jogged when the columns are recreated
	// TODO: Make sure this always happens. At the moment it might not.
	//  So this method is safe for editor use, but not yet for client use.

	// if we have any caches then they will get refreshed when we bind.
	// if any cache keeps info across 'bind' calls, then another notification
	// could be added here ... currently however, none do.
}


/**
 *	This method changes this chunk's transform temporarily while bound.
 *	It should only be used on an online chunk, and it should be set back
 *	to its proper transform before any other operation is performed on
 *	this chunk or its neighbours, including binding (so all neighbouring
 *	chunks must be loaded and online)
 */
void Chunk::transformTransiently( const Matrix & transform )
{
	BW_GUARD;
	IF_NOT_MF_ASSERT_DEV( this->online() )
	{
		return;
	}

	transform_ = transform;
	transformInverse_.invert( transform );

	// move the bounding box
	boundingBox_ = localBB_;
	boundingBox_.transformBy( transform );

	// set the centre point
	centre_ = boundingBox_.centre();

	// go through all our boundaries
	for (ChunkBoundaries::iterator bit = joints_.begin();
		bit != joints_.end();
		bit++)
	{
		// go through all their bound portals
		for (ChunkBoundary::Portals::iterator pit =
				(*bit)->boundPortals_.begin();
			pit != (*bit)->boundPortals_.end(); pit++)
		{
			(*pit)->centre = transform.applyPoint( (*pit)->lcentre );
		}

		// go through all their unbound portals
		for (ChunkBoundary::Portals::iterator pit =
				(*bit)->unboundPortals_.begin();
			pit != (*bit)->unboundPortals_.end(); pit++)
		{
			(*pit)->centre = transform.applyPoint( (*pit)->lcentre );

			// if we are not online then also resolve extern portals here
			// (now that the portal knows its centre)
			if ((*pit)->isExtern() && !this->online())
				(*pit)->resolveExtern( this );
		}
	}
}


/**
 *	This method determines whether or not the given point is inside
 *	this chunk. It uses only the convex hull of the space - internal
 *	chunks and their friends are not considered.
 */
bool Chunk::contains( const Vector3 & point, const float radius ) const
{
	BW_GUARD;
	// first check the bounding box
	if (!boundingBox_.intersects( point )) return false;

	// bring the point into our own space
	Vector3 localPoint = transformInverse_.applyPoint( point );

	// now check the actual boundary
	for (ChunkBoundaries::const_iterator it = bounds_.begin();
		it != bounds_.end();
		it++)
	{
		if ((*it)->plane().distanceTo( localPoint ) < radius) return false;
	}

	return true;
}

/**
 *	This method determines whether or not the given point is inside
 *	this chunk. Unlike contains, it will check for internal chunks
 */
bool Chunk::owns( const Vector3 & point )
{
	BW_GUARD;
	if (isOutsideChunk())
	{
		if (!contains( point ))
		{
			return false;
		}

		std::vector<Chunk*> overlappers = overlapperFinder()( this );

		for (std::vector<Chunk*>::iterator iter = overlappers.begin();
			iter != overlappers.end(); ++iter)
		{
			Chunk* overlapper = *iter;

			if (overlapper->contains( point ))
			{
				return false;
			}
		}
		return true;
	}
	return contains( point );
}

/**
 *	This method approximates the volume of the chunk.
 *	For now we just return the volume of its bounding box.
 */
float Chunk::volume() const
{
	Vector3		v =
		boundingBox_.maxBounds() - boundingBox_.minBounds();
	return v[0] * v[1] * v[2];
}


/**
 *	The binary data file name for this chunk.
 */
std::string Chunk::binFileName() const
{
	return mapping()->path() + identifier() + ".cdata";
}


#ifndef MF_SERVER
const BoundingBox & Chunk::visibilityBox()
{
	BW_GUARD;
	if (this->visibilityBoxMark_ != s_nextVisibilityMark_)
	{
		this->visibilityBoxCache_ = this->visibilityBox_;
		Items::const_iterator itemsIt  = dynoItems_.begin();
		Items::const_iterator itemsEnd = dynoItems_.end();
		while (itemsIt != itemsEnd)
		{
			(*itemsIt)->addYBounds(this->visibilityBoxCache_);
			++itemsIt;
		}
		if (!this->visibilityBoxCache_.insideOut())
		{
			this->visibilityBoxCache_.transformBy(this->transform());
		}
		this->visibilityBoxMark_ = s_nextVisibilityMark_;
	}

	return this->visibilityBoxCache_;
}


void Chunk::addYBoundsToVisibilityBox(float minY, float maxY)
{
	this->visibilityBox_.addYBounds(minY);
	this->visibilityBox_.addYBounds(maxY);
}
#endif // MF_SERVER

/**
 *	Reconstruct the resource ID of this chunk
 */
std::string Chunk::resourceID() const
{
	return pMapping_->path() + identifier() + ".chunk";
}

/** This static method tries to find a more suitable portal from
 *	two given portals (first portal could be NULL) according to
 *	test point (in local coordinate)
 */
bool Chunk::findBetterPortal( ChunkBoundary::Portal * curr, float withinRange,
	ChunkBoundary::Portal * test, const Vector3 & v )
{
	BW_GUARD;
	if (test == NULL)
	{
		WARNING_MSG( "Chunk::findBetterPortal: testing portal is NULL\n" );
		return false;
	}

	// projection of point onto portal plane must lie inside portal
	float testArea = 0.f;
	bool inside = true;
	Vector2 pt2D( test->uAxis.dotProduct( v ), test->vAxis.dotProduct( v ) );
	Vector2 hpt = test->points.back();
	uint32 npts = test->points.size();
	for (uint32 i = 0; i < npts; i++)
	{
		Vector2 tpt = test->points[i];
		testArea += hpt.x*tpt.y - tpt.x*hpt.y;

		inside &= ((tpt-hpt).crossProduct(pt2D - hpt) > 0.f);
		hpt = tpt;
	}
	if (!inside) return false;

	if (withinRange > 0.f && fabs( test->plane.distanceTo( v ) ) > withinRange)
		return false;

	// if there's no competition then test is the winner
	if (curr == NULL) return true;

	// prefer smaller chunks
	if (test->pChunk != curr->pChunk)
		return test->pChunk->volume() < curr->pChunk->volume();

	// prefer portals close to the test point.
	return fabs(test->plane.distanceTo( v )) < fabs(curr->plane.distanceTo( v ));

	// prefer connections through smaller portals
	float currArea = 0.f;
	hpt = curr->points.back();
	npts = curr->points.size();
	for (uint32 i = 0; i < npts; i++)
	{
		Vector2 tpt = test->points[i];
		currArea += hpt.x*tpt.y - tpt.x*hpt.y;
		hpt = tpt;
	}
	return fabs( testArea ) < fabs( currArea );
}

/**
 *	This static method registers the input factory as belonging
 *	to the input section name. If there is already a factory
 *	registered by this name, then this factory supplants it if
 *	it has a (strictly) higher priority.
 */
void Chunk::registerFactory( const std::string & section,
	const ChunkItemFactory & factory )
{
	BW_GUARD;
	INFO_MSG( "Registering factory for %s\n", section.c_str() );

	// avoid initialisation-order problems
	if (pFactories_ == NULL)
	{
		pFactories_ = new Factories();
	}

	// get a reference to the entry. if it's a new entry, the default
	// 'pointer' constructor will make it NULL.
	const ChunkItemFactory *& pEntry = (*pFactories_)[ section ];

	// and whack it in
	if (pEntry == NULL || pEntry->priority() < factory.priority())
	{
		pEntry = &factory;
	}
}

/**
 *	This static method registers the input cache type. It records the
 *	touch function passed in, which gets called for each type every time
 *	a chunk is loaded (the cache could create itself for that chunk at
 *	that point if it wished). It returns the cache's ID which is stored
 *	by the template 'Instance' class instantiation for that cache type.
 */
int Chunk::registerCache( TouchFunction tf )
{
	touchType().push_back( tf );

	return nextCacheID_++;
}


/**
 *	This method simply tells whether this chunk can see the heavens or not.
 *
 *	@return	true	iff the chunk can see heaven.
 */
bool Chunk::canSeeHeaven()
{
	BW_GUARD;
	for (piterator it = this->pbegin(); it != this->pend(); it++)
	{
		if (it->isHeaven())
			return true;
	}
	return false;
}


#if UMBRA_ENABLE
/**
 *	Get the umbra cell for this chunk
 *	@return the umbra cell of this chunk
 */
Umbra::Cell* Chunk::getUmbraCell() const
{
	BW_GUARD;
	if (pUmbraCell_)
		return pUmbraCell_;

	if (!isOutsideChunk_)
		return pSpace_->umbraInsideCell();

	// If we don't have a cell, assume we are an outside chunk and
	// return the umbra cell for the chunkmanager.
	return  pSpace_->umbraCell();
}
#endif

/**
 *	This method returns the number of static items in this chunk
 */
int Chunk::sizeStaticItems() const
{
	MatrixMutexHolder lock( this );
	return selfItems_.size();
}

#ifndef _RELEASE
/**
 *	Constructor
 */
ChunkCache::ChunkCache()
{
}

/**
 *	Destructor
 */
ChunkCache::~ChunkCache()
{
}
#endif // !_RELEASE


#ifndef MF_SERVER
/**
 *	Draws the chunk debug culler.
 */
void Chunks_drawCullingHUD()
{
	BW_GUARD;
	#if ENABLE_CULLING_HUD
		if (s_cullDebugEnable)
		{
			Chunks_drawCullingHUD_Priv();
		}

		s_traversedChunks.erase(s_traversedChunks.begin(), s_traversedChunks.end());
		s_visibleChunks.erase(s_visibleChunks.begin(), s_visibleChunks.end());
		s_fringeChunks.erase(s_fringeChunks.begin(), s_fringeChunks.end());
		s_reflectedChunks.erase(s_reflectedChunks.begin(), s_reflectedChunks.end());
		s_debugBoxes.erase(s_debugBoxes.begin(), s_debugBoxes.end());
	#endif // ENABLE_CULLING_HUD
}

namespace { // anonymous

#if ENABLE_CULLING_HUD

void Chunks_drawCullingHUD_Priv()
{
	#define DRAW_VBOXES(type, containter, colour)					\
	{																\
		type::const_iterator travIt  = containter.begin();			\
		type::const_iterator travEnd = containter.end();			\
		while (travIt != travEnd)									\
		{															\
			Geometrics::wireBox(travIt->second, colour, true);		\
			++travIt;												\
		}															\
	}

	BW_GUARD;
	Matrix saveView = Moo::rc().view();
	Matrix saveProj = Moo::rc().projection();

	Moo::rc().push();
	Moo::rc().world(Matrix::identity);

	Matrix view = Matrix::identity;
	Vector3 cameraPos = ChunkManager::instance().cameraNearPoint();
	cameraPos.y += s_cullHUDDist;
	view.lookAt(cameraPos, Vector3(0, -1, 0), Vector3(0, 0, 1));
	Moo::rc().view(view);

	Matrix project = Matrix::identity;

	project.orthogonalProjection(
		s_cullHUDDist * Moo::rc().screenWidth() / Moo::rc().screenHeight(),
		s_cullHUDDist, 0, -s_cullHUDDist * 2);
	project.row(0).z = 0;
	project.row(1).z = 0;
	project.row(2).z = 0;
	project.row(3).z = 0;
	Moo::rc().projection(project);

	Moo::rc().setRenderState( D3DRS_ZENABLE, FALSE );
	Moo::rc().setRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
	DRAW_VBOXES(BBoxVector, s_traversedChunks, Moo::Colour(0.5, 0.5, 0.5, 1.0));
	DRAW_VBOXES(BBoxVector, s_visibleChunks, Moo::Colour(1.0, 0.0, 0.0, 1.0));
	DRAW_VBOXES(BBoxVector, s_fringeChunks, Moo::Colour(1.0, 1.0, 0.0, 1.0));
	DRAW_VBOXES(BBoxVector, s_reflectedChunks, Moo::Colour(0.0, 0.0, 1.0, 1.0));

	/** Chunk debug helper. Was useful when debugging
	 *  chunk link culling problems. May be useful again
	 *  in the future. That's why I am leaving it here.
	 * /
	// update debug bounding boxes
	BBoxMap::iterator boxIt  = s_debugBoxes.begin();
	BBoxMap::iterator boxEnd = s_debugBoxes.end();
	while (boxIt != boxEnd)
	{
		BoundingBox expandedBox = boxIt->first->visibilityBox();
		expandedBox.expandSymmetrically(3, 0, 3);
		boxIt->second = expandedBox;
		++boxIt;
	}
	DRAW_VBOXES(BBoxMap, s_debugBoxes, Moo::Colour(0.0, 1.0, 0.0, 1.0));
	*/

	Vector3 cameraX = ChunkManager::instance().cameraAxis(X_AXIS) * 50;
	Vector3 cameraY = ChunkManager::instance().cameraAxis(Y_AXIS) * 50;
	Vector3 cameraZ = ChunkManager::instance().cameraAxis(Z_AXIS) * 150;

	Moo::Material::setVertexColour();
	std::vector<Vector3> cameraLines;
	cameraLines.push_back(cameraPos);
	cameraLines.push_back(cameraPos + cameraZ + cameraX + cameraY);
	cameraLines.push_back(cameraPos + cameraZ - cameraX + cameraY);
	cameraLines.push_back(cameraPos);
	cameraLines.push_back(cameraPos + cameraZ + cameraX - cameraY);
	cameraLines.push_back(cameraPos + cameraZ - cameraX - cameraY);
	cameraLines.push_back(cameraPos);
	cameraLines.push_back(cameraPos + cameraZ + cameraX + cameraY);
	cameraLines.push_back(cameraPos + cameraZ + cameraX - cameraY);
	cameraLines.push_back(cameraPos);
	cameraLines.push_back(cameraPos + cameraZ - cameraX + cameraY);
	cameraLines.push_back(cameraPos + cameraZ - cameraX - cameraY);
	cameraLines.push_back(cameraPos);
	Geometrics::drawLinesInWorld(&cameraLines[0],
		cameraLines.size(),
		cameraZ.y >= 0
			? Moo::Colour(1.0f, 1.0f, 1.0f, 1.0f)
			: Moo::Colour(0.7f, 0.7f, 0.7f, 1.0f));



	/** Experimental **/
	ChunkSpacePtr space = ChunkManager::instance().cameraSpace();
	if (space.exists())
	{
		for (ChunkMap::iterator it = space->chunks().begin();
			it != space->chunks().end();
			it++)
		{
			for (uint i = 0; i < it->second.size(); i++)
			{
				if (it->second[i]->online())
				{
					Geometrics::wireBox(
						it->second[i]->boundingBox(),
						true // it->second[i]->removable()
							? Moo::Colour(1.0f, 1.0f, 1.0f, 1.0f)
							: Moo::Colour(0.0f, 1.0f, 0.0f, 1.0f),
						true);
				}
			}
		}
	}






	Moo::rc().pop();
	Moo::rc().view(saveView);
	Moo::rc().projection(saveProj);

	#undef DRAW_VBOXES
}

#endif // ENABLE_CULLING_HUD

} // namespace anonymous
#endif // MF_SERVER

/*
A bit of explanation about chunk states:
	When chunks are initially created, they are not loaded.
	They are created by the loading thread as stubs for portals
	connect to. These stubs are on a chunk that is already
	loaded AND online. The loading thread doesn't attempt to
	access the space's map of portals to see if there's already
	one there, and it certainly doesn't add one itself (contention
	issues). After a chunk has been loaded, its 'loaded' flag is
	set, and this is picked up by the main thread, which then
	binds the new chunk to the other chunks around it. When
	a chunk has been bound and is ready for use (even if some
	of the chunks it should be bound to haven't loaded yet),
	its 'online' flag is set and it is ready for general use.
	As part of the binding process, the chunk examines all the
	stubs the loader has provided it with. It looks for the
	chunk described by these stubs in the appropriate space's
	map, and if it is there it replaces the stub with a
	reference to the existing chunk, otherwise it adds the
	stub itself to the space's map - the stub becomes a
	fully-fledged unloaded chunk. To prevent the same chunk
	being loaded twice, chunks may not be loaded until they
	have been added to their space's map by some other
	chunk binding them. (The first chunk is of course a special
	case, but the same lesson still hold).

	The birth of a chunk:
		- Created by loading thread as a stub to a chunk being loaded
		- Added to space map when the chunk that caused its
			creation is bound   ('ratified' set to true)
		- Put on ChunkManager's and ChunkLoader's loading queues
		- Loaded by ChunkLoader ('loaded' set to true)
				own portals are stubs
		- Bound by ChunkManager ('online' set to true)
				own portals are real, but maybe some unbound
	[ ============== can now call most functions on the chunk ============== ]
		- Later: Referenced chunks loaded and bound
				own portals are real and all bound

	The main lesson out of all that is this: Just because it's
		in the space map doesn't mean you can draw it -
		check that it is online first!

	Addendum:
	There is a new piece of chunk state information now, and that
	is whether or not the chunk is focussed. A chunk is focussed when
	it is in the area covered by the focus grid in the chunk space.
	Being focussed is similar to the concept of being 'in the world'
	for a model or an entity.
 */

// chunk.cpp
