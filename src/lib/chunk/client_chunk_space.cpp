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

#include "chunk_space.hpp"

#include "cstdmf/debug.hpp"
#include "cstdmf/guard.hpp"
#include "cstdmf/dogwatch.hpp"

#include "physics2/quad_tree.hpp"

#include "resmgr/bwresource.hpp"

#include "chunk.hpp"
#include "chunk_obstacle.hpp"

#include "grid_traversal.hpp"

#include "chunk_manager.hpp"
#include "romp/time_of_day.hpp"


DECLARE_DEBUG_COMPONENT2( "Chunk", 0 )

// ----------------------------------------------------------------------------
// Section: ChunkSpace
// ----------------------------------------------------------------------------

/**
 *	Constructor.
 */
ClientChunkSpace::ClientChunkSpace( ChunkSpaceID id ) :
	BaseChunkSpace( id ),
	ticking_( false ),
	pOutLight_( NULL ),
	sunLight_( NULL ),
	ambientLight_( 0.1f, 0.1f, 0.1f, 1.f ),
	lights_( new Moo::LightContainer() ),
	enviro_(id),
	common_( Matrix::identity ),
	commonInverse_( Matrix::identity ),
	isMapped_(false)
#if UMBRA_ENABLE
	,umbraCell_( NULL ),
	umbraInsideCell_( NULL )
#endif
{
}


/**
 *	Destructor.
 */
ClientChunkSpace::~ClientChunkSpace()
{
	BW_GUARD;
	this->blurSpace();
#if UMBRA_ENABLE
	if (umbraCell_)
	{
		umbraCell_->release();
	}
	if (umbraInsideCell_)
	{
		umbraInsideCell_->release();
	}
#endif
}



/**
 *	We have received some settings from a mapping.
 *	If we haven't set anything up, then now's the time to use them.
 */
void ClientChunkSpace::mappingSettings( DataSectionPtr pSS )
{
	BW_GUARD;
	if (sunLight_) return;

	enviro_.load( pSS );

	sunLight_ = new Moo::DirectionalLight(
		Moo::Colour( 0.8f, 0.5f, 0.1f, 1.f ),
		Vector3( 0, 1 ,0 ) );
	lights_->addDirectional( sunLight_ );
	lights_->ambientColour( ambientLight_ );

	if (enviro_.timeOfDay() != NULL)
	{
		this->heavenlyLightSource( &enviro_.timeOfDay()->lighting() );
	}
	
	this->isMapped_ = true;
}




/**
 *	Blur the whole space
 */
void ClientChunkSpace::blurSpace()
{
	// move the focus miles away so it'll drop all its references
	currentFocus_.origin( currentFocus_.originX() + 10000, 0 );
}


/**
 *	Clear out all loaded stuff from this space
 */
void ClientChunkSpace::clear()
{
	BW_GUARD;
	this->blurSpace();

	this->homeless_.clear();

	this->BaseChunkSpace::clear();
#if UMBRA_ENABLE
	if (umbraCell_)
	{
		umbraCell_->release();
		umbraCell_ = NULL;
	}
	if (umbraInsideCell_)
	{
		umbraInsideCell_->release();
		umbraInsideCell_ = NULL;
	}
#endif
}


extern std::string g_specialConsoleString;
//#define HULL_DEBUG(x) g_specialConsoleString += x
#define HULL_DEBUG(x) (void)0


uint64 g_CSCTimeLimit;

#ifndef MF_SERVER
// see also max scan path in chunk_manager.cpp
const int FOCUS_RANGE = int(3001.f / GRID_RESOLUTION);

//extern uint64 g_hullTreeAddTime, g_hullTreePlaneTime, g_hullTreeMarkTime;


/**
 *	This method sets the focus point for this space
 */
