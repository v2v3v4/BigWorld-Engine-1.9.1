/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/*~ module ModelEditor
 *	@components{ modeleditor }
 *
 *	The ModelEditor Module is a Python module that provides an interface to
 *	the various information about the model(s) loaded into ModelEditor.
 *	It also provides an interface to change and edit model-specific information
 *	and the various ModelEditor preferences.
 */

#include "pch.hpp"

#include "main_frm.h"

#include "model_editor.h"
#include "me_shell.hpp"
#include "material_properties.hpp"

#include "common/file_dialog.hpp"

#include "appmgr/options.hpp"
#include "appmgr/commentary.hpp"

#include "resmgr/string_provider.hpp"
#include "resmgr/auto_config.hpp"

#include "physics2/material_kinds.hpp"

#include "pyscript/py_data_section.hpp"

#include "panel_manager.hpp"

DECLARE_DEBUG_COMPONENT( 0 )
#include "me_error_macros.hpp"

#include "me_app.hpp"

static AutoConfigString s_defaultFloor( "system/defaultFloorTexture" );

MeApp* MeApp::s_instance_ = NULL;

MeApp::MeApp()
{
	ASSERT(s_instance_ == NULL);
	s_instance_ = this;
	
	// We need this initialised for the Objects and Materials pages.
	bool material_initted = MaterialKinds::init();
	ASSERT( material_initted );
	//We need to call this so that we can set material properties
	runtimeInitMaterialProperties();

	bool groundModel = !!Options::getOptionInt( "settings/groundModel", 0 );
	bool centreModel = !!Options::getOptionInt( "settings/centreModel", 0 );

	floor_ = new Floor ( Options::getOptionString("settings/floorTexture", s_defaultFloor ) );
	mutant_ = new Mutant( groundModel, centreModel );
	lights_ = new Lights;

	blackLight_ = new Moo::LightContainer;
	blackLight_->ambientColour( Moo::Colour( 0.f ,0.f, 0.f, 1.f ) );

	whiteLight_ = new Moo::LightContainer;
	whiteLight_->ambientColour( Moo::Colour( 1.f, 1.f, 1.f, 1.f ) );

	initCamera();
}

MeApp::~MeApp()
{
	delete floor_;
	delete mutant_;
	delete lights_;

	MaterialKinds::fini();
	s_instance_ = NULL;
}

void MeApp::initCamera()
{
	camera_ = ToolsCameraPtr(new ToolsCamera(), true);
	camera_->windowHandle( MeShell::instance().hWndGraphics() );
	std::string speedName = Options::getOptionString( "camera/speed", "Slow" );
	camera_->speed( Options::getOptionFloat( "camera/speed/" + speedName, 1.f ) );
	camera_->turboSpeed( Options::getOptionFloat( "camera/speed/" + speedName + "/turbo", 2.f ) );
	camera_->mode( Options::getOptionInt( "camera/mode", 0 ) );
	camera_->invert( !!Options::getOptionInt( "camera/invert", 0 ) );
	camera_->rotDir( Options::getOptionInt( "camera/rotDir", -1 ) );
	camera_->orbitSpeed( Options::getOptionFloat( "camera/orbitSpeed", 1.f ) );

	if (!!Options::getOptionInt( "startup/loadLastModel", 1 ))
	{
		camera_->origin( Options::getOptionVector3(
			"startup/lastOrigin",
			camera_->origin() ) );
	}
	
	camera_->view( Options::getOptionMatrix34(
		"startup/lastView",
		camera_->view() ));

	camera_->setAnimateZoom( !!Options::getOptionInt( "settings/animateZoom", 1 ) );

	camera_->render();
}

Floor* MeApp::floor()
{
	return floor_;
}

Mutant* MeApp::mutant()
{
	return mutant_;
}

Lights* MeApp::lights()
{
	return lights_;
}

ToolsCameraPtr MeApp::camera()
{
	return camera_;
}

