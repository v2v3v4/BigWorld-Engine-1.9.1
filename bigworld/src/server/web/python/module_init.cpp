/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "Python.h"
#include <string>

#include "resmgr/bwresource.hpp"
#include "entitydef/entity_description_map.hpp"
#include "server/bwconfig.hpp"
#include "server/bwservice.hpp"

#include "web_integration.hpp"

extern int Math_token;
extern int ResMgr_token;
extern int PyScript_token;
static int s_moduleTokens =
	Math_token | ResMgr_token | PyScript_token;

extern int PyPatrolPath_token;
static int s_patrolPathToken = PyPatrolPath_token;
extern int ChunkStationGraph_token;
static int s_chunkStationGraphToken = ChunkStationGraph_token;
extern int UserDataObjectLinkDataType_token;
static int s_userDataObjectLinkDataType_token = UserDataObjectLinkDataType_token;

DECLARE_DEBUG_COMPONENT( 0 )

/** Class that prints messages on construction and destruction. */
class LoadUnloadSentry
{
public:
	LoadUnloadSentry();
	~LoadUnloadSentry();
};


LoadUnloadSentry::LoadUnloadSentry()
{
}

LoadUnloadSentry::~LoadUnloadSentry()
{
	TRACE_MSG( "BigWorld module unloaded\n" );
}


/** Static instance of LoadUnloadSentry. */
static LoadUnloadSentry s_loadUnloadSentry;

/** Finalisation function. */
static PyObject* bigworld_fini_fn( PyObject * self, PyObject * args );

/**
 * Method definition for finalisation function.
 */
static PyMethodDef s_bigworld_fini =
{
	"_fini", 				/* char* ml_name */
	bigworld_fini_fn, 		/* PyCFunction ml_meth */
	METH_NOARGS,			/* int ml_flags */
	NULL,					/* char * ml_doc */
};


/**
 * Finalisation function.
 */
static PyObject* bigworld_fini_fn( PyObject * self, PyObject * args )
{
	delete WebIntegration::pInstance();
	delete BWResource::pInstance();
	Script::fini( /* shouldFinalise */ false );

	Py_RETURN_NONE;
}


/**
 * Python dynamic extension module initialisation function.
 */
PyMODINIT_FUNC initBigWorld( void )
{
	new BWResource();
	BWResource::init( 0, NULL );
	BWConfig::init( 0, NULL );

	PyObject* module = Py_InitModule3( "BigWorld", NULL,
		"BigWorld Web Integration module." );

	// hack to retain old sys.path

	// get new reference to sys module
	PyObject *sysModule = NULL;
	if (NULL == ( sysModule = PyImport_ImportModule("sys") ))
	{
		PyErr_SetString( PyExc_ImportError, "Could not import module 'sys'!" );
		return;
	}

	// get new reference to sys.path variable
	PyObject* pathList = PyObject_GetAttrString( sysModule, "path" );
	if (!pathList || !PySequence_Check( pathList ))
	{
		PyErr_SetString( PyExc_ImportError,
			"Could not find sys.path, or is not a sequence" );
		return;
	}

	// build python path string
	std::string pythonPaths;
	{
		bool first = true;
		for (int i = 0; i < PySequence_Size( pathList ); ++i)
		{
			PyObject* pathString = PySequence_GetItem( pathList, i );
			if (!PyString_Check( pathString ))
			{
				ERROR_MSG( "Found a non-string in sys.path at index %d, "
					"ignoring", i );
				continue;
			}
			if (!first)
			{
				pythonPaths += ":" ;
			}
			pythonPaths += PyString_AS_STRING( pathString );
			first = false;
		}
	}

	// release new references we have
	Py_DECREF( sysModule );
	Py_DECREF( pathList );

	Script::init( pythonPaths );

	// reset sys.path to old value
	PySys_SetPath( const_cast<char*> ( pythonPaths.c_str() ) );


	// register finalisation function with atexit

	PyObject* atExitModule = PyImport_ImportModule( "atexit" );
	if (!atExitModule)
	{
		PyErr_SetString( PyExc_ImportError,
			"Could not import module atexit" );
		return;
	}

	PyObject* pyCFunction = PyCFunction_New( &s_bigworld_fini, module );
	if (!pyCFunction)
	{
		Py_DECREF( atExitModule );
		PyErr_SetString( PyExc_ImportError,
			"Could not create finalisation function object" );
		return;
	}

	PyObject* res = PyObject_CallMethod( atExitModule, "register", "O",
		pyCFunction );
	if (!res)
	{
		Py_DECREF( pyCFunction );
		Py_DECREF( atExitModule );
		PyErr_SetString( PyExc_ImportError,
			"Could not register finalisation function "
				"with atexit.register" );
		return;
	}

	// clean up after registering bigworld_fini
	Py_DECREF( atExitModule );
	Py_DECREF( pyCFunction );
	Py_XDECREF( res );

	// initialise mailbox component factory function
	WebIntegration * pWebIntegration = new WebIntegration();
	if (!pWebIntegration->init())
	{
		// exception set in init on failure
		return;
	}

	START_MSG( "WebIntegrationModule" );
}

// module_init.cpp
