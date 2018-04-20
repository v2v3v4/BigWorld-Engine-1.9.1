/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef USER_DATA_OBJECT_HPP
#define USER_DATA_OBJECT_HPP

#include "Python.h"

#include "pyscript/pyobject_plus.hpp"
#include "network/mercury.hpp"
#include "network/msgtypes.hpp"
#include "user_data_object_type.hpp"
#include "math/matrix.hpp"
#include "cstdmf/time_queue.hpp"
#include "cstdmf/unique_id.hpp"
#include "cstdmf/guard.hpp"
#include "pyscript/script.hpp"
#include <set>
#include <vector>

// Forward declarations
class Chunk;
class ChunkSpace;
class UserDataObject;
class UserDataObjectType;
class Space;
class MemoryOStream;

typedef SmartPointer< UserDataObject >					UserDataObjectPtr;
typedef SmartPointer< PyObject >						PyObjectPtr;
typedef SmartPointer<ChunkSpace>						ChunkSpacePtr;
typedef std::set< UserDataObjectPtr >					UserDataObjectSet;
typedef std::map< UniqueID, UserDataObjectPtr >			UserDataObjectMap;
typedef struct UserDataObjectInitData{
	UniqueID guid;
	Position3D position;
	Direction3D direction;
	DataSectionPtr propertiesDS;
} UserDataObjectInitData;


/*~ class BigWorld.UserDataObject
 *  @components{ cell }
 *  A user data object. Defined by DEF files.
 *
 */
class UserDataObject : public PyInstancePlus
{
	Py_InstanceHeader( UserDataObject )

public:
	// Preventing NaN's getting through, hopefully
	static bool isValidPosition( const Coord &c )
	{
		const float MAX_ENTITY_POS = 1000000000.f;
		return (-MAX_ENTITY_POS < c.x_ && c.x_ < MAX_ENTITY_POS &&
			-MAX_ENTITY_POS < c.z_ && c.z_ < MAX_ENTITY_POS);
	}

	static UserDataObject* get( const UniqueID& guid );

	static UserDataObject* build(
		const UserDataObjectInitData& initData, UserDataObjectTypePtr type );

	void unbuild();

	static UserDataObject* createRef( const std::string& guid );
	static UserDataObject* createRef( const UniqueID& guid );
	static void createRefType();

	/// @name Construction and Destruction
	//@{
	UserDataObject( UserDataObjectTypePtr pUserDataObjectType );
	~UserDataObject();

	//@}

	// Full initialisation
	void init( const UserDataObjectInitData& initData );

	bool isReady() const;

	/// @name Accessors
	//@{
	const UniqueID& guid() const;
	const Position3D & position() const;
	const Direction3D & direction() const;
	// DEBUG
	//@}

	const UserDataObjectType& getType() const
	{ return *pUserDataObjectType_.getObject(); }
	void resetType( UserDataObjectTypePtr pNewType );

private:
	UserDataObject( const UserDataObject & );
	/// @name Script related methods
	//@{
	PY_RO_ATTRIBUTE_DECLARE( guid_, guid )
	PY_RO_ATTRIBUTE_DECLARE( (Vector3&) globalDirection_, direction )
	PY_RO_ATTRIBUTE_DECLARE( (Vector3&) globalPosition_, position)
	PY_RO_ATTRIBUTE_DECLARE( globalDirection_.yaw, yaw )
	PY_RO_ATTRIBUTE_DECLARE( globalDirection_.pitch, pitch )
	PY_RO_ATTRIBUTE_DECLARE( globalDirection_.roll, roll )

	PyObject * pyGet_spaceID();
	PY_RO_ATTRIBUTE_SET( spaceID )

	void callScriptInit();

	void setGlobalPosition( const Vector3 & v );
	PyObject * pyGetAttribute( const char * attr );
	int pySetAttribute( const char * attr, PyObject * value );
	//@}

	// Private data
	UserDataObjectTypePtr	pUserDataObjectType_;

	typedef std::map<UniqueID,UserDataObject*> UdoMap;
	static UdoMap s_created_;

	UniqueID		guid_;
	Position3D		globalPosition_;
	Direction3D		globalDirection_;
	ChunkSpacePtr	pSpace_;
	bool			propsLoaded_;
};

#ifdef CODE_INLINE
#include "user_data_object.ipp"
#endif

#endif // CUSTOM_CHUNK_ITEM_HPP
