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
#include "res_mgr_import.hpp"

#ifdef USE_RES_MGR_IMPORT_HOOK

#include "resmgr/bin_section.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/multi_file_system.hpp"

// This is for PyMarshal_*, and isn't in Python.h for some reason.
#include "marshal.h"

DECLARE_DEBUG_COMPONENT2( "Script", 0 )

#ifndef CODE_INLINE
#include "res_mgr_import.ipp"
#endif


// -----------------------------------------------------------------------------
// Section: Static methods
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Section: Script definition
// -----------------------------------------------------------------------------

/*~ class NoModule.PyResMgrImportHook
 *	@components{ all }
 *	A hook module that operates as a factory callable for PyResMgrImportLoader
 *  instances specialised for a particular entry in sys.paths.
 *	For internal BigWorld/Python integration operation.
 *  This module is callable, calls are forwarded to getImporter
 */
/* We have a "PyObject* _pyCall( PyObject*, PyObject*, PyObject* )" method */
PY_TYPEOBJECT_WITH_CALL( PyResMgrImportHook )

PY_BEGIN_METHODS( PyResMgrImportHook )
	PY_METHOD( getImporter )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( PyResMgrImportHook )
PY_END_ATTRIBUTES()


// -----------------------------------------------------------------------------
// Section: General methods
// -----------------------------------------------------------------------------

/**
 *	The constructor for PyResMgrImportHook.
 */
PyResMgrImportHook::PyResMgrImportHook( PyTypePlus * pType )
	: PyObjectPlus( pType )
{
//	TRACE_MSG( "PyResMgrImportHook::PyResMgrImportHook: %s.\n",
//		Py_OptimizeFlag?"Optimized":"Not optimized");
	if ( Py_OptimizeFlag )
	{
		PyResMgrImportLoader::s_suffixes_.erase( "pyc" );
	} else {
		PyResMgrImportLoader::s_suffixes_.erase( "pyo" );
	}

}

/**
 *	This method overrides the PyObjectPlus method.
 */
PyObject* PyResMgrImportHook::pyGetAttribute( const char* attr )
{
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}

/**
 *	This method overrides the PyObjectPlus method.
 */
int PyResMgrImportHook::pySetAttribute( const char* attr, PyObject* value )
{
	PY_SETATTR_STD();

	return PyObjectPlus::pySetAttribute( attr, value );
}


// -----------------------------------------------------------------------------
// Section: Callable methods
// -----------------------------------------------------------------------------

/*~ function PyResMgrImportHook.getImporter
 *	@components{ all }
 *
 *	This method returns an instance of PyResMgrImportLoader for the specified
 *  path or None if that path doesn't exist or looks like a folder that
 *  holds C Extensions.
 *
 *	@param	path	A string containing the resource path to produce a loader for.
 *
 *	@return			PyResMgrImportLoader instance if successful, or None otherwise.
 */
/**
 *	This method returns a PyResMgrImportLoader if the path exists and is usable,
 *  or Py_None otherwise.
 *
 *	@param path		A string containing the resource path to produce a loader for.
 */
PyObject* PyResMgrImportHook::getImporter( std::string& path )
{
//	TRACE_MSG( "PyResMgrImportHook::getImporter: Looking for %s\n",
//		path.c_str() );
	DataSectionPtr pDataSection = BWResource::openSection( path );
	if ( !pDataSection )
	{
		PyErr_Format( PyExc_ImportError, "No such Path" );
		return NULL;
	}
	if ( path.find( "DLL" ) != std::string::npos ||
		path.find( "lib-dynload" ) != std::string::npos )
	{
//		WARNING_MSG( "PyResMgrImportHook::getImporter: %s: "
//			"We can't handle C_EXTENSIONS, leaving path for default "
//			"Python import routines\n", path.c_str() );
		Py_Return;
	}
	
	return new PyResMgrImportLoader( path, pDataSection );
}

// -----------------------------------------------------------------------------
// Section: Script methods
// -----------------------------------------------------------------------------


/* PyResMgrImportLoader */

