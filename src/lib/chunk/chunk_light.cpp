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

#include "chunk_light.hpp"

#include "resmgr/datasection.hpp"
#include "resmgr/bwresource.hpp"
#include "moo/render_context.hpp"
#include "moo/animation_manager.hpp"
#include "moo/node.hpp"
#include "moo/animation.hpp"

#include "chunk.hpp"

#ifdef EDITOR_ENABLED
#include "appmgr/options.hpp"
#endif

DECLARE_DEBUG_COMPONENT2( "Chunk", 1 );

PROFILER_DECLARE( ChunkPulseLight_tick, "ChunkPulseLight Tick" );

int ChunkLight_token;




// ----------------------------------------------------------------------------
// Section: ChunkLight
// ----------------------------------------------------------------------------

/**
 *	Add ourselves to or remove ourselves from the given chunk
 */
void ChunkLight::toss( Chunk * pChunk )
{
	BW_GUARD;
	if (pChunk_ != NULL)
	{
		ChunkLightCache & clc = ChunkLightCache::instance( *pChunk_ );
		this->delFromContainer( clc.pOwnLights() );
		this->delFromContainer( clc.pOwnSpecularLights() );
		clc.dirtySeep();
	}

	this->ChunkItem::toss( pChunk );

	if (pChunk_ != NULL)
	{
		ChunkLightCache & clc = ChunkLightCache::instance( *pChunk_ );

		updateLight( pChunk_->transform() );

		addToCache( clc );

		clc.dirtySeep();
	}
}

// ----------------------------------------------------------------------------
// Section: ChunkMooLight
// ----------------------------------------------------------------------------

ChunkMooLight::ChunkMooLight( WantFlags wantFlags ) :
	ChunkLight( wantFlags ),
	dynamicLight_( true ),
	specularLight_( true )
{
}

void ChunkMooLight::addToCache( ChunkLightCache& cache ) const
{
	BW_GUARD;
	if (dynamicLight_)
		addToContainer( cache.pOwnLights() );

	if (specularLight_)
		addToContainer( cache.pOwnSpecularLights() );
}

void ChunkMooLight::dynamicLight( const bool dyn )
{
	BW_GUARD;
	if (dyn != dynamicLight_)
	{
		dynamicLight_ = dyn;

		if (pChunk_)
		{
			ChunkLightCache & clc = ChunkLightCache::instance( *pChunk_ );

			if (dyn)
				this->addToContainer( clc.pOwnLights() );
			else
				this->delFromContainer( clc.pOwnLights() );

			clc.dirtySeep();
		}
	}
}

void ChunkMooLight::specularLight( const bool spec )
{
	BW_GUARD;
	if (spec != specularLight_)
	{
		specularLight_ = spec;

		if (pChunk_)
		{
			ChunkLightCache & clc = ChunkLightCache::instance( *pChunk_ );

			if (spec)
				this->addToContainer( clc.pOwnSpecularLights() );
			else
				this->delFromContainer( clc.pOwnSpecularLights() );

			clc.dirtySeep();
		}
	}
}
// ----------------------------------------------------------------------------
// Section: ChunkDirectionalLight
// ----------------------------------------------------------------------------

IMPLEMENT_CHUNK_ITEM( ChunkDirectionalLight, directionalLight, 0 )

/**
 *	Constructor
 */
ChunkDirectionalLight::ChunkDirectionalLight() :
	pLight_( new Moo::DirectionalLight(
		Moo::Colour( 0, 0, 0, 1 ), Vector3( 0, -1, 0 ) ) )
{
}


/**
 *	Destructor
 */
ChunkDirectionalLight::~ChunkDirectionalLight()
{
	// needn't clear pointer 'coz when it goes out of scope its ref
	// count will decrease for us (disposing us if necessary)
}


void ChunkDirectionalLight::updateLight( const Matrix& world ) const
{
	pLight_->worldTransform( world );
}


/**
 *	This method adds this light to the input container
 */
void ChunkDirectionalLight::addToContainer( Moo::LightContainerPtr pLC ) const
{
	pLC->addDirectional( pLight_ );
}

