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
#include "chunk_format.hpp"

#include "cstdmf/debug.hpp"
#include "cstdmf/dogwatch.hpp"
#include "cstdmf/guard.hpp"

#include "physics2/quad_tree.hpp"
#include "physics2/worldtri.hpp"

#include "resmgr/bwresource.hpp"

#include "chunk.hpp"
#include "chunk_obstacle.hpp"
#include "chunk_manager.hpp"

#include "grid_traversal.hpp"

#include "chunk_umbra.hpp"

#include "terrain/terrain_settings.hpp"


DECLARE_DEBUG_COMPONENT2( "Chunk", 0 )


// -----------------------------------------------------------------------------
// Section: ChunkSpace collisions
// -----------------------------------------------------------------------------


/**
 *	This template class is a shape that can be swept through a chunk space and
 *	collided with the obstacles therein.
 *
 *	The shape may be no bigger than GRID_RESOLUTION in any dimension.
 */
template <class X> class SweepShape
{
	//	const Vector3 & leader() const;
	//	void boundingBox( BoundingBox & bb ) const;
	//	void transform( X & shape, Vector3 & extent,
	//		const Matrix & transformer,
	//		float sdist, float edist, const Vector3 & dir,
	//		const Vector3 & bbCenterAtSDistTransformed,
	//		const Vector3 & bbCenterAtEDistTransformed ) const;
	//	inline static const float transformRangeToRadius( const Matrix & trInv,
	//		const Vector3 & shapeRange )
};

/**
 *	This function collides the volume formed by sweeping the shape in source
 *	along the line segment from source's leading point to extent, with the
 *	obstacles in this space (or, more accurately, with the obstacles in the
 *	columns currently under the focus grid).
 *
 *	@return 	-1 if no obstacles were found, or the last value of dist passed
 *					into the collision callback object.
 */
template <class X>
float ChunkSpace_collide(
	const ChunkSpace::ColumnGrid &	currentFocus_,
	const SweepShape<X> &			start,
	const Vector3 &					end,
	CollisionCallback &				cc )
{
	// increment the hull obstacle mark
	ChunkObstacle::nextMark();

	// find the min and max of X and Z
	BoundingBox shapeBox;
	start.boundingBox( shapeBox );

	Vector3 shapeRange = shapeBox.maxBounds() - shapeBox.minBounds();
	float shapeRad = shapeRange.length() * 0.5f;

	// find our real source and extent
	Vector3 bsource = shapeBox.minBounds();
	Vector3 bextent = bsource + (end - start.leader());

	// get the source point moved to the centre of the bb
	// (for hulltree rounded cylinder collisions)
	Vector3 csource = bsource + shapeRange * 0.5f;
	Vector3 cextent = csource + (end - start.leader());

	// and make the grid traversal object
	SpaceGridTraversal sgt( bsource, bextent, shapeRange, GRID_RESOLUTION );

	const ChunkObstacle * pObstacle;

	// make the collision state object
	CollisionState cs( cc );

	do
	{
		bool inSpan = currentFocus_.inSpan( sgt.sx, sgt.sz );
		const ChunkSpace::Column * pCol = currentFocus_( sgt.sx, sgt.sz );

		// check this column as long as it's in range
		if (inSpan && pCol != NULL)
		{
			const ObstacleTree & tree = pCol->obstacles();

			// traverse the hulltree from cast to land
			ObstacleTreeTraversal htt = tree.traverse(
				csource + sgt.dir * (sgt.cellSTravel - shapeRad),
				csource + sgt.dir * (sgt.cellETravel + shapeRad),
				shapeRad );

			while ((pObstacle =
				static_cast< const ChunkObstacle *>( htt.next() )) != NULL)
			{
				if (pObstacle->mark()) continue;
				if (pObstacle->pChunk() == NULL) continue;

				//dprintf( "Considering obstacle 0x%08X\n", &pObstacle->data_ );

				Vector3 sTr, eTr;
				const Matrix & trInv = pObstacle->transformInverse_;
				trInv.applyPoint( sTr, csource );
				trInv.applyPoint( eTr, cextent );

				// find the biggest axis in this system
				int bax = 0;
				float babs = fabsf( eTr[0] - sTr[0] );
				float aabs;
				aabs = fabsf( eTr[1] - sTr[1] );
				if (aabs > babs) { babs = aabs; bax = 1; }
				aabs = fabsf( eTr[2] - sTr[2] );
				if (aabs > babs) { babs = aabs; bax = 2; }

				float sTrba = sTr[bax], dTrba = eTr[bax] - sTr[bax];

				// Clip the line to the bounding box ('tho it should always
				//  be inside since we found it through the hull tree)
				if ( !pObstacle->clipAgainstBB( sTr, eTr,
					start.transformRangeToRadius( trInv, shapeRange ) + 0.01f ))
					continue;

				// set traveled and traveled to be the start and end distances
				//  along the line (not their original use, but it fits)
				cs.sTravel_ = (sTr[bax] - sTrba) / dTrba * sgt.fullDist;
				cs.eTravel_ = (eTr[bax] - sTrba) / dTrba * sgt.fullDist;

				// see if we can reject this bb outright
				if (cs.onlyLess_ && cs.sTravel_ > cs.dist_) continue;
				if (cs.onlyMore_ && cs.eTravel_ < cs.dist_) continue;

				// ok, let's search in it then
				X shape;
				Vector3 shapeEnd;
				start.transform( shape, shapeEnd, trInv,
					cs.sTravel_, cs.eTravel_, sgt.dir, sTr, eTr );
				if (pObstacle->collide( shape, shapeEnd, cs ))
					return cs.dist_;
			}
		}
	}
	while ((!cs.onlyLess_ || (cs.dist_ + shapeRad > sgt.cellETravel)) && sgt.next());

	return cs.dist_;
}