// -----------------------------------------------------------------------------
// Section: Static methods
// -----------------------------------------------------------------------------

// Initialise suffixLookups
// http://www.daniweb.com/forums/post199050-7.html
// Boost.assign provides map_list_of, which is much cleaner looking.
template <typename T, int N>
char (&array(T(&)[N]))[N];

PyResMgrImportLoader::suffixLookup pythonSuffixes[] = {
	PyResMgrImportLoader::suffixLookup( "py", PyResMgrImportLoader::PY_SOURCE ),
	PyResMgrImportLoader::suffixLookup( "pyc", PyResMgrImportLoader::PY_OBJECT ),
	PyResMgrImportLoader::suffixLookup( "pyo", PyResMgrImportLoader::PY_OBJECT ),
#if MF_SERVER
	// Not yet supported, we can't dlopen a memory block.
	PyResMgrImportLoader::suffixLookup( "so", PyResMgrImportLoader::C_EXTENSION )
#else // MF_SERVER
	// Not yet supported, we can't load a pyd file unless we are linking to a
	// Python DLL, and as of this writing, we are not doing that.
	// Also, we can't LoadLibraryEx a memory block
	PyResMgrImportLoader::suffixLookup( "pyd", PyResMgrImportLoader::C_EXTENSION )
#endif // MF_SERVER
};
PyResMgrImportLoader::suffixLookupMap
	PyResMgrImportLoader::s_suffixes_( pythonSuffixes,
		pythonSuffixes + sizeof array( pythonSuffixes ) );

// -----------------------------------------------------------------------------
// Section: Script definition
// -----------------------------------------------------------------------------

/*~ class NoModule.PyResMgrImportLoader
 *	@components{ all }
 *	An implementation of the PEP 302 Importer Protocol that loads Python Source
 *  and Python Object files from ResMgr, produced by PyResMgrImportHook.
 *	For internal BigWorld/Python integration operation.
 */
PY_TYPEOBJECT( PyResMgrImportLoader )

PY_BEGIN_METHODS( PyResMgrImportLoader )
	PY_METHOD( find_module )
	PY_METHOD( load_module )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( PyResMgrImportLoader )
PY_END_ATTRIBUTES()


// -----------------------------------------------------------------------------
// Section: General methods
// -----------------------------------------------------------------------------

/**
 *	The constructor for PyResMgrImportLoader
 */
 PyResMgrImportLoader::PyResMgrImportLoader( const std::string path,
	DataSectionPtr pDirectory, PyTypePlus * pType )	:
	PyObjectPlus( pType ),
	path_( path ),
	pDirectory_( pDirectory )

{
//	TRACE_MSG( "PyResMgrImportLoader(%s)::PyResMgrImportLoader\n",
//		path_.c_str() );
}

/**
 *	This method overrides the PyObjectPlus method.
 */
PyObject* PyResMgrImportLoader::pyGetAttribute( const char* attr )
{
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}

/**
 *	This method overrides the PyObjectPlus method.
 */
int PyResMgrImportLoader::pySetAttribute( const char* attr, PyObject* value )
{
	PY_SETATTR_STD();

	return PyObjectPlus::pySetAttribute( attr, value );
}


// -----------------------------------------------------------------------------
// Section: Script methods
// -----------------------------------------------------------------------------

/**
 * This static routine identifies a Python module for the given name in
 * the supplied DataSection, returning the relevant DataSectionPtr and
 * the type of the module.
 *
 *	@param	name 	The name of the python module to find
 *
 *	@param	pDirectory	The DataSection to locate the module within
 *
 *	@return	A pair of < pythonModuleType, DataSectionPtr > for the requested
 *  module file, or < NOT_FOUND, NULL > if one could not be found.
 */
