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
#include "lens_app.hpp"


#include "romp/lens_effect_manager.hpp"
#include "romp/progress.hpp"


#include "app.hpp"
#include "canvas_app.hpp"
#include "device_app.hpp"


static DogWatch g_lensEffectsWatch( "LensEffects" );


LensApp LensApp::instance;

int LensApp_token = 1;

PROFILER_DECLARE( AppDraw_Lens, "AppDraw Lens" );

LensApp::LensApp() : 
	dTime_( 0.f )
{ 
	BW_GUARD;
	MainLoopTasks::root().add( this, "Lens/App", NULL ); 
}

LensApp::~LensApp()
{
	BW_GUARD;
	/*MainLoopTasks::root().del( this, "Lens/App" );*/ 
}

bool LensApp::init()
{
	BW_GUARD;
	return DeviceApp::s_pStartupProgTask_->step(APP_PROGRESS_STEP);
}


void LensApp::fini()
{
	BW_GUARD;
	LensEffectManager::instance().finz();
}


void LensApp::tick( float dTime )
{
	dTime_ = dTime;
}


void LensApp::draw()
{
	BW_GUARD_PROFILER( AppDraw_Lens );

	if(!::gWorldDrawEnabled)
		return;

	g_lensEffectsWatch.start();
	LensEffectManager::instance().tick( dTime_ );
	g_lensEffectsWatch.stop();

	// Finish off the back buffer filters now
	CanvasApp::instance.finishFilters();
	// Note: I have moved this after drawFore, because I reckon the things
	// in there (seas, rain) prolly want to be affected by the filters too.

	// draw the lens effects
	g_lensEffectsWatch.start();
	LensEffectManager::instance().draw();
	g_lensEffectsWatch.stop();
}


// lens_app.cpp