/**
 *	This is the simplest instantiation of the SweepShape template,
 *	for sweeping a single point through the space (making a ray)
 */
template <>
class SweepShape<Vector3>
{
public:
	SweepShape( const Vector3 & pt ) : pt_( pt ) {}

	const Vector3 & leader() const
	{
		return pt_;
	}

	void boundingBox( BoundingBox & bb ) const
	{
		bb.setBounds( pt_, pt_ );
	}

	void transform( Vector3 & shape, Vector3 & end,
		const Matrix &, float, float, const Vector3 &,
		const Vector3 & bbCenterAtSDistTransformed,
		const Vector3 & bbCenterAtEDistTransformed ) const
	{
		shape = bbCenterAtSDistTransformed;
		end = bbCenterAtEDistTransformed;
	}

	inline static const float transformRangeToRadius( const Matrix & /*trInv*/,
		const Vector3 & /*shapeRange*/ )
	{
		return 0.f;
	}

private:
	Vector3 pt_;
};


/**
 *	This is the chunk space collide wrapper method for colliding a ray with the
 *	chunk space.
 *
 *	@param start	The start of the ray.
 *	@param end		The end of the ray.
 *	@param cc		The callback object.
 *
 *	@see ChunkSpace_collide
 */
float ChunkSpace::collide( const Vector3 & start, const Vector3 & end,
	CollisionCallback & cc ) const
{
	BW_GUARD;
	// Check for stray collision tests. This is usually an indicator that
	// something else has become corrupted (such as model animations).
	//TODO : remove for trade shows.  add back in during development (bug 22332)
	MF_ASSERT_DEV( -100000.f < start.x && start.x < 100000.f &&
			-100000.f < start.z && start.z < 100000.f );
	MF_ASSERT_DEV( -100000.f < end.x && end.x < 100000.f &&
			-100000.f < end.z && end.z < 100000.f );

	return ChunkSpace_collide(
		currentFocus_, SweepShape<Vector3>(start), end, cc );
}


/**
 *	This is the instantiation of the SweepShape template for a WorldTriangle,
 *	which swept through the chunk space makes a prism.
 */
template <> class SweepShape<WorldTriangle>
{
public:
	SweepShape( const WorldTriangle & wt ) : wt_( wt ) {}

	const Vector3 & leader() const
	{
		return wt_.v0();
	}

	void boundingBox( BoundingBox & bb ) const
	{
		bb.setBounds( wt_.v0(), wt_.v0() );
		bb.addBounds( wt_.v1() );
		bb.addBounds( wt_.v2() );
	}

	void transform( WorldTriangle & shape, Vector3 & end,
		const Matrix & tr, float sdist, float edist, const Vector3 & dir,
		const Vector3&, const Vector3& ) const
	{
		Vector3 off = dir * sdist;
		shape = WorldTriangle(
			tr.applyPoint( wt_.v0() + off ),
			tr.applyPoint( wt_.v1() + off ),
			tr.applyPoint( wt_.v2() + off ) );
		end = tr.applyPoint( wt_.v0() + dir * edist );
	}

	inline static const float transformRangeToRadius( const Matrix & trInv,
		const Vector3 & shapeRange )
	{
		const Vector3 clipRange = trInv.applyVector( shapeRange );
		return clipRange.length() * 0.5f;
	}

private:
	WorldTriangle wt_;
};


/**
 *	This is the chunk space collide wrapper method for colliding a triangular
 *  prism.
 *
 *	@param start	The start triangle for one end of the triangle.
 *	@param end		The end position of the first point on the triangle.
 *  @param cc		The CollisionCallback class to use for notification.
 *
 *	@see ChunkSpace_collide
 */
float ChunkSpace::collide( const WorldTriangle & start,
						const Vector3 & end, CollisionCallback & cc ) const
{
	BW_GUARD;
	return ChunkSpace_collide(
		currentFocus_, SweepShape<WorldTriangle>(start), end, cc );
}


/// static initialiser for SpaceGridTraversal
VectorNoDestructor<SpaceGridTraversal::CellSpec> SpaceGridTraversal::altCells;


// -----------------------------------------------------------------------------
// Section: ChunkDirMapping
// -----------------------------------------------------------------------------

extern THREADLOCAL(bool) g_versionPointSafe;

/**
 *	Constructor.
 *	The path supplied should _not_ end with a slash.
 */
ChunkDirMapping::ChunkDirMapping( ChunkSpacePtr pSpace, Matrix & m,
		const std::string & path,
		DataSectionPtr pSettings ) :
	pSpace_( pSpace ),
	mapper_( m ),
	path_( path + "/" ),
	name_( path.substr( 1 + std::min(
		path.find_last_of( '/' ), path.find_last_of( '\\' ) ) ) ),
	pDirSection_( NULL ),
	condemned_( false ),
	singleDir_( false )
{
	BW_GUARD;
	if (!pSettings)
	{
		pSettings = this->openSettings( path );
	}

	invMapper_.invert( mapper_ );
	char addrStr[32];
#ifndef _WIN32
	bw_snprintf( addrStr, sizeof( addrStr ), "@%p", this );
#else
	bw_snprintf( addrStr, sizeof( addrStr ), "@0x%p", this );
#endif

	name_ += addrStr;	// keep the name unique

	if (pSettings)
	{
		// read our grid bounds...
		minLGridX_ = pSettings->readInt( "bounds/minX" );
		minLGridY_ = pSettings->readInt( "bounds/minY" );
		maxLGridX_ = pSettings->readInt( "bounds/maxX" );
		maxLGridY_ = pSettings->readInt( "bounds/maxY" );
		BoundingBox sbb;
		sbb.setBounds(	Vector3( float(minLGridX_), 0,
								 float(minLGridY_) ) * GRID_RESOLUTION,
						Vector3( float(maxLGridX_+1), 0,
								 float(maxLGridY_+1) ) * GRID_RESOLUTION );

		// ...as mapped by our mapper
		sbb.transformBy( mapper_ );

		minGridX_ = int(floorf( sbb.minBounds().x / GRID_RESOLUTION + 0.5f ));
		maxGridX_ = int(floorf( sbb.maxBounds().x / GRID_RESOLUTION + 0.5f ) - 1);
		minGridY_ = int(floorf( sbb.minBounds().z / GRID_RESOLUTION + 0.5f));
		maxGridY_ = int(floorf( sbb.maxBounds().z / GRID_RESOLUTION + 0.5f) - 1);

		singleDir_ = pSettings->readBool( "singleDir", false );

#if UMBRA_ENABLE
		// read whether we want to use umbra occlusion or not for this space
		// TODO: Remove this and make a proper fix for occlusion culling in arid space
		UmbraHelper::instance().occlusionCulling(
				pSettings->readBool( "umbraOcclusion", true ) );
#endif


		// tell the space about this exciting new opportunity
		pSpace->mappingSettings( pSettings );
	}
	else
	{
		pSpace_ = NULL;	// we failed
	}
}

