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
#include "adjacent_chunk_set.hpp"
#include "cstdmf/debug.hpp"


DECLARE_DEBUG_COMPONENT2( "WayPoint", 0 )


/**
 *	This is the AdjacentChunkSet constructor.
 */
AdjacentChunkSet::AdjacentChunkSet()
{
}


/**
 *	This function reads an AdjacentChunkSet from disk.
 *
 *	@param pChunkDir	A DataSection that contains the chunks to keep track
 *						of.
 *	@param chunkID		The id of the chunk to read.
 *	@return 			True if the adjacency information could be read, false otherwise.
 */
bool AdjacentChunkSet::read(DataSectionPtr pChunkDir, const ChunkID& chunkID)
{
	DataSection::iterator chunkIter;
	DataSection::iterator boundaryIter;
	DataSectionPtr pChunk, pBoundary, pPortal, pAdjChunk;
	std::string name;
	
	pChunk = pChunkDir->openSection(chunkID + ".chunk");

	if(!pChunk)
	{
		ERROR_MSG("Failed to open chunk %s\n", chunkID.c_str());
		return false;
	}

	startChunk_ = chunkID;
	this->addChunk(chunkID);
	this->readChunk(pChunk);

	for(chunkIter = pChunk->begin();
		chunkIter != pChunk->end();
		chunkIter++)
	{
		pBoundary = *chunkIter;

		if(pBoundary->sectionName() != "boundary")
			continue;

		for(boundaryIter = pBoundary->begin();
			boundaryIter != pBoundary->end();
			boundaryIter++)
		{
			pPortal = *boundaryIter;

			if(pPortal->sectionName() != "portal")
				continue;

			std::string name = pPortal->readString("chunk");

			if (name == "heaven" || name == "earth" || name == "")
				continue;

			pAdjChunk = pChunkDir->openSection(name + ".chunk");

			if (!pAdjChunk)
			{
				ERROR_MSG("Failed to open adjacent chunk %s\n", name.c_str());
			}
			else if(!this->hasChunk(name))
			{
				this->addChunk(name);
				this->readChunk(pAdjChunk);
			}
		}
	}

	return true;
}


/**
 *	This function gets the ChunkId of the Chunk that the AdjacentChunkSet 
 *	represents.
 *
 *	@return 		The ChunkId of the chunk that the AdjacentChunkSet 
 *					represents.
 */
const ChunkID& AdjacentChunkSet::startChunk() const
{
	return startChunk_;
}


/**
 *	This function tests whether the given chunk is adjacent.
 *
 *	@param chunkID	The ChunkId of the Chunk to test.
 *	@return			True if the given Chunk is adjacent, false otherwise.
 */
bool AdjacentChunkSet::hasChunk(const ChunkID& chunkID) const
{
	unsigned int i;

	for(i = 0; i < chunks_.size(); i++)
	{
		if(chunks_[i].chunkID == chunkID)
			return true;
	}

	return false;
}


/**
 *	This adds an adjacent chunk.
 *
 *	@param chunkID		The ChunkId of the adjacent chunk to add.
 */
void AdjacentChunkSet::addChunk(const ChunkID& chunkID)
{
	ChunkDef chunkDef;
	chunkDef.chunkID = chunkID;
	chunks_.push_back(chunkDef);
}


/**
 *	This adds a bounding plane to the last added chunk.
 *
 *	@param planeEq		The plane equation to add.
 */
void AdjacentChunkSet::addPlane(const PlaneEq& planeEq)
{
	chunks_.back().planes.push_back(planeEq);
}


/**
 *	This function checks whether the given position lies within any of the
 *	adjacent chunks.  Internal chunks are tested first.
 *
 *	@param position		The position to test.
 *	@param chunkID		If position is inside one of the adjacent chunks then
 *						this is set to the id of that chunk.  If position is
 *						not inside any chunks then it is left alone.
 *  @return				True if the point was in the adjacent chunks, false 
 *						otherwise.
 */
bool AdjacentChunkSet::test(const Vector3& position, ChunkID& chunkID) const
{	
//	dprintf( "Testing point (%f,%f,%f) against adj chunk set:\n",
//		position.x, position.y, position.z );

	// Test internal chunks first.	
	for (unsigned int i = 0; i < chunks_.size(); i++)
	{
		if (chunks_[i].chunkID[8] == 'i' && testChunk(position, i))
		{
			chunkID = chunks_[i].chunkID;
			return true;
		}
	}

	// Test non-internal chunks next.	
	for (unsigned int i = 0; i < chunks_.size(); i++)
	{
		if (chunks_[i].chunkID[8] != 'i' && testChunk(position, i))
		{
			chunkID = chunks_[i].chunkID;
			return true;
		}
	}

//	dprintf( "\tNot in any chunk\n" );
	return false;
}


/**
 *	This reads the adjacency information for a neighbouring chunk.
 *
 *	@param pChunk		A pointer to the neighbouring Chunk's DataSection.
 *	@return 			True.
 */
bool AdjacentChunkSet::readChunk(DataSectionPtr pChunk)
{
	DataSection::iterator chunkIter;
	DataSectionPtr pBoundary;
	Vector3 normal;
	Matrix transform;
	float d;

	for(chunkIter = pChunk->begin();
		chunkIter != pChunk->end();
		chunkIter++)
	{
		pBoundary = *chunkIter;

		transform = pChunk->readMatrix34("transform");

		if(pBoundary->sectionName() != "boundary")
			continue;

		if(pBoundary->readBool("portal/internal"))
		{
			// don't want internal ones.
			continue;
		}

		if(pBoundary->readString("portal/chunk") == "heaven" ||
		   pBoundary->readString("portal/chunk") == "earth" ||
		   pBoundary->readString("portal/chunk") == "extern")
		{
			// don't want heaven or earth (or extern).
			continue;
		}

		normal = pBoundary->readVector3("normal");
		d = pBoundary->readFloat("d");

		Vector3 ndtr = transform.applyPoint(normal * d);
		Vector3 ntr = transform.applyVector(normal);
		PlaneEq plane(ntr, ntr.dotProduct(ndtr));
		
		/*
		// 25/02/2003: JWD: Removed this as I cannot figure out what it is for.
		// Nick added it about this time last year, saying that it fixed y heights,
		// but it looks like a terrible hack to me and was stuffing things up.
		if(fabs(plane.normal().y - 1.0f) < 0.0001f)
		{
			plane = PlaneEq(plane.normal(), plane.d() - 5.0f);
		}
		*/

		this->addPlane(plane);
	}

	return true;
}


/**
 *	This tests whether the given position is within the i'th neighbour.
 *
 *	@param position		The position to test.
 *	@param i			The index of the neighbouring Chunk to test.
 *	@return 			True if position lies within the boundary of the i'th
 *						chunk as determined by the bounding planes, false otherwise.
 */
bool AdjacentChunkSet::testChunk(const Vector3& position, int i) const
{
	const ChunkDef& chunkDef = chunks_[i];

//	dprintf( "\tChecking chunk %s, nplanes %d\n",
//		chunkDef.chunkID.c_str(), chunkDef.planes.size() );

	for(unsigned int p = 0; p < chunkDef.planes.size(); p++)
	{
//		const Vector3 & n = chunkDef.planes[p].normal();
//		dprintf( "\t\tPlane (%f,%f,%f)>%f (dp %f)\n",
//			n.x, n.y, n.z, chunkDef.planes[p].d(), n.dotProduct(position) );
		if(!chunkDef.planes[p].isInFrontOf(position))
		{
			return false;
		}
	}

	// the point is in front of all boundary planes, so point is in the chunk.
	return true;
}