/**
 *	This method removes this light from the input container
 */
void ChunkDirectionalLight::delFromContainer( Moo::LightContainerPtr pLC ) const
{
	BW_GUARD;
	DirectionalLightVector & lv = pLC->directionals();
	for (uint i = 0; i < lv.size(); i++)
	{
		if (lv[i] == pLight_)
		{
			lv.erase( lv.begin() + i );
			break;
		}
	}
}


/**
 *	This method loads the light from the section.
 */
bool ChunkDirectionalLight::load( DataSectionPtr pSection )
{
	BW_GUARD;	
#ifdef EDITOR_ENABLED
	Vector3 colour = pSection->readVector3( "colour" ) / 255.f;
	pLight_->colour( Moo::Colour( colour[0], colour[1], colour[2], 1 ) );
#else//EDITOR_ENABLED
	Vector3 colour = pSection->readVector3( "colour" ) / 255.f;
	float multiplier = pSection->readFloat( "multiplier" );
	pLight_->colour( Moo::Colour( colour[0], colour[1], colour[2], 1 ) * multiplier );
#endif//EDITOR_ENABLED


    Vector3 direction( pSection->readVector3( "direction" ) );
	pLight_->direction( direction );

	dynamicLight_ = pSection->readBool( "dynamic", true );
	specularLight_ = pSection->readBool( "specular", true );

	return true;
}


// ----------------------------------------------------------------------------
// Section: ChunkOmniLight
// ----------------------------------------------------------------------------

IMPLEMENT_CHUNK_ITEM( ChunkOmniLight, omniLight, 0 )

/**
 *	Constructor
 */
ChunkOmniLight::ChunkOmniLight( WantFlags wantFlags ) :
	ChunkMooLight( wantFlags ),
	pLight_( new Moo::OmniLight(
		Moo::Colour( 0, 0, 0, 1 ), Vector3( 0, 0, 0 ), 0, 0 ) )
{
}


/**
 *	Destructor
 */
ChunkOmniLight::~ChunkOmniLight()
{
}


void ChunkOmniLight::updateLight( const Matrix& world ) const
{
	BW_GUARD;
	pLight_->worldTransform( world );
}

/**
 *	This method adds this light to the input container
 */
void ChunkOmniLight::addToContainer( Moo::LightContainerPtr pLC ) const
{
	BW_GUARD;
	pLC->addOmni( pLight_ );
}

/**
 *	This method removes this light from the input container
 */
void ChunkOmniLight::delFromContainer( Moo::LightContainerPtr pLC ) const
{
	BW_GUARD;
	OmniLightVector & lv = pLC->omnis();
	for (uint i = 0; i < lv.size(); i++)
	{
		if (lv[i] == pLight_)
		{
			lv.erase( lv.begin() + i );
			break;
		}
	}
}


/**
 *	This method loads the light from the section
 */
bool ChunkOmniLight::load( DataSectionPtr pSection )
{
	BW_GUARD;	
#ifdef EDITOR_ENABLED
	Vector3 colour = pSection->readVector3( "colour" ) / 255.f;
	pLight_->colour( Moo::Colour( colour[0], colour[1], colour[2], 1 ) );
#else//EDITOR_ENABLED
	Vector3 colour = pSection->readVector3( "colour" ) / 255.f;
	float multiplier = pSection->readFloat( "multiplier" );
	pLight_->colour( Moo::Colour( colour[0], colour[1], colour[2], 1 ) * multiplier );
#endif//EDITOR_ENABLED
	pLight_->position( Vector3( pSection->readVector3( "position" ) ) );
	pLight_->innerRadius( pSection->readFloat( "innerRadius" ) );
	pLight_->outerRadius( pSection->readFloat( "outerRadius" ) );

	dynamicLight_ = pSection->readBool( "dynamic", true );
	specularLight_ = pSection->readBool( "specular", true );

	return true;
}


// ----------------------------------------------------------------------------
// Section: ChunkSpotLight
// ----------------------------------------------------------------------------

