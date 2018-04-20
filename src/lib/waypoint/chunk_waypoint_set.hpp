/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_WAYPOINT_SET_HPP
#define CHUNK_WAYPOINT_SET_HPP


#include "chunk/chunk_item.hpp"
#include "chunk/chunk_boundary.hpp"
#include "chunk/chunk.hpp"

#include <map>
#include <vector>


/**
 *	This class wraps a block of memory as an array.  The DependentArray is
 *	assumed to be followed with a uint16 size.
 */
template <class C> class DependentArray
{
public:
	typedef C value_type;
	typedef value_type * iterator;
	typedef const value_type * const_iterator;
	typedef typename std::vector<C>::size_type size_type;

	/**
	 *	This is the DependentArray default constructor.
	 */
	DependentArray() :
		data_( 0 )
	{
		_size() = 0;
	}

	/**
	 *	This constructs a DependentArray.
	 *
	 *	@param beg		An iterator to the beginning of the array.
	 *	@param end		An iterator to the end of the array.
	 */
	DependentArray( iterator beg, iterator end ) :
		data_( beg )
	{
		_size() = end - beg;
	}

	/**
	 *	This is the DependentArray copy constructor.  Note that copying is
	 *	shallow - the two DependentArrays will point to the same data.
	 *
	 *	@param other	The DependentArray to copy from.
	 */
	DependentArray( const DependentArray & other ) :
		data_( other.data_ )
	{
		_size() = other._size();
	}

	/**
	 *	This is the DependentArray assignment operator.  Note that copying is
	 *	shallow - the two DependentArrays will point to the same data.
	 *
	 *	@param other	The DependentArray to copy from.
	 *	@return			A reference to this object.
	 */
	DependentArray & operator=( const DependentArray & other )
	{
		new (this) DependentArray( other );
		return *this;
	}

	/**
	 *	This is the DependentArray destructor.
	 */
	~DependentArray()
		{ }

	/**
	 *	This gets the size of the DependentArray.
	 *
	 *	@return 		The size of the DependentArray.
	 */
	size_type size() const	{ return _size(); }

	/**
	 *	This gets the i'th element of the array.
	 *
	 *	@param i		The index of the array to get.
	 *	@return			A reference to the i'th element of the array.
	 */
	const value_type & operator[]( int i ) const { return data_[i]; }

	/**
	 *	This gets the i'th element of the array.
	 *
	 *	@param i		The index of the array to get.
	 *	@return			A reference to the i'th element of the array.
	 */
	value_type & operator[]( int i ){ return data_[i]; }

	/**
	 *	This gets the first element of the array.
	 *
	 *	@return			A const reference to the first element of the array.
	 */
	const value_type & front() const{ return *data_; }

	/**
	 *	This gets the first element of the array.
	 *
	 *	@return			A reference to the first element of the array.
	 */
	value_type & front()			{ return *data_; }

	/**
	 *	This gets the last element of the array.
	 *
	 *	@return			A const reference to the last element of the array.
	 */
	const value_type & back() const { return data_[_size()-1]; }

	/**
	 *	This gets the last element of the array.
	 *
	 *	@return			A reference to the last element of the array.
	 */
	value_type & back()				{ return data_[_size()-1]; }

	/**
	 *	This gets a const iterator to the first element of the array.
	 *
	 *	@return			A const iterator to the first element of the array.
	 */
	const_iterator begin() const	{ return data_; }

	/**
	 *	This gets a const iterator that refers to one past the last element of
	 *	the array.
	 *
	 *	@return			A const iterator that refers to one past the last
	 *					element of the array.
	 */
	const_iterator end() const		{ return data_+_size(); }

	/**
	 *	This gets an iterator to the first element of the array.
	 *
	 *	@return			An iterator to the first element of the array.
	 */
	iterator begin()				{ return data_; }

	/**
	 *	This gets a iterator that refers to one past the last element of
	 *	the array.
	 *
	 *	@return			A iterator that refers to one past the last element of
	 *					the array.
	 */
	iterator end()					{ return data_+_size(); }

private:
	value_type * data_;

	uint16 _size() const	{ return *(uint16*)(this+1); }
	uint16 & _size()		{ return *(uint16*)(this+1); }
};


