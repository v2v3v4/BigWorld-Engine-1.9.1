/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include <Python.h>
#include "zend_API.h"
#include "zend_ini.h"
#include "functions.hpp"

#include <string>


// ----------------------------------------------------------------------------
// Section: Macros for debugging
// ----------------------------------------------------------------------------

enum DebugLevels
{
	DBG_LEVEL_ERROR,
	DBG_LEVEL_INFO,
	DBG_LEVEL_TRACE,
	DBG_LEVEL_MEMORY
};

#define ERROR_MSG  	zend_error
#define INFO_MSG 	if (debug_verbosity() >= DBG_LEVEL_INFO) 	zend_error
#define TRACE_MSG 	if (debug_verbosity() >= DBG_LEVEL_TRACE) 	zend_error
#define MEMORY_MSG	if (debug_verbosity() >= DBG_LEVEL_MEMORY) 	zend_error

// ----------------------------------------------------------------------------
// Section: Zend helper function declarations
// ----------------------------------------------------------------------------

// Typemapping from Python objects to PHP objects
void mapPyDictToPHP( PyObject* pyDict, zval* return_value );
void mapPySequenceToPHP( PyObject* pySequence, zval* return_value );
void mapPyStringToPHP( PyObject* pyStr, zval* return_value );
void mapPyIntToPHP( PyObject* pyInt, zval* return_value );
void mapPyLongToPHP( PyObject* pyInt, zval* return_value );
void mapPyFloatToPHP( PyObject* pyInt, zval* return_value );
void mapPyObjToPHP( PyObject* pyObj, zval* return_value,
	int objType = le_pyobject );


// Typemapping from PHP objects to Python objects
bool phpArrayIsList( zval* array );
void mapPHPListArrayToPy( zval* array, PyObject** returnValue );
void mapPHPDictArrayToPy( zval* array, PyObject** returnValue );


// Helper functions
std::string PyErr_GetString();
long debug_verbosity();

// ----------------------------------------------------------------------------
// Section: Destructor functions for PHP resources
// ----------------------------------------------------------------------------

/**
 *	The destructing function for a Python object PHP resource.
 */
ZEND_RSRC_DTOR_FUNC( PyObject_ResourceDestructionHandler )
{
	PyObject* obj = reinterpret_cast<PyObject*>( rsrc->ptr );
	MEMORY_MSG( E_NOTICE, "Destructing PyObject resource: %p\n", obj );
	Py_XDECREF( obj );
}


// ----------------------------------------------------------------------------
// Section: Helper class definitions
// ----------------------------------------------------------------------------

/**
 *	Calls efree() on the given pointer when destroyed. Useful when created as
 *	an automatic variable.
 */
class ZendAutoPtr
{
public:
	ZendAutoPtr( void* ptr ):
		ptr_( ptr )
	{
		MEMORY_MSG( E_NOTICE, "ZendAutoPtr: %p\n", ptr_ );

	}

	~ZendAutoPtr()
	{
		if (ptr_)
		{
			MEMORY_MSG( E_NOTICE, "ZendAutoPtr: efree(%p)\n", ptr_ );
			efree( ptr_ );
		}
	}

private:
	void* 	ptr_;
};


/**
 *	Calls Py_XDECREF() on the given PyObject pointer when destroyed. Useful
 *	when created as an automatic variable.
 */
class PyObjectAutoPtr
{
public:
	PyObjectAutoPtr( PyObject* pyObj ):
			pyObj_( pyObj )
	{
		MEMORY_MSG( E_NOTICE, "PyObjectAutoPtr: %p", pyObj );
	}

	~PyObjectAutoPtr()
	{
		MEMORY_MSG( E_NOTICE, "PyObjectAutoPtr: decref %p", pyObj_ );
		Py_XDECREF( pyObj_ );
	}
private:
	PyObject* 	pyObj_;
};


// ----------------------------------------------------------------------------
// Section: Zend function implementations
// ----------------------------------------------------------------------------