IMPLEMENT_CHUNK_ITEM( ChunkSpotLight, spotLight, 0 )

/**
 *	Constructor
 */
ChunkSpotLight::ChunkSpotLight() :
	pLight_( new Moo::SpotLight(
		Moo::Colour( 0, 0, 0, 1 ),
		Vector3( 0, 0, 0 ),
		Vector3( 0, -1, 0 ),
		0, 0, 0 ) )
{
}


/**
 *	Destructor
 */
ChunkSpotLight::~ChunkSpotLight()
{
}


void ChunkSpotLight::updateLight( const Matrix& world ) const
{
	BW_GUARD;
	pLight_->worldTransform( world );
}

/**
 *	This method adds this light to the input container
 */
void ChunkSpotLight::addToContainer( Moo::LightContainerPtr pLC ) const
{
	BW_GUARD;
	pLC->addSpot( pLight_ );
}

/**
 *	This method removes this light from the input container
 */
void ChunkSpotLight::delFromContainer( Moo::LightContainerPtr pLC ) const
{
	BW_GUARD;
	SpotLightVector & lv = pLC->spots();
	for (uint i = 0; i < lv.size(); i++)
	{
		if (lv[i] == pLight_)
		{
			lv.erase( lv.begin() + i );
			break;
		}
	}
}


/**
 *	This method loads the light from the section
 */
bool ChunkSpotLight::load( DataSectionPtr pSection )
{
	BW_GUARD;	
#ifdef EDITOR_ENABLED
	Vector3 colour = pSection->readVector3( "colour" ) / 255.f;
	pLight_->colour( Moo::Colour( colour[0], colour[1], colour[2], 1 ) );
#else//EDITOR_ENABLED
	Vector3 colour = pSection->readVector3( "colour" ) / 255.f;
	float multiplier = pSection->readFloat( "multiplier" );
	pLight_->colour( Moo::Colour( colour[0], colour[1], colour[2], 1 ) * multiplier );
#endif//EDITOR_ENABLED

	pLight_->position( Vector3( pSection->readVector3( "position" ) ) );
	pLight_->direction( Vector3( pSection->readVector3( "direction" ) ) );
	pLight_->innerRadius( pSection->readFloat( "innerRadius" ) );
	pLight_->outerRadius( pSection->readFloat( "outerRadius" ) );
	pLight_->cosConeAngle( pSection->readFloat( "cosConeAngle" ) );

	dynamicLight_ = pSection->readBool( "dynamic", true );
	specularLight_ = pSection->readBool( "specular", true );

	return true;
}


// ----------------------------------------------------------------------------
// Section: ChunkPulseLight
// ----------------------------------------------------------------------------

IMPLEMENT_CHUNK_ITEM( ChunkPulseLight, pulseLight, 0 )

/**
 *	Constructor
 */
ChunkPulseLight::ChunkPulseLight( WantFlags wantFlags ) :
	ChunkMooLight( wantFlags ),
	pLight_( new Moo::OmniLight(
		Moo::Colour( 0, 0, 0, 1 ), Vector3( 0, 0, 0 ), 0, 0 ) ),
	positionAnimFrame_( 0 ),
	colourAnimFrame_( 0 ),
	position_(0,0,0),
	animPosition_(0,0,0),
	colour_( 0, 0, 0, 1 )
{
	BW_GUARD;
	pLight_->dynamic( true );
	this->specularLight( true );
	this->dynamicLight( true );

	Moo::NodePtr pNode = new Moo::Node;

	pAnimation_ = Moo::AnimationManager::instance().get( "system/animation/lightnoise.animation", pNode );
	DataSectionPtr pSection = BWResource::openSection( "system/data/pulse_light.xml" );
	
	if (pSection)
	{
		float timeScale = pSection->readFloat( "timeScale", 1.f );
		float duration = pSection->readFloat( "duration", 0 );
		std::vector< Vector2 > animFrames;
		pSection->readVector2s( "frame", animFrames );
		for (uint32 i = 0; i <animFrames.size(); i ++)
		{
			colourAnimation_.addKey( animFrames[i].x * timeScale, animFrames[i].y );
		}
		if (colourAnimation_.getTotalTime() != 0.f)
		{
			colourAnimation_.loop( true, duration > 0.f ? duration * timeScale : colourAnimation_.getTotalTime() );
		}
	}
	if (colourAnimation_.getTotalTime() == 0.f)
	{
		colourAnimation_.addKey( 0.f, 1.f );
		colourAnimation_.addKey( 1.f, 1.f );
	}
}