PyResMgrImportLoader::moduleCacheEntry PyResMgrImportLoader::find_module_file(
	const std::string& name, DataSectionPtr pDirectory )
{
	moduleCacheEntry result;
	result.first = NOT_FOUND;

	DataSectionPtr package = pDirectory->findChild( name );
	if ( package )
	{
		// packageInit is basically a wasted load here. It'll be needed
		// soon enough.
		moduleCacheEntry packgeInit = find_module_file( "__init__", package );
		switch( packgeInit.first ) {
			case PY_SOURCE:
			case PY_OBJECT:
			case C_EXTENSION:
				return std::make_pair( PKG_DIRECTORY, package );
				break;
			case NOT_FOUND:
			case PKG_DIRECTORY:
				break;
		}
	}

	suffixLookupMap::const_iterator suffixIt;
	for ( suffixIt = s_suffixes_.begin(); suffixIt != s_suffixes_.end(); suffixIt++ )
	{
		DataSectionPtr pCandidate = pDirectory->openSection( name + "." + suffixIt->first );
		if ( !pCandidate )
			continue;

		// This shouldn't happen...
		MF_ASSERT( suffixIt->second != PKG_DIRECTORY );

		// We have a match, check if it's a better match than any known match
		if ( suffixIt->second > result.first )
		{
			result.first = suffixIt->second;
			result.second = pCandidate;
		}
	}

	return result;

}

/*~ function PyResMgrImportHook.find_module
 *	@components{ all }
 *
 *	This method implements the PEP 302 Importer Protocol's find_module
 *  method, returning a loader (ourselves) that can handle the named
 *  module, or None if we can't handle it.
 *
 *	@param	name	A string containing the full module name to find.
 *
 *	@return			PyResMgrImportLoader instance if found, or None otherwise.
 */
/**
 *	This method returns ourselves if we are able to load the supplied module name,
 *  or Py_None otherwise.
 *
 *	@param name 	A string containing the full module name to find.
 */
/*
 * name will be the fully-qualified package name in question, but
 * we are the loader for a given directory (eg. package) so we should
 * only get this if we are already the correct path for a given module
 * We cache by fully-qualified name though, since that's what we need
 * everywhere else.
 */
PyObject* PyResMgrImportLoader::find_module( const std::string& name )
{
//	TRACE_MSG( "PyResMgrImportLoader(%s)::find_module: %s\n", path_.c_str(),
//		name.c_str() );

	std::string moduleName = name;
	std::string::size_type dotPos = name.rfind( "." );
	if ( dotPos != std::string::npos )
	{
		moduleName = name.substr( dotPos + 1 );
	}

	moduleCache::const_iterator moduleIt = modules_.find( name );
	// If we haven't already cached this module, do so now
	if ( moduleIt == modules_.end() )
	{
		modules_[ name ] = find_module_file( moduleName, pDirectory_ );
	}

	// Do we have a matching module?
	if ( modules_[ name ].first == NOT_FOUND )
	{
		Py_Return;
	}

	if ( modules_[ name ].first == C_EXTENSION )
	{
		ERROR_MSG( "PyResMgrImportLoader(%s)::find_module: Can't load module %s "
			"as a C extension\n", path_.c_str(), name.c_str() );
		Py_Return;
	}
	Py_INCREF( this );
	return this;
}

/*~ function PyResMgrImportHook.load_module
 *	@components{ all }
 *
 *	This method implements the PEP 302 Importer Protocol's load_module
 *  method, importing and returning the named module if possible or None
 *  if we can't handle it.
 *
 *	@param	name	A string containing the full module name to import.
 *
 *	@return			PyResMgrImportLoader instance if successful, or None otherwise.
 */
/**
 *	This method returns ourselves if we are able to load the named module,
 *  or Py_None otherwise.
 *
 *	@param name 	A string containing the full module name to import.
 */