/**
 *	Destructor.
 */
ChunkDirMapping::~ChunkDirMapping()
{
	BW_GUARD;
	if (pSpace_)
	{
		DEBUG_MSG( "ChunkDirMapping::~ChunkDirMapping: "
			"Deleted %s from space %u\n", path_.c_str(), pSpace_->id() );
	}

	MF_ASSERT_DEV( this->condemned() );
	pSpace_ = NULL;
}


/**
 *	pDirSection accessor.
 *
 *	Should only be called from loading thread as it may block,
 *	especially if chunk data is out of date.
 */
DataSectionPtr ChunkDirMapping::pDirSection()
{
	BW_GUARD;
	if (!pDirSection_)
		pDirSection_ = BWResource::openSection(
			path_.substr( 0, path_.size()-1 ) );

	return pDirSection_;
}


/**
 *	This method opens the settings file associated with the input path to space
 *	geometry.
 */
DataSectionPtr ChunkDirMapping::openSettings( const std::string & path )
{
	BW_GUARD;
	DataSectionPtr pSettings = BWResource::openSection( path );
	if (pSettings)
	{
		pSettings = pSettings->openSection( SPACE_SETTING_FILE_NAME );
	}

	return pSettings;
}


/**
 *	This method returns the identifier for the outside chunk in which the
 *	given point would lie. If the checkBounds parameter is true, and the
 *	point does not lie within the bounds of the mapping, then the empty
 *	string is returned instead.
 */
std::string ChunkDirMapping::outsideChunkIdentifier( const Vector3 & localPoint,
	bool checkBounds ) const
{
	BW_GUARD;
	int gridX = int(floorf( localPoint.x / GRID_RESOLUTION ));
	int gridZ = int(floorf( localPoint.z / GRID_RESOLUTION ));

	if (checkBounds)
	{
		if (gridX < minLGridX_ || gridX > maxLGridX_ ||
			gridZ < minLGridY_ || gridZ > maxLGridY_)
				return std::string();
	}

	return ChunkFormat::outsideChunkIdentifier( gridX, gridZ, singleDir_ );
}

/**
 *	This method returns the identifier for the outside chunk in which the
 *	given point would lie. If the checkBounds parameter is true, and the
 *	point does not lie within the bounds of the mapping, then the empty
 *	string is returned instead.
 */
std::string ChunkDirMapping::outsideChunkIdentifier( int x, int z,
	bool checkBounds ) const
{
	BW_GUARD;
	if (checkBounds)
	{
		if (x < minLGridX_ || x > maxLGridX_ ||
			z < minLGridY_ || z > maxLGridY_)
				return std::string();
	}

	return ChunkFormat::outsideChunkIdentifier( x, z, singleDir_ );
}

class HexLookup
{
public:
	HexLookup()
	{
		for( int i = 0; i < 256; ++i )
		{
			hexLookup_[ i ] = 0;
			hexFactor_[ i ] = 0;
		}

		for (unsigned char i='0'; i<='9'; i++)
		{
			hexLookup_[i] = (uint16)(i-'0');
			hexFactor_[i] = 1;
		}

		for (unsigned char i='a'; i<='f'; i++)
		{
			hexLookup_[i] = (uint16)(i-'a' + 10);
			hexFactor_[i] = 1;
		}

		for (unsigned char i='A'; i<='F'; i++)
		{
			hexLookup_[i] = (uint16)(i-'A' + 10);
			hexFactor_[i] = 1;
		}
	};

	//done this way to avoid conditionals.
	//ascii-hex numbers directly looked up, while the factors are anded.
	//if any ascii-hex lookup is invalid, one of the factors will be 0 and thus
	//anded out, all will be invalid.
	bool fromHex( const unsigned char* f, int16& value )
	{
		value = (hexLookup_[f[3]] +
			(hexLookup_[f[2]]<<4) +
			(hexLookup_[f[1]]<<8) +
			(hexLookup_[f[0]]<<12));
		int8 factor = hexFactor_[f[0]] & hexFactor_[f[1]] & hexFactor_[f[2]] & hexFactor_[f[3]];
		return (!!factor);
	}

	uint16 hexLookup_[256];
	uint8 hexFactor_[256];
};

/*static*/ bool ChunkDirMapping::gridFromChunkName( const std::string& chunkName, int16& x, int16& z )
{
	BW_GUARD;
	static HexLookup hexLookup;

	//Assume name is dir/path/path/.../chunkNameo for speed
	const unsigned char* f = (const unsigned char*)chunkName.c_str();
	while ( *f ) f++;

	//subtract "xxxxxxxxo" which is always the last part of an
	//outside chunk identifier.
	if(*(f-1) == 'o')
	{
		return (hexLookup.fromHex(f-9,x) && hexLookup.fromHex(f-5,z));
	}

	return false;
}

