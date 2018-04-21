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
#include "gui_app.hpp"


#include "ashes/simple_gui.hpp"
#include "ashes/simple_gui_component.hpp"
#include "chunk/chunk_manager.hpp"
#include "cstdmf/base64.h"
#include "moo/visual_channels.hpp"
#include "romp/flora.hpp"
#include "romp/progress.hpp"
#include "zip/zlib.h"

#include "app.hpp"
#include "alert_manager.hpp"
#include "app_config.hpp"
#include "device_app.hpp"


// Define this to one to draw our watermark (BigWorld Technology logo)
#define DRAW_WATERMARK 1


GUIApp GUIApp::instance;

int GUIApp_token = 1;



GUIApp::GUIApp()
{
	BW_GUARD;
	MainLoopTasks::root().add( this, "GUI/App", NULL );
}


GUIApp::~GUIApp()
{
	BW_GUARD;
	/*MainLoopTasks::root().del( this, "GUI/App" );*/
}


bool GUIApp::init()
{
	BW_GUARD;
	// access simple gui
	SimpleGUI::instance().hInstance( DeviceApp::s_hInstance_ );
	SimpleGUI::instance().hwnd( DeviceApp::s_hWnd_ );

	DataSectionPtr configSection = AppConfig::instance().pRoot();

	// Init the alert manager instance (so it creates its GUI stuff)
	AlertManager::instance().init();

	return DeviceApp::s_pStartupProgTask_->step(APP_PROGRESS_STEP);
}


void GUIApp::fini()
{
	BW_GUARD;
	//put here to avoid problems when client is shut down at weird spots
	// in the startup loop.
#if ENABLE_WATCHERS
	Watcher::fini();
#endif
	DeviceApp::instance.deleteGUI();


	SimpleGUI::fini();
}


void GUIApp::tick( float dTime )
{
	BW_GUARD;
	static DogWatch	dwGUI("GUI");
	dwGUI.start();

	// update the GUI components
	SimpleGUI::instance().update( dTime );

	dwGUI.stop();
}


void GUIApp::draw()
{
	BW_GUARD;
	static DogWatch	dwGUI("GUI");
	dwGUI.start();

	// draw UI
	SimpleGUI::instance().draw();
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (pSpace != NULL && pSpace->enviro().flora() != NULL)
	{
		pSpace->enviro().flora()->drawDebug();
	}
	Moo::SortedChannel::draw();

	dwGUI.stop();
}



// gui_app.cpp