PHP_FUNCTION( bw_logon )
{
	const char* username 	= NULL;
	int usernameLen 		= 0;
	const char* password 	= NULL;
	int passwordLen 		= 0;
	zend_bool allow_already_logged_on = 0;

	if (FAILURE == zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
		"ss|b",
		&username, &usernameLen,
		&password, &passwordLen,
		&allow_already_logged_on ))
	{
		RETURN_FALSE;
	}

	INFO_MSG( E_NOTICE, "bw_logon(%s, ********, allow_already_logged_on=%s)",
		username, allow_already_logged_on ? "true" : "false" );

	PyObject* args = Py_BuildValue( "(ssN)",
		username, password, PyBool_FromLong( allow_already_logged_on ) );
	PyObjectAutoPtr _args( args ); // auto-decref

	PyObject* method = PyObject_GetAttrString(
		BWG( bwModule ),
		"logOn" );
	PyObjectAutoPtr _method( method ); // auto-decref

	PyObject* resultObj = PyObject_Call( method, args, NULL );
	PyObjectAutoPtr _resultObj( resultObj ); // automatically decref

	if (resultObj == NULL)
	{
		std::string errString = PyErr_GetString();
		PyErr_Clear();
		INFO_MSG( E_NOTICE, "bw_logon failed due to %s\n", errString.c_str() );
		//zend_error( E_NOTICE, "bw_logon error: %s", errString.c_str() );
		RETURN_STRING( const_cast<char*> ( errString.c_str() ), 1 );
	}

	RETURN_TRUE;
}


PHP_FUNCTION( bw_look_up_entity_by_name )
{
	const char* entityType 	= NULL;
	int entityTypeLen 		= 0;
	const char* entityName 	= NULL;
	int entityNameLen 		= 0;

	if( FAILURE == zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
		"ss",
		&entityType, &entityTypeLen,
		&entityName, &entityNameLen ) )
	{
		RETURN_FALSE;
	}

	INFO_MSG( E_NOTICE, "bw_look_up_entity_by_name(%s, %s)",
		entityType, entityName );

	PyObject* mailboxObj = PyObject_CallMethod(
		BWG( bwModule ),
		"lookUpEntityByName", "ss",
		entityType, entityName );
	// creates a new reference which is decref'd when the resource destructs

	if (mailboxObj == NULL)
	{
		std::string errString = PyErr_GetString();
		PyErr_Clear();
		RETURN_STRING( const_cast<char*>( errString.c_str() ), 1 );
	}
	if (PyBool_Check( mailboxObj ))
	{
		if (mailboxObj == Py_True)
		{
			Py_DECREF( mailboxObj );
			RETURN_TRUE;
		}
		else if (mailboxObj == Py_False)
		{
			Py_DECREF( mailboxObj );
			RETURN_FALSE;
		}
		else
		{
			Py_DECREF( mailboxObj );
			RETURN_NULL();
		}
	}
	else
	{
		// create new resource
		mapPyObjToPHP( mailboxObj, return_value );
	}

}


PHP_FUNCTION( bw_look_up_entity_by_dbid )
{
	const char* entityType 	= NULL;
	int entityTypeLen 		= 0;
	long dbId 				= 0;
	int entityNameLen 		= 0;

	if (FAILURE == zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
		"sl",
		&entityType, &entityTypeLen,
		&dbId ))
	{
		RETURN_FALSE;
	}

	INFO_MSG( E_NOTICE, "bw_look_up_entity_by_dbid(%s, %ld)",
		entityType, dbId );

	PyObject* mailboxObj = PyObject_CallMethod(
		BWG( bwModule ),
		"lookUpEntityByDBID", "sl",
		entityType, dbId );
	// creates a new reference which is decrefed when the resource destructs

	if (mailboxObj == NULL)
	{
		std::string errString = PyErr_GetString();
		PyErr_Clear();
		RETURN_STRING( const_cast<char*>( errString.c_str() ), 1 );
	}
	if (PyBool_Check( mailboxObj ))
	{
		if (mailboxObj == Py_True)
		{
			Py_DECREF( mailboxObj );
			RETURN_TRUE;
		}
		else if (mailboxObj == Py_False)
		{
			Py_DECREF( mailboxObj );
			RETURN_FALSE;
		}
		else
		{
			Py_DECREF( mailboxObj );
			RETURN_NULL();
		}
	}
	else
	{
		// create new resource
		mapPyObjToPHP( mailboxObj, return_value );
	}

}


/**
 *	bw_test(...)
 *
 *	Test function. Not registered.
 */
PHP_FUNCTION( bw_test )
{

}


