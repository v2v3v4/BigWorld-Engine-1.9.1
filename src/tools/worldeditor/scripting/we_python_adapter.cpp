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
#include "worldeditor/scripting/we_python_adapter.hpp"
#include "worldeditor/terrain/texture_mask_cache.hpp"
#include "worldeditor/gui/controls/limit_slider.hpp"
#include "pyscript/script.hpp"
#include "resmgr/bwresource.hpp"
#include <afxcmn.h>


DECLARE_DEBUG_COMPONENT2( "PythonAdapter", 0 )


WEPythonAdapter::WEPythonAdapter() :
	PythonAdapter()
{
}


WEPythonAdapter::~WEPythonAdapter()
{
}


void WEPythonAdapter::onBrowserObjectItemSelect( const std::string& theTabName, 
		const std::string& itemName, bool dblClick )
{
	if ( !proActive_ || !pScriptObject_ )
		return;

    std::string fnName( "brwObject" );
	fnName += theTabName + "ItemSelect";
    std::string selectedFile( itemName );

    if ( !itemName.empty() )
    {
        // Get the relative path, but preserve the case on the filename
		selectedFile = BWResource::getFilePath(
            BWResource::dissolveFilename( itemName )) +
            BWResource::getFilename( itemName );
    }


	// tell the python which object is selected
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
                const_cast<char*>( fnName.c_str() ) );

    if (pFunction != NULL)
    {
		Script::call(
	       	pFunction,
            Py_BuildValue( "(si)",
			selectedFile.c_str(), dblClick ? 1 : 0 ),
            "PythonAdapter::onBrowserObjectItemSelect: " );
    }
    else
	{
		PyErr_Clear();
	    Script::call(
			PyObject_GetAttrString( pScriptObject_,
				"brwObjectItemSelect" ),
			Py_BuildValue( "(ss)",
                theTabName.c_str(),
                selectedFile.c_str() ),
			"PythonAdapter::onBrowserObjectItemSelect: " );
    }


	// update the selection criteria
	std::string tabName = "tabObject" + theTabName;
	Script::call( 
		PyObject_GetAttrString( pScriptObject_,
			"pgcObjectsTabSelect" ),
		Py_BuildValue( "(s)",
			tabName.c_str() ),
		"PythonAdapter::onBrowserObjectItemSelect: " );
}

void WEPythonAdapter::onBrowserObjectItemAdd()
{
	if ( !proActive_ || !pScriptObject_ )
		return;

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_, "brwObjectItemAdd");

    if (pFunction != NULL)
    {
		Script::call(
	       	pFunction,
            Py_BuildValue( "()" ),
            "PythonAdapter::onBrowserObjectItemAdd: " );
    }
}

void WEPythonAdapter::onPageControlTabSelect( const std::string& fnPrefix, const std::string& theTabName )
{
	if ( !proActive_ || !pScriptObject_ )
		return;

    const std::string fnName = fnPrefix + "TabSelect";
	const std::string tabName = "tab" + theTabName;

    PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
                const_cast<char*>( fnName.c_str() ) );

    if ( pFunction != NULL )
    {
        Script::call( pFunction,
                   		Py_BuildValue( "(s)",
               			tabName.c_str() ),
                   		"PythonAdapter::onPageControlTabSelect: " );
    }
    else
	{
		PyErr_Clear();
        Script::call(
            PyObject_GetAttrString( pScriptObject_,
                "pgcAllToolsTabSelect" ),
            Py_BuildValue( "(s)",
                tabName.c_str() ),
            "PythonAdapter::onPageControlTabSelect: " );
    }
}

