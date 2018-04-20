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
#include "worldeditor/world/static_lighting.hpp"
#include "worldeditor/world/world_manager.hpp"


using namespace StaticLighting;


// -----------------------------------------------------------------------------
// Section: markChunk
// -----------------------------------------------------------------------------

void StaticLighting::markChunk( Chunk* chunk )
{
	WorldManager::instance().dirtyLighting( chunk );
}

// -----------------------------------------------------------------------------
// Section: findLightsInfluencing
// -----------------------------------------------------------------------------

bool StaticLighting::findLightsInfluencing( Chunk* forChunk, Chunk* inChunk,
						   StaticLighting::StaticLightContainer* lights,
						   std::set<Chunk*>& searchedChunks,
						   int currentDepth)
{
	// Add all the lights in inChunk
	StaticLightContainer* currentLights = StaticChunkLightCache::instance( *inChunk ).lights();

	if (forChunk == inChunk)
	{
		// Adding our own lights, just add em all
		lights->addLights( currentLights );
	}
	else
	{
		// Adding someone elses lights, check they can reach forChunk first
		lights->addLights( currentLights, forChunk->boundingBox() );
	}

	// Mark that we've now done inChunk
	searchedChunks.insert( inChunk );

	// If we're up to our max portal traversal count, don't search through the
	// connected portals
	if (currentDepth == STATIC_LIGHT_PORTAL_DEPTH)
		return true;

	// Call for each of our connected chunks that havn't yet been searched
	for (Chunk::piterator pit = inChunk->pbegin(); pit != inChunk->pend(); pit++)
	{
		if (!pit->hasChunk())
			continue;

		if (!pit->pChunk->online())
			return false;

		// We've already marked it, skip
		if (searchedChunks.find( pit->pChunk ) != searchedChunks.end())
			continue;

		if (!findLightsInfluencing( forChunk, pit->pChunk, lights, searchedChunks, currentDepth + 1 ))
			return false;
	}

	return true;
}


// -----------------------------------------------------------------------------
// Section: StaticLightContainer
// -----------------------------------------------------------------------------

StaticLightContainer::StaticLightContainer() : ambient_( D3DCOLOR( 0x00000000 ) )
{
}

void StaticLightContainer::addLights( StaticLightContainer* from )
{
	directionalLights_.insert( directionalLights_.end(),
		from->directionalLights_.begin(), from->directionalLights_.end() );

	omniLights_.insert( omniLights_.end(),
		from->omniLights_.begin(), from->omniLights_.end() );

	spotLights_.insert( spotLights_.end(),
		from->spotLights_.begin(), from->spotLights_.end() );
}



/**
 * Functor to add a light if it intersects the boundingbox
 */
template<class LightType>
class LightInserter : public std::unary_function<LightType, void>
{
private:
	const BoundingBox& bb_;
	std::vector<LightType>& lights_;
public:
	LightInserter( std::vector<LightType>& lights, const BoundingBox& bb )
		: lights_( lights ), bb_( bb )
	{}

    void operator() (LightType l)
	{
		if (l->intersects( bb_ ))
			lights_.push_back( l );
	}
};


void StaticLightContainer::addLights( StaticLightContainer* from, const BoundingBox& bb )
{
	directionalLights_.insert( directionalLights_.end(),
		from->directionalLights_.begin(), from->directionalLights_.end() );

    LightInserter<Moo::OmniLightPtr> omniInserter( omniLights_, bb );
	std::for_each( from->omniLights_.begin(), from->omniLights_.end(), omniInserter);

    LightInserter<Moo::SpotLightPtr> spotInserter( spotLights_, bb );
	std::for_each( from->spotLights_.begin(), from->spotLights_.end(), spotInserter);
}

void StaticLightContainer::removeLight( Moo::DirectionalLightPtr pDirectional )
{
	DirectionalLightVector::iterator i = std::find( directionalLights_.begin(),
		directionalLights_.end(),
		pDirectional );

	if (i != directionalLights_.end())
		directionalLights_.erase( i );
}

void StaticLightContainer::removeLight( Moo::OmniLightPtr pOmni )
{
	OmniLightVector::iterator i = std::find( omniLights_.begin(),
		omniLights_.end(),
		pOmni );

	if (i != omniLights_.end())
		omniLights_.erase( i );
}

void StaticLightContainer::removeLight( Moo::SpotLightPtr pSpot )
{
	SpotLightVector::iterator i = std::find( spotLights_.begin(),
		spotLights_.end(),
		pSpot );

	if (i != spotLights_.end())
		spotLights_.erase( i );
}

bool StaticLightContainer::empty()
{
	return (directionalLights_.empty() &&
		spotLights_.empty() &&
		omniLights_.empty() && 
		ambient_ == 0x00000000);
}






// -----------------------------------------------------------------------------
// Section: StaticChunkLightCache
// -----------------------------------------------------------------------------

StaticChunkLightCache::StaticChunkLightCache( Chunk & chunk ) : chunk_( chunk )
{
}

void StaticChunkLightCache::touch( Chunk & chunk )
{
	StaticChunkLightCache::instance( chunk );
}

/** Functor to mark chunks as dirty */
template<class LightType>
class ChunkMarker : public std::unary_function<LightType, void>
{
private:
	Chunk* chunk_;
public:
	ChunkMarker( Chunk* chunk ) : chunk_( chunk ) {}

	void operator() (LightType l)
	{
		markChunks( chunk_, l );
	}
};

void StaticChunkLightCache::markInfluencedChunksDirty()
{
	markChunk( &chunk_ );

    ChunkMarker<Moo::OmniLightPtr> omniMarker( &chunk_ );
	std::for_each( lights()->omnis().begin(), lights()->omnis().end(), omniMarker);

    ChunkMarker<Moo::SpotLightPtr> spotMarker( &chunk_ );
	std::for_each( lights()->spots().begin(), lights()->spots().end(), spotMarker);
}


/// Static instance accessor initialiser
ChunkCache::Instance<StaticChunkLightCache> StaticChunkLightCache::instance;