/**
 *	bw_exec($mailbox, $methodName, ...)
 *
 *	@param mailbox resource
 *	@param methodName
 *
 */
PHP_FUNCTION( bw_exec )
{

	int numArgs = ZEND_NUM_ARGS();
	if( numArgs < 2 ) WRONG_PARAM_COUNT;


	zval*** argArray = ( zval*** )
		emalloc( numArgs * sizeof( zval** ) );
	if (!argArray)
	{
		ERROR_MSG( E_ERROR, "bw_exec: could not allocate argArray" );
		RETURN_FALSE;
	}

	// destroy argArray when _argArray goes out of scope
	ZendAutoPtr _argArray( argArray );

	// get variable argument list
	if (FAILURE ==
		zend_get_parameters_array_ex( numArgs, argArray ))
	{
		WRONG_PARAM_COUNT;
	};


	// get the mailbox from the resource, the first arg
	zval* mailboxObj = ( *argArray )[0];
	PyObject* mailbox = NULL;


	// if there's an issue retrieving the resource, this object prints
	// and error and returns from this function
	ZEND_FETCH_RESOURCE( mailbox, PyObject*,
		&mailboxObj,
		-1, // default resource ID
		"PyObject", le_pyobject );

	// create trace string to print out via zend_error
	std::string traceString( "bw_exec(" );
	PyObject* mailboxString = PyObject_Str( mailbox );
	PyObjectAutoPtr _mailboxString( mailboxString );
	traceString += PyString_AsString( mailboxString );
	traceString += ", ";

	// get the method name, the second arg
	const char* methodName = NULL;
	if ((*argArray)[1]->type != IS_STRING)
	{
		zend_error( E_ERROR, "bw_exec: Method name is not a string" );
	}
	methodName = (*argArray)[1]->value.str.val;

	traceString += methodName;

	// see if the method exists!
	PyObject* method = PyObject_GetAttrString( mailbox,
		const_cast<char*>( methodName ) );
	PyObjectAutoPtr _method( method ); // automatically decref method

	if (!method)
	{
		PyErr_ZendError();
		RETURN_FALSE;
	}

	// get the rest of the method arguments
	PyObject* methodArgs = PyTuple_New( numArgs - 2 );
	PyObjectAutoPtr _methodArgs( methodArgs ); // destroy when out of scope

	if (!methodArgs)
	{
		PyErr_ZendError();
		RETURN_FALSE;
	}

	for (int i = 2; i < numArgs; ++i)
	{
		PyObject *pyArg = NULL;
		// map each php typed argument to a newly referenced
		// python object which is stolen by PyTuple_SetItem
		mapPHPTypeToPy( ( *argArray )[i], &pyArg );

		PyObject* argString = PyObject_Str( pyArg );
		PyObjectAutoPtr _argString( argString );
		traceString += ", ";
		traceString += PyString_AsString( argString );

		if (0 != PyTuple_SetItem( methodArgs, i - 2, pyArg ))
		{
			zend_error( E_ERROR, "Could not set %dth item in "
				"argument tuple for call to %s", i - 2, methodName );
			RETURN_FALSE;
		}
	}

	traceString += ")";
	INFO_MSG( E_NOTICE, traceString.c_str() );

	// make the call, get back a dictionary of return values
	PyObject* resultObj = PyObject_CallObject( method, methodArgs );
	PyObjectAutoPtr _resultObj( resultObj ); // automatically decref resultObj


	if (resultObj == NULL)
	{
		std::string errString = PyErr_GetString();
		PyErr_Clear();
		RETURN_STRING( const_cast<char*>( errString.c_str() ), 1 );
	}

	// map it back to PHP typed return value
	mapPyTypeToPHP( resultObj, return_value );

}


/**
 *	bw_set_nub_port($port)
 *
 *	Recreates the nub, using the given port.
 *
 *	@param port the port number, or 0 for a random port.
 */
PHP_FUNCTION( bw_set_nub_port )
{
	long port = 0;

	if (FAILURE == zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
		"l",
		&port ))
	{
		RETURN_FALSE;
	}

	INFO_MSG( E_NOTICE, "bw_set_nub_port(%d)", port );

	PyObject* res = PyObject_CallMethod( BWG( bwModule ),
		"setNubPort", "l",
		port );

	if (!res)
	{
		PyErr_ZendError();
		RETURN_FALSE;
	}
	Py_DECREF( res );

	RETURN_TRUE;
}