// -----------------------------------------------------------------------------
// Section: LoadMappingTask
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
ChunkSpace::LoadMappingTask::LoadMappingTask(
		ChunkSpacePtr pChunkSpace,
		SpaceEntryID mappingID,
		float * matrix,
		const std::string & path ) :
	pChunkSpace_( pChunkSpace ),
	mappingID_( mappingID ),
	matrix_( *(Matrix *)matrix ),
	path_( path ),
	pSettings_( NULL )
{
}


/**
 *	This method performs the disk activity required in setting up a
 *	ChunkDirMapping.
 */
void ChunkSpace::LoadMappingTask::doBackgroundTask( BgTaskManager & mgr )
{
	BW_GUARD;
	pSettings_ = ChunkDirMapping::openSettings( path_ );

	mgr.addMainThreadTask( this );
}


/**
 *	This method performs the ChunkSpace::addMapping once the background work has
 *	been done.
 */
void ChunkSpace::LoadMappingTask::doMainThreadTask( BgTaskManager & mgr )
{
	BW_GUARD;
	if (pChunkSpace_->validatePendingTask( this ))
	{
		if (pSettings_)
		{
			pChunkSpace_->addMapping( mappingID_, matrix_, path_, pSettings_ );
		}
		else
		{
			ERROR_MSG( "ChunkSpace::addMappingAsync: "
				"No space.settings file for '%s'\n", path_.c_str() );
		}
	}
}


// -----------------------------------------------------------------------------
// Section: ChunkSpace
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
ChunkSpace::ChunkSpace( ChunkSpaceID id ) :
	ConfigChunkSpace( id ),
	closestUnloadedChunk_( 0.f )
{
	BW_GUARD;
	ChunkManager::instance().addSpace( this );
}

/**
 *	Destructor.
 */
ChunkSpace::~ChunkSpace()
{
	BW_GUARD;
	ChunkManager::instance().delSpace( this );
}


/**
 *	This method adds a mapping to this chunk space in a asynchronous way. It
 *	does the IO tasks in a background thread before calling
 *	ChunkSpace::addMappingAsync.
 */
void ChunkSpace::addMappingAsync( SpaceEntryID mappingID,
	float * matrix, const std::string & path )
{
	BW_GUARD;
	BackgroundTaskPtr pTask =
		new LoadMappingTask( this, mappingID, matrix, path );

	backgroundTasks_.insert( pTask.getObject() );

	BgTaskManager::instance().addBackgroundTask( pTask );
}


/**
 *	This method adds a mapping to this chunk space. It returns the mapping if
 *	the addition was successful.
 */
ChunkDirMapping * ChunkSpace::addMapping( SpaceEntryID mappingID,
	float * matrix, const std::string & path, DataSectionPtr pSettings )
{
	BW_GUARD;
	Matrix m = *(Matrix*)matrix;	// unaligned cast ok since just for copying

	// Add the ChunkDirMapping object
	ChunkDirMapping * pMapping =
		new ChunkDirMapping( this, m, path, pSettings );

	if (!pMapping->pSpace())
	{
		ERROR_MSG( "ChunkSpace::addMapping: "
			"No space settings file found in %s\n", path.c_str() );
		pMapping->condemn();
		delete pMapping;
		return NULL;
	}

	const Vector3 & xa = m.applyToUnitAxisVector( 0 );
	const Vector3 & tr = m.applyToOrigin();
	DEBUG_MSG( "ChunkSpace::addMapping: "
		"Adding %s at (%f,%f,%f) xaxis (%f,%f,%f) to space %u\n",
		path.c_str(), tr.x, tr.y, tr.z, xa.x, xa.y, xa.z, this->id() );

	pMapping->incRef();
	mappingsLock_.grab();
	mappings_.insert( std::make_pair( mappingID, pMapping ) );
	mappingsLock_.give();

	// See if there are any unresolved externs that can now be resolved
	ChunkMap::iterator it;
	for (it = currentChunks_.begin(); it != currentChunks_.end(); it++)
	{
		for (uint i = 0; i < it->second.size(); i++)
		{
			Chunk * pChunk = it->second[i];
			if (!pChunk->online()) continue;

			pChunk->resolveExterns();
			// TODO: This might stuff up our iterators!
		}
	}

	if (!pSettings.hasObject())
	{
		pSettings = ChunkDirMapping::openSettings( path );
	}

	this->recalcGridBounds();

	terrainSettings_ = new Terrain::TerrainSettings;
	DataSectionPtr terrainSettingsData = pSettings->openSection("terrain") ;
#if defined(EDITOR_ENABLED)
	if (!terrainSettingsData)  // settings without terrain section
	{
		// get version from chunk file
		uint32 version = 0;

		for (int i = pMapping->minGridX(); i <= pMapping->maxGridX(); ++i)
		{
			for (int j = pMapping->minGridY(); j <= pMapping->maxGridY(); ++j)
			{
				std::string resName = pMapping->path() +
					pMapping->outsideChunkIdentifier( i, j ) +
					".cdata/terrain";
				version = Terrain::BaseTerrainBlock::terrainVersion( resName );

				if (version > 0)
					break;
			}

			if (version > 0)
				break;
		}

		if (version == 100 || version == 200) // chunk has version info
		{
			// create terrain section
			terrainSettingsData = pSettings->openSection( "terrain", true );
			terrainSettings_->initDefaults();
			terrainSettings_->version( version );

			if (version == 200)
			{
				terrainSettings_->heightMapSize( 128 );
				terrainSettings_->normalMapSize( 128 );
				terrainSettings_->holeMapSize( 25 );
				terrainSettings_->shadowMapSize( 32 );
				terrainSettings_->blendMapSize( 256 );
			}

			terrainSettings_->save( terrainSettingsData );
			pSettings->save();
		}
	}
	// Must open the space in WE for the terrain settings to be created.
	IF_NOT_MF_ASSERT_DEV( terrainSettingsData != NULL )
	{
		pMapping->condemn();
		delete pMapping;
		return NULL;
	}
#endif // EDITOR_ENABLED

	terrainSettings_->init( terrainSettingsData );

#ifdef MF_SERVER
	if (terrainSettings_->serverHeightMapLod() > 0)
	{
		NOTICE_MSG( "ChunkSpace::addMapping: "
						"Loading reduced detail level (%u) for %s.\n",
					terrainSettings_->serverHeightMapLod(), path.c_str() );
	}
#endif

	// And we're done!
	return pMapping;
}