/**
 *	Destructor
 */
ChunkPulseLight::~ChunkPulseLight()
{
}


void ChunkPulseLight::updateLight( const Matrix& world ) const
{
	BW_GUARD;
	pLight_->worldTransform( world );
}


void ChunkPulseLight::tick( float dTime )
{
	BW_GUARD_PROFILER( ChunkPulseLight_tick );

	if (pAnimation_)
	{
		positionAnimFrame_ += dTime * 30.f;
		if (positionAnimFrame_ >= pAnimation_->totalTime())
		{
			positionAnimFrame_ = fmodf(positionAnimFrame_, pAnimation_->totalTime());
		}
		Matrix m;
		Matrix res = Matrix::identity;

		for (uint32 i = 0; i < pAnimation_->nChannelBinders(); i++)
		{
			Moo::AnimationChannelPtr pChannel = pAnimation_->channelBinder(i).channel();
			if (pChannel)
			{
				pChannel->result( positionAnimFrame_, m );
				res.preMultiply( m );
			}
		}
		animPosition_ = res.applyToOrigin();
	}

	colourAnimFrame_ += dTime;
	colourAnimFrame_ = fmodf(colourAnimFrame_, colourAnimation_.getTotalTime());
	float colourMod = colourAnimation_.animate( colourAnimFrame_ );

	pLight_->colour( Moo::Colour( colour_.r * colourMod, colour_.g * colourMod, 
		colour_.b * colourMod, 1.f ) );
	pLight_->position( position_ + animPosition_ );
	pLight_->worldTransform( (pChunk_ != NULL) ?
		pChunk_->transform() : Matrix::identity );
}

/**
 *	This method adds this light to the input container
 */
void ChunkPulseLight::addToContainer( Moo::LightContainerPtr pLC ) const
{
	BW_GUARD;
	pLC->addOmni( pLight_ );
}

/**
 *	This method removes this light from the input container
 */
void ChunkPulseLight::delFromContainer( Moo::LightContainerPtr pLC ) const
{
	BW_GUARD;
	OmniLightVector & lv = pLC->omnis();
	for (uint i = 0; i < lv.size(); i++)
	{
		if (lv[i] == pLight_)
		{
			lv.erase( lv.begin() + i );
			break;
		}
	}
}


/**
 *	This method loads the light from the section
 */
bool ChunkPulseLight::load( DataSectionPtr pSection )
{
	BW_GUARD;	
#ifdef EDITOR_ENABLED
	Vector3 colour = pSection->readVector3( "colour" ) / 255.f;
	colour_ = Moo::Colour( colour[0], colour[1], colour[2], 1 );
#else//EDITOR_ENABLED
	Vector3 colour = pSection->readVector3( "colour" ) / 255.f;
	float multiplier = pSection->readFloat( "multiplier" );
	colour_ = Moo::Colour( colour[0], colour[1], colour[2], 1 ) * multiplier;
#endif//EDITOR_ENABLED
	position_ = Vector3( pSection->readVector3( "position" ) );
	pLight_->innerRadius( pSection->readFloat( "innerRadius" ) );
	pLight_->outerRadius( pSection->readFloat( "outerRadius" ) );

	return true;
}

// ----------------------------------------------------------------------------
// Section: ChunkAmbientLight
// ----------------------------------------------------------------------------

IMPLEMENT_CHUNK_ITEM( ChunkAmbientLight, ambientLight, 0 )

/**
 *	Constructor
 */
ChunkAmbientLight::ChunkAmbientLight() :
	colour_( 0, 0, 0, 1 )
{
}


/**
 *	Destructor
 */
ChunkAmbientLight::~ChunkAmbientLight()
{
}