/**
 *	bw_serialise( $mailbox )
 *
 *	Serialises the given mailbox.
 *
 *	@param mailbox the mailbox resource for the entity
 *	@return string the serialised mailbox string
 */
PHP_FUNCTION( bw_serialise )
{
	int numArgs = ZEND_NUM_ARGS();
	if( numArgs != 1 ) WRONG_PARAM_COUNT;

	zval* resource = NULL;

	if (zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "r",
				&resource )
			== FAILURE)
	{
		RETURN_FALSE;
	}

	PyObject* pyObj = NULL;
	ZEND_FETCH_RESOURCE( pyObj, PyObject*, &resource, -1, "PyObject",
		le_pyobject );

	if (pyObj == NULL)
	{
		RETURN_FALSE;
	}

	PyObject * strObj = PyObject_Str( pyObj );
	PyObjectAutoPtr _strObj( strObj );

	INFO_MSG( E_NOTICE, "bw_serialise(%s)", PyString_AsString( strObj ) );

	PyObject * result = PyObject_CallMethod( pyObj, "serialise", NULL  );
	PyObjectAutoPtr _result( result );

	if (result == NULL)
	{
		PyErr_ZendError();
		RETURN_FALSE;
	}

	RETURN_STRING( PyString_AsString( result ), 1 );
}


/**
 *	bw_deserialise( $string )
 *
 *	Deserialises the given serialised mailbox string.
 *
 *	@param string the serialised mailbox string
 *	@return resource the deserialised mailbox
 */
PHP_FUNCTION( bw_deserialise )
{
	int numArgs = ZEND_NUM_ARGS();
	if( numArgs != 1 ) WRONG_PARAM_COUNT;

	char* serialised;
	int serialisedLen;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
				"s", &serialised, &serialisedLen )
			== FAILURE)
	{
		RETURN_FALSE;
	}

	INFO_MSG( E_NOTICE, "bw_deserialise(%s)", serialised );

	PyObject* result = PyObject_CallMethod( BWG( bwModule ),
		"deserialise", "s#", serialised, serialisedLen );
	if (result == NULL)
	{
		PyErr_ZendError();
		RETURN_FALSE;
	}

	mapPyObjToPHP( result, return_value );
}


/**
 *	bw_pyprint ($pyResource)
 *
 *	Returns a string of the PyResource's string representation, e.g. str(obj)
 *
 *	@param bw_pyprint
 *	@return string
 */
PHP_FUNCTION( bw_pystring )
{
	int numArgs = ZEND_NUM_ARGS();
	if (numArgs != 1) WRONG_PARAM_COUNT;

	zval* resource = NULL;

	if (zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
		"r", &resource )
		== FAILURE)
	{
		RETURN_FALSE;
	}

	PyObject* pyObj = NULL;
	ZEND_FETCH_RESOURCE( pyObj, PyObject*, &resource, -1, "PyObject",
		le_pyobject );

	if (pyObj == NULL)
	{
		ERROR_MSG( E_ERROR,
			"bw_pystring: could not get PyObject from resource" );
		RETURN_FALSE;
	}

	PyObject* result = PyObject_Str( pyObj );
	PyObjectAutoPtr _result( result );
	if (result == NULL)
	{
		PyErr_ZendError();
		RETURN_FALSE;
	}

	INFO_MSG( E_NOTICE, "bw_pystring(%s)", PyString_AsString( result ) );

	RETURN_STRING( PyString_AsString( result ), 1 );

}

/**
 *	Set the keepalive period for the given mailbox.
 */
PHP_FUNCTION( bw_set_keep_alive_seconds )
{
	int numArgs = ZEND_NUM_ARGS();
	if (numArgs != 2) WRONG_PARAM_COUNT;
	zval * mailboxResource;
	long keepAlive;

	if (FAILURE == zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
		"rl", &mailboxResource, &keepAlive ))
	{
		RETURN_FALSE;
	}

	PyObject * mailbox = NULL;
	ZEND_FETCH_RESOURCE( mailbox, PyObject*, &mailboxResource, -1,
			"PyObject", le_pyobject );

	if (mailbox == NULL)
	{
		RETURN_FALSE;
	}

	PyObject * mailboxString = PyObject_Str( mailbox );
	PyObjectAutoPtr _mailboxString( mailboxString );

	INFO_MSG( E_NOTICE, "bw_set_keep_alive_seconds( %s, %ld )",
		PyString_AsString( mailboxString ), keepAlive );

	PyObject * keepAliveObj = PyLong_FromLong( keepAlive );
	PyObjectAutoPtr _keepAliveObj( keepAliveObj );

	if (-1 == PyObject_SetAttrString( mailbox,
			"keepAliveSeconds", keepAliveObj ))
	{
		PyErr_ZendError();

		RETURN_FALSE;
	}

	RETURN_TRUE;
}