ChunkDirMapping * ChunkSpace::getMapping( SpaceEntryID mappingID )
{
	BW_GUARD;
	ChunkDirMappings::iterator found = mappings_.find( mappingID );
	if (found == mappings_.end())
		return NULL;

	return found->second;
}

PROFILER_DECLARE( ChunkSpace_delMapping1, "ChunkSpace_delMapping1" );
PROFILER_DECLARE( ChunkSpace_delMapping2, "ChunkSpace_delMapping2" );

#if PROFILE_D3D_RESOURCE_RELEASE
extern void DumpD3DResourceReleaseResults();
extern bool g_doProfileD3DResourceRelease;
#endif

/**
 *	This method removes the named mapping from this chunk space.
 */
void ChunkSpace::delMapping( SpaceEntryID mappingID )
{
	BW_GUARD;	
#ifndef MF_SERVER
	ScopedSyncMode scopedSyncMode;
#endif//MF_SERVER

#if PROFILE_D3D_RESOURCE_RELEASE
	g_doProfileD3DResourceRelease = true;
#endif

	ChunkDirMappings::iterator found = mappings_.find( mappingID );
	if (found == mappings_.end()) return;

	ChunkDirMapping * pMapping = found->second;
	mappingsLock_.grab();
	mappings_.erase( found );
	mappingsLock_.give();
	pMapping->condemn();

	DEBUG_MSG( "ChunkSpace::delMapping: Comdemned %s in space %u\n",
		pMapping->path().c_str(), this->id() );

	// Find all the chunks mapped in with this mapping and condemn them
	// This is very important since they have ordinary pointers to the mapping
	std::vector<Chunk*> comdemnedChunks;
	ChunkMap::iterator it;

	for (it = currentChunks_.begin(); it != currentChunks_.end(); it++)
	{
		for (uint i = 0; i < it->second.size(); i++)
		{
			Chunk * pChunk = it->second[i];

			if (pChunk->mapping() != pMapping) continue;
			if (pChunk->loading()) continue;	// we'll get back to you later

			// this chunk is going to disappear
			{
				PROFILER_SCOPED( ChunkSpace_delMapping1 );
				if (pChunk->online()) pChunk->loose( false );
			}
		
			{
				PROFILER_SCOPED( ChunkSpace_delMapping2 );
				if (pChunk->loaded()) pChunk->eject();
			}

			comdemnedChunks.push_back( pChunk );
		}
	}

	// Find all the loaded chunks that are not in this mapping that refer
	// to chunks in this mapping (through portals) and set them back to extern.
	for (it = currentChunks_.begin(); it != currentChunks_.end(); it++)
	{
		for (uint i = 0; i < it->second.size(); i++)
		{
			Chunk * pChunk = it->second[i];
			if (pChunk->mapping() == pMapping) continue;

			if (!pChunk->online()) continue;

			pChunk->resolveExterns( pMapping );
			// TODO: This might stuff up our iterators!
		}
	}

	// Delete all the condemned chunks
	for (uint i = 0; i < comdemnedChunks.size(); i++)
		delete comdemnedChunks[i];

	// And we're done!
	this->recalcGridBounds();
	pMapping->decRef();	// delete the mapping unless in use by loading chunks
	// (may be in use by a chunk in this mapping that is loading or by the stub
	// chunk through an extern portal of a chunk in another mapping)

#if PROFILE_D3D_RESOURCE_RELEASE
	g_doProfileD3DResourceRelease = false;
	DumpD3DResourceReleaseResults();
#endif
}


/**
 *	Clear method.
 *	@see BaseChunkSpace::clear
 */
void ChunkSpace::clear()
{
	BW_GUARD;
	DEBUG_MSG( "ChunkSpace::clear: Clearing space %u\n", this->id() );

	MF_ASSERT_DEV( this->refCount() != 0 );
	ChunkSpacePtr pThis = this;

	this->ConfigChunkSpace::clear();

	while (!mappings_.empty())
		this->delMapping( mappings_.begin()->first );

	// If there are any pending addMappings, they should no longer be performed.
	backgroundTasks_.clear();

	// if camera is in this space, move
	// it out of it. If player entity exists,
	// the camera will be moved back to the
	// player space in the next tick.
	if (this == ChunkManager::instance().cameraSpace())
	{
 		ChunkManager::instance().camera( Matrix::identity, NULL );
	}

	closestUnloadedChunk_ = 0.0f;
}


/**
 *	This slow function is the last-resort way to find which chunk
 *	a given point belongs in. The only thing that should use
 *	it every frame is the camera, as it is not subject to the
 *	same laws of physics as mortals are.
 */
Chunk * ChunkSpace::findChunkFromPoint( const Vector3 & point )
{
	BW_GUARD;
	// NOTE: add a small fudge factor on point.y. This is an attempt
	// to resolve issue rised from the situation where point (mostly
	// comes from entity) position on the terrain matchs "exactly" with
	// chunk bounding box. We could encounter floating point errors and
	// cann't find the chunk. Maybe there is a better solution for this
	// problem.

	Vector3 pt = point;
	pt.y += 0.0001f;
	Column * pCol = this->column( pt, false );
	return (pCol != NULL) ? pCol->findChunk( pt ) : NULL;
}


