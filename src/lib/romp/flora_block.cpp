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
#include "flora_block.hpp"
#include "flora_constants.hpp"
#include "flora_renderer.hpp"
#include "ecotype.hpp"
#include "flora.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/avector.hpp"
#include "math/vector2.hpp"
#include "math/planeeq.hpp"

DECLARE_DEBUG_COMPONENT2( "romp", 0 )


static float s_ecotypeBlur = 1.2f;
void setEcotypeBlur( float amount )
{
	s_ecotypeBlur = amount;
	std::vector< Flora* > floras = Flora::floras();
	for( unsigned i=0; i<floras.size(); i++)
	{
		floras[i]->initialiseOffsetTable(amount);
		floras[i]->floraReset();
	}
}

float getEcotypeBlur()
{
	return s_ecotypeBlur;
}

static float s_positionBlur = 1.2f;
void setPositionBlur( float amount )
{
	s_positionBlur = amount;
	std::vector< Flora* > floras = Flora::floras();
	for( unsigned i=0; i<floras.size(); i++)
	{
		floras[i]->initialiseOffsetTable(amount);
		floras[i]->floraReset();
	}
}

float getPositionBlur()
{
	return s_positionBlur;
}


/**
 *	Constructor.
 */
FloraBlock::FloraBlock( Flora* flora ):
	bRefill_( true ),
	center_( 0.f, 0.f ),
	culled_( true ),
	blockID_( -1 ),
	flora_( flora )
{	
}


/**
 *	Thie method initialises the flora block.  FloraBlocks must be given
 *	a position.
 *
 *	@param	pos		Vector2 describing the center of the block.
 *	@param	offset	number of vertexes offset from start of vertex memory
 */
void FloraBlock::init( const Vector2& pos, uint32 offset )
{
	static bool s_added = false;
	if (!s_added)
	{		
		s_added = true;
		MF_WATCH( "Client Settings/Flora/Ecotype Blur",
			&::getEcotypeBlur,
			&::setEcotypeBlur,
			"Multiplier for ecotype sample point for each flora object.  Set "
			"to higher values means neighbouring ecotypes will encroach upon "
			"a flora block" );
		MF_WATCH( "Client Settings/Flora/Position Blur",
			&::getPositionBlur,
			&::setPositionBlur,
			"Multiplier for positioning each flora object.  Set to a higher "
			"value to make flora objects encroach upon neighbouring blocks.");
	}

	offset_ = offset;
	this->center( pos );
}

class FloraItem : public Aligned
{
public:
	FloraItem( Ecotype* e, Matrix& o, Vector2& p ):
		ecotype_(e),
		objectToWorld_(o),
		ecotypeSamplePt_(p)
	{
	};

	Ecotype* ecotype_;
	Matrix	objectToWorld_;
	Vector2	ecotypeSamplePt_;
};

typedef std::avector<FloraItem>	FloraItems;

/**
 *	This method fills a vertex allocation with appropriate stuff.
 *
 *	Note all ecotypes are reference counted.  This is so flora knows
 *	when an ecotype is not used, and can thus free its texture memory
 *	for newly activated ecotypes.
 */
void FloraBlock::fill( uint32 numVertsAllowed )
{
	static DogWatch watch( "Flora fill" );
	ScopedDogWatch dw( watch );

	Matrix objectToChunk;
	Matrix objectToWorld;

	//First, check if there is a terrain block at our location.
	Vector2 center( bb_.minBounds().x, bb_.minBounds().z );
	center.x += BLOCK_WIDTH/2.f;
	center.y += BLOCK_WIDTH/2.f;
	Terrain::TerrainFinder::Details details = 
		Terrain::BaseTerrainBlock::findOutsideBlock( 
			Vector3(center.x,0.f,center.y) );
	if (!details.pBlock_)
		return;	//return now.  fill later.
	center.x -= BLOCK_WIDTH/2.f;	
	center.y -= BLOCK_WIDTH/2.f;	

	const Matrix& chunkToWorld = *details.pMatrix_;
	blockID_ = (int)flora_->getTerrainBlockID( chunkToWorld );	
	Matrix worldToChunk = *details.pInvMatrix_;	
	uint32 numVertices = numVertsAllowed;	

	
	FloraItems items;

	//Seed the look up table of random numbers given
	//the center position.  This means we get a fixed
	//set of offsets given a geographical location.
	flora_->seedOffsetTable( center );

	Matrix transform;
	Vector2 ecotypeSamplePt;
	uint32 idx = 0;
	bool hasEmptyEcotype = false;

	while (1)
	{
		if (!nextTransform(center, objectToWorld, ecotypeSamplePt))
			break;			

		objectToChunk.multiply( objectToWorld, worldToChunk );
		Ecotype& ecotype = flora_->ecotypeAt( ecotypeSamplePt );

		//The ecotypes covering this FloraBlock are not yet fully loaded.
		//We will come back next frame and try again.  This is the main
		//reason for this initial while loop (check the loaded flags).
		if (ecotype.isLoading_)
		{				
			return;
		}

		if (ecotype.isEmpty())
		{
			//Check whether the current block contains an ecotype that reckons
			//it should be empty.  If we find one such ecotype, then don't fill
			//the whole block or allow encroaching ecotypes to seed here.
			//This allows paths to remain undisturbed (this is a pretty loose
			//feature and possibly can be taken out)
			hasEmptyEcotype = true;
			break;
		}

		FloraItem fi( &ecotype, objectToWorld, ecotypeSamplePt );
		items.push_back(fi);

		//Calling generate with NULL as the first parameter indicates we just want to know
		//the number of vertices that would have been generated, but leave the VB intact.
		uint32 nVerts = ecotype.generate( NULL, idx, numVertices, fi.objectToWorld_, objectToChunk, bb_ );
		if (nVerts == 0)
			break;			
		numVertices -= nVerts;
		if (numVertices == 0)
			break;			
		idx++;
	}

	//Alright so we have all the information for the items, and all the
	//ecotypes are loaded and valid.  Now produce vertices.
	idx = 0;
	numVertices = numVertsAllowed;
	//we will get a properly calculated bounding box from ecotype generation.
	bb_ = BoundingBox::s_insideOut_;
	FloraVertexContainer* pVerts = flora_->pRenderer()->lock( offset_, numVertsAllowed );

	if (!hasEmptyEcotype)
	{
		//Seed the look up table of random numbers again
		//This means the ecotype generators will end up using
		//exactly the same random numbers as in the first pass.
		flora_->seedOffsetTable( center );

		for (uint32 i=0; i<items.size(); i++)
		{
			FloraItem& fi = items[i];
			Ecotype& ecotype = *fi.ecotype_;
			ecotypes_.push_back( &ecotype );
			ecotype.incRef();
			objectToChunk.multiply( fi.objectToWorld_, worldToChunk );
			uint32 nVerts = ecotype.generate( pVerts, idx, numVertices, fi.objectToWorld_, objectToChunk, bb_ );
			if (nVerts == 0)			
				break;			
			idx++;
			numVertices -= nVerts;
			if (numVertices == 0)
				break;
		}
	}

	//fill the rest of the given vertices with degenerate triangles.
	MF_ASSERT( (numVertices%3) == 0 );
	pVerts->clear( numVertices );
	flora_->pRenderer()->unlock( pVerts );

	if ( bb_ == BoundingBox::s_insideOut_ )
		blockID_ = -1;
	bRefill_ = false;
}


