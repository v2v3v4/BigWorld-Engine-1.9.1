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
#include "user_data_object.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "chunk/user_data_object_link_data_type.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/guard.hpp"
#include "entitydef/user_data_object_description_map.hpp"
#include "pyscript/pyobject_base.hpp"
#include "resmgr/bwresource.hpp"
#include <algorithm>


DECLARE_DEBUG_COMPONENT( 0 );


#ifndef CODE_INLINE
#include "user_data_object.ipp"
#endif


namespace
{
	UserDataObjectTypePtr s_baseType;

	/*~ class BigWorld.UnresolvedUDORefException
	 *  This custom exception is thrown when trying to access a reference to a
	 *  UserDataObject that hasn't been loaded yet.
	 */
	PyObjectPtr s_udoRefException;


	/**
	 *	This is a helper class to release the static resources allocated by the
	 *	UserDataObject class.
	 */
	class UserDataObjectReferenceIniter : public Script::FiniTimeJob
	{
	public:
		virtual void fini()
		{
			s_udoRefException = NULL;
		}
	};

	UserDataObjectReferenceIniter userDataObjectReferenceIniter;
}


int PyUserDataObject_token = 1;
// Not sure where or how these are defined, so they are here for now...
PY_BASETYPEOBJECT( UserDataObject )
PY_BEGIN_METHODS( UserDataObject )
PY_END_METHODS()
PY_BEGIN_ATTRIBUTES( UserDataObject )

/*~ attribute UserDataObject guid
	*  @components{ cell }
	*  This is the unique identifier for the UserDataObject. It is common across client,
	*  cell, and base applications.
	*  @type Read-only GUID (string)
	*/
	PY_ATTRIBUTE( guid )

	/*~ attribute UserDataObject position
	*  @components{ cell }
	*  The position attribute is the current location of the UserDataObject, in world
	*  coordinates.
	*  @type Read only Tuple of 3 floats as (x, y, z)
	*/
	PY_ATTRIBUTE( position )

	/*~ attribute UserDataObject yaw
	*  @components{ cell }
	*  yaw provides the yaw component of the direction attribute. It is the yaw
	*  of the current facing direction of the UserDataObject, in radians and in world
	*  space.
	*  This can be set via the direction attribute.
	*  @type Read-only Float
	*/
	PY_ATTRIBUTE( yaw )

	/*~ attribute UserDataObject pitch
	*  @components{ cell }
	*  pitch provides the pitch component of the direction attribute. It is
	*  the pitch of the current facing direction of the UserDataObject, in radians
	*  and in world space.
	*  This can be set via the direction attribute.
	*  @type Read-only Float
	*/
	PY_ATTRIBUTE( pitch )

	/*~ attribute UserDataObject roll
	*  @components{ cell }
	*  roll provides the roll component of the direction attribute. It is the
	*  roll of the current facing direction of the UserDataObject, in radians and in
	*  world space.
	*  This can be set via the direction attribute.
	*  @type Read-only Float
	*/
	PY_ATTRIBUTE( roll )
	/*~ attribute UserDataObject direction
	*  @components{ cell }
	*  The direction attribute is the current orientation of the UserDataObject, in
	*  world space.
	*  This property is roll, pitch and yaw combined (in that order).
	*  @type Read only Tuple of 3 floats as (roll, pitch, yaw)
	*/
	PY_ATTRIBUTE( direction )

PY_END_ATTRIBUTES()


// -----------------------------------------------------------------------------
// Section: UserDataObject
// -----------------------------------------------------------------------------

/*static*/ UserDataObject::UdoMap UserDataObject::s_created_;

/**
 *	This static method returns a UDO by id if it has been created, or NULL
 *  otherwise.
 *
 *	@param guid		Unique id of the desired UDO.
 *	@return			UDO corresponding to the id, or NULL if the UDO with that
 *					id hasn't been loaded yet.
 */