void ChunkAmbientLight::addToCache( ChunkLightCache& cache ) const
{
	BW_GUARD;
	addToContainer( cache.pOwnLights() );
}

/**
 *	This method adds this light to the input container
 */
void ChunkAmbientLight::addToContainer( Moo::LightContainerPtr pLC ) const
{
	BW_GUARD;
	pLC->ambientColour( colour_ * multiplier() );
}

/**
 *	This method removes this light from the input container
 */
void ChunkAmbientLight::delFromContainer( Moo::LightContainerPtr pLC ) const
{
	BW_GUARD;
	pLC->ambientColour( Moo::Colour( 0.f, 0.f, 0.f, 1.f ) );
}


/**
 *	This method loads the light from the section
 */
bool ChunkAmbientLight::load( DataSectionPtr pSection )
{
	BW_GUARD;	
#ifdef EDITOR_ENABLED
	Vector3 colour = pSection->readVector3( "colour" ) / 255.f;
	colour_ = Moo::Colour( colour[0], colour[1], colour[2], 1 );
#else//EDITOR_ENABLED
	Vector3 colour = pSection->readVector3( "colour" ) / 255.f;
	float multiplier = pSection->readFloat( "multiplier", 1.f );
	colour = colour * multiplier;
	colour_ = Moo::Colour( colour[0], colour[1], colour[2], 1 );
#endif//EDITOR_ENABLED

	return true;
}


// ----------------------------------------------------------------------------
// Section: ChunkLightCache
// ----------------------------------------------------------------------------

// TODO: Make it so chunks know about their own space...
#include "chunk_manager.hpp"
#include "chunk_space.hpp"


/**
 *	Constructor
 */
ChunkLightCache::ChunkLightCache( Chunk & chunk ) :
	chunk_( chunk ),
	pOwnLights_( NULL ),
	pAllLights_( NULL ),
	lightContainerDirty_( true ),
	heavenSeen_( false )
{
	BW_GUARD;
	pOwnLights_ = new Moo::LightContainer();
	pOwnLights_->ambientColour( Moo::Colour( 0, 0, 0, 1 ) );

	pOwnSpecularLights_ = new Moo::LightContainer();
}

/**
 *	Destructor
 */
ChunkLightCache::~ChunkLightCache()
{
	// smart pointers will release their references for us
}


/**
 *	Draw method. We refresh ourselves if necessary, and
 *	then load ourselves into the Moo render context.
 */
void ChunkLightCache::draw()
{
	BW_GUARD;
	static DogWatch drawWatch( "ChunkLightCache" );
	ScopedDogWatch watcher( drawWatch );

	// first of all collect all lights
	if (lightContainerDirty_)
	{
		this->collectLights();
		this->collectSpecularLights();
		lightContainerDirty_ = false;
	}

	// update the ambient colour if it's changed
	if (heavenSeen_)
	{
		pAllSpecularLights_->ambientColour( chunk_.space()->ambientLight() );
		pAllLights_->ambientColour( chunk_.space()->ambientLight() );
	}

	// tell Moo about them
#ifndef EDITOR_ENABLED
	Moo::rc().lightContainer( pAllLights_ );
#else

	static int renderLighting = 0;
	static uint32 s_settingsMark_ = -16;
	if (Moo::rc().frameTimestamp() != s_settingsMark_)
	{
		renderLighting = Options::getOptionInt( "render/lighting", 0 );
		s_settingsMark_ = Moo::rc().frameTimestamp();
	}

	if (renderLighting == 2)
		Moo::rc().lightContainer( pAllSpecularLights_ );
	else
		Moo::rc().lightContainer( pAllLights_ );

#endif	// EDITOR_ENABLED

	Moo::rc().specularLightContainer( pAllSpecularLights_ );
}


/**
 *	Bind method. We flag our cache as dirty, because we
 *	have to pick up lights from adjoining chunks
 */
void ChunkLightCache::bind( bool looseNotBind )
{
	BW_GUARD;
	dirtySeep();
}


