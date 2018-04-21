/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WITNESS_HPP
#define WITNESS_HPP

#include "real_entity.hpp"
#include "entity_cache.hpp"

class AoITrigger;

#define PY_METHOD_ATTRIBUTE_WITNESS_WITH_DOC( METHOD_NAME, DOC_STRING )		\
	PyObject * get_##METHOD_NAME()											\
	{																		\
		return new WitnessMethod( this, &_##METHOD_NAME );					\
	}																		\


/**
 *	This class is the Python object for methods in a Witness
 */
class WitnessMethod : public PyObjectPlus
{
	Py_Header( WitnessMethod, PyObjectPlus )

public:
	typedef PyObject * (*StaticGlue)(
		PyObject * pWitness, PyObject * args, PyObject * kwargs );

	WitnessMethod( Witness * w, StaticGlue glueFn,
		PyTypePlus * pType = &s_type_ );

	PY_KEYWORD_METHOD_DECLARE( pyCall )

private:
	EntityPtr	pEntity_;
	StaticGlue	glueFn_;
};

#undef PY_METHOD_ATTRIBUTE_WITH_DOC
#define PY_METHOD_ATTRIBUTE_WITH_DOC PY_METHOD_ATTRIBUTE_WITNESS_WITH_DOC


/**
 *	This class is a witness to the movements and perceptions of a RealEntity.
 *	It is created when a client is attached to this entity. Its main activity
 *	centres around the management of an Area of Interest list.
 */
class Witness : public Updatable
{
	PY_FAKE_PYOBJECTPLUS_BASE_DECLARE()
	Py_FakeHeader( Witness, PyObjectPlus )

public:
	// Creation/Destruction
	Witness( RealEntity & owner, BinaryIStream & data,
			CreateRealInfo createRealInfo, bool hasChangedSpace = false );
	virtual ~Witness();
private:
	void init();
public:
	RealEntity & real()					{ return real_; }
	const RealEntity & real() const		{ return real_; }

	Entity & entity()					{ return entity_; }
	const Entity & entity() const		{ return entity_; }


	// Ex-overrides from RealEntity
	void writeOffloadData( BinaryOStream & data,
			const Mercury::Address & dstAddr );
	void writeBackupData( BinaryOStream & data );

	bool sendToClient( int entityMessageType, MemoryOStream & stream );
	void sendToProxy( int mercuryMessageType, MemoryOStream & stream );

	void setWitnessCapacity( EntityID id, int bps );

	void requestEntityUpdate( EntityID id,
			EventNumber * pEventNumbers, int size );

	void addToAoI( Entity * pEntity );
	void removeFromAoI( Entity * pEntity );

	void newPosition( const Vector3 & position );

	void updateReferencePosition( uint8 seqNum );
	void cancelReferencePosition();
	void dumpAoI();
	void debugDump();

	// Misc
	void update();

	// Python
	virtual PyObject * pyGetAttribute( const char * attr );
	virtual int pySetAttribute( const char * attr, PyObject * value );

	virtual PyObject * pyAdditionalMembers( PyObject * pSeq ) { return pSeq; }
	virtual PyObject * pyAdditionalMethods( PyObject * pSeq ) { return pSeq; }

	PY_RW_ATTRIBUTE_DECLARE( maxPacketSize_, bandwidthPerUpdate );

	PY_RW_ATTRIBUTE_DECLARE( stealthFactor_, stealthFactor )

	void unitTest();
	PY_AUTO_METHOD_DECLARE( RETVOID, unitTest, END )

	PY_AUTO_METHOD_DECLARE( RETVOID, dumpAoI, END )

	void setAoIRadius( float radius, float hyst = 5.f );
	PY_AUTO_METHOD_DECLARE( RETVOID, setAoIRadius,
		ARG( float, OPTARG( float, 5.f, END ) ) )

	PY_RW_ATTRIBUTE_DECLARE( aoiUpdates_, enableAoICallbacks )

	static void addWatchers();

private:

	typedef std::vector< EntityCache * > KnownEntityQueue;


	// Private methods
	void addToSeen( EntityCache * pCache );
	void deleteFromSeen( Mercury::Bundle & bundle,
			KnownEntityQueue::iterator iter,
			EntityID id = 0 );

	void sendEnter( Mercury::Bundle & bundle, EntityCache * pCache );
	void sendCreate( Mercury::Bundle & bundle, EntityCache * pCache );

	void sendGameTime( );

	void onLeaveAoI( EntityCache * pCache, EntityID id );

	Mercury::Bundle & bundle();
	void sendToClient();

	IDAlias allocateIDAlias( const Entity & entity );

	static bool priorityCompare( const EntityCache * l, const EntityCache * r )
					{ return l->priority() > r->priority(); }

	void calculateReferencePosition();

	// variables

	RealEntity		& real_;
	Entity			& entity_;


	TimeStamp		noiseCheckTime_;
	TimeStamp		noisePropagatedTime_;
	bool			noiseMade_;

	uint32			maxPacketSize_;

	KnownEntityQueue	entityQueue_;
	EntityCacheMap		aoiMap_;

	float stealthFactor_;

	float aoiHyst_;
	float aoiRadius_;
	bool aoiUpdates_;

	uint32 bandwidthDeficit_;

	IDAlias freeAliases_[256];
	int		numFreeAliases_;

	// This is used as a reference for shorthand positions sent as 3 uint8's
	// relative to this reference position.  (see also RelPosRef). It is used
	// to reduce bandwidth.
	Vector3 referencePosition_;
	// This is the sequence number of the relative position reference sent
	// from the client.
	uint8 	referenceSeqNum_;
	bool hasReferencePosition_;

	// This is the first spaceData sequence we have not sent to the client
	// When the client is up-to-date, this will equal pSpace->endDataSeq();
	int32 knownSpaceDataSeq_;

	uint32 allSpacesDataChangeSeq_;

	AoITrigger * pAoITrigger_;

	friend BinaryIStream & operator>>( BinaryIStream & stream,
			EntityCache & entityCache );
	friend BinaryOStream & operator<<( BinaryOStream & stream,
			const EntityCache & entityCache );
};

#undef PY_METHOD_ATTRIBUTE_WITH_DOC
#define PY_METHOD_ATTRIBUTE_WITH_DOC PY_METHOD_ATTRIBUTE_BASE_WITH_DOC

#ifdef CODE_INLINE
#include "witness.ipp"
#endif

#endif // WITNESS_HPP