/*static*/ UserDataObject* UserDataObject::get( const UniqueID& guid )
{
	BW_GUARD;
	UdoMap::iterator it = s_created_.find( guid );
	if ( it != s_created_.end() )
		return (*it).second;

	return NULL;
}


/**
 *  This static method can do two things. If a UDO corresponding to the id in
 *	'initData' has already been created as a not-yet-loaded UDO (i.e. created
 *  via 'createRef'), calling this method will finish loading it. If a UDO with
 *	that id hasn't been created yet, this method returns a new, fully created
 *	UDO. Finally, if there is already a fully-created UDO with this id, an
 *	error occurs.
 *
 *	@param initData		Information used in the creation of the UDO
 *	@param type			Python type of the UDO
 *	@return				New or recreated UDO, or NULL if the UDO already exists
 */
/*static*/ UserDataObject* UserDataObject::build(
	const UserDataObjectInitData& initData, UserDataObjectTypePtr type )
{
	BW_GUARD;
	UserDataObject* result = UserDataObject::get( initData.guid );
	if ( result && result->isReady() )
	{
		CRITICAL_MSG( "UserDataObject::build: "
				   		"object %s has already been built.\n",
			   initData.guid.toString().c_str() );
		return NULL;
	}

	if ( result == NULL )
	{
		// Not created yet, so create it.
		result = type->newUserDataObject( initData );
	}
	else if ( type )
	{
		// Reuse the UDO object if it has already been created in an
		// unloaded state, using the new type.
		result->resetType( type );
		result->init( initData );
		Py_INCREF( result );
	}

	return result;
}


/**
 *	This method resets a UDO back to the unloaded state, clearing also its
 *	dicctionary, so links to other UDOs are broken, preventing leaks caused by
 *	circular references, etc.
 *	
 */
void UserDataObject::unbuild()
{
	BW_GUARD;

	// Only reset ourselves if we were fully built.
	if (isReady())
	{
		// reset to the unloaded state
		createRefType();

		if (s_baseType != NULL)
		{
			if (PyObject_DelAttrString( this, "__dict__" ) == -1)
			{
				WARNING_MSG( "UserDataObject::init: could not delete __dict__"
					" for user data object guid: %s\n",
					guid_.toString().c_str() );
				PyErr_Print();
			}

			resetType( s_baseType );

			propsLoaded_ = false;
		}
	}
}


/**
 *	This static method creates a UDO in an unloaded state, which is called a
 *  reference, that will be properly loaded at a later time when the chunk it
 *  lives in is loaded. This is used for links.
 *
 *  @param guid		String containing the unique id of the UDO to create.
 *  @return			New UDO reference.
 */
/*static*/ UserDataObject* UserDataObject::createRef( const std::string& guid )
{
	BW_GUARD;
	if ( guid.empty() )
		return NULL;
	
	return UserDataObject::createRef( UniqueID( guid ) );
}


/**
 *	This static method creates a UDO in an unloaded state, which is called a
 *  reference, that will be properly loaded at a later time when the chunk it
 *  lives in is loaded. This is used for links.
 *
 *  @param guid		Unique identifier of the UDO to create.
 *  @return			New UDO reference.
 */
/*static*/ UserDataObject* UserDataObject::createRef( const UniqueID& guid )
{
	BW_GUARD;
	UserDataObject* udo = UserDataObject::get( guid );
	if (udo == NULL)
	{
		createRefType();

		if (s_baseType != NULL)
		{
			UserDataObjectInitData initData;
			initData.guid = guid;
			udo = s_baseType->newUserDataObject( initData );
		}
	}
	else
	{
		Py_INCREF( udo );
	}
	return udo;
}


/**
 *	This static method creates the base type for a UserDataObjectRef type,
 *  which is used in UDOs when they are in an unloaded state. And example of
 *  a UDO in this state would be a UDO_REF property in a UDO that points to a
 *	UDO in an unloaded chunk.
 */
/*static*/ void UserDataObject::createRefType()
{
	BW_GUARD;

	static bool firstTime = true;

	if (firstTime)
	{
		firstTime = false;

		// Initialise our custom exception
		PyObject* module = PyImport_AddModule( "BigWorld" );
		s_udoRefException = PyErr_NewException(
			const_cast<char *>("BigWorld.UnresolvedUDORefException"), NULL, NULL );
		// Since 's_udoRefException' is a smart pointer that
		// incremented the reference, so its safe for
		// PyModule_AddObject to steal the reference.
		PyModule_AddObject( module, "UnresolvedUDORefException",
			s_udoRefException.getObject() );

		// Initialise the base user data object reference.
		PyObjectPtr pModule(
			PyImport_ImportModule( "UserDataObjectRef" ),
			PyObjectPtr::STEAL_REFERENCE );
		if (!pModule)
		{
			ERROR_MSG( "UserDataObjectLinkDataType::createUserDataObject: "
				"Could not load module UserDataObjectRef\n");
			PyErr_Print();
			return;
		}

		PyObjectPtr pClass(
			PyObject_GetAttrString( pModule.getObject(), "UserDataObjectRef" ),
			PyObjectPtr::STEAL_REFERENCE );

		if (!pModule)
		{
			ERROR_MSG( "UserDataObjectLinkDataType::createUserDataObject: "
				"Could not get base class UserDataObjectRef\n" );
			PyErr_Print();
			return;
		}

		s_baseType =
			new UserDataObjectType(
				UserDataObjectDescription(),
				(PyTypeObject*)pClass.getObject() );
	}
}


/**
 *	The constructor for UserDataObject.
 *
 */
UserDataObject::UserDataObject( UserDataObjectTypePtr pUserDataObjectType):
		PyInstancePlus( pUserDataObjectType->pPyType(), true ),
		pUserDataObjectType_( pUserDataObjectType)
{
	propsLoaded_ = false;
}


/**
 *	Destructor
 */
UserDataObject::~UserDataObject()
{
	BW_GUARD;
	UdoMap::iterator it = s_created_.find( guid_ );
	if ( it != s_created_.end() )
		s_created_.erase( it );
}


/** Initiator method
 *  Set the properties that can fail here
 */
void UserDataObject::init(const UserDataObjectInitData& initData)
{
	BW_GUARD;
	IF_NOT_MF_ASSERT_DEV( !this->isReady() )
	{
		return;
	}

	IF_NOT_MF_ASSERT_DEV( !initData.guid.toString().empty() )
	{
		return;
	}

	s_created_[ initData.guid ] = this;

	if ( initData.propertiesDS == NULL )
	{
		// Creating an empty UDO of the default type, so add it to the list of
		// uninitialised objects.
		guid_ = initData.guid;
		return;
	}

	if (!isValidPosition( initData.position ))
	{
		ERROR_MSG( "UserDataObject::setPositionAndDirection: "
				   		"(%f,%f,%f) is not a valid position for entity %s\n",
			   initData.position.x, initData.position.y, initData.position.z,
			   guid_.toString().c_str() );
		return;
	}
	guid_= initData.guid;
	globalPosition_ = initData.position;
	globalDirection_ = initData.direction;
	PyObject* pDict = PyDict_New();
	pUserDataObjectType_->description().addToDictionary( initData.propertiesDS, pDict );
	//set the __dict__ property of myself to be this pDict
	if	( PyObject_SetAttrString( this, "__dict__", pDict ) == -1 ) {
		WARNING_MSG("UserDataObject::init: could not set __dict__ for user data object guid:%s\n",
				guid_.toString().c_str() );
		PyErr_Print();
	}
	Py_DECREF(pDict);
	propsLoaded_ = true;
	// Now call the python init method */
	this->callScriptInit();
}


/**
 *  This method returns true if the UDO has been fully loaded and is ready to
 *  be used.
 */
bool UserDataObject::isReady() const
{
	return propsLoaded_;
}


/* Call the init method in the python script. */
void UserDataObject::callScriptInit()
{
	BW_GUARD;
	// Call the __init__ method of the object, if it has one.
	PyObject * pFunction = PyObject_GetAttrString( this,
		const_cast< char * >( "__init__" ) );

	if (pFunction != NULL)
	{
		PyObject * pResult = PyObject_CallFunction( pFunction, const_cast<char *>("()") );
		PY_ERROR_CHECK()
		Py_XDECREF( pResult );
		Py_DECREF( pFunction );
	}
	else
	{
		PyErr_Clear();
	}
}

/**
 *	This method resets our type object, e.g. after a reloadScript() operation.
 */
void UserDataObject::resetType( UserDataObjectTypePtr pNewType )
{
	BW_GUARD;
	IF_NOT_MF_ASSERT_DEV( pNewType )
	{
		return;
	}

	pUserDataObjectType_ = pNewType;
	if (PyObject_SetAttrString( this, "__class__",
		reinterpret_cast< PyObject* >( pUserDataObjectType_->pPyType() ) ) == -1)
	{
		ERROR_MSG( "UserDataObject::resetType: "
			"Failed to update __class__ for %s to %s.\n",
			guid_.toString().c_str(),
			pNewType->description().name().c_str() );
		PyErr_Print();
	}
}

/**
 *	This method is responsible for getting script attributes associated with
 *	this object.
 */
PyObject * UserDataObject::pyGetAttribute( const char * attr )
{
	BW_GUARD;
	if ( attr[0] != '_' && strcmp( attr, "guid" ) != 0 && !isReady() )
	{
		IF_NOT_MF_ASSERT_DEV( s_udoRefException != NULL )
		{
			PyErr_Format( PyExc_AttributeError, "Cannot access attribute '%s' in UserDataObject '%s', not loaded and no exception set.",
						attr, guid_.toString().c_str() );
			return NULL;
		}

		// Only allow getting the 'guid' when the UDO hasn't been loaded
		std::string excstr =
			"Cannot access attribute '" + std::string(attr) +
			"' in UserDataObject " + guid_.toString() +
			", it has not been loaded yet.";
		PyErr_SetString(
			s_udoRefException.getObject(),
			excstr.c_str() );
		return NULL;
	}

	// Check through our ordinary methods and attributes
	PY_GETATTR_STD();
	// finally let the base class have the scraps (ephemeral props, etc.)
	return PyInstancePlus::pyGetAttribute( attr );
}


/**
 *	This method is responsible for setting script attributes associated with
 *	this object.
 *  Therefore in this method we search to see if we have a description available for the
 *  property, if not we allow it to be changed. This allows python scripts to have temporary
 *  scratchpad variables. However they will not be retained if the chunk is unloaded
 *  or the server is shut down.
*/
int UserDataObject::pySetAttribute( const char * attr, PyObject * value )
{
	BW_GUARD;
	// See if it's one of our standard attributes
	PY_SETATTR_STD();

	// If all my properties have been loaded do not allow them to change
	if (propsLoaded_){
		bool hasProperty = pUserDataObjectType_->hasProperty( attr );
		if (hasProperty == true)
		{
			PyErr_Format( PyExc_AttributeError,
				"UserDataObject.%s is a persistent UserDataObject"
				" property and cannot be changed", attr);
			return -1;
		}
	}

	// Don't support changing properties other than the required built-in ones
	if (attr[0] != '_')
	{
		WARNING_MSG( "UserDataObject::pySetAttribute: Changing User Data "
				"Object attributes is not supported (type: %s, guid: %s, "
				"attribute: %s)\n",
				pUserDataObjectType_->description().name().c_str(),
				guid_.toString().c_str(),
				attr );
	}

	return this->PyInstancePlus::pySetAttribute( attr, value );
}

// user_data_object.cpp