void ClientChunkSpace::focus( const Vector3 & point )
{
	BW_GUARD;
	static DogWatch dwFocus( "Focus Chunk" );
	dwFocus.start();

	// figure out the grid square this is in
	int	cx = int(point.x / GRID_RESOLUTION);		if (point.x < 0.f) cx--;
	int	cz = int(point.z / GRID_RESOLUTION);		if (point.z < 0.f) cz--;

	// tell it to the grid
	currentFocus_.origin( cx, cz );

	// also delete any columns that are stale
	for (int x = cx - ColumnGrid::SPANH; x <= cx + ColumnGrid::SPANH; x++)
	{
		for (int z = cz - ColumnGrid::SPANH; z <= cz + ColumnGrid::SPANH; z++)
		{
			Column * & rpCol = currentFocus_( x, z );
			if (rpCol != NULL && rpCol->isStale())
			{
				delete rpCol;
				rpCol = NULL;
			}
		}
	}

	int bs = blurred_.size();
	//uint64 timeTaken = timestamp();
	g_CSCTimeLimit = timestamp() + stampsPerSecond() * 2 / 1000;
	bool hitTimeLimit = false;

	//g_hullTreeAddTime = 0;
	//g_hullTreePlaneTime = 0;
	//g_hullTreeMarkTime = 0;

	// cache any added obstacles for later sorting
//	Column::cacheControl( true );

	// focus any chunks that are now in range
	for (uint i = 0; i < blurred_.size(); i++)
	{
		Chunk * pChunk = blurred_[i];

		const Vector3 & cen = pChunk->centre();
		int nx = int(cen.x / GRID_RESOLUTION);	if (cen.x < 0.f) nx--;
		int nz = int(cen.z / GRID_RESOLUTION);	if (cen.z < 0.f) nz--;
		nx -= cx;
		nz -= cz;

		if (-FOCUS_RANGE <= nx && nx <= FOCUS_RANGE &&
			-FOCUS_RANGE <= nz && nz <= FOCUS_RANGE)
		{
			// skip this one if we're outta time
			if (hitTimeLimit && (
				nx <= -(FOCUS_RANGE-1) || nx >= (FOCUS_RANGE-1) ||
				nz <= -(FOCUS_RANGE-1) || nz >= (FOCUS_RANGE-1))) continue;

			// this chunk is no longer blurred
			blurred_.erase( blurred_.begin() + i );
			i--;

			// see if this chunk is new to (nx,nz) and adjacent columns,
			// close them if it isn't
			for (int x = nx - 1; x <= nx + 1; x++) for (int z = nz - 1; z <= nz + 1; z++)
			{
				Column * pCol = currentFocus_( cx + x, cz + z );
				if (pCol != NULL) pCol->shutIfSeen( pChunk );
			}

			// do the actual focussing work
			pChunk->focus();
			// it's ok for a chunk to re-add itself on failure to focus,
			// 'coz it'll go to the end of our vector (not that chunks
			// currently ever fail to focus)

			// open all the columns, and mark them as having seen this chunk
			for (int x = nx - 1; x <= nx + 1; x++) for (int z = nz - 1; z <= nz + 1; z++)
			{
				Column * pCol = currentFocus_( cx + x, cz + z );
				if (pCol != NULL) pCol->openAndSee( pChunk );
			}

			// get out now if we're an edge chunk
			if (timestamp() > g_CSCTimeLimit) hitTimeLimit = true;
			// time taken to cache data should be somewhat proportional to
			// time taken to construct hull tree.
#ifdef EDITOR_ENABLED
			hitTimeLimit = false;
#endif//
		}

	}

	// let every column know if it's soft (this sucks I know!)
	for (int x = cx - ColumnGrid::SPANH; x <= cx + ColumnGrid::SPANH; x++)
	{
		for (int z = cz - ColumnGrid::SPANH; z <= cz + ColumnGrid::SPANH; z++)
		{
			Column * pCol = currentFocus_( x, z );
			if (pCol != NULL) pCol->soft(
				x <= cx-(FOCUS_RANGE-1) || x >= cx+(FOCUS_RANGE-1) ||
				z <= cz-(FOCUS_RANGE-1) || z >= cz+(FOCUS_RANGE-1) );
		}
	}

	// process column caches
//	Column::cacheControl( false );

	dwFocus.stop();

	// if we focussed any chunks then see if any homeless items
	// would prefer to live in them now instead
	if (bs != blurred_.size())
	{
		for (int i = int(homeless_.size()-1); i >= 0; i--)
		{
			i = std::min( i, int(homeless_.size()-1) );
			ChunkItemPtr pHomelessItem = homeless_[i];
			pHomelessItem->nest( static_cast<ChunkSpace*>( this ) );
		}
	}


	/*
	if (bs != blurred_.size())
	{
		g_focusGridSize = 0;
		for (int z = cz - ColumnGrid::SPANH; z <= cz + ColumnGrid::SPANH; z++)
		{
			for (int x = cx - ColumnGrid::SPANH; x <= cx + ColumnGrid::SPANH; x++)
			{
				Column * pCol = currentFocus_( x, z );
				if (pCol != NULL) g_focusGridSize += pCol->size();
			}
		}
	}
	*/
	/*
	static std::string	htsize;
	if (bs != blurred_.size())
	{
		htsize = "";

		int totsz = 0;
		for (int z = cz - ColumnGrid::SPANH; z <= cz + ColumnGrid::SPANH; z++)
		{
			for (int x = cx - ColumnGrid::SPANH; x <= cx + ColumnGrid::SPANH; x++)
			{
				Column * pCol = currentFocus_( x, z );
				if (pCol != NULL) totsz += pCol->size();

				if (pCol == NULL || pCol->pOutsideChunk() == NULL)
				{
					htsize += " None";
				}
				else
				{
					char abuf[16];
					bw_snprintf( abuf, sizeof(abuf), "%5.0f", pCol->pOutsideChunk()->pathSum() );
					htsize += abuf;
				}

			}
			htsize += "\n";
		}
		g_hullTreeSize = totsz;
	}
	g_specialConsoleString += htsize;
	*/
	/*
	if (bs != blurred_.size())
	{
		timeTaken = timestamp() - timeTaken;

		char sbuf[256];
		std::strstream ss( sbuf, sizeof(sbuf) );
		ss << "ChunkSpace focussed " << bs - blurred_.size();
		ss << ": " << NiceTime(timeTaken);
		ss << " tree add " << NiceTime( g_hullTreeAddTime );
		ss << " plane add " << NiceTime( g_hullTreePlaneTime );
		ss << " mark time " << NiceTime( g_hullTreeMarkTime );
		ss << std::ends;
		dprintf( "%s\n", ss.str() );
	}
	*/
}

