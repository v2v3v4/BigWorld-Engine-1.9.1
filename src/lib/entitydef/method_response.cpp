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

#include "Python.h"

#include "method_response.hpp"

#include "network/bundle.hpp"
#include "network/nub.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

// -----------------------------------------------------------------------------
// Section: BlockingResponseHandler
// -----------------------------------------------------------------------------

void BlockingResponseHandler::handleMessage( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * args )
{

	// in case there's already one
	Py_XDECREF( returnValueDict_ );

	returnValueDict_ = PyDict_New();

	int numReturnValues = methodDesc_.returnValues();
	for (int i = 0; i < numReturnValues; ++i)
	{
		DataTypePtr type = methodDesc_.returnValueType( i );
		std::string name = methodDesc_.returnValueName( i );

		PyObjectPtr value = type->createFromStream( data, false );
		PyDict_SetItemString( returnValueDict_ , name.c_str(),
			value.getObject() );
	}

	err_ = Mercury::REASON_SUCCESS;
	nub_.breakProcessing();
	done_ = true;
}

void BlockingResponseHandler::handleException(
		const Mercury::NubException& exception,
		void* args)
{
	ERROR_MSG( "BlockingResponseHandler::handleException: %s\n",
		Mercury::reasonToString( exception.reason() ) );

	err_ = exception.reason();

	nub_.breakProcessing();
	done_ = true;

}


void BlockingResponseHandler::await()
{
	bool wasBroken = nub_.processingBroken();

	while (!done_)
	{
		try
		{
			nub_.processContinuously();
			nub_.processUntilChannelsEmpty();
		}
		catch (Mercury::NubException & ne)
		{
			ERROR_MSG( "BlockingResponseHandler::await: caught exception: %s\n",
				Mercury::reasonToString( ne.reason() ) );
			err_ = ne.reason();
			done_ = true;
		}
	}
	nub_.breakProcessing( wasBroken );
}

// -----------------------------------------------------------------------------
// Section: MethodResponse
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( MethodResponse )

PY_BEGIN_METHODS( MethodResponse )
	PY_METHOD( done )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( MethodResponse )
	PY_ATTRIBUTE( id )
PY_END_ATTRIBUTES()

PY_SCRIPT_CONVERTERS( MethodResponse )

/**
 *	Constructor.
 *
 *	@param	replyID the reply ID
 *	@param replyAddr the reply address
 *	@param nub the nub to listen for a reply on
 *	@param methodDesc The description of the method.
 */
MethodResponse::MethodResponse( int replyID,
		const Mercury::Address & replyAddr,
		Mercury::Nub & nub,
		const MethodDescription & methodDesc ):
	PyObjectPlus( &MethodResponse::s_type_, false ),
	replyID_( replyID ),
	replyAddr_( replyAddr ),
	nub_( nub ),
	methodDesc_( methodDesc ),
	returnValueData_()
{
	uint numReturnValues = methodDesc.returnValues();

	for (uint i = 0 ; i < numReturnValues; ++i)
	{
		DataTypePtr pDataType = methodDesc.returnValueType( i );
		if (!pDataType.hasObject())
		{
			ERROR_MSG( "Could not get return value for i=%d\n", i );
			returnValueData_.clear();
			return;
		}

		// add default values
		PyObjectPtr pDefaultValue =
			methodDesc.returnValueType( i )->pDefaultValue();

		if (!pDefaultValue.hasObject())
		{
			ERROR_MSG( "Could not get default value for "
				"return value %i: data type=%s\n",
				i, pDataType->typeName().c_str() );
			returnValueData_.clear();
			return;
		}

		Py_INCREF( pDefaultValue.getObject() );
		returnValueData_[methodDesc.returnValueName( i )] = pDefaultValue;
	}

	// there may be situations where the number of returnValueData elements is
	// not equal to the number reported by methodDesc - ie if two return values
	// have the same name - should check for this
	if (numReturnValues != returnValueData_.size())
	{
		ERROR_MSG( "MethodResponse::MethodResponse(): "
			"Method description reports %d return values, but %zu "
			"value data objects are present",
			numReturnValues, returnValueData_.size() );
	}

}


/**
 *	Destructor.
 */
MethodResponse::~MethodResponse()
{
	ReturnValueData::iterator iter;
	while ((iter = returnValueData_.begin()) != returnValueData_.end())
	{
		Py_DECREF( iter->second.getObject() );
		returnValueData_.erase( iter );
	}
}


/**
 *	This method overrides the PyObjectPlus::pyGetAttribute method.
 *
 *	@param	attr	the attribute name
 *	@return	the Python object, or NULL otherwise (with an exception thrown)
 */
PyObject * MethodResponse::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	// see if we have it
	PyObjectPtr obj = returnValueData( std::string( attr ) );
	if (obj.getObject())
	{
		Py_INCREF( obj.getObject());
		return obj.getObject();
	}

	return PyObjectPlus::pyGetAttribute( attr );
}


/**
 *	This method overrides the PyObjectPlus::pySetAttribute method.
 *
 *	@param	attr	the attribute name
 *	@param	value	the python object to set to
 *
 *	@return	0 on success, nonzero on failure
 */
int MethodResponse::pySetAttribute( const char* attr, PyObject* value )
{
	// See if it's one of our standard attributes
	PY_SETATTR_STD();

	// see if we have it
	MethodResponse::ReturnValueData::iterator iter =
		returnValueData_.find( std::string( attr ) );

	if (iter != returnValueData_.end())
	{
		PyObject* oldValueObj = iter->second.getObject();
		Py_INCREF( value );
		iter->second = PyObjectPtr( value );
		Py_DECREF( oldValueObj );
		return 0;
	}

	return PyObjectPlus::pySetAttribute( attr, value );
}

/**
 *	Returns a reference to the value for attribute identified by name,
 *	or NULL if no such attribute exists.
 *
 *	@param name the return value name
 */
PyObjectPtr MethodResponse::returnValueData( const std::string& name )
{
	return returnValueData_[name];
}


/**
 *	MethodResponse.done()
 */
PyObject* MethodResponse::py_done( PyObject* args )
{
	// create a reply bundle
	Mercury::Bundle b;
	b.startReply( replyID_ );

	int numReturnValues = methodDesc_.returnValues();

	// stream each return value's..err.. value onto the bundle
	for (int i = 0; i < numReturnValues; ++i)
	{
		DataTypePtr type = methodDesc_.returnValueType( i );
		PyObjectPtr value = returnValueData_[methodDesc_.returnValueName( i )];

		type->addToStream( value.getObject(), b, false );
	}

	// send the reply message back to sender
	try
	{
		nub_.send( replyAddr_, b );

	}
	catch (Mercury::NubException & e)
	{
		PyErr_Format( PyExc_RuntimeError,
			"Exception thrown while sending reply %d: %s",
			replyID_, Mercury::reasonToString( e.reason() ) );
		return NULL;
	}

	Py_Return;
}

// method_response.cpp