/*
 * According to PEP 302, load_module has a number of responsibilities:
 * Note that load_package hands off to another module loader for handling __init__,
 * which will happily overwrite supplied values where it sees fit.
 * ) Module must be added to sys.modules before loading, and if there is
 *   already one there, use it.
 *  - load_package: We call PyImport_AddModule
 *  - load_compiled_module: We call PyImport_ExecCodeModuleEx
 *  - load_source_module: We call PyImport_ExecCodeModuleEx
 * ) __file__ must be set
 *  - load_package: We set it to the given file name
 *  - load_compiled_module: We pass it to PyImport_ExecCodeModuleEx
 *  - load_source_module: We pass it to PyImport_ExecCodeModuleEx
 * ) __name__ must be set (PyImport_AddModule handles this)
 *  - load_package: We call PyImport_AddModule
 *  - load_compiled_module: We call PyImport_ExecCodeModuleEx
 *  - load_source_module: We call PyImport_ExecCodeModuleEx
 * ) __path__ must be a list, if it's a package
 *  - load_package: We do this by hand, it gets sent back to getImporter
 * ) __loader__ should be set to the loader
 *  - load_package: We do this
 *  - load_compiled_module: We do this
 *  - load_source_module: We do this
 */
PyObject* PyResMgrImportLoader::load_module( const std::string& name )
{
//	TRACE_MSG( "PyResMgrImportLoader(%s)::load_module: %s\n", path_.c_str(),
//		name.c_str() );
	MF_ASSERT( modules_[ name ].first != NOT_FOUND );
	switch( modules_[ name ].first )
	{
		case PKG_DIRECTORY:
			return load_package( name, modules_[ name ].second );
		case PY_OBJECT:
			return load_compiled_module( name,
				modules_[ name ].second->asBinary() );
		case PY_SOURCE:
			return load_source_module( name,
				modules_[ name ].second->asBinary(),
				pDirectory_ );
		case NOT_FOUND:
		case C_EXTENSION:
			break;
	}
	Py_Return;
}

/**
 * This routine imports the named package into Python and returns it.
 *
 *	@param	name 	The name of the python module to find
 *
 *	@param	package	The DataSection representing the package
 *
 *	@return	The imported package
 */
/*
 * This routine needs to emulate load_package in import.c in
 * python, but from a DataSectionPtr
 */
PyObject* PyResMgrImportLoader::load_package( const std::string& name,
	DataSectionPtr package )
{
	// We don't erase ourselves from the modules_ list, since whatever
	// we call to process our __init__ script will do it for us.
	std::string moduleName = name;
	std::string::size_type dotPos = name.rfind( "." );
	if ( dotPos != std::string::npos )
	{
		moduleName = name.substr( dotPos + 1 );
	}

	PyObject *module = PyImport_AddModule( name.c_str() );
	if ( module == NULL )
	{
		// Propagate the PyErr up
		return NULL;
	}
	PyObject *moduleDict = PyModule_GetDict( module );
	PyObject *file = PyString_FromString( ( path_ + "/" + moduleName).c_str() );
	if ( file == NULL )
	{
		return NULL;
	}
	PyObject *path = Py_BuildValue( "[O]", file );
	if ( path == NULL )
	{
		Py_DECREF( file );
		return NULL;
	}
	int err = PyDict_SetItemString( moduleDict, "__file__", file );
	Py_DECREF( file );

	if ( err != 0 )
	{
		Py_DECREF( path );
		return NULL;
	}
	err = PyDict_SetItemString( moduleDict, "__path__", path );
	Py_DECREF( path );
	if ( err != 0 )
	{
		return NULL;
	}
	err = PyDict_SetItemString( moduleDict, "__loader__", this );
	if ( err != 0 )
	{
		return NULL;
	}

	// This call was tested in find_module_file earlier.
	moduleCacheEntry packageInit = find_module_file( "__init__", package );

//	TRACE_MSG( "PyResMgrImportLoader(%s)::load_package: processing %s\n",
//		path_.c_str(), name.c_str() );

	switch( packageInit.first )
	{
		case PY_OBJECT:
			return load_compiled_module( name,
				packageInit.second->asBinary() );
		case PY_SOURCE:
			return load_source_module( name,
				packageInit.second->asBinary(),
				package );
		case NOT_FOUND:
		case PKG_DIRECTORY:
		case C_EXTENSION:
			break;
	}
	Py_Return;
}

/**
 * This routine checks that the named module's Python Object data has the
 * same mtime as the requested mtime
 *
 * This function does not set a python error if it is invalid.
 *
 *	@param	name 	The name of the python module to check
 *
 *	@param	pyc_data	A BinaryPtr holding the data from the Python Object
 *
 *	@param	mtime	The mtime to test against
 *
 *	@return	Whether or not the file is considered up-to-date
 */