/**
 *	This class is a waypoint as it exists in a chunk, when fully loaded.
 */
class ChunkWaypoint
{
public:
	bool contains( const Vector3 & point ) const;
	bool containsProjection( const Vector3 & point ) const;
	float distanceSquared( const Chunk* chunk, const Vector3 & point ) const;
	void clip( const Chunk* chunk, Vector3& lpoint ) const;

	/**
	 *	This is the minimum height of the waypoint.
	 */
	float minHeight_;

	/**
	 *	This is the maximum height of the waypoint.
	 */
	float maxHeight_;

	/**
	 *	This class is an edge in a waypoint/navpoly.
	 */
	struct Edge
	{
		/**
		 *	The start coordinate of the edge.
		 */
		Vector2		start_;

		/**
		 *	This contains adjacency information.  If this value ranges between
		 *	0 and 32768, then that is the ID of the waypoint adjacent to this
		 *	edge.  If this value is between 32768 and 65535, then it is
		 *	adjacent to	the chunk boundary.
		 *	Otherwise, it may contain some vista flags indicating cover, for
		 *	example, if its value's top bit is 1.
		 */
		uint32		neighbour_;

		/**
		 *	This returns the neighbouring waypoint's index.
		 *
		 *	@return			The index of the neighbouring waypoint if there is
		 *					one, or -1 if there	is none.
		 */
		int neighbouringWaypoint() const		// index into waypoints_
		{
			return neighbour_ < 32768 ? int(neighbour_) : -1;
		}

		/**
		 *	This returns whether this edge is adjacent to a chunk boundary.
		 *
		 *	@return			True if the edge is adjacent to a chunk boundary, false
		 *					otherwise.
		 */
		bool adjacentToChunk() const
		{
			return neighbour_ >= 32768 && neighbour_ <= 65535;
		}

		/**
		 *	This gets the vista bit flags.
		 *
		 *	@returns		The vista bit flags.
		 */
		uint32 neighbouringVista() const		// vista bit flags
		{
			return int(neighbour_) < 0 ? ~neighbour_ : 0;
		}
	};
	void print()
	{
		DEBUG_MSG( "MinHeight: %g\tMaxHeight: %g\tEdgeNum:%zu\n",
			minHeight_, maxHeight_, edges_.size()  );

		for (uint16 i = 0; i < edges_.size(); ++i)
		{
			DEBUG_MSG( "\t%d (%g, %g) %d - %s\n", i,
				edges_[ i ].start_.x, edges_[ i ].start_.y, edges_[ i ].neighbouringWaypoint(),
				edges_[ i ].adjacentToChunk() ? "chunk" : "no chunk" );
		}
	}

	typedef DependentArray<Edge> Edges;

	/**
	 *	This is the list of edges of the ChunkWaypoint.
	 */
	Edges	edges_;

	/**
	 *	This is the number of edges of the ChunkWaypoint.
	 *
	 *	DO NOT move this away!  DependentArray depends on edgeCount_ to exist
	 *	on its construction.
	 */
	uint16	edgeCount_;

	mutable uint16	mark_;

	static uint16 s_nextMark_;
};


typedef std::vector<ChunkWaypoint> ChunkWaypoints;


typedef uint32 WaypointEdgeIndex;


/**
 *	This class contains the data read in from a waypoint set.
 *	It may be shared between chunks. (In which case it is in local co-ords.)
 */
class ChunkWaypointSetData : public SafeReferenceCount
{
public:
	ChunkWaypointSetData();
	virtual ~ChunkWaypointSetData();

	bool loadFromXML( DataSectionPtr pSection, const std::string & sectionName);
	void transform( const Matrix & tr );

	int find( const Vector3 & lpoint, bool ignoreHeight = false );
	int find( const Chunk* chunk, const Vector3 & lpoint, float & bestDistanceSquared );