/**
 *	This method returns the column at the given point, or NULL if it is
 *	out of range (or not created and canCreate is false)
 */
ChunkSpace::Column * ChunkSpace::column( const Vector3 & point, bool canCreate )
{
	BW_GUARD;
	// find grid coords
	int x = pointToGrid( point.x );
	int z = pointToGrid( point.z );

	// check range
	if (!currentFocus_.inSpan( x, z ))
	{
		return NULL;
	}

	// get entry
	Column * & rpCol = currentFocus_( x, z );

	// create if willing and able
	if (!rpCol && canCreate)
	{
		rpCol = new Column( x, z );
	}

	// and that's it
	return rpCol;
}


/**
 *	This method dumps debug information about this space.
 */
void ChunkSpace::dumpDebug() const
{
	BW_GUARD;
	const ColumnGrid & focus = currentFocus_;

	const int xBegin = focus.xBegin();
	const int xEnd = focus.xEnd();
	const int zBegin = focus.zBegin();
	const int zEnd = focus.zEnd();

	DEBUG_MSG( "----- Total Size -----\n" );

	int total = 0;

	for (int z = zBegin; z < zEnd; z++)
	{
		for (int x = xBegin; x < xEnd; x++)
		{
			const Column * pCol = focus( x, z );

			int totalSize = pCol ? pCol->size() : 0;
			total += totalSize;
			dprintf( "%8d ", totalSize );
		}

		dprintf( "\n" );
	}
	DEBUG_MSG( "Total = %d\n", total );
	total = 0;

	DEBUG_MSG( "----- Obstacle Size -----\n" );

	for (int z = zBegin; z < zEnd; z++)
	{
		for (int x = xBegin; x < xEnd; x++)
		{
			const Column * pCol = focus( x, z );

			int obstacleSize =
				pCol ? pCol->obstacles().size() : 0;
			total += obstacleSize;
			dprintf( "%8d ", obstacleSize );
		}

		dprintf( "\n" );
	}
	DEBUG_MSG( "Total = %d\n", total );

	/*
	const Column * pCol = focus( focus.originX(), focus.originZ() );
	DEBUG_MSG( "Printing obstacles\n" );
	pCol->obstacles().print();
	*/
}


/**
 *	This method calculates the bounding box of the space in world coords.
 */
BoundingBox ChunkSpace::gridBounds() const
{
	return BoundingBox(
		Vector3( gridToPoint( minGridX() ), MIN_CHUNK_HEIGHT, gridToPoint( minGridY() ) ),
		Vector3( gridToPoint( maxGridX() + 1 ), MAX_CHUNK_HEIGHT, gridToPoint( maxGridY() + 1 ) )
		);
}


/**
 *	This method guesses which chunk to load based on an input point.
 *	It returns an unloaded chunk, or NULL if none could be found.
 *	Note: the returned chunk notionally holds a reference to its mapping.
 *
 *	If lookInside is false, then there must be no chunks in the space,
 *	however the chunk returned is not ratified (used for bootstrapping).
 *
 *	If lookInside is true, then an inside chunk is returned in preference
 *	to an outside one, and there may be other chunks, and the chunk returned
 *	is not ratified (used for resolving extern portals).
 */