bool PyResMgrImportLoader::check_compiled_module_mtime( const std::string& name,
	BinaryPtr pyc_data, time_t mtime )
{
	MF_ASSERT_DEBUG( check_compiled_module( name, pyc_data ) );
	// We use != since SVN sends files backwards in time when reverting
	// XXX: On-disk format is little-endian, we're not checking that here
//	TRACE_MSG( "PyResMgrImportLoader::load_compiled_module_mtime: mtime 0x%016llx "
//		"with pyc 0x%08lx\n",
//		mtime, reinterpret_cast< const int32* >( pyc_data->data())[ 1 ] );
	
	// pyc files only have four bytes to store their .py file's modification time
	const int32 trimmed_mtime = static_cast< const int32 >( mtime );
	if ( reinterpret_cast< const int32* >(pyc_data->data())[ 1 ] == trimmed_mtime )
	{
		return true;
	}
	else
	{
		return false;
	}
}

/**
 * This routine checks that the named module's Python Object data is valid,
 * at least as far as the file header is concerned.
 *
 * This method will not set a Python error if the object is invalid.
 *
 *	@param	name 	The name of the python module to check
 *
 *	@param	pyc_data	A BinaryPtr holding the data from the Python Object
 *
 *	@param	mtime	Optional mtime to test against
 *
 *	@return	Whether or not the file is considered valid and up-to-date
 */
bool PyResMgrImportLoader::check_compiled_module( const std::string& name,
	BinaryPtr pyc_data )
{
	// Check it's not too short
	if ( pyc_data->len() < 8 )
	{
		return false;
	}
	// Check we have PYC magic

	// PyImport_GetMagicNumber() returns a long, but the on-disk
	// format is 4 bytes. If we don't do this unsigned, there's
	// a sign-extension risk. So we just truncate to 32-bits instead.
	// The current value as of Python 2.5.2 has the high-bit unset,
	// and that should never change.
	// If you decide to customise the PYC storage format, make sure
	// you change the magic number

	// XXX: On-disk format is little-endian, we're not checking that here
	if ( reinterpret_cast< const uint32* >(pyc_data->data())[ 0 ]
			!= static_cast< const uint32 >(PyImport_GetMagicNumber()) )
	{
		return false;
	}
	return true;
}

/**
 * This routine imports the named Python Object into Python and returns it.
 * Optionally, it will throw a PyExc_ImportError exception if the module exists
 * but does not have an mtime matching the specified mtime
 *
 *	@param	name 	The name of the python module to find
 *
 *	@param	pyc_data	A BinaryPtr holding the data from the Python Object
 *
 *	@param	known_valid	Optional flag to indicate we've already called
 *  check_compiled_module successfully on this pyc_data.
 *
 *	@return	The imported package
 */
/*
 * This routine needs to emulate load_compiled_module in import.c in
 * python, but with DataSectionPtr using FILE*
 */