void MeApp::saveModel()
{
	ME_INFO_MSG( L("MODELEDITOR/APP/ME_APP/SAVING", mutant_->modelName() ) );

	// Regen the visibility box if needed...
	if (mutant_->visibilityBoxDirty())
	{
		// Make sure we get a redraw first
		AfxGetApp()->m_pMainWnd->Invalidate();
		AfxGetApp()->m_pMainWnd->UpdateWindow();
		
		// Now regen the visibility box
		CModelEditorApp::instance().OnFileRegenBoundingBox(); 
	}

	mutant_->save();
}

void MeApp::saveModelAs()
{
	// Regen the visibility box if needed...
	if (mutant_->visibilityBoxDirty())
	{
		// Make sure we get a redraw first
		AfxGetApp()->m_pMainWnd->Invalidate();
		AfxGetApp()->m_pMainWnd->UpdateWindow();
		
		// Now regen the visibility box
		CModelEditorApp::instance().OnFileRegenBoundingBox(); 
	}

	static char BASED_CODE szFilter[] =	"Model (*.model)|*.model||";
	BWFileDialog fileDlg (FALSE, "", "", OFN_OVERWRITEPROMPT, szFilter);

	std::string modelDir;
	MRU::instance().getDir("models", modelDir );
	fileDlg.m_ofn.lpstrInitialDir = modelDir.c_str();

	if ( fileDlg.DoModal() == IDOK )
	{
		std::string modelFile = BWResource::dissolveFilename( std::string( fileDlg.GetPathName() ));

		if (BWResource::validPath( modelFile ))
		{
			if (mutant_->saveAs( modelFile ))
			{
				MRU::instance().update( "models", modelFile, true );
				CModelEditorApp::instance().updateRecentList( "models" );

				PanelManager::instance().ualAddItemToHistory( modelFile );

				// Forcefully update any gui stuff
				CMainFrame::instance().updateGUI( true );
			}
			else
			{
				ERROR_MSG( "Cannot determine the visual type of the model\"%s\".\n"
					"Unable to save model.", mutant_->modelName().c_str() );
			}
		}
		else
		{
			::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
				L("MODELEDITOR/APP/ME_APP/BAD_DIR"),
				L("MODELEDITOR/APP/ME_APP/UNABLE_RESOLVE"),
				MB_OK | MB_ICONWARNING );
		}
	}
	
	
}


/**
 * This method can clear any records of changes
 */
void MeApp::forceClean()
{
	mutant_->forceClean();
}


/**
 * This method can return if there are any changes need to be saved
 * @return true if there are any changes, false otherwise.
 */
bool MeApp::isDirty() const
{
	return mutant_->dirty();
}


bool MeApp::canExit( bool quitting )
{
	//Hmm... A bit of a hack but fields are updated on loss of focus,
	//thus we need to do this focus juggle to commit any changes made.
	HWND currItem = GetFocus();
	SetFocus( AfxGetApp()->m_pMainWnd->GetSafeHwnd() );
	SetFocus( currItem );

	bool isSaving = false;
	if (mutant_->dirty())
	{
		int result = ::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
			L("MODELEDITOR/APP/ME_APP/MODEL_CHANGED_Q"),
			L("MODELEDITOR/APP/ME_APP/MODEL_CHANGED"), MB_YESNOCANCEL | MB_ICONWARNING );
		if( result == IDCANCEL )
		{
			return false;
		}
		if( result == IDYES )
		{
			isSaving = true;
			saveModel();
		}
		if( result == IDNO )
		{
			ME_WARNING_MSG( L("MODELEDITOR/APP/ME_APP/MODEL_NOT_SAVED") );
		}
	}

	mutant_->saveCorrectPrimitiveFile( isSaving );

	if ((quitting) && (lights_->dirty()))
	{
		int result = ::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
			L("MODELEDITOR/APP/ME_APP/LIGHTS_CHANGED_Q"),
			L("MODELEDITOR/APP/ME_APP/LIGHTS_CHANGED"), MB_YESNOCANCEL | MB_ICONWARNING );
		if( result == IDCANCEL )
		{
			return false;
		}
		if( result == IDYES )
		{
			return lights_->save();
		}
		if( result == IDNO )
		{
			ME_WARNING_MSG( L("MODELEDITOR/APP/ME_APP/LIGHTS_NOT_SAVED") );
		}
	}

	if (quitting)
	{
		ME_INFO_MSG( L("MODELEDITOR/APP/ME_APP/EXITING") );
	}

	return true;
}