	int getAbsoluteEdgeIndex( const ChunkWaypoint::Edge & edge ) const;

private:
	float						girth_;
	ChunkWaypoints				waypoints_;
	std::string					source_;
	ChunkWaypoint::Edge * 		edges_;

	friend class ChunkWaypointSet;
	friend class ChunkNavPolySet;
};


typedef SmartPointer<ChunkWaypointSetData> ChunkWaypointSetDataPtr;


class	ChunkWaypointSet;
typedef SmartPointer<ChunkWaypointSet> ChunkWaypointSetPtr;
typedef std::vector<ChunkWaypointSetPtr> ChunkWaypointSets;

typedef std::map<ChunkWaypointSetPtr, ChunkBoundary::Portal *>
	ChunkWaypointConns;

typedef std::map<WaypointEdgeIndex, ChunkWaypointSetPtr>
	ChunkWaypointEdgeLabels;


/**
 *	This class is a set of connected waypoints in a chunk.
 *	It may have connections to other waypoint sets when its chunk is bound.
 */
class ChunkWaypointSet : public ChunkItem
{
	DECLARE_CHUNK_ITEM( ChunkWaypointSet )

public:
	ChunkWaypointSet();
	~ChunkWaypointSet();

	bool load( Chunk * pChunk, DataSectionPtr pSection,
		const char * sectionName, bool inWorldCoords );

	virtual void toss( Chunk * pChunk );

	void bind();

	/**
	 *	This finds the waypoint that contains the given point.
	 *
	 *	@param lpoint		The point that is used to find the waypoint.
	 *	@param ignoreHeight	Flag indicates vertical range should be considered
	 *						in finding waypoint.  If not, the best waypoint
	 *						that is closest to give point is selected (of
	 *						course the waypoint should contain projection of
	 *						the given point regardless).
	 *	@return				The index of the waypoint that contains the point,
	 *						-1 if no waypoint contains the point.
	 */
	int find( const Vector3 & lpoint, bool ignoreHeight = false )
	{
		return data_->find( lpoint, ignoreHeight );
	}

	/**
	 *	This finds the waypoint that contains the given point.
	 *
	 *	@param lpoint		The point that is used to find the waypoint.
	 *	@param bestDistanceSquared	The point must be closer than this to the
	 *						waypoint.  It is updated to the new best distance
	 *						if a better waypoint is found.
	 *	@return				The index of the waypoint that contains the point,
	 *						-1 if no waypoint contains the point.
	 */
	int find( const Vector3 & lpoint, float & bestDistanceSquared )
	{ 
		return data_->find( chunk(), lpoint, bestDistanceSquared ); 
	}

	/**
	 *	This gets the girth of the waypoints.
	 *
	 *	@returns			The girth of the waypoints.
	 */
	float girth() const
	{
		return data_->girth_;
	}

	/**
	 *	This gets the number of waypoints in the set.
	 *
	 *	@returns			The number of waypoints.
	 */
	int waypointCount() const
	{
		return data_->waypoints_.size();
	}

	/**
	 *	This gets the index'th waypoint.
	 *
	 *	@param index		The index of the waypoint to get.
	 *	@return				A reference to the index'th waypoint.
	 */
	ChunkWaypoint & waypoint( int index )
	{
		return data_->waypoints_[index];
	}

	/**
	 *	This returns an iterator to the first connection.
	 *
	 *	@return				An iterator to the first connection.
	 */
	ChunkWaypointConns::iterator connectionsBegin()
	{
		return connections_.begin();
	}

	/**
	 *	This returns a const iterator to the first connection.
	 *
	 *	@return				A const iterator to the first connection.
	 */
	ChunkWaypointConns::const_iterator connectionsBegin() const
	{
		return connections_.begin();
	}

	/**
	 *	This returns an iterator that points one past the last connection.
	 *
	 *	@return				An iterator to one past the last connection.
	 */
	ChunkWaypointConns::iterator connectionsEnd()
	{
		return connections_.end();
	}

	/**
	 *	This returns a const iterator that points one past the last connection.
	 *
	 *	@return				A const iterator to one past the last connection.
	 */
	ChunkWaypointConns::const_iterator connectionsEnd() const
	{
		return connections_.end();
	}

