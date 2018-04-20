/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_SPACE_HPP
#define CHUNK_SPACE_HPP

#ifdef MF_SERVER
#include "server_chunk_space.hpp"
typedef ServerChunkSpace ConfigChunkSpace;
#else
#include "client_chunk_space.hpp"
class ClientChunkSpace;
typedef ClientChunkSpace ConfigChunkSpace;
#endif

#include "chunk_item.hpp"

#include "cstdmf/aligned.hpp"
#include "cstdmf/bgtask_manager.hpp"
#include "cstdmf/concurrency.hpp"
#include "cstdmf/guard.hpp"


#include <set>

//	Forward declarations relating to chunk obstacles
class CollisionCallback;
extern CollisionCallback & CollisionCallback_s_default;
class ChunkObstacle;

typedef uint32 ChunkSpaceID;

class ChunkSpace;
class ChunkDirMapping;
typedef SmartPointer<ChunkSpace> ChunkSpacePtr;

namespace Terrain
{
	class TerrainSettings;
}

typedef SmartPointer<Terrain::TerrainSettings> TerrainSettingsPtr;


/**
 *	This class defines a space and maintains the chunks that live in it.
 *
 *	A space is a continuous three dimensional Cartesian medium. Each space
 *	is divided piecewise into chunks, which occupy the entire space but
 *	do not overlap. i.e. every point in the space is in exactly one chunk.
 *	Examples include: planets, parallel spaces, spacestations, 'detached'
 *	apartments / dungeon levels, etc.
 */
class ChunkSpace : public ConfigChunkSpace
{
public:
	ChunkSpace( ChunkSpaceID id );
	~ChunkSpace();

	typedef std::map<SpaceEntryID,ChunkDirMapping*> ChunkDirMappings;

	ChunkDirMapping * addMapping( SpaceEntryID mappingID, float * matrix,
		const std::string & path, DataSectionPtr pSettings = NULL );
	void addMappingAsync( SpaceEntryID mappingID, float * matrix,
		const std::string & path );

	ChunkDirMapping * getMapping( SpaceEntryID mappingID );
	void delMapping( SpaceEntryID mappingID );
	const ChunkDirMappings & getMappings()
		{ return mappings_; }

	void clear();

	Chunk * findChunkFromPoint( const Vector3 & point );

	Column * column( const Vector3 & point, bool canCreate = true );

	// Collision related methods
	float collide( const Vector3 & start, const Vector3 & end,
		CollisionCallback & cc = CollisionCallback_s_default ) const;

	float collide( const WorldTriangle & start, const Vector3 & end,
		CollisionCallback & cc = CollisionCallback_s_default ) const;

	void dumpDebug() const;

	BoundingBox gridBounds() const;

	Chunk * guessChunk( const Vector3 & point, bool lookInside = false );

	void emulate( ChunkSpacePtr pRightfulSpace );

	void ejectLoadedChunkBeforeBinding( Chunk * pChunk );

	void ignoreChunk( Chunk * pChunk );
	void noticeChunk( Chunk * pChunk );

	void closestUnloadedChunk( float closest ) { closestUnloadedChunk_ = closest; }
	float closestUnloadedChunk()			   { return closestUnloadedChunk_;	}

	bool validatePendingTask( BackgroundTask * pTask );

	TerrainSettingsPtr terrainSettings() const;
private:
	void recalcGridBounds();

	/**
	 *	This class is used by addMappingAsync to perform the required background
	 *	loading.
	 */
	class LoadMappingTask : public BackgroundTask
	{
	public:
		LoadMappingTask( ChunkSpacePtr pChunkSpace, SpaceEntryID mappingID,
				float * matrix, const std::string & path );

		virtual void doBackgroundTask( BgTaskManager & mgr );
		virtual void doMainThreadTask( BgTaskManager & mgr );

	private:
		ChunkSpacePtr pChunkSpace_;

		SpaceEntryID mappingID_;
	   	Matrix matrix_;
		std::string path_;

		DataSectionPtr pSettings_;
	};

	TerrainSettingsPtr	terrainSettings_;

	ChunkDirMappings		mappings_;
	float					closestUnloadedChunk_;
	std::set< BackgroundTask * > backgroundTasks_;

	SimpleMutex				mappingsLock_;
};


/**
 *	This class is a mapping of a resource directory containing chunks
 *	into a chunk space.
 *
 *	@note Only its chunk space and chunks queued to load retain references
 *	to this object.
 */
class ChunkDirMapping : public Aligned, public SafeReferenceCount
{
public:
	ChunkDirMapping( ChunkSpacePtr pSpace, Matrix & m,
		const std::string & path, DataSectionPtr pSettings );
	~ChunkDirMapping();

	ChunkSpacePtr	pSpace() const			{ return pSpace_; }

	const Matrix &	mapper() const			{ return mapper_; }
	const Matrix &	invMapper() const		{ return invMapper_; }

	const std::string & path() const		{ return path_; }
	const std::string & name() const		{ return name_; }

	DataSectionPtr	pDirSection();
	static DataSectionPtr openSettings( const std::string & path );

	/// The following accessors return the world-space grid bounds of this
	/// mapping, after the transform is applied. These bounds are expanded
	/// to include even the slightest intersection of the mapping with a
	/// grid square in the space's coordinate system
	int minGridX() const					{ return minGridX_; }
	int maxGridX() const 					{ return maxGridX_; }
	int minGridY() const 					{ return minGridY_; }
	int maxGridY() const 					{ return maxGridY_; }

	/// The following accessors return the bounds of this mapping in its
	/// own local coordinate system.
	int minLGridX() const					{ return minLGridX_; }
	int maxLGridX() const 					{ return maxLGridX_; }
	int minLGridY() const 					{ return minLGridY_; }
	int maxLGridY() const 					{ return maxLGridY_; }

	bool condemned()						{ return condemned_; }
	void condemn()							{ condemned_ = true; }

	std::string outsideChunkIdentifier( const Vector3 & localPoint,
		bool checkBounds = true ) const;
	std::string outsideChunkIdentifier( int x, int z, bool checkBounds = true ) const;
	static bool gridFromChunkName( const std::string& chunkName, int16& x, int16& z );

	void add( Chunk* chunk );
	void remove( Chunk* chunk );
	Chunk* chunkFromGrid( int16 x, int16 z );

private:
	ChunkSpacePtr	pSpace_;

	Matrix			mapper_;
	Matrix			invMapper_;

	std::string		path_;
	std::string		name_;
	DataSectionPtr	pDirSection_;

	std::map< std::pair<int, int>, Chunk* > chunks_;

	int				minGridX_;
	int				maxGridX_;
	int				minGridY_;
	int				maxGridY_;

	int				minLGridX_;
	int				maxLGridX_;
	int				minLGridY_;
	int				maxLGridY_;

	bool			condemned_;
	bool			singleDir_;
};


#endif // CHUNK_SPACE_HPP