#if UMBRA_ENABLE
Umbra::Cell* ClientChunkSpace::umbraCell()
{
	if (umbraCell_ == NULL)
		umbraCell_ = Umbra::Cell::create();
	return umbraCell_;
}

Umbra::Cell* ClientChunkSpace::umbraInsideCell()
{
	if (umbraInsideCell_ == NULL)
		umbraInsideCell_ = Umbra::Cell::create();
	return umbraInsideCell_;
}
#endif

/**
 *	Everyone's favourite function - tick!
 */
void ClientChunkSpace::tick( float dTime )
{
	BW_GUARD;
	// start the embargo on chunk item changes
	ticking_ = true;

	// first update our lighting
	this->updateHeavenlyLighting();

	// now update all our chunks
	for (ChunkMap::iterator it = currentChunks_.begin();
		it != currentChunks_.end();
		it++)
	{
		for (uint i = 0; i < it->second.size(); i++)
		{
			if (!it->second[i]->online()) continue;

			it->second[i]->tick( dTime );
		}
	}

	static DogWatch dwHomeless( "Homeless" );
	dwHomeless.start();
	// and any homeless items
	for (uint i = 0; i < homeless_.size(); i++)
	{		// using indices in case homeless_ changes under us
		homeless_[i]->tick( dTime );
	}
	dwHomeless.stop();

	// ok, chunk items can move around again now
	ticking_ = false;
}

/**
 *	This method is called by our tick, and occasionally by the world editor.
 */
void ClientChunkSpace::updateHeavenlyLighting()
{
	BW_GUARD;
	if (pOutLight_ != NULL && sunLight_)
	{
    	Vector3 localDir( 0, 0, -1 );
		sunLight_->direction( localDir );
		sunLight_->colour( Moo::Colour( pOutLight_->sunColour ) );

		//the dawn/dusk sneaky swap changes moonlight for
		//sunlight when the moon is brighter
		sunLight_->worldTransform( pOutLight_->mainLightTransform() );		

		ambientLight_ = Moo::Colour( pOutLight_->ambientColour );
		lights_->ambientColour( ambientLight_ );
	}
}
#endif // MF_SERVER


/**
 *	This method adds a homeless chunk item
 */
void ClientChunkSpace::addHomelessItem( ChunkItemPtr pItem )
{
	BW_GUARD;
	homeless_.push_back( pItem );
}

/**
 *	This method removes a homeless chunk item
 */
void ClientChunkSpace::delHomelessItem( ChunkItemPtr pItem )
{
	BW_GUARD;
	std::vector<ChunkItemPtr>::iterator found =
		std::find( homeless_.begin(), homeless_.end(), pItem );
	if (found != homeless_.end()) homeless_.erase( found );
}

/**
 *	This method sets the colour of the ambient light from the heavens
 */