	/**
	 *	This gets the portal for the given waypoint set.
	 *
	 *	@param pWaypointSet	The ChunkWaypointSet to get the portal for.
	 *	@return				The portal for the ChunkWaypointSet.
	 */
	ChunkBoundary::Portal * connectionPortal( ChunkWaypointSetPtr pWaypointSet )
	{
		return connections_[pWaypointSet];
	}

	/**
	 *	This gets the ChunkWaypointSet for an edge.
	 *
	 *	@param edge			The ChunkWaypoint::Edge to get the ChunkWaypointSet
	 *						for.
	 *	@return				The ChunkWaypointSet along the edge.
	 */
	ChunkWaypointSetPtr connectionWaypoint( const ChunkWaypoint::Edge & edge )
	{
		return edgeLabels_[data_->getAbsoluteEdgeIndex( edge )];
	}

	void addBacklink( ChunkWaypointSetPtr pWaypointSet );

	void removeBacklink( ChunkWaypointSetPtr pWaypointSet );

	void print()
	{
		DEBUG_MSG( "ChunkWayPointSet: 0x%p - %s\tWayPointCount: %d\n", this, pChunk_->identifier().c_str(), waypointCount() );

		for (int i = 0; i < waypointCount(); ++i)
			waypoint( i ).print();

		for (ChunkWaypointConns::iterator iter = connectionsBegin();
			iter != connectionsEnd(); ++iter)
		{
			DEBUG_MSG( "**** connecting to 0x%p %s", iter->first.getObject(), iter->first->pChunk_->identifier().c_str() );
		}
	}

private:
	void deleteConnection( ChunkWaypointSetPtr pSet );

	void removeOurConnections();
	void removeOthersConnections();

	void connect(
		ChunkWaypointSetPtr pWaypointSet,
		ChunkBoundary::Portal * pPortal,
		ChunkWaypoint::Edge & edge );

protected:
	ChunkWaypointSetDataPtr	data_;
	ChunkWaypointConns			connections_;
	ChunkWaypointEdgeLabels		edgeLabels_;
	ChunkWaypointSets			backlinks_;
};


/**
 *  This class is a cache of all the waypoint sets in a chunk.
 */
class ChunkNavigator : public ChunkCache
{
public:
	ChunkNavigator( Chunk & chunk );
	~ChunkNavigator();

	virtual void bind( bool looseNotBind );

	/**
	 *	This class is used to return the results of a find operation.
	 */
	struct Result
	{
		Result() : pSet_( NULL ), waypoint_( -1 ) { }

		ChunkWaypointSetPtr	pSet_;
		int					waypoint_;
		bool				exactMatch_;
	};

	bool find( const Vector3 & lpoint, float girth,
				Result & res, bool ignoreHeight = false );

	bool isEmpty();
	bool hasNavPolySet( float girth );

	void add( ChunkWaypointSet * pSet );
	void del( ChunkWaypointSet * pSet );

	static Instance<ChunkNavigator> instance;
	static bool s_useGirthGrids_;

private:
	Chunk & chunk_;

	ChunkWaypointSets	wpSets_;

	struct GGElement
	{
		ChunkWaypointSet	* pSet_;	// dumb pointer
		int					waypoint_;

		GGElement( ChunkWaypointSet	* pSet, int waypoint )
			: pSet_(pSet), waypoint_(waypoint)
		{}
	};

	class GGList : public std::vector<GGElement>
	{
	public:
		bool find( const Vector3 & lpoint, Result & res,
							bool ignoreHeight = false );
		void find( const Chunk* chunk, const Vector3 & lpoint,
			float & bestDistanceSquared, Result & res );
	};

	struct GirthGrid
	{
		float	girth;
		GGList	* grid;
	};
	typedef std::vector<GirthGrid> GirthGrids;

	GirthGrids	girthGrids_;
	Vector2		ggOrigin_;
	float		ggResolution_;

	static const uint GG_SIZE = 12;
};


#endif // CHUNK_WAYPOINT_SET_HPP