/**
 *	Get the keepalive period for the given mailbox.
 */
PHP_FUNCTION( bw_get_keep_alive_seconds )
{
	int numArgs = ZEND_NUM_ARGS();
	if (numArgs != 1) WRONG_PARAM_COUNT;
	zval * mailboxResource;

	if (FAILURE == zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
		"r", &mailboxResource ))
	{
		RETURN_FALSE;
	}

	PyObject * mailbox = NULL;
	ZEND_FETCH_RESOURCE( mailbox, PyObject*, &mailboxResource, -1,
		"PyObject", le_pyobject );

	if (mailbox == NULL)
	{
		RETURN_FALSE;
	}

	PyObject * mailboxString = PyObject_Str( mailbox );
	PyObjectAutoPtr _mailboxString( mailboxString );

	INFO_MSG( E_NOTICE, "bw_get_keep_alive_seconds( %s )",
		PyString_AsString( mailboxString ) );

	PyObject * keepAliveSecondsObj =
		PyObject_GetAttrString( mailbox, "keepAliveSeconds" );
	PyObjectAutoPtr _keepAliveSecondsObj( keepAliveSecondsObj );

	if (!keepAliveSecondsObj)
	{
		PyErr_ZendError();

		RETURN_FALSE;
	}

	RETURN_LONG( PyLong_AsLong( keepAliveSecondsObj ) );
}

/**
 *	Set the default keep-alive period for new mailboxes.
 */
PHP_FUNCTION( bw_set_default_keep_alive_seconds )
{
	int numArgs = ZEND_NUM_ARGS();
	if (numArgs != 1) WRONG_PARAM_COUNT;

	long defaultKeepAlive;

	if (FAILURE == zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
		"l", &defaultKeepAlive ))
	{
		RETURN_FALSE;
	}

	INFO_MSG( E_NOTICE, "bw_set_default_keep_alive_seconds( %ld )",
		defaultKeepAlive );

	PyObject* res = PyObject_CallMethod( BWG( bwModule ),
		"setDefaultKeepAliveSeconds", "l",
		defaultKeepAlive );

	if (!res)
	{
		PyErr_ZendError();
		RETURN_FALSE;
	}
	Py_DECREF( res );

	RETURN_TRUE;
}

// ----------------------------------------------------------------------------
// Section: Type-mapping functions from Python types to PHP
// ----------------------------------------------------------------------------

/**
 *	Maps a Python object to a PHP object.
 *
 *	@param pyObj			the Python object
 *	@param return_value 	the zval return value (the name "return_value"
 *							enables the ZEND macros to be used).
 */
void mapPyTypeToPHP( PyObject* pyObj, zval* return_value )
{

	PyObject* typeStr = PyObject_Str( ( PyObject* ) pyObj->ob_type );
	PyObjectAutoPtr _typeStr( typeStr );


	if (PyDict_Check( pyObj ))
	{
		mapPyDictToPHP( pyObj, return_value );
	}
	else if(PyString_Check( pyObj ))
	{
		mapPyStringToPHP( pyObj, return_value );
	}
	else if(PySequence_Check( pyObj ))
	{
		mapPySequenceToPHP( pyObj, return_value );
	}
	else if(PyInt_Check( pyObj ))
	{ // includes bool
		mapPyIntToPHP( pyObj, return_value );
	}
	else if(PyLong_Check( pyObj ))
	{
		mapPyLongToPHP( pyObj, return_value );
	}
	else if(PyFloat_Check( pyObj ))
	{
		mapPyFloatToPHP( pyObj, return_value );
	}
	else if(pyObj == Py_None)
	{
		RETURN_NULL();
	}
	else
	{
		mapPyObjToPHP( pyObj, return_value );
	}
}