PyObject* PyResMgrImportLoader::load_compiled_module( const std::string& name,
	BinaryPtr pyc_data, bool known_valid /* = false */ )
{
	std::string compiledExtension = "pyc";
	if ( Py_OptimizeFlag )
		compiledExtension = "pyo";
	std::string moduleName = name;
	std::string::size_type dotPos = name.rfind( "." );
	if ( dotPos != std::string::npos )
	{
		moduleName = name.substr( dotPos + 1 );
	}

	std::string modulePathStub;
	if ( modules_[ name ].first == PKG_DIRECTORY )
	{
		modulePathStub = path_ + "/" + moduleName + "/__init__";
	}
	else
	{
		modulePathStub = path_ + "/" + moduleName;
	}

	// Remove this from the cache. We have it now, and will feed it to Python.
	// This ensures that a reload() call will work correctly.
	modules_.erase( name );

	if ( !known_valid && !check_compiled_module( name, pyc_data ) )
	{
		PyErr_Format( PyExc_ImportError,
			"%s.%s is not a valid Python Object file",
			modulePathStub.c_str(), compiledExtension.c_str()
		);
		return NULL;
	}

	// The first four bytes are magic
	// the second four are the source modification date
	// This does the same thing as read_compiled_module in import.c
	PyObject* codeObject = PyMarshal_ReadObjectFromString(
		pyc_data->cdata() + 8, pyc_data->len() - 8 );
	if ( !PyCode_Check( codeObject ) )
	{
		PyErr_Format( PyExc_ImportError,
			"%s.%s is a non-code object",
			modulePathStub.c_str(), compiledExtension.c_str()
		);
		Py_DECREF( codeObject );
		return NULL;
	}
	PyObject* module = PyImport_ExecCodeModuleEx(
		const_cast< char* >( name.c_str() ), codeObject,
		const_cast< char* >( ( modulePathStub + compiledExtension ).c_str() )
		);
	Py_DECREF( codeObject );

	if ( module )
	{
		PyObject *moduleDict = PyModule_GetDict( module );
		int err = PyDict_SetItemString( moduleDict, "__loader__", this );
		if ( err != 0 )
		{
			Py_DECREF( module );
			return NULL;
		}

//		TRACE_MSG( "PyResMgrImportLoader(%s)::load_compiled_module: loaded %s\n",
//			path_.c_str(), name.c_str() );
	}
	return module;
}

/**
 * This routine imports the named Python Source into Python and returns it.
 * If an up to date Python Object is found, that will be used instead.
 * Otherwise, it will write out a Python Object if the import is successful
 *
 *	@param	name 	The name of the python module to find
 *
 *	@param	pyc_data	A BinaryPtr holding the data from the Python Object
 *
 *	@param	pDirectory	A DataSection where a related Python Object might be found
 *
 *	@return	The imported package
 */
/*
 * This routine needs to emulate load_source_module in import.c in
 * python, but with DataSectionPtr using FILE*
 * However, we can't get an mTime (there's no interface for it in DataSection)
 * Need to resolve this before we can commit
 */
