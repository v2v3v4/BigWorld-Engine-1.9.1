/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ENTITY_TYPE_HPP
#define ENTITY_TYPE_HPP

#include <Python.h>

#include <string>
#include <map>

#include "entitydef/entity_description.hpp"
#include "entitydef/method_description.hpp"
#include "network/basictypes.hpp"

class RealEntity;
class Entity;
class BinaryIStream;
class BinaryOStream;

enum CreateRealInfo
{
	CREATE_REAL_FROM_INIT,
	CREATE_REAL_FROM_OFFLOAD,
	CREATE_REAL_FROM_RESTORE
};


typedef SmartPointer<Entity> EntityPtr;

class EntityType;
typedef SmartPointer<EntityType> EntityTypePtr;
typedef std::vector< EntityTypePtr > EntityTypes;

/**
 *	This class is used to represent an entity type.
 */
class EntityType : public ReferenceCount
{
public:
	EntityType( const EntityDescription& entityDescription,
			PyTypeObject * pType );

	~EntityType();

	Entity * newEntity() const;

	const VolatileInfo & volatileInfo() const;

	DataDescription * description( const char * attr ) const;
	const EntityDescription & description() const;

	uint propCountGhost() const						{ return propCountGhost_; }
	uint propCountGhostPlusReal() const				{ return propDescs_.size();}
	DataDescription * propIndex( int index ) const	{ return propDescs_[index];}

	uint propCountClientServer() const
	{
		return entityDescription_.clientServerPropertyCount();
	}

	PyTypeObject * pPyType() const 	{ return pPyType_; }
	void setPyType( PyTypeObject * pPyType );

	const char * name() const;

	bool hasBaseScript() const  { return entityDescription_.hasBaseScript(); }
	bool hasCellScript() const  { return entityDescription_.hasCellScript(); }

	EntityTypeID typeID() const			{ return entityDescription_.index(); }
	EntityTypeID clientTypeID() const;

	void addDataToStream( Entity * pEntity,
			BinaryOStream & stream,
			EntityDataFlags dataType );


	EntityTypePtr old() const	{ return pOldSelf_; }
	void old( EntityTypePtr pOldType );

	/// @name static methods
	//@{
	static bool init( bool isReload = false );
	static bool reloadScript( bool isRecover = false );
	static void migrate( bool isFullReload = true );
	static void cleanupAfterReload( bool isFullReload = true );

	static void clearStatics();

	static EntityTypePtr getType( EntityTypeID typeID );
	static EntityTypePtr getType( const char * className );

	static EntityTypes& getTypes();
	//@}

#if ENABLE_WATCHERS
	static WatcherPtr pWatcher();

	void countSentToOwnClient(int bytes) {sentToOwnClient_ += bytes;}
	void countNonVolatileSentToOtherClients(int bytes) 
	{nonVolatileSentToOtherClients_ += bytes;}
	void countVolatileSentToOtherClients(int bytes) 
	{volatileSentToOtherClients_ += bytes;}
	void countAddedToHistoryQueue(int bytes) {addedToHistoryQueue_ += bytes;}
	void countSentToGhosts(int bytes) {sentToGhosts_ += bytes;}
	void countSentToBase(int bytes) {sentToBase_ += bytes;}
#endif
private:
	EntityDescription entityDescription_;
	PyTypeObject * pPyType_;

	uint							propCountGhost_;
	std::vector<DataDescription*>	propDescs_;

	EntityTypePtr	pOldSelf_;

	static EntityTypes s_curTypes_, s_newTypes_;
	static StringHashMap< EntityTypeID > s_nameToIndexMap_;

	static PyObject * s_pInitModules_, * s_pNewModules_;
#if ENABLE_WATCHERS
	uint32 sentToOwnClient_;
	uint32 nonVolatileSentToOtherClients_;
	uint32 volatileSentToOtherClients_;
	uint32 addedToHistoryQueue_;
	uint32 sentToGhosts_;
	uint32 sentToBase_;
#endif
};

typedef SmartPointer<EntityType> EntityTypePtr;


#ifdef CODE_INLINE
#include "entity_type.ipp"
#endif

#endif // ENTITY_TYPE_HPP

// entity_type.hpp