void WEPythonAdapter::onBrowserTextureItemSelect( const std::string& itemName )
{
	if ( !proActive_ || !pScriptObject_ )
		return;

	const std::string fnName( "brwTexturesItemSelect" );
    std::string selectedFile( itemName );

    if ( !itemName.empty() )
    {
        // Get the relative path, but preserve the case on the filename
		selectedFile = BWResource::getFilePath(
            BWResource::dissolveFilename( itemName )) +
            BWResource::getFilename( itemName );
    }


	// tell the python which object is selected
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
                const_cast<char*>( fnName.c_str() ) );

    if (pFunction != NULL)
    {
		Script::call(
	       		pFunction,
				Py_BuildValue( "(s)",
				selectedFile.c_str() ),
				"PythonAdapter::onBrowserTextureItemSelect: " );
    }
    else
	{
		ERROR_MSG( "script call [%s] does not exist\n", fnName.c_str() );
		PyErr_Clear();
    }
}


void WEPythonAdapter::setTerrainPaintMode(int mode)
{
	if ( !proActive_ || !pScriptObject_ )
		return;

    std::string fnName( "setTerrainPaintMode" );

	PyObject * pFunction = 
        PyObject_GetAttrString
        ( 
            pScriptObject_,
            fnName.c_str() 
        );

    if (pFunction != NULL)
    {
		Script::call
        (
            pFunction,
            Py_BuildValue("(i)", mode),
            "PythonAdapter::setTerrainPaintMode: " 
        );
    }
    else
	{
		ERROR_MSG( "script call [%s] does not exist\n", fnName.c_str() );
		PyErr_Clear();
    }
}


void WEPythonAdapter::setTerrainPaintBrush( TerrainPaintBrushPtr paintBrush )
{
	if ( !proActive_ || !pScriptObject_ )
		return;

    std::string fnName( "setTerrainPaintBrush" );

	PyObject * pFunction = 
        PyObject_GetAttrString
        ( 
            pScriptObject_,
            fnName.c_str() 
        );

    if (pFunction != NULL)
    {
		Script::call
        (
            pFunction,
            Py_BuildValue("(O)", paintBrush.getObject()),
            "PythonAdapter::setTerrainPaintBrush: " 
        );
    }
    else
	{
		ERROR_MSG( "script call [%s] does not exist\n", fnName.c_str() );
		PyErr_Clear();
    }
}


void WEPythonAdapter::onLimitSliderAdjust( const std::string& name, float pos, float min, float max )
{
	if ( !proActive_ || !pScriptObject_ )
		return;

	std::string fnName = name + "Adjust";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction != NULL )
	{
		Script::call( pFunction,
			Py_BuildValue(
				"(fff)",
				(float)pos,
				(float)min,
				(float)max ),
			"PythonAdapter::onSliderAdjust: " );
	}
	else
	{
		PyErr_Clear();
		Script::call(
			PyObject_GetAttrString( pScriptObject_,
				"onSliderAdjust" ),
			Py_BuildValue(
				"(sfff)",
				name.c_str(),
				(float)pos,
				(float)min,
				(float)max ),
			"PythonAdapter::onSliderAdjust: " );
	}
}

void WEPythonAdapter::limitSliderUpdate( LimitSlider* control, const std::string& controlName )
{
	if ( !pScriptObject_ )
		return;

    proActive_ = false;
    
	std::string fnName = controlName + "Update";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::sliderUpdate: " );
	}
	else
	{
		PyErr_Clear();
	}

	if (pResult != NULL)
	{
		if (PyFloat_Check( pResult ))
		{
			control->setValue((float)PyFloat_AsDouble( pResult ));
		}
		else if (PyInt_Check( pResult ))
		{
			control->setValue((float)PyInt_AsLong( pResult ));
		}
		else
		{
			float value;
			float min;
			float max;

			if (PyArg_ParseTuple( pResult, "fff", &value, &min, &max ))
			{
				control->setRange( min, max );
				control->setValue( value);
			}
			else
			{
				ERROR_MSG( "PythonAdapter::sliderUpdate - %s did not return a float (or three).\n",
					controlName.c_str() );
				PyErr_Clear();
			}
		}

		Py_DECREF( pResult );
	}

    proActive_ = true;
}