/**
 *	Maps a Python dictionary object to a PHP hash array.
 *
 *	@param pyDict			the Python dictionary object
 *	@param return_value 	the zval return value (the name "return_value"
 *							enables the ZEND macros to be used).
 */
void mapPyDictToPHP( PyObject* pyDict, zval* return_value )
{

	array_init( return_value );

	Py_ssize_t pos = 0;
	PyObject* pyKey = NULL;
	PyObject* pyValue = NULL;

	while (PyDict_Next( pyDict, &pos, &pyKey, &pyValue ))
	{
		// pyKey and pyValue are borrowed

		PyObject* keyString = PyObject_Str( pyKey ); // new reference
		PyObjectAutoPtr _keyString( keyString );

		char * phpKey = PyString_AsString( keyString );

		zval* phpValue = NULL;
		MAKE_STD_ZVAL( phpValue );

		mapPyTypeToPHP( pyValue, phpValue );

		if (SUCCESS != add_assoc_zval( return_value, phpKey, phpValue ))
		{
			zend_error( E_ERROR, "Could not add value for key = %s", phpKey );
			return;
		}
	}

}


/**
 *	Maps a Python sequence object to a PHP numerically indexed hash array. This
 *	includes lists and tuples, but not strings (handled by mapPyStringToPHP).
 *
 *	@param pySequence		the Python sequence object
 *	@param return_value 	the zval return value (the name "return_value"
 *							enables the ZEND macros to be used).
 */
void mapPySequenceToPHP( PyObject* pySequence, zval* return_value )
{
	array_init( return_value );

	int pos = 0;
	PyObject* pyValue = NULL;

	for (int i = 0; i < PySequence_Size( pySequence ); ++i)
	{
		pyValue = PySequence_GetItem( pySequence, i ); // borrowed

		zval* phpValue = NULL;
		MAKE_STD_ZVAL( phpValue );

		mapPyTypeToPHP( pyValue, phpValue );

		if (SUCCESS != add_index_zval( return_value, i, phpValue ))
		{
			zend_error( E_ERROR, "Could not add value for index = %d", i );
			return;
		}
	}

}


/**
 *	Maps a Python string to a PHP string.
 *
 *	@param pyStr			the Python string object
 *	@param return_value 	the zval return value (the name "return_value"
 *							enables the ZEND macros to be used).
 */
void mapPyStringToPHP( PyObject* pyStr, zval* return_value )
{
	RETURN_STRING( PyString_AsString( pyStr ), 1 );
}


/**
 *	Maps a Python plain integer to a PHP integer.
 *
 *	@param pyInt			the Python plain integer object
 *	@param return_value 	the zval return value (the name "return_value"
 *							enables the ZEND macros to be used).
 */
void mapPyIntToPHP( PyObject* pyInt, zval* return_value )
{
	if (PyBool_Check( pyInt ))
	{
		if (pyInt == Py_True)
		{
			RETURN_TRUE;
		}
		else
		{
			RETURN_FALSE;
		}
	}
	RETURN_LONG( PyInt_AsLong( pyInt ) );
}


/**
 *	Maps a Python long integer to a PHP string. They can then be manipulated
 *	using the bc* functions for arbitrary precision arithmetic in PHP.
 *
 *	@param pyInt			the Python long integer object
 *	@param return_value 	the zval return value (the name "return_value"
 *							enables the ZEND macros to be used).
 */
void mapPyLongToPHP( PyObject* pyInt, zval* return_value )
{
	PyObject* intStrObj = PyObject_Str( pyInt );
	PyObjectAutoPtr _intStrObj( intStrObj );
	mapPyStringToPHP( intStrObj, return_value );
}

/**
 *	Maps a Python float to a PHP double.
 *
 *	@param
 */
void mapPyFloatToPHP( PyObject* pyFloat, zval* return_value )
{
	RETURN_DOUBLE( PyFloat_AsDouble( pyFloat ) );
}

/**
 *	Maps a generic Python object to a PHP resource.
 *
 *	@param pyObj			the Python object
 *	@param return_value 	the zval return value (the name "return_value"
 *							enables the ZEND macros to be used).
 *	@param objType			the PHP resource object type (defaults to
 *							le_pyobject)
 */
