/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef FUNCTIONS_HPP
#define FUNCTIONS_HPP

#include "php.h"
#include "php_bigworld.h"

// ----------------------------------------------------------------------------
// Section: PHP Resource list entry IDs
// ----------------------------------------------------------------------------

/**
 *	List entry ID for PyObject* resources.
 */
extern int le_pyobject;

// ----------------------------------------------------------------------------
// Section: PHP Resource Destruction handlers
// ----------------------------------------------------------------------------

/**
 *	The destructing function for a Python object PHP resource.
 */
void PyObject_ResourceDestructionHandler( zend_rsrc_list_entry *rsrc
	TSRMLS_DC );

// ----------------------------------------------------------------------------
// Section: Globals
// ----------------------------------------------------------------------------
ZEND_EXTERN_MODULE_GLOBALS( bigworld_php )

// ----------------------------------------------------------------------------
// Section: PHP API function declarations
// ----------------------------------------------------------------------------

PHP_FUNCTION( bw_logon );
PHP_FUNCTION( bw_test );
PHP_FUNCTION( bw_look_up_entity_by_name );
PHP_FUNCTION( bw_look_up_entity_by_dbid );
PHP_FUNCTION( bw_exec );
PHP_FUNCTION( bw_set_nub_port );
PHP_FUNCTION( bw_serialise );
PHP_FUNCTION( bw_deserialise );
PHP_FUNCTION( bw_pystring );
PHP_FUNCTION( bw_set_keep_alive_seconds );
PHP_FUNCTION( bw_get_keep_alive_seconds );
PHP_FUNCTION( bw_set_default_keep_alive_seconds );

// ----------------------------------------------------------------------------
// Section: Typemapping functions
// ----------------------------------------------------------------------------

/**
 *	Map a Python typed object to its most appropriate corresponding PHP type.
 *
 *	@param pyObj the python Object
 *	@param return_value the PHP typed return value
 */
void mapPyTypeToPHP( PyObject* pyObj, zval* return_value );


/**
 *	Map a PHP typed object to its most appropriate corresponding Python type.
 *	Creates a new reference.
 *
 *	@param phpObj the PHP object
 *	@param returnValue the Python typed return value
 */
void mapPHPTypeToPy( zval* phpObj, PyObject** returnValue );


// ----------------------------------------------------------------------------
// Section: Helper function declarations
// ----------------------------------------------------------------------------

/**
 *	Add additional python search directories to the Python interpreter.
 *
 *	@param additionPaths string of additional paths separated by a colon
 *
 */
void appendAdditionalPythonPaths( const char* additionalPaths );


/**
 *	If a Python Exception has occurred, then the error type and string
 *	representation are printed out through zend_error with level E_ERROR.
 */
void PyErr_ZendError( const char * msg = NULL );

#endif // ifndef FUNCTIONS_HPP