Chunk * ChunkSpace::guessChunk( const Vector3 & point, bool lookInside )
{
	BW_GUARD;	
/* It is ok to do this now
	if (!lookInside)
	{
		MF_ASSERT_DEV( currentChunks_.empty() );
	}
*/

	ChunkDirMapping * pBestMapping = NULL;
	std::string bestChunkIdentifier;

	// lock the mappings before iterating over them since this method
	// can be called from either thread.
	SimpleMutexHolder smh( mappingsLock_ );

	if (mappings_.empty()) return NULL;

	//DEBUG_MSG( "ChunkSpace::guessChunk call:\n" );
	//DEBUG_MSG( "wpoint is (%f,%f,%f)\n", point.x, point.y, point.z );

	// go through all our mappings
	for (ChunkDirMappings::iterator it = mappings_.begin();
		it != mappings_.end();
		it++)
	{
		// find the point local to this mapping
		ChunkDirMapping * pMapping = it->second;
		Vector3 lpoint = pMapping->invMapper().applyPoint( point );
		int gridmx = int(floorf( lpoint.x / GRID_RESOLUTION ));
		int gridmz = int(floorf( lpoint.z / GRID_RESOLUTION ));
		int grido = lookInside ? 1 : 0;

		//DEBUG_MSG( "Investigating mapping %s\n", pMapping->path().c_str() );
		//DEBUG_MSG( "lpoint is (%f,%f,%f) grid (%d,%d)\n",
		//	lpoint.x, lpoint.y, lpoint.z, gridmx, gridmz );

		std::string chunkIdentifier;

		// unfortunately since there is only one overlapper section
		// per chunk, we have to look in all 9 outside chunks in the area :(
		// at least it usually happens in the loading thread
		// TODO: 16/2/2004 - this loop is only to support legacy spaces...
		// the editor now creates multiple overlappers sections
		// (i.e. one in every outside chunk that the inside chunk overlaps)
		for (int gridx = gridmx-grido; gridx <= gridmx+grido; gridx++)
			for (int gridz = gridmz-grido; gridz <= gridmz+grido; gridz++)
		{	// start of unindented for loop (a noop unless looking inside)

		// build the chunk identifier
		std::string gridChunkIdentifier =
			pMapping->outsideChunkIdentifier( lpoint );
		if (gridChunkIdentifier.empty()) continue;
		/*
		char chunkIdentifierCStr[32];
		std::string gridChunkIdentifier;
		uint16 gridxs = uint16(gridx), gridzs = uint16(gridz);
		if ((gridxs + 4096) >= 8192 || (gridzs + 4096) >= 8192)
		{
			bw_snprintf( chunkIdentifierCStr, sizeof(chunkIdentifierCStr), "%01xxxx%01xxxx/",
				int(gridxs >> 12), int(gridzs >> 12) );
			gridChunkIdentifier = chunkIdentifierCStr;
		}
		if ((gridxs + 256) >= 512 || (gridzs + 256) >= 512)
		{
			bw_snprintf( chunkIdentifierCStr, sizeof(chunkIdentifierCStr), "%02xxx%02xxx/",
				int(gridxs >> 8), int(gridzs >> 8) );
			gridChunkIdentifier += chunkIdentifierCStr;
		}
		if ((gridxs + 16) >= 32 || (gridzs + 16) >= 32)
		{
			bw_snprintf( chunkIdentifierCStr, sizeof(chunkIdentifierCStr), "%03xx%03xx/",
				int(gridxs >> 4), int(gridzs >> 4) );
			gridChunkIdentifier += chunkIdentifierCStr;
		}
		bw_snprintf( chunkIdentifierCStr, sizeof(chunkIdentifierCStr), "%04x%04xo", int(gridxs), int(gridzs) );
		gridChunkIdentifier += chunkIdentifierCStr;
		*/

		// see if we have an outside chunk for that point
		DataSectionPtr pDir = pMapping->pDirSection();
		DataSectionPtr pOutsideDS =
			pDir->openSection( gridChunkIdentifier + ".chunk" );
		if (!pOutsideDS)
		{
			// see if the whole space is in one flat directory
			/*
			std::string flatChunkIdentifier = chunkIdentifierCStr;
			if (gridChunkIdentifier == flatChunkIdentifier) continue;

			gridChunkIdentifier = flatChunkIdentifier;
			pOutsideDS = pDir->openSection( gridChunkIdentifier + ".chunk" );
			if (!pOutsideDS) continue;
			// yes it's all flat, so proceed
			*/
			continue;
		}

		// we have an outside chunk for that point, yay!
		//DEBUG_MSG( "Outside chunk %s exists\n", gridChunkIdentifier.c_str() );

		// TODO: Don't bother opening the section if we're not looking inside
		// (ResMgr does not yet have such an interface though)

		// this could be the chunk we want if we're in the middle grid square
		if (gridx == gridmx && gridz == gridmz && chunkIdentifier.empty())
			chunkIdentifier = gridChunkIdentifier;

		// if we want to look inside, then see if there's any overlappers
		// that might do a better job
		if (lookInside)
		{
			std::vector<std::string> overlappers;
			pOutsideDS->readStrings( "overlapper", overlappers );
			for (uint i = 0; i < overlappers.size(); i++)
			{
				DataSectionPtr pBBSect =
					pDir->openSection( overlappers[i]+".chunk/boundingBox" );
				if (!pBBSect) continue;

				BoundingBox bb;
				bb.setBounds(	pBBSect->readVector3( "min" ),
								pBBSect->readVector3( "max" ) );
				//DEBUG_MSG( "Considering overlapper %s (%f,%f,%f)-(%f,%f,%f)\n",
				//	overlappers[i].c_str(),
				//	bb.minBounds().x, bb.minBounds().y, bb.minBounds().z,
				//	bb.maxBounds().x, bb.maxBounds().y, bb.maxBounds().z );
				if (bb.intersects( lpoint ))
				{
					// we found a good one, so overwrite chunkIdentifier
					//DEBUG_MSG( "\tIt's a match!\n" );
					chunkIdentifier = overlappers[i];
					pBestMapping = NULL;	// is inside so is best
					break;
				}
			}
		}

		}	// end of unindented for loop

		// record this as the best mapping if we think it is
		if (pBestMapping == NULL && !chunkIdentifier.empty())
		{
			pBestMapping = pMapping;
			bestChunkIdentifier = chunkIdentifier;

			if (!lookInside) break;	// only inside chunks can beat this
		}
	}

	// now make and return the chunk if we found one
	if (pBestMapping != NULL)
	{
		//DEBUG_MSG( "Best chunk is %s in %s\n", bestChunkIdentifier.c_str(),
		//	pBestMapping->path().c_str() );
		Chunk * pChunk = new Chunk( bestChunkIdentifier, pBestMapping );
		pBestMapping->incRef();
		return pChunk;
	}

	//DEBUG_MSG( "Could not guess a chunk\n" );

	return NULL;
}

/**
 *	This method recalculates the grid bounds of this chunk after mappings
 *	have changed.
 */
void ChunkSpace::recalcGridBounds()
{
	BW_GUARD;
	if (mappings_.empty())
	{
		minGridX_ = 0;
		minGridY_ = 0;
		maxGridX_ = 0;
		maxGridY_ = 0;
	}
	else
	{
		minGridX_ = 1000000000;
		minGridY_ = 1000000000;
		maxGridX_ = -1000000000;
		maxGridY_ = -1000000000;

		for (ChunkDirMappings::iterator it = mappings_.begin();
			it != mappings_.end();
			it++)
		{
			minGridX_ = std::min( minGridX_, it->second->minGridX() );
			minGridY_ = std::min( minGridY_, it->second->minGridY() );
			maxGridX_ = std::max( maxGridX_, it->second->maxGridX() );
			maxGridY_ = std::max( maxGridY_, it->second->maxGridY() );
		}
	}

	this->ConfigChunkSpace::recalcGridBounds();
}


/**
 *	Server-only emulate method.
 *	TODO: Remove this method when the server loads spaces properly.
 */