void mapPyObjToPHP( PyObject* pyObj, zval* return_value, int objType )
{
	// General Python Object -> PHP resource
	ZEND_REGISTER_RESOURCE( return_value, pyObj, objType );
}


// ----------------------------------------------------------------------------
// Section: Type-mapping functions from PHP types to Python objects
// ----------------------------------------------------------------------------

void mapPHPTypeToPy( zval* phpObj, PyObject** returnValue )
{
	switch (phpObj->type)
	{
		case IS_NULL:
			Py_INCREF( Py_None );
			*returnValue = Py_None;
		break;

		case IS_LONG:
			*returnValue = PyInt_FromLong( phpObj->value.lval );
		break;

		case IS_DOUBLE:		// Python float
			*returnValue = PyFloat_FromDouble( phpObj->value.dval );
		break;

		case IS_STRING:		// Python string
			*returnValue = PyString_FromStringAndSize(
				phpObj->value.str.val, phpObj->value.str.len );
		break;

		case IS_ARRAY:
		{

			if (phpArrayIsList( phpObj ))
			{
				mapPHPListArrayToPy( phpObj, returnValue );
			}
			else
			{
				mapPHPDictArrayToPy( phpObj, returnValue );
			}

		}

		break;

		case IS_BOOL:	// Python plain integer
			if (phpObj->value.lval == 0)
			{
				Py_INCREF( Py_False );
				*returnValue = Py_False;
			}
			else
			{
				Py_INCREF( Py_True );
				*returnValue = Py_True;
			}
		break;

		case IS_CONSTANT:	// Python string
			*returnValue = PyString_FromStringAndSize(
				phpObj->value.str.val, phpObj->value.str.len );
		break;

		case IS_RESOURCE:	 // Python resource

			// get the Python object in the resource, if it is a Python object
			PyObject* pyObj;

			pyObj = (PyObject*) zend_fetch_resource(
				&phpObj TSRMLS_CC,
				-1, // default resource ID
				"PyObject", NULL, 1, le_pyobject );
			if (!pyObj)
			{
				zend_error( E_ERROR,
					"Could not retrieve Python object resource" );
				*returnValue = Py_None;
			}
			else
			{
				*returnValue = pyObj;
			}
			Py_INCREF( *returnValue );

		break;

		case IS_OBJECT: // Python object with incref
		case IS_CONSTANT_ARRAY:	// Python object with incref
		default:
			zend_error( E_ERROR, "Could not get arg format type for "
				"unknown PHP type: %d", phpObj->type );

		break;
	}
	return;
}


/**
 *	Return true if the given array zval is a list, that is, its keys are
 *	numerical, sequential and start from 0.

 *	@param array the array zval
 *	@return true if the array is a list
 */
bool phpArrayIsList( zval* array )
{
	HashPosition pos;
	zval** entry = NULL;
	char* key = NULL;
	uint keyLen = 0;
	ulong index = 0;

	bool isFirst = true;
	ulong lastIndex = 0;

	zend_hash_internal_pointer_reset_ex( HASH_OF( array ), &pos );
	while (zend_hash_get_current_data_ex(
		HASH_OF( array ), ( void** ) &entry, &pos ) == SUCCESS)
	{
		zend_hash_get_current_key_ex( HASH_OF( array ),
			&key, &keyLen, &index, 0, &pos );

		if (key && keyLen)
		{
			return false;
		}
		else
		{
			if (( isFirst && index != 0 ) ||
				( !isFirst && index != lastIndex + 1 ))
			{
				return false;
			}
			lastIndex = index;
		}
		isFirst = false;
		zend_hash_move_forward_ex( HASH_OF( array ), &pos );
	}

	return true;
}


/**
 *	Maps a PHP list array to a Python list.
 *
 *	@param array 		the PHP array object
 *	@param returnValue 	a pointer to a PyObject* where the new PyList will
 *						be created
 */
void mapPHPListArrayToPy( zval* array, PyObject** returnValue )
{
	int numElements = zend_hash_num_elements( HASH_OF( array ) );
	*returnValue = PyList_New( numElements );

	HashPosition pos;
	zval** entry = NULL;
	char* key = NULL;
	uint keyLen = 0;
	ulong index = 0;
	zend_hash_internal_pointer_reset_ex( HASH_OF( array ), &pos );
	while (zend_hash_get_current_data_ex(
		HASH_OF( array ), ( void** )&entry, &pos ) == SUCCESS)
	{
		zend_hash_get_current_key_ex( HASH_OF( array ),
			&key, &keyLen, &index, 0, &pos );

		PyObject* valueObj = NULL;
		mapPHPTypeToPy( *entry, &valueObj );

		PyList_SetItem( *returnValue, index, valueObj );

		zend_hash_move_forward_ex( HASH_OF( array ), &pos );
	}

}