/*~ function ModelEditor.isModelLoaded
 *	@components{ modeleditor }
 *
 *  This function checks whether there currently is a loaded model.
 *
 *	@return Returns True (1) if a model is currently loaded, False (0) otherwise.
 */
static PyObject * py_isModelLoaded( PyObject * args )
{
	return PyInt_FromLong( MeApp::instance().mutant()->modelName() != "" );
}
PY_MODULE_FUNCTION( isModelLoaded, ModelEditor )

/*~ function ModelEditor.isModelDirty
 *	@components{ modeleditor }
 *
 *	This function checks whether the model is dirty.
 *	A dirty model is a model that has been modified and not yet saved.
 *
 *	@return Returns True (1) if the model is dirty, False (0) otherwise.
 */

static PyObject * py_isModelDirty( PyObject * args )
{
	return PyInt_FromLong( MeApp::instance().mutant()->dirty() );
}
PY_MODULE_FUNCTION( isModelDirty, ModelEditor )

/*~ function ModelEditor.revertModel
 *	@components{ modeleditor }
 *
 *	This function reverts the model to the last saved model. Any modifications made
 *	to the model that have not been saved will be lost.
 */
static PyObject * py_revertModel( PyObject * args )
{
	int result = ::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
		L("MODELEDITOR/APP/ME_APP/REVERT_MODEL_Q"),
		L("MODELEDITOR/APP/ME_APP/REVERT_MODEL"), MB_YESNO | MB_ICONWARNING );
	if( result == IDYES )
	{	
		MeApp::instance().mutant()->revertModel();
		ME_INFO_MSG( L("MODELEDITOR/APP/ME_APP/REVERTING_MODEL") );
	}
	Py_Return;
}
PY_MODULE_FUNCTION( revertModel, ModelEditor )

/*~ function ModelEditor.saveModel
 *	@components{ modeleditor }
 *
 *	This function saves the changes made to the model.
 */
static PyObject * py_saveModel( PyObject * args )
{
	//Hmm... A bit of a hack but fields are updated on loss of focus,
	//thus we need to do this focus juggle to commit any changes made.
	HWND currItem = GetFocus();
	SetFocus( AfxGetApp()->m_pMainWnd->GetSafeHwnd() );
	SetFocus( currItem );
	
	MeApp::instance().saveModel();

	Py_Return;
}
PY_MODULE_FUNCTION( saveModel, ModelEditor )

/*~ function ModelEditor.saveModelAs
 *	@components{ modeleditor }
 *
 *	This function allows the model to be saved in a chosen directory and 
 *	under a chosen name.
 */
static PyObject * py_saveModelAs( PyObject * args )
{
	//Hmm... A bit of a hack but fields are updated on loss of focus,
	//thus we need to do this focus juggle to commit any changes made.
	HWND currItem = GetFocus();
	SetFocus( AfxGetApp()->m_pMainWnd->GetSafeHwnd() );
	SetFocus( currItem );
	
	MeApp::instance().saveModelAs();

	Py_Return;
}
PY_MODULE_FUNCTION( saveModelAs, ModelEditor )

/*~ function ModelEditor.zoomToExtents
 *	@components{ modeleditor }
 *
 *	This function centres the model in view and zooms the camera until
 *	the model just fits in view.
 */
static PyObject * py_zoomToExtents( PyObject * args )
{
	MeApp::instance().camera()->zoomToExtents( true,
		MeApp::instance().mutant()->zoomBoundingBox());

	Py_Return;
}
PY_MODULE_FUNCTION( zoomToExtents, ModelEditor )