/**
 *	This static function makes sure that a chunk light cache exists in
 *	the chunk that is about to be loaded, since we want to exist in
 *	every chunk so that their lighting is right.
 */
void ChunkLightCache::touch( Chunk & chunk )
{
	BW_GUARD;
	// this is all we have to do!
	ChunkLightCache::instance( chunk );
}


/**
 *	Flag this light container and the light container of all adjoining
 *	bound online chunks as dirty.
 */
void ChunkLightCache::dirtySeep()
{
	BW_GUARD;
	this->dirty();
	
	std::set<Chunk*> neighbours;
	this->collectNeighbouringChunks( neighbours );

	for (std::set<Chunk*>::iterator pit = neighbours.begin(); pit != neighbours.end(); pit++)
	{
		ChunkLightCache::instance(**pit).dirty();
	}	
}


/**
 *	Private method to flag this light container as dirty,
 *	so it'll collect all its lights again.
 */
void ChunkLightCache::dirty()
{
	lightContainerDirty_ = true;
}


/**
 *	Private method to collect lights that might
 *	seep through from adjoining chunks.
 */
void ChunkLightCache::collectLights()
{
	BW_GUARD;
	pAllLights_ = new Moo::LightContainer();
	pAllLights_->ambientColour( pOwnLights_->ambientColour() );
	BoundingBox lightBB = BoundingBox(
		chunk_.boundingBox().minBounds() - Vector3( GRID_RESOLUTION, 0.f, GRID_RESOLUTION ),
		chunk_.boundingBox().maxBounds() + Vector3( GRID_RESOLUTION, 0.f, GRID_RESOLUTION ) );
	pAllLights_->addToSelf( pOwnLights_, lightBB, false );

	heavenSeen_ = chunk_.canSeeHeaven();
	if (heavenSeen_)
	{
		pAllLights_->ambientColour( chunk_.space()->ambientLight() );
		pAllLights_->addDirectional( chunk_.space()->sunLight() );
	}

	std::set<Chunk*> neighbours;
	this->collectNeighbouringChunks( neighbours );

	for (std::set<Chunk*>::iterator it = neighbours.begin(); it != neighbours.end(); it++)
	{
		pAllLights_->addToSelf(
			ChunkLightCache::instance(**it).pOwnLights_,
			lightBB,
			false );
	}
}

void ChunkLightCache::collectSpecularLights()
{
	BW_GUARD;
	pAllSpecularLights_ = new Moo::LightContainer();
	pAllSpecularLights_->addToSelf( pOwnSpecularLights_, chunk_.boundingBox(), false );

	if (chunk_.canSeeHeaven())
	{
		pAllSpecularLights_->ambientColour( chunk_.space()->ambientLight() );
		pAllSpecularLights_->addDirectional( chunk_.space()->sunLight() );
	}

	std::set<Chunk*> neighbours;
	this->collectNeighbouringChunks( neighbours );

	for (std::set<Chunk*>::iterator it = neighbours.begin(); it != neighbours.end(); it++)
	{
		pAllSpecularLights_->addToSelf(
			ChunkLightCache::instance(**it).pOwnSpecularLights_,
			chunk_.boundingBox(),
			false );		
	}	
}


void ChunkLightCache::collectNeighbouringChunks( std::set<Chunk*>& ret )
{	
	BW_GUARD;
	//TODO : We should really do a bounding box intersection tests for
	//all chunks regardless of whether they are direct neighbours.

	for (Chunk::piterator pit = chunk_.pbegin(); pit != chunk_.pend(); pit++)
	{
		if (pit->hasChunk() && pit->pChunk->online())
		{
			Chunk* chunk = pit->pChunk;
			ret.insert( chunk );

			for (Chunk::piterator pit2 = chunk->pbegin(); pit2 != chunk->pend(); pit2++)
			{
				if (pit2->hasChunk() && pit2->pChunk->online())
				{
					ret.insert( pit2->pChunk );
				}
			}
		}
	}
}


/// Static instance accessor initialiser
ChunkCache::Instance<ChunkLightCache> ChunkLightCache::instance;

// chunk_light.cpp