/**
 *	This method should be called to move a flora block to a new position.
 *
 *	@param	c	The new center of the block.
 */
void FloraBlock::center( const Vector2& c )
{
	center_ = c;
	this->invalidate();
}


/**
 *	This method invalidates the flora block.  It sets the bRefill flag to true,
 *	and releases reference counts for all ecoytpes currently used.
 */
void FloraBlock::invalidate()
{
	bb_.setBounds(	Vector3(	center_.x - BLOCK_WIDTH/2.f,
								-20000.f, 
								center_.y - BLOCK_WIDTH/2.f ),
					Vector3(	center_.x + BLOCK_WIDTH/2.f,
								-20000.f, 
								center_.y + BLOCK_WIDTH/2.f) );
	blockID_ = -1;

	std::vector<Ecotype*>::iterator it = ecotypes_.begin();
	std::vector<Ecotype*>::iterator end = ecotypes_.end();

	while ( it != end )
	{
		if (*it)
			(*it)->decRef();
		it++;
	}

	ecotypes_.clear();

	bRefill_ = true;
}


void FloraBlock::cull()
{
	if (blockID_ != -1 )
	{
		bb_.calculateOutcode( Moo::rc().viewProjection() );
		culled_ = !!bb_.combinedOutcode();
	}
	else
	{
		culled_ = true;
	}
}


/**
 *	This method calculates the next random transform
 *	for a block.  Each transform is aligned to the
 *	terrain and positions an object on the terrain.
 *
 *	It also returns an ecotype sample point.  This
 *	suggest where to choose an ecotype from (not at
 *	the same place as the object is, in order to anti-
 *	alias the ecotype data)
 */
bool FloraBlock::nextTransform( const Vector2& center, Matrix& ret, Vector2& retEcotypeSamplePt )
{
	//we blur the ecotypes by choosing a sample point that can encroach on
	//neighbouring ecotypes
	Vector2 off( flora_->nextOffset() );	
	retEcotypeSamplePt.x = off.x * s_ecotypeBlur + center.x;
	retEcotypeSamplePt.y = off.y * s_ecotypeBlur + center.y;

	//get the new position.
	off = flora_->nextOffset();
	float rotY( flora_->nextRotation() );
	Vector3 pos( center.x + (off.x * s_positionBlur), 0.f, center.y + (off.y * s_positionBlur) );

	//get the terrain block, and the relative position of
	//pos within the terrain block.
	Vector3 relPos;
	Terrain::BaseTerrainBlockPtr pBlock = flora_->getTerrainBlock( pos, relPos, NULL );

	if ( !pBlock )
	{
		return false;
	}
	else
	{		
		//sit on terrain
		pos.y = pBlock->heightAt( relPos.x, relPos.z );
		if ( pos.y == Terrain::BaseTerrainBlock::NO_TERRAIN )
			return false;
		Vector3 intNorm = pBlock->normalAt( relPos.x, relPos.z );

		//align to terrain
		const PlaneEq eq( intNorm, intNorm.dotProduct( pos ) );
		Vector3 xyz[2];
		xyz[0].set( 0.f, eq.y( 0.f, 0.f ), 0.f );
		xyz[1].set( 0.f, eq.y( 0.f, 1.f ), 1.f );
		Vector3 up = xyz[1] - xyz[0];
		up.normalise();
		ret.lookAt( Vector3( 0.f, 0.f, 0.f ),
			up, Vector3( eq.normal() ) );
		ret.invertOrthonormal();

		//rotate randomly
		Matrix rot;
		rot.setRotateY( rotY );
		ret.preMultiply( rot );

		//move to terrain block local coords
		ret.translation( pos );	
	}

	return true;
}