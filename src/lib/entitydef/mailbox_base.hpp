/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MAILBOX_BASE_HPP
#define MAILBOX_BASE_HPP

#include "network/basictypes.hpp"
#include "cstdmf/binary_stream.hpp"

#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"

class MethodDescription;
class EntityType;

/**
 *	This class is used to represent a destination of an entity that messages
 *	can be sent to.
 *
 *	Its virtual methods are implemented differently on each component.
 */
class PyEntityMailBox: public PyObjectPlus
{
	Py_Header( PyEntityMailBox, PyObjectPlus )

public:
	PyEntityMailBox( PyTypePlus * pType = &PyEntityMailBox::s_type_ ) :
		PyObjectPlus(pType)
	{}
	virtual ~PyEntityMailBox() {}

	virtual PyObject * pyGetAttribute( const char * attr );

	PyObject * pyRepr();

	virtual const MethodDescription * findMethod( const char * attr ) const = 0;

	virtual BinaryOStream * getStream( const MethodDescription & desc ) = 0;
	virtual void sendStream() = 0;
	// This method is used to return values from remote methods back to 
	// the caller
	// Serving suggestion: subclasses should override this to provide an object 
	// created from a BlockingReplyHandler for a Mercury request created
	// (presumably) in sendStream()
	virtual PyObject* returnValue() { Py_Return; }

	static PyObject * constructFromRef( const EntityMailBoxRef & ref );
	static bool reducibleToRef( PyObject * pObject );
	static EntityMailBoxRef reduceToRef( PyObject * pObject );

	typedef PyObject * (*FactoryFn)( const EntityMailBoxRef & ref );
	static void registerMailBoxComponentFactory(
		EntityMailBoxRef::Component c, FactoryFn fn,
		PyTypeObject * pType );

	typedef bool (*CheckFn)( PyObject * pObject );
	typedef EntityMailBoxRef (*ExtractFn)( PyObject * pObject );
	static void registerMailBoxRefEquivalent( CheckFn cf, ExtractFn ef );

	PY_PICKLING_METHOD_DECLARE( MailBox )
};

PY_SCRIPT_CONVERTERS_DECLARE( PyEntityMailBox )

/**
 *  This class implements a simple helper Python type. Objects of this type are
 *  used to represent methods that the base can call on another script object
 *  (e.g. the cell, the client, or another base).
 */
class RemoteEntityMethod : public PyObjectPlus
{
	Py_Header( RemoteEntityMethod, PyObjectPlus )

	public:
		RemoteEntityMethod( PyEntityMailBox * pMailBox,
				const MethodDescription * pMethodDescription,
				PyTypePlus * pType = &s_type_ ) :
			PyObjectPlus( pType ),
			pMailBox_( pMailBox ),
			pMethodDescription_( pMethodDescription )
		{
		}
		~RemoteEntityMethod() { }

		PY_METHOD_DECLARE( pyCall )

	private:
		SmartPointer<PyEntityMailBox> pMailBox_;
		const MethodDescription * pMethodDescription_;
};

namespace Script
{
	int setData( PyObject * pObj, EntityMailBoxRef & mbr,
		const char * varName = "" );
	PyObject * getData( const EntityMailBoxRef & mbr );
};

#endif // MAILBOX_BASE_HPP