void WEPythonAdapter::selectFilterChange( const std::string& value )
{
	if ( proActive_ && pScriptObject_ )
	{
		const std::string fnName( "cmbSelectFilterChange" );

		PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
					const_cast<char*>( fnName.c_str() ) );

		if ( pFunction != NULL )
		{
			Script::call( pFunction,
					Py_BuildValue( "(s)",
						value.c_str() ),
					"PythonAdapter::selectFilterChange: " );
		}
	}
}


void WEPythonAdapter::selectFilterUpdate( CComboBox* comboList )
{
	if ( !pScriptObject_ )
		return;

    proActive_ = false;
    
	if (comboList->GetCount() == 0)
		fillFilterKeys( comboList );

	std::string fnName = "cmbSelectFilterUpdate";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::selectFilterUpdate: " );
	}
	else
	{
		PyErr_Clear();
	}

	if (pResult != NULL)
	{
		char * filterValue;
		if (PyArg_ParseTuple( pResult, "s", &filterValue ))
		{
			int index = comboList->FindStringExact( -1, filterValue );
			if( index != comboList->GetCurSel() )
				comboList->SetCurSel( index );
		}
		else
		{
			ERROR_MSG( "PythonAdapter::selectFilterUpdate - "
						"%s did not return a tuple of one string.\n",
						fnName.c_str() );
			PyErr_Clear();
		}

		Py_DECREF( pResult );
	}

    proActive_ = true;
}


void WEPythonAdapter::fillFilterKeys( CComboBox* comboList )
{
	if ( !pScriptObject_ )
		return;

    proActive_ = false;
    
	std::string fnName = "cmbSelectFilterKeys";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::selectFilterKeys: " );
	}
	else
	{
		PyErr_Clear();
	}

	if (pResult != NULL)
	{
		MF_ASSERT(PyTuple_Check( pResult ));
		int size = PyTuple_Size( pResult );
		for (int i = 0; i < size; i++)
		{
			PyObject * pItem = PyTuple_GetItem( pResult, i );
			MF_ASSERT(PyTuple_Check( pItem ));
			comboList->InsertString( i, PyString_AsString( PyTuple_GetItem( pItem, 0 ) ) );
		}

		Py_DECREF( pResult );
	}

    proActive_ = true;
}


void WEPythonAdapter::coordFilterChange( const std::string& value )
{
	if ( proActive_ && pScriptObject_ )
	{
		const std::string fnName( "cmbCoordFilterChange" );

		PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
					const_cast<char*>( fnName.c_str() ) );

		if ( pFunction != NULL )
		{
			Script::call( pFunction,
					Py_BuildValue( "(s)",
						value.c_str() ),
					"PythonAdapter::coordFilterChange: " );
		}
	}
}


void WEPythonAdapter::coordFilterUpdate( CComboBox* comboList )
{
	if ( !pScriptObject_ )
		return;

    proActive_ = false;
    
	if (comboList->GetCount() == 0)
		fillCoordFilterKeys( comboList );

	std::string fnName = "cmbCoordFilterUpdate";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::coordFilterUpdate: " );
	}
	else
	{
		PyErr_Clear();
	}

	if (pResult != NULL)
	{
		char * filterValue;
		if (PyArg_ParseTuple( pResult, "s", &filterValue ))
		{
			int index = comboList->FindStringExact( -1, filterValue );
			comboList->SetCurSel( index == CB_ERR ? 0 : index );
		}
		else
		{
			ERROR_MSG( "PythonAdapter::coordFilterUpdate - "
						"%s did not return a tuple of one string.\n",
						fnName.c_str() );
			PyErr_Clear();
		}

		Py_DECREF( pResult );
	}

    proActive_ = true;
}


