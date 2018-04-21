/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ENTITY_CACHE_HPP
#define ENTITY_CACHE_HPP

#include "cstdmf/binary_stream.hpp"
#include "network/mercury.hpp"
#include "network/msgtypes.hpp"
#include "cstdmf/smartpointer.hpp"

#include <set>

const IDAlias NO_ID_ALIAS = 0xff;

class Entity;
typedef ConstSmartPointer<Entity> EntityConstPtr;


/**
 *	This class is used by RealEntityWithWitnesses to cache information about
 *	other entities.
 */
class EntityCache
{
public:
	// TODO: Remove this restriction.
	static const int MAX_LOD_LEVELS = 4;

	typedef double Priority;

	EntityCache( const Entity * pEntity );
	EntityCache( EntityID dummyID );
	~EntityCache();

	static EntityCache * newDummy( EntityID dummyID );
	INLINE void construct();

	float updatePriority( const Vector3 & origin );

	void updateDetailLevel( Mercury::Bundle & bundle, float lodPriority );
	void addOuterDetailLevel( Mercury::Bundle & bundle );

	void addLeaveAoIMessage( Mercury::Bundle & bundle, EntityID id ) const;

	INLINE int numLoDLevels() const;
	INLINE static int numLoDLevels( const Entity & e );

	// Accessors

	EntityConstPtr pEntity() const	{ return pEntity_; }
	EntityConstPtr & pEntity()		{ return pEntity_; }

	// When offloading/onloading, vehicleChangeNum_ is changed to one of these
	// states.
	enum
	{
		VEHICLE_CHANGE_NUM_OLD,			// Cache not sync'ed with vehicle id.
		VEHICLE_CHANGE_NUM_HAS_VEHICLE,		// Cache sync'ed and has a vehicle
		VEHICLE_CHANGE_NUM_HAS_NO_VEHICLE	// Cache sync'ed and has no vehicle
	};
	typedef uint8 VehicleChangeNum;

	VehicleChangeNum vehicleChangeNum() const		{ return vehicleChangeNum_; }
	void vehicleChangeNum( VehicleChangeNum num )	{ vehicleChangeNum_ = num; }

	Priority priority() const;
	void priority( Priority newPriority );

	INLINE EntityID dummyID() const;
	INLINE void dummyID( EntityID dummyID );

	void lastEventNumber( EventNumber eventNumber );
	EventNumber lastEventNumber() const;

	void lastVolatileUpdateNumber( VolatileNumber number );
	VolatileNumber lastVolatileUpdateNumber() const;

	void detailLevel( DetailLevel detailLevel );
	DetailLevel detailLevel() const;

	IDAlias idAlias() const;
	void idAlias( IDAlias idAlias );

	void lodEventNumbers( EventNumber * pEventNumbers, int size );
	// EntityCache( const EntityCache & );

	void setEnterPending()		{ flags_ |= ENTER_PENDING; }
	void setRequestPending()	{ flags_ |= REQUEST_PENDING; }
	void setCreatePending()		{ flags_ |= CREATE_PENDING; }
	void setGone()				{ flags_ |= GONE; }

	void clearEnterPending()		{ flags_ &= ~ENTER_PENDING; }
	void clearRequestPending()		{ flags_ &= ~REQUEST_PENDING; }
	void clearCreatePending()		{ flags_ &= ~CREATE_PENDING; }
	void clearGone()				{ flags_ &= ~GONE; }

	bool isEnterPending() const		{ return (flags_ & ENTER_PENDING) != 0; }
	bool isRequestPending() const	{ return (flags_ & REQUEST_PENDING) != 0; }
	bool isCreatePending() const	{ return (flags_ & CREATE_PENDING) != 0; }
	bool isGone() const				{ return (flags_ & GONE) != 0; }

	bool isUpdatable() const		{ return (flags_ & NOT_UPDATABLE) == 0; }

private:
	typedef uint8 Flags;

	enum	// Flags bits
	{
		ENTER_PENDING	= 1 << 0, ///< Waiting to send enterAoI to client
		REQUEST_PENDING	= 1 << 1, ///< Expecting requestEntityUpdate from client
		CREATE_PENDING	= 1 << 2, ///< Waiting to send createEntity to client
		GONE            = 1 << 3, ///< Waiting to remove from priority queue

		/// We any of these are set, we shouldn't do a normal update in
		/// RealEntityWithWitnesses::update. (Actually, REQUEST_PENDING should
		/// never be set on something in the queue).
		NOT_UPDATABLE = ENTER_PENDING|REQUEST_PENDING|CREATE_PENDING|GONE,
	};

	void lodEventNumber( int level, EventNumber eventNumber );
	EventNumber lodEventNumber( int level ) const;

	EntityCache & operator=( const EntityCache & );

	void addChangedPropertiesToBundle( Mercury::Bundle & bundle,
		   bool needsToAddHeader );

	EntityConstPtr	pEntity_;
	Flags			flags_;	// TODO: Not good structure packing.

	VehicleChangeNum	vehicleChangeNum_;

	union
	{
		Priority	priority_;					// double
		EntityID	dummyID_;	// Only used if we have no entity.
	};

	EventNumber		lastEventNumber_;			// int32
	VolatileNumber	lastVolatileUpdateNumber_;	// uint16
	DetailLevel		detailLevel_;				// uint8
	IDAlias			idAlias_;					// uint8

	EventNumber		lodEventNumbers_[ MAX_LOD_LEVELS ];		// int32 * num lod levels

	friend BinaryIStream & operator>>( BinaryIStream & stream,
			EntityCache & entityCache );
	friend BinaryOStream & operator<<( BinaryOStream & stream,
			const EntityCache & entityCache );
};

inline
bool operator<( const EntityCache & left, const EntityCache & right )
{
	return left.pEntity() < right.pEntity();
}



/**
 *	This class is a map of entity caches
 */
class EntityCacheMap
{
public:
	~EntityCacheMap();

	EntityCache * add( const Entity & e );
	void del( EntityCache * ec );

	EntityCache * find( const Entity & e );
	EntityCache * find( EntityID id );

	uint32 size() const				{ return set_.size(); }

	void writeToStream( BinaryOStream & stream );

	static void addWatchers();

	typedef std::set< EntityCache > Implementor;

private:
	Implementor set_;

	/*
	static EntityCache * fromIterator( Implementor::iterator it );
	static Implementor::iterator toIterator( EntityCache * ec );
	*/
};

#ifdef CODE_INLINE
#include "entity_cache.ipp"
#endif

#endif // ENTITY_CACHE_HPP
