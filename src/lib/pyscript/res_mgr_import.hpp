/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef RES_MGR_IMPORT_HPP
#define RES_MGR_IMPORT_HPP

#if !BWCLIENT_AS_PYTHON_MODULE
#define USE_RES_MGR_IMPORT_HOOK
#endif // !BWCLIENT_AS_PYTHON_MODULE

#ifdef USE_RES_MGR_IMPORT_HOOK

#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"

#include "resmgr/datasection.hpp"

/**
 * This class creates PyResMgrImportLoaders specialised to
 * a directory in the python script tree
 *
 * It should probably be a singleton, which'll be fun to implement
 */
class PyResMgrImportHook : public PyObjectPlus
{
	Py_Header( PyResMgrImportHook, PyObjectPlus );

public:
	PyResMgrImportHook( PyTypePlus * pType = &PyResMgrImportHook::s_type_ );

	// PyObjectPlus overrides
	PyObject*			pyGetAttribute( const char * attr );
	int					pySetAttribute( const char * attr, PyObject * value );

	/* Return a PyResMgrImportLoader specialised to the given path */
	PyObject* getImporter( std::string& path );
	PY_AUTO_METHOD_DECLARE( RETOWN, getImporter, ARG( std::string, END ) );

	/* Route a call on an instance to getImporter */
	static PyObject * _pyCall( PyObject * self, PyObject * args, PyObject * kw)
		{ return _py_getImporter( self, args, kw ); }

};

typedef SmartPointer<PyResMgrImportHook> PyResMgrImportHookPtr;

/**
 * This class locates and loads Python modules. It is both loader and
 * importer as per PEP 302.
 */
/*
 * The optional extensions to the loader protocol in PEP 302 are
 * not yet implemented.
 * loader.get_data isn't really useful, ResMgr already handles this better.
 * loader.is_package, get_code and get_source could be useful and easy.
 */
class PyResMgrImportLoader : public PyObjectPlus
{
	Py_Header( PyResMgrImportLoader, PyObjectPlus );

public:
	PyResMgrImportLoader( const std::string path, DataSectionPtr pDirectory,
		PyTypePlus * pType = &PyResMgrImportLoader::s_type_ );

	// PyObjectPlus overrides
	PyObject*			pyGetAttribute( const char * attr );
	int					pySetAttribute( const char * attr, PyObject * value );

	// Python Module Importer interface
	/* Check that we have a source for a given module name */
	PyObject* find_module( const std::string& path );
	PY_AUTO_METHOD_DECLARE( RETOWN, find_module, ARG( std::string, END ) );

	// Python Module Loader interface
	/* Return the module for a given module name */
	PyObject* load_module( const std::string& path );
	PY_AUTO_METHOD_DECLARE( RETOWN, load_module, ARG( std::string, END ) );

	// These are basically borrowed from Python's importdl.h, and should be in order
	// of increasing preference
	// PKG_DIRECTORY isn't preferenced, per se, we just take it if we get a respath
	// without suffix, that has an appropriate child.
	// PY_SOURCE is more preferred than PY_OBJECT since the PY_SOURCE handler will
	// load the PY_OBJECT if PY_SOURCE is older, and create a PY_OBJECT appropriately.
	// C_EXTENSION should probably be higher in the list, but we can't handle it,
	// so we don't want to take it if a PY or PYC/PYO is present
	typedef enum { NOT_FOUND, C_EXTENSION, PY_OBJECT, PY_SOURCE, PKG_DIRECTORY }
		pythonModuleType;
	typedef std::pair< const std::string, pythonModuleType > suffixLookup;
	typedef std::map< const std::string, pythonModuleType > suffixLookupMap;

private:
	const std::string path_;
	DataSectionPtr pDirectory_;

	// A cache of found modules, along with their type and data.
	// Entries in this cache only survive between a find_module for a given
	// name, and the related load_module.
	// A negative cache (NOT_FOUND) _does_ persist, in case we're asked for
	// it again later. This may not be neccessary.
	typedef std::pair< pythonModuleType, DataSectionPtr > moduleCacheEntry;
	typedef std::map< const std::string, moduleCacheEntry > moduleCache;
	moduleCache modules_;

	// A map of known suffixes, and their type
	// Whatever Script::init() does to flip Py_OptmizeFlag, should delete the
	// appropriate entry from this map.
	static suffixLookupMap s_suffixes_;
	// PyResMgrImportHook needs to touch s_suffixes_
	friend class PyResMgrImportHook;

	// The actual loading methods
	// These all emulate their namesakes in Python's import.c
	PyObject* load_package( const std::string& name, DataSectionPtr package );
	PyObject* load_compiled_module( const std::string& name, BinaryPtr pyc_data, bool known_good = false );
	PyObject* load_source_module( const std::string& name, BinaryPtr py_data, DataSectionPtr pDirectory );

	// Compiled module utility methods
	// Neither of these set PyErr or otherwise interact with Python
	bool check_compiled_module( const std::string& name, BinaryPtr pyc_data );
	bool check_compiled_module_mtime( const std::string& name, BinaryPtr pyc_data, time_t mtime );


	// Static utility methods
	// Find a module in a given directory
	static moduleCacheEntry find_module_file( const std::string& name, DataSectionPtr pDirectory );


};

typedef SmartPointer<PyResMgrImportLoader> PyResMgrImportLoaderPtr;

#ifdef CODE_INLINE
#include "res_mgr_import.ipp"
#endif

#endif // USE_RES_MGR_IMPORT_HOOK

#endif // RES_MGR_IMPORT_HPP