PyObject* PyResMgrImportLoader::load_source_module( const std::string& name, BinaryPtr py_data, DataSectionPtr pDirectory )
{
	std::string compiledExtension = "pyc";
	if ( Py_OptimizeFlag )
		compiledExtension = "pyo";
	std::string moduleName = name;
	std::string::size_type dotPos = name.rfind( "." );
	if ( dotPos != std::string::npos )
	{
		moduleName = name.substr( dotPos + 1 );
	}

	// Find source (.py) and object (.pyc/.pyo) files to
	// process for this source module or package.
	std::string modulePath;
	bool isPackage = false;
	std::string objectModuleName;
	if ( modules_[ name ].first == PKG_DIRECTORY )
	{
		isPackage = true;
		modulePath = path_ + "/" + moduleName + "/__init__.py";
		objectModuleName = "__init__." + compiledExtension;
	} else {
		modulePath = path_ + "/" + moduleName + ".py";
		objectModuleName = moduleName + "." +  compiledExtension;
	}
	// Remove this from the cache. We have it now, and will feed it to Python.
	// This ensures that a reload() call will work correctly.
	modules_.erase( name );

	// Fetch mtime of py file
	time_t pyModTime = static_cast< time_t >( -1 );
	IFileSystem::FileInfo fInfo;
	IFileSystem::FileType fType =
		BWResource::instance().fileSystem()->getFileType( modulePath, &fInfo );
	if ( fType != IFileSystem::FT_NOT_FOUND )
	{
		pyModTime = fInfo.modified;
	}
	// If possible, palm this off to load_compiled_module
	DataSectionPtr pycSection = pDirectory->openSection( objectModuleName );
	if ( pycSection && check_compiled_module( name, pycSection->asBinary() ))
	{
		if ( check_compiled_module_mtime( name, pycSection->asBinary(), pyModTime ) )
		{
			// We know the module was valid and up-to-date, so trust the loader
			// to either load it or fail noisily
			return load_compiled_module( name, pycSection->asBinary(), true );
		}
	}
	if ( pycSection )
	{
		// Get rid of our reference to the old compiled python file.
		// TODO: Purge this section from the DataSection cache, we're about
		// to replace it on disk
		pycSection = NULL;
	}
	
	// We got here, the object file for this source either doesn't exist, isn't
	// valid, or isn't as recent as the source.
	// Emulate parse_source_module
	// The code string needs to have (\n) as a line seperator, and needs an EOF
	// (-1) or null termination, and has to end in a newline.
	// Also, need to ensure there's no embedded nulls.
	// So have to make a copy of the string.
	// We shouldn't ever do this in release anyway.
	std::string codeString( py_data->cdata(), py_data->len() );
	if ( codeString.find( '\0' ) != std::string::npos )
	{
		PyErr_Format( PyExc_ImportError,
			"%s contains an embedded null character",
			modulePath.c_str()
		);
		return NULL;
	}
	std::string::size_type winNLpos;
	// Convert any Windows newlines into UNIX newlines
	if ( ( winNLpos = codeString.find( "\r\n", 0 ) ) != std::string::npos )
	{
		do
		{
			codeString.replace( winNLpos, 2, "\n" );
			winNLpos = codeString.find( "\r\n", winNLpos );
		} while ( winNLpos != std::string::npos );
	}
	// Ensure we're newline-terminated
	codeString.append( "\n" );
	PyObject* codeObject = Py_CompileString( codeString.c_str(),
		const_cast< char* >( modulePath.c_str() ),
		Py_file_input );
	if ( codeObject == NULL )
		// Compiler didn't like it. Propagate the error up
		return NULL;

	// OK, we have a module, now we just execute it into the correct space.
	// Always call it a .py, even though we've created a .pyc
	PyObject* module = PyImport_ExecCodeModuleEx(
		const_cast< char* >( name.c_str() ), codeObject,
		const_cast< char* >( modulePath.c_str() )
		);

	if ( module == NULL )
	{
		Py_DECREF( codeObject );
		return NULL;
	}

	// It executed OK, so write out an object file for later use, if possible
	// Emulates write_compiled_module( co, cpathname, mtime )
	do {
		PyObject* codeString = PyMarshal_WriteObjectToString( codeObject, Py_MARSHAL_VERSION );
		// XXX: Maybe we should care if _this_ fails, or at least report it?
		if ( codeString == NULL || !PyString_Check( codeString ) )
		{
			PyErr_Clear();
			break;
		}
		char* dataBlock = new char[ PyString_Size( codeString ) + 8 ];
		// XXX: On-disk format is little-endian, we're not checking that here
		reinterpret_cast< uint32* >(dataBlock)[ 0 ] = PyImport_GetMagicNumber();
		reinterpret_cast< int32* >(dataBlock)[ 1 ] = static_cast< int32 >( pyModTime );
		memcpy( dataBlock + 8, PyString_AsString( codeString ), PyString_Size( codeString ) );

		// The following is a little nasty, we end up copying the data a couple of times
		// Wrap dataBlock in a BinaryBlock (which takes a copy of it)
		BinaryPtr pycData = new BinaryBlock( dataBlock,
			PyString_Size( codeString ) + 8,
			"PyResMgrImportLoader::load_source_module" );
		delete[] dataBlock;
		if ( !pycData )
			break;
		// Save out our new pyc file
		pycSection = pDirectory->openSection( objectModuleName, true, BinSection::creator() );
		if ( !pycSection )
			break;
		pycSection->setBinary( pycData );
		pycSection->save();
	} while( false );

	Py_DECREF( codeObject );

	PyObject *moduleDict = PyModule_GetDict( module );
	if ( moduleDict != NULL )
	{
		int err = PyDict_SetItemString( moduleDict, "__loader__", this );
		if ( err != 0 )
		{
			Py_DECREF( module );
			return NULL;
		}
	}

//	TRACE_MSG( "PyResMgrImportLoader(%s)::load_source_module: loaded %s\n", path_.c_str(),
//		name.c_str() );
	return module;
}

#endif // USE_RES_MGR_IMPORT_HOOK

// py_data_section.cpp