/**
 *	Maps a PHP dictionary array to a Python dictionary.
 *
 *	@param array		the PHP array object
 *	@param returnValue 	a pointer to a PyObject* where the new PyDict will be
 *						created
 */
void mapPHPDictArrayToPy( zval* array, PyObject** returnValue )
{
	*returnValue = PyDict_New();

	HashPosition pos;
	zval** entry = NULL;
	char* key = NULL;
	uint keyLen = 0;
	ulong index = 0;
	zend_hash_internal_pointer_reset_ex( HASH_OF( array ), &pos );
	while (zend_hash_get_current_data_ex(
		HASH_OF( array ), ( void** )&entry, &pos ) == SUCCESS)
	{
		zend_hash_get_current_key_ex( HASH_OF( array ),
			&key, &keyLen, &index, 0, &pos );

		PyObject* keyObj;
		if (key)
		{
			keyObj = PyString_FromString( key );
		}
		else
		{
			keyObj = PyLong_FromLong( index );
		}

		PyObject* valueObj = NULL;
		mapPHPTypeToPy( *entry, &valueObj );

		PyDict_SetItem( *returnValue, keyObj, valueObj );

		zend_hash_move_forward_ex( HASH_OF( array ), &pos );
	}

}


// ----------------------------------------------------------------------------
// Section: Helper method implementations
// ----------------------------------------------------------------------------

void appendAdditionalPythonPaths( const char* additionalPaths )
{
	PyObject* pathsObj = PySys_GetObject( "path" ); //borrowed ref

	// expecting colon delimited paths
	std::string paths( additionalPaths );
	int pos = 0;
	int sep = 0;
	while (( sep = paths.find( ':', pos ) ) != std::string::npos)
	{
		std::string path = paths.substr( pos, sep - pos );
		PyObject* pathObj = PyString_FromString( path.c_str() );
		PyObjectAutoPtr _pathObj( pathObj ); //auto decref
		PyList_Append( pathsObj, pathObj );
		pos = sep + 1;
	}

	if (pos != paths.size())
	{
		std::string path = paths.substr( pos, paths.size() - pos );
		PyObject* pathObj = PyString_FromString( path.c_str() );
		PyObjectAutoPtr _pathObj( pathObj ); //auto decref

		PyList_Append( pathsObj, pathObj );
	}


}


/**
 *	Return a string describing the current Python exception, and clear the
 *	exception.
 */
std::string PyErr_GetString()
{
	if (PyErr_Occurred())
	{
		PyObject* errType = NULL;
		PyObject* errValue = NULL;
		PyObject* errTraceback = NULL;

		PyErr_Fetch( &errType, &errValue, &errTraceback );
		// errValue and errTraceback may be NULL
		PyObject* errTypeString = NULL;
		PyObject* errValueString = NULL;

		errTypeString = PyObject_Str( errType );

		if (errValue)
		{
			errValueString = PyObject_Str( errValue );
		}

		std::string out;

		if (errValueString)
		{
			out += PyString_AsString( errValueString );
		}
		else
		{
			out += PyString_AsString( errTypeString );
		}

		Py_DECREF( errTypeString );
		Py_XDECREF( errValueString );
		Py_DECREF( errType );
		Py_XDECREF( errValue );
		Py_XDECREF( errTraceback );

		return out;
	}
	else
	{
		return std::string( "Unknown error" );
	}
}


/**
 *	If a Python Exception has occurred, then the error type and string
 *	representation are printed out through zend_error with level E_ERROR.
 */
void PyErr_ZendError( const char * msg )
{
	if (msg)
	{
		ERROR_MSG( E_ERROR, "%s: Python Exception: %s",
			msg, PyErr_GetString().c_str() );
	}
	else
	{
		ERROR_MSG( E_ERROR, "Python Exception: %s", PyErr_GetString().c_str() );
	}
}

inline long debug_verbosity()
{
	return BWG( debugLevel );
}
