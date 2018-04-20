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

#include "chunk_nav_poly_set.hpp"
#include "chunk/chunk_space.hpp"

#include "cstdmf/concurrency.hpp"
#include "resmgr/bwresource.hpp"


DECLARE_DEBUG_COMPONENT2( "navPolySet", 0 )


int ChunkNavPolySet_token = 0;


// -----------------------------------------------------------------------------
// Section: ChunkNavPolySet in XML
// -----------------------------------------------------------------------------


/**
 *	Constructor
 */
ChunkNavPolySet::ChunkNavPolySet()
{
}


/**
 *	Destructor
 */
ChunkNavPolySet::~ChunkNavPolySet()
{
}


#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS ( pChunk, pSection, "navPoly", false )
IMPLEMENT_CHUNK_ITEM( ChunkNavPolySet, navPolySet, 0 )	// in local coords


// -----------------------------------------------------------------------------
// Section: ChunkNavPolySet as navmesh in cdata
// -----------------------------------------------------------------------------


typedef std::vector< ChunkWaypointSetData * > NavmeshPopulationRecord;
typedef std::map< std::string, NavmeshPopulationRecord > NavmeshPopulation;
static NavmeshPopulation s_navmeshPopulation;
static SimpleMutex s_navmeshPopulationLock;


void NavmeshPopulation_remove( const std::string& source )
{
	SimpleMutexHolder smh( s_navmeshPopulationLock );

	s_navmeshPopulation.erase( source );
}


//DependentArrayAlloc * g_DependentArrayAlloc;


template <class C> inline C consume( const char * & ptr )
{
	// gcc shoots itself in the foot with aliasing of this
	//return *((C*&)ptr)++;

	C * oldPtr = (C*)ptr;
	ptr += sizeof(C);
	return *oldPtr;
}


static const int navPolySetEltSize = 16;
static const int navPolyEltSize = 12;
static const int navPolyEdgeEltSize = 12;


/**
 *	This is a factory function to create ChunkNavPolySet items from a binary 
 *	navmesh.
 *
 *	@param pChunk		The chunk to load the ChunkNavPolySet for.
 *	@param pSection		The DataSection to load the ChunkNavPolySet from.
 *	@return				The result of the creation.
 */
ChunkItemFactory::Result ChunkNavPolySet::navmeshFactory( Chunk * pChunk, 
	DataSectionPtr pSection )
{
	//if (!g_DependentArrayAlloc)
	//	g_DependentArrayAlloc = new DependentArrayAlloc(65536);
	//static int sumNPSets = 0, sumNavPolyElts = 0, sumNavPolyEdges = 0;

	std::string resName = pSection->readString( "resource" );
	std::string fullName = pChunk->mapping()->path() + resName;

	// see if we've already loaded this navmesh
	s_navmeshPopulationLock.grab();
	NavmeshPopulation::iterator found = s_navmeshPopulation.find( fullName );
	if (found != s_navmeshPopulation.end() && found->second.back()->incRefTry())
	{
		const NavmeshPopulationRecord & poprec = found->second;

		// store the sets into a vector and add them into chunk
		// after release s_navmeshPopulationLock to avoid deadlock
		std::vector<ChunkNavPolySet*> sets;
		sets.reserve( poprec.size() );

		for (uint i = 0; i < poprec.size(); ++i)
		{
			ChunkNavPolySet * pSet = new ChunkNavPolySet();
			pSet->data_ = poprec[i];
			sets.push_back( pSet );
		}

		found->second.back()->decRef();
		// have to wait until all members inc'd before lock released
		// or else destructors of later members could erase map entry
		// (note that 'back' is destructed first so it gets the incRefTry)
		s_navmeshPopulationLock.give();

		for (std::vector<ChunkNavPolySet*>::iterator iter = sets.begin();
			iter != sets.end(); ++iter)
		{
			pChunk->addStaticItem( *iter );
		}

		return ChunkItemFactory::SucceededWithoutItem();
	}
	s_navmeshPopulationLock.give();

	// ok, no, time to load it then
	BinaryPtr pNavmesh = BWResource::instance().rootSection()->readBinary(
		pChunk->mapping()->path() + resName );
	if (!pNavmesh)
	{
		ERROR_MSG( "Could not read navmesh '%s'\n", resName.c_str() );
		return ChunkItemFactory::Result( NULL );
	}

	if (!pNavmesh->len())
	{
		// empty navmesh (not put in popln)
		return ChunkItemFactory::SucceededWithoutItem();
	}

	// note: assumes a single loading thread
	s_navmeshPopulationLock.grab();
	found = s_navmeshPopulation.insert( std::make_pair(
		fullName, NavmeshPopulationRecord() ) ).first;
	s_navmeshPopulationLock.give();

	const char * dataBeg = pNavmesh->cdata();
	const char * dataEnd = dataBeg + pNavmesh->len();
	const char * dataPtr = dataBeg;

	while (dataPtr < dataEnd)
	{
		int aVersion = consume<int>( dataPtr );
		float aGirth = consume<float>( dataPtr );
		int navPolyElts = consume<int>( dataPtr );
		int navPolyEdges = consume<int>( dataPtr );
		// navPolySetEltSize

		MF_ASSERT( aVersion == 0 );

		/*
		sumNPSets++;
		sumNavPolyElts += navPolyElts;
		sumNavPolyEdges += navPolyEdges;
		*/

		ChunkWaypointSetDataPtr cwsd = new ChunkWaypointSetData();
		cwsd->girth_ = aGirth;

		const char * edgePtr = dataPtr + navPolyElts * navPolyEltSize;
		cwsd->waypoints_.resize( navPolyElts );
		cwsd->edges_ = new ChunkWaypoint::Edge[ navPolyEdges ];
		ChunkWaypoint::Edge * nedge = cwsd->edges_;
		for (int p = 0; p < navPolyElts; p++)
		{
			ChunkWaypoint & wp = cwsd->waypoints_[p];

			wp.minHeight_ = consume<float>( dataPtr );
			wp.maxHeight_ = consume<float>( dataPtr );
			int vertexCount = consume<int>( dataPtr );
			// navPolyEltSize

			//dprintf( "poly %d:", p );
			//wp.edges_.resize( vertexCount );
			wp.edges_ = ChunkWaypoint::Edges( nedge, nedge+vertexCount );
			nedge += vertexCount;
			for (int e = 0; e < vertexCount; e++)
			{
				ChunkWaypoint::Edge & edge = wp.edges_[e];
				//ChunkWaypoint::Edge edge;
				edge.start_.x =	consume<float>( edgePtr );
				edge.start_.y = consume<float>( edgePtr );
				edge.neighbour_ = consume<int>( edgePtr );
					// 'adj' already encoded as we like it
				--navPolyEdges;
				// navPolyEdgeEltSize

				//dprintf( " %08X", edge.neighbour_ );
			}
			//dprintf( "\n" );

			wp.mark_ = wp.s_nextMark_ - 16;
		}

		MF_ASSERT( navPolyEdges == 0 );
		dataPtr = edgePtr;

		cwsd->source_ = found->first;
		found->second.push_back( &*cwsd );

		ChunkNavPolySet * pSet = new ChunkNavPolySet();
		pSet->data_ = cwsd;
		pChunk->addStaticItem( pSet );
	}

	return ChunkItemFactory::SucceededWithoutItem();
}

// link it in to the chunk item section map
ChunkItemFactory navmeshFactoryLink( "worldNavmesh", 0,
	&ChunkNavPolySet::navmeshFactory );

// chunk_nav_poly_set.cpp
