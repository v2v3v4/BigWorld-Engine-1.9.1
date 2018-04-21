/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SPACE_HPP
#define SPACE_HPP

#include "cellapp_interface.hpp"

#include "cell_range_list.hpp"
#include "cell_app_channel.hpp"

#include "math/math_extra.hpp"
#include "network/basictypes.hpp"

#include <vector>

class BinaryIStream;
class Cell;
class Chunk;
class ChunkSpace;
typedef SmartPointer<ChunkSpace> ChunkSpacePtr;


// From "entity.hpp"
typedef SmartPointer<Entity> EntityPtr;

// These do not need to be reference counted since they are added in the
// construction and removed in destruction (or destroy) of entity.
typedef std::vector< EntityPtr >                 SpaceEntities;
typedef SpaceEntities::size_type                 SpaceRemovalHandle;

// From "entity.hpp"
class Entity;
typedef SmartPointer<Entity> EntityPtr;

struct DirMappingLoader;
typedef std::map<SpaceEntryID,DirMappingLoader*> DirMappingLoaders;


/**
 *	This class is used to represent a space.
 */
class Space : public Mercury::TimerExpiryHandler
{
public:
	Space( SpaceID id );
	virtual ~Space();

	class CellInfo;

	/**
	 *	This interface is implemented by classes that are used to visit all
	 *	cells in a space.
	 */
	class CellVisitor
	{
	public:
		virtual ~CellVisitor() {};
		virtual void visit( CellInfo & cell ) {};
	};

	/**
	 *	This base class is used to represent the nodes in the BSP tree that is
	 *	used to partition a space.
	 */
	class Node
	{
	public:
		virtual void deleteTree() = 0;

		virtual CellInfo * pCellAt( float x, float z ) = 0;
		virtual void visitRect(
				const BW::Rect & rect, CellVisitor & visitor ) = 0;
		virtual void addToStream( BinaryOStream & stream ) const = 0;

	protected:
		virtual ~Node() {};
	};

	/**
	 *	This class is used to represent an internal node of the BSP. It
	 *	corresponds to a partitioning plane.
	 */
	class Branch : public Node
	{
	public:
		Branch( Space & space, const BW::Rect & rect,
				BinaryIStream & stream, bool isHorizontal );
		virtual ~Branch();
		virtual void deleteTree();

		virtual CellInfo * pCellAt( float x, float z );
		virtual void visitRect( const BW::Rect & rect, CellVisitor & visitor );
		virtual void addToStream( BinaryOStream & stream ) const;

	private:
		float	position_;
		bool	isHorizontal_;
		Node *	pLeft_;
		Node *	pRight_;
	};

	/**
	 *	This class is used to represent a leaf node of the BSP. It corresponds
	 *	to a cell in the space.
	 */
	class CellInfo : public Node, public ReferenceCount
	{
	public:
		CellInfo( const BW::Rect & rect,
				const Mercury::Address & addr, BinaryIStream & stream );
		~CellInfo();

		static Watcher& watcher();

		void updateFromStream( BinaryIStream & stream );

		virtual void deleteTree() {};
		virtual CellInfo * pCellAt( float x, float z );
		virtual void visitRect( const BW::Rect & rect, CellVisitor & visitor );
		virtual void addToStream( BinaryOStream & stream ) const;

		const Mercury::Address & addr() const	{ return addr_; }
		float getLoad() const					{ return load_; }

		bool shouldDelete() const			{ return shouldDelete_; }
		void shouldDelete( bool v )			{ shouldDelete_ = v; }

		const BW::Rect & rect() const		{ return rect_; }
		void rect( const BW::Rect & rect )	{ rect_ = rect; }

		bool contains( const Vector3 & pos ) const
			{ return rect_.contains( pos.x, pos.z ); }

	private:
		Mercury::Address	addr_;
		float				load_;
		bool				shouldDelete_;
		BW::Rect			rect_;
	};

	typedef std::map< Mercury::Address,
							SmartPointer< CellInfo > > CellInfos;
	typedef SmartPointer< CellInfo > CellInfoPtr;
	typedef ConstSmartPointer< CellInfo > ConstCellInfoPtr;

	CellInfo * pCellAt( float x, float z ) const;
	void visitRect( const BW::Rect & rect, CellVisitor & visitRect );

	// ---- Accessors ----
	SpaceID id() const						{ return id_; }

	Cell * pCell() const					{ return pCell_; }
	void pCell( Cell * pCell );