void ChunkSpace::emulate( ChunkSpacePtr pRightfulSpace )
{
	BW_GUARD;
	// make sure everything is suitable for emulation
	MF_ASSERT_DEV( mappings_.size() == 1 );
	MF_ASSERT_DEV( pRightfulSpace->mappings_.size() == 1 );

	ChunkDirMapping * pOwnMapping = mappings_.begin()->second;

	MF_ASSERT_DEV( pOwnMapping->path() ==
		pRightfulSpace->mappings_.begin()->second->path() );

	// get out of the chunk manager
	ChunkManager & cm = ChunkManager::instance();
	cm.delSpace( this );

	// do the base class stuff
	this->BaseChunkSpace::emulate( pRightfulSpace );

	// now keep our ChunkDirMapping but use pRightfulSpace's key.
	mappings_.clear();
	mappings_.insert( std::make_pair(
		pRightfulSpace->mappings_.begin()->first, pOwnMapping ) );
	// no need to lock mappings since this is done on server w/ou threads
	// ... in fact, absence of threads is whole reason for this method :)

	this->recalcGridBounds();

	// and finally replace the emulated chunk space in the chunk manager
	cm.delSpace( &*cm.space( this->id(), false ) );
	cm.addSpace( this );
}


/**
 *	This method deletes a chunk that has just finished loading that is
 *	in a mapping that has been condemned by a 'delMapping' call, or is
 *	otherwise unwanted.
 *
 *	All online chunks have already been taken care of, but chunks that
 *	were loading when the call was received are left dangling.
 *
 *	This method takes all action necessary to discard such a chunk
 *	after the loading thread is done with it. If the chunk has ever been
 *	bound (even if it is currently unbound) then the normal 'eject' method
 *	should be used instead.
 *
 *	If the chunk was in a condemned mapping, then it is safe to delete the
 *	chunk object after calling this function.
 */
void ChunkSpace::ejectLoadedChunkBeforeBinding( Chunk * pChunk )
{
	BW_GUARD;
	IF_NOT_MF_ASSERT_DEV( pChunk->loaded() && !pChunk->online() )
	{
		return;
	}

	DEBUG_MSG( "ChunkSpace::ejectLoadedChunkBeforeBinding: %s\n",
		pChunk->identifier().c_str() );

	bool ejectChunk = true;

	// throw away all its stub chunks (only in unbound)
	for (ChunkBoundaries::iterator bit = pChunk->joints().begin();
		bit != pChunk->joints().end();
		bit++)
	{
		ChunkBoundary::Portals::iterator pit;
		for (pit = (*bit)->unboundPortals_.begin();
			pit != (*bit)->unboundPortals_.end();
			pit++)
		{
			if ((*pit)->hasChunk())
			{
				ChunkDirMapping * pMapping = (*pit)->pChunk->mapping();

				IF_NOT_MF_ASSERT_DEV( !(*pit)->pChunk->ratified() )
				{
					ejectChunk = false;
					continue;
				}
				delete (*pit)->pChunk;
				(*pit)->pChunk = NULL;

				// don't forget to decRef mapping on extern stub chunks
				if (pMapping != pChunk->mapping())
					pMapping->decRef();
			}
		}
	}

	// eject it
	if( ejectChunk )
		pChunk->eject();
}


/**
 *	Ignore a chunk as it's going to be disposed (unloaded).
 *
 *	Note that this method may delete columns from the focus grid, so the
 *	focus method must be called before anything robust accesses it.
 *	This is done from the 'camera' method in the chunk manager.
 */
void ChunkSpace::ignoreChunk( Chunk * pChunk )
{
	BW_GUARD;
	// can't ignore unbound chunks
	if (!pChunk->online())
	{
		ERROR_MSG( "ChunkSpace::ignoreChunk: "
			"Attempted to ignore offline chunk '%s'\n",
			pChunk->identifier().c_str() );
		return;
	}

	// find out where it is in the focus grid
	const Vector3 & cen = pChunk->centre();
	int nx = int(cen.x / GRID_RESOLUTION);	if (cen.x < 0.f) nx--;
	int nz = int(cen.z / GRID_RESOLUTION);	if (cen.z < 0.f) nz--;

	// and get it out of there
	for (int x = nx-1; x <= nx+1; x++) for (int z = nz-1; z <= nz+1; z++)
	{
		if ( currentFocus_.inSpan( x, z ) )
		{
			Column *& rpCol = currentFocus_( x, z );
			if (rpCol != NULL)
			{
				//pCol->stale();
				// Note: We actually delete these columns instead of setting
				// them as stale. This is not to save time in focus!
				// It is to make sure we never find anything about this chunk
				// in the focus grid, because that would be very bad. (e.g. if
				// findChunkFromPoint returned it, and we added an item to it)
				//
				// note: deleting the column smudges chunks in it (adds to
				// blurred list). Use this when recreating column.
				delete rpCol;
				rpCol = NULL;
				// Consequently, the grid must directly be regenerated.
			}
		}
	}

	this->removeFromBlurred( pChunk );

}

/**
 *	This method notifies the chunk space that the given chunk is now online
 *	and may be focussed.
 */
void ChunkSpace::noticeChunk( Chunk * pChunk )
{
	BW_GUARD;
	this->blurredChunk( pChunk );

	// currently does not do anything else :-/
	// but at least it makes sense with 'ignore'
}


/**
 *	This method is called by the LoadMappingTask when it has finished background
 *	loading. It checks whether it is still valid to proceed with mapping in
 *	geometry. It will not be valid if ChunkSpace::clear has been called in the
 *	meantime.
 */
bool ChunkSpace::validatePendingTask( BackgroundTask * pTask )
{
	BW_GUARD;
	return backgroundTasks_.erase( pTask ) != 0;
}

TerrainSettingsPtr ChunkSpace::terrainSettings() const
{
	return terrainSettings_;
}

// chunk_space.cpp