void WEPythonAdapter::fillCoordFilterKeys( CComboBox* comboList )
{
	if ( !pScriptObject_ )
		return;

    proActive_ = false;
    
	std::string fnName = "cmbCoordFilterKeys";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::coordFilterKeys: " );
	}
	else
	{
		PyErr_Clear();
	}

	if (pResult != NULL)
	{
		MF_ASSERT(PyTuple_Check( pResult ));
		int size = PyTuple_Size( pResult );
		for (int i = 0; i < size; i++)
		{
			PyObject * pItem = PyTuple_GetItem( pResult, i );
			MF_ASSERT(PyTuple_Check( pItem ));
			comboList->InsertString( i, PyString_AsString( PyTuple_GetItem( pItem, 0 ) ) );
		}

		Py_DECREF( pResult );
	}

    proActive_ = true;
}


void WEPythonAdapter::projectLock( const std::string& commitMessage )
{
	const std::string fnName = "projectLock";
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction == NULL )
		return;

	Script::call( pFunction, Py_BuildValue( "(s)", commitMessage.c_str() ),
			"PageProject::OnBnClickedProjectSelectionLock: " );
}


void WEPythonAdapter::commitChanges(const std::string& commitMessage, bool keepLocks)
{
	const std::string fnName = "projectCommitChanges";
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction == NULL )
		return;

	Script::call( pFunction, Py_BuildValue( "(si)", commitMessage.c_str(), keepLocks ),
			"PageProject::OnBnClickedProjectCommitAll: " );
}


void WEPythonAdapter::discardChanges(const std::string& commitMessage, bool keepLocks)
{
	const std::string fnName = "projectDiscardChanges";
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction == NULL )
		return;

	Script::call( pFunction, Py_BuildValue( "(si)", commitMessage.c_str(), keepLocks ),
			"PageProject::OnBnClickedProjectDiscardAll: " );
}


void WEPythonAdapter::updateSpace()
{
	const std::string fnName = "projectUpdateSpace";
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction == NULL )
		return;

	Script::call( pFunction, PyTuple_New(0),
			"PageProject::OnBnClickedProjectUpdate: " );
}


void WEPythonAdapter::calculateMap()
{
	const std::string fnName = "projectCalculateMap";
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction != NULL )
	{
		Script::call( pFunction,
				PyTuple_New(0),
				"PythonAdapter::calculateMap: " );
	}
	else
	{
		PyErr_Clear();
	}
}


void WEPythonAdapter::exportMap()
{
	const std::string fnName = "projectExportMap";
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction != NULL )
	{
		Script::call( pFunction,
				PyTuple_New(0),
				"PythonAdapter::exportMap: " );
	}
	else
	{
		PyErr_Clear();
	}
}


bool WEPythonAdapter::canSavePrefab()
{
	if ( pScriptObject_ )
    {
		const std::string fnName = "canSavePrefab";

		PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
					const_cast<char*>( fnName.c_str() ) );

		if (pFunction != NULL)
		{
			PyObject * pResult = Script::ask(
				pFunction,
				Py_BuildValue( "()" ),
				"PythonAdapter::canSavePrefab: " );
				
			if (pResult != NULL)
			{
				if(PyInt_Check( pResult ))
				{
					return (int)PyInt_AsLong( pResult ) != 0;
				}
				else
				{
					ERROR_MSG( "PythonAdapter::canSavePrefab - canSavePrefab did not return an int.\n" );
					PyErr_Print();
				}
				Py_DECREF( pResult );
			}
		}
		else
		{
			PyErr_Clear();
		}
    }
	return false;
}


void WEPythonAdapter::saveSelectionPrefab( std::string fileName )
{
	if ( pScriptObject_ )
    {
		const std::string fnName = "savePrefab";

		PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
					const_cast<char*>( fnName.c_str() ) );

		if (pFunction != NULL)
		{
			Script::call(
				pFunction,
				Py_BuildValue( "(s)",
				fileName.c_str() ),
				"PythonAdapter::saveSelectionPrefab: " );
		}
    }
}