	ChunkSpacePtr pChunkSpace() const;

	// ---- Entity ----
	void createGhost( BinaryIStream & data );
		// ( EntityID id, TypeID type, Position3D & pos,
		//  string & ghostState, Mercury::Address & owner );

	void addEntity( Entity * pEntity );
	void removeEntity( Entity * pEntity );

	EntityPtr newEntity( EntityID id, EntityTypeID entityTypeID );

	Entity * findNearestEntity( const Vector3 & position );

	// ---- Static methods ----
	static Space * findMessageHandler( BinaryIStream & data );

	enum UpdateCellAppMgr
	{
		UPDATE_CELL_APP_MGR,
		DONT_UPDATE_CELL_APP_MGR
	};

	enum DataEffected
	{
		ALREADY_EFFECTED,
		NEED_TO_EFFECT
	};

	static class Watcher & watcher();

	// ---- Space data ----
	void spaceData( BinaryIStream & data );
	void allSpaceData( BinaryIStream & data );

	void updateGeometry( BinaryIStream & data );

	void spaceGeometryLoaded( BinaryIStream & data );

	void setLastMappedGeometry( const std::string& lastMappedGeometry )
	{ lastMappedGeometry_ = lastMappedGeometry; }

	void shutDownSpace( const CellAppInterface::shutDownSpaceArgs & args );
	void requestShutDown();

	CellInfo * findCell( const Mercury::Address & addr ) const;

	Node * readTree( BinaryIStream & stream, const BW::Rect & rect );

	bool spaceDataEntry( const SpaceEntryID & entryID, uint16 key,
		const std::string & value,
		UpdateCellAppMgr cellAppMgrAction = UPDATE_CELL_APP_MGR,
		DataEffected effected = NEED_TO_EFFECT );

	int32 begDataSeq() const	{ return begDataSeq_; }
	int32 endDataSeq() const	{ return endDataSeq_; }

	const std::string * dataBySeq( int32 seq,
		SpaceEntryID & entryID, uint16 & key ) const;
	int dataRecencyLevel( int32 seq ) const;

	const RangeList & rangeList() const	{ return rangeList_; }
	bool getRealEntitiesBoundary( BW::Rect & boundary,
		   int numToSkip = 0 ) const;

	void debugRangeList();

	SpaceEntities & spaceEntities() { return entities_; }
	const SpaceEntities & spaceEntities() const { return entities_; }

	void writeDataToStream( BinaryOStream & steam ) const;
	void readDataFromStream( BinaryIStream & stream );

	void chunkTick();
	void calcLoadedRect( BW::Rect & loadedRect ) const;
	void prepareNewlyLoadedChunksForDelete();

	// old chunk loader:
	void bindLoadedChunks();
	void loadChunk( Chunk * pChunk );

	bool isFullyUnloaded() const;

	float timeOfDay() const;

	bool isShuttingDown() const			{ return shuttingDownTimerID_ != 0; }

	void writeRecoveryData( BinaryOStream & stream ) const;

private:
	virtual int handleTimeout( Mercury::TimerID id, void * arg );

	void calcBound( bool isMin, bool isY, int numToSkip,
			BW::Rect & boundary ) const;

	void checkForShutDown();

	bool hasSingleCell() const;

	SpaceID	id_;

	Cell *	pCell_;
	ChunkSpacePtr pChunkSpace_;

	SpaceEntities	entities_;
	CellInfos		cellInfos_;

	RangeList	rangeList_;

	int32	begDataSeq_;
	int32	endDataSeq_;

	/**
	 *	This is the information we record about recent data entries
	 */
	struct RecentDataEntry
	{
		SpaceEntryID	entryID;
		TimeStamp		time;
		uint16			key;
	};
	typedef std::vector<RecentDataEntry> RecentDataEntries;
	RecentDataEntries	recentData_;
	// TODO: Need to clean out recent data every so often (say once a minute)
	// Also don't let beg/endDataSeq go negative.
	// Yes that will require extensive fixups.

	DirMappingLoaders	dirMappingLoaders_;
	std::list< Chunk * > loadingChunks_;

	float	initialTimeOfDay_;
	float	gameSecondsPerSecond_;

	std::string lastMappedGeometry_;

	Node *	pCellInfoTree_;

	Mercury::TimerID shuttingDownTimerID_;

public:
	static uint32 s_allSpacesDataChangeSeq_;
};

#ifdef CODE_INLINE
#include "space.ipp"
#endif

#endif // SPACE_HPP

// space.hpp