void ClientChunkSpace::ambientLight( Moo::Colour col )
{
	BW_GUARD;
	ambientLight_ = col;
	lights_->ambientColour( ambientLight_ );
}


/**
 *	This method transforms the given point into the coordinate system of
 *	common space, where all points are as equals and thus comparable.
 */
void ClientChunkSpace::transformSpaceToCommon( Vector3 & pos, Vector3 & dir )
{
	BW_GUARD;
	if (&*ChunkManager::instance().cameraSpace() == this) return;

	Matrix m;
	m.setRotate( dir[0], dir[1], dir[2] );
	m.translation( pos );
	m.postMultiply( common_ );
	pos = m.applyToOrigin();
	dir[0] = m.yaw();
	dir[1] = m.pitch();
	dir[2] = m.roll();
}

/**
 *	This method transforms the given point from the coordinate system of
 *	common space, into our own coordinate system.
 */
void ClientChunkSpace::transformCommonToSpace( Vector3 & pos, Vector3 & dir )
{
	BW_GUARD;
	if (&*ChunkManager::instance().cameraSpace() == this) return;

	Matrix m;
	m.setRotate( dir[0], dir[1], dir[2] );
	m.translation( pos );
	m.postMultiply( commonInverse_ );
	pos = m.applyToOrigin();
	dir[0] = m.yaw();
	dir[1] = m.pitch();
	dir[2] = m.roll();
}

bool ClientChunkSpace::isMapped() const
{
	return this->isMapped_;
}

// ----------------------------------------------------------------------------
// Section: ClientChunkSpace::Column
// ----------------------------------------------------------------------------

#include "physics2/hulltree.hpp"
#include "physics2/worldtri.hpp"

/**
 *	Column constructor
 */
ClientChunkSpace::Column::Column( int x, int z ) :
	BaseChunkSpace::Column( x, z ),
	soft_( false )
{
}


/**
 *	Column destructor
 */
ClientChunkSpace::Column::~Column()
{
}



// ----------------------------------------------------------------------------
// Section: FocusGrid
// ----------------------------------------------------------------------------


/**
 *	FocusGrid constructor
 */
template <class T, int ISPAN>
FocusGrid<T,ISPAN>::FocusGrid() :
	cx_( 0 ),
	cz_( 0 )
{
	BW_GUARD;
	for (int z = 0; z < SPANX; z++) for (int x = 0; x < SPANX; x++)
	{
		grid_[z][x] = NULL;
	}
}


/**
 *	FocusGrid destructor
 */
template <class T, int ISPAN>
FocusGrid<T,ISPAN>::~FocusGrid()
{
	BW_GUARD;
	for (int z = 0; z < SPANX; z++) for (int x = 0; x < SPANX; x++)
	{
		delete grid_[z][x];
	}
}


/**
 *	This method sets the origin of the grid. The grid cell at
 *	the given location becomes the new centre of the grid.
 *	Cells are accessible for +/- SPAN/2, inclusive, around
 *	the cell at (x,y). (Since SPAN is odd, SPAN/2 always
 *	rounded down)
 */
template <class T, int ISPAN>
void FocusGrid<T,ISPAN>::origin( int cx, int cz )
{
	BW_GUARD;
	// note: could make this condition: (cz_>>1) < (z>>1), because there's
	// always one row unused, but I won't for now (stability, sanity)

	// move z to the right position
	while (cz_ < cz)
	{
		for (int x = 0; x < SPANX; x++) this->eraseEntry( x, cz_ - SPANH );
		cz_++;
	}
	while (cz_ > cz)
	{
		for (int x = 0; x < SPANX; x++) this->eraseEntry( x, cz_ + SPANH );
		cz_--;
	}

	// move x to the right position
	while (cx_ < cx)
	{
		for (int z = 0; z < SPANX; z++) this->eraseEntry( cx_ - SPANH, z );
		cx_++;
	}
	while (cx_ > cx)
	{
		for (int z = 0; z < SPANX; z++) this->eraseEntry( cx_ + SPANH, z );
		cx_--;
	}
}


/**
 *	This private method erases the indicated entry
 */
template <class T, int ISPAN>
void FocusGrid<T,ISPAN>::eraseEntry( int x, int z )
{
	BW_GUARD;
	T * & pT = this->operator()( x, z );
	if (pT != NULL)
	{
		delete pT;
		pT = NULL;
	}
}

// client_chunk_space.cpp
