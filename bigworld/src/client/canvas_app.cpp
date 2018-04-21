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
#include "canvas_app.hpp"


#include "cstdmf/debug.hpp"
#include "cstdmf/memory_trace.hpp"
#include "chunk/chunk_manager.hpp"
#include "particle/particle_system_manager.hpp"
#include "math/colour.hpp"
#include "moo/animating_texture.hpp"
#include "moo/effect_visual_context.hpp"
#include "moo/visual_channels.hpp"
#include "resmgr/datasection.hpp"
#include "romp/bloom_effect.hpp"
#include "romp/console.hpp"
#include "romp/console_manager.hpp"
#include "romp/flash_bang_effect.hpp"
#include "romp/heat_shimmer.hpp"
#include "romp/histogram_provider.hpp"
#include "romp/progress.hpp"
#include "romp/distortion.hpp"


#include "action_matcher.hpp"
#include "app.hpp"
#include "app_config.hpp"
#include "device_app.hpp"
#include "physics.hpp"
#include "player.hpp"
#include "player_fader.hpp"
#include "script_bigworld.hpp"


CanvasApp CanvasApp::instance;

int CanvasApp_token = 1;

PROFILER_DECLARE( AppDraw_Canvas, "AppDraw Canvas" );

CanvasApp::CanvasApp() :
	gammaCorrectionOutside_( 1.f ),
	gammaCorrectionInside_( 1.f ),
	gammaCorrectionSpeed_( 0.2f ),
	dTime_( 0.f ),
	bloomFilter_( NULL ),
	heatFilter_( NULL ),
	distortion_( NULL ),
	playerFader_( NULL ),
	flashBang_( NULL )
{
	BW_GUARD;
	MainLoopTasks::root().add( this, "Canvas/App", NULL );
}


CanvasApp::~CanvasApp()
{
	BW_GUARD;
	/*MainLoopTasks::root().del( this, "Canvas/App" );*/
}


bool CanvasApp_getStaticSkyToggle()
{
	BW_GUARD;
	return
		(CanvasApp::instance.drawSkyCtrl_
		& EnviroMinder::DrawSelection::staticSky) != 0;
}


void CanvasApp_setStaticSkyToggle(bool on)
{
	BW_GUARD;
	CanvasApp::instance.drawSkyCtrl_ = on
		? CanvasApp::instance.drawSkyCtrl_ | EnviroMinder::DrawSelection::staticSky
		: CanvasApp::instance.drawSkyCtrl_ & ~EnviroMinder::DrawSelection::staticSky;
}


bool CanvasApp::init()
{
	BW_GUARD;	
#if ENABLE_WATCHERS
	DEBUG_MSG( "CanvasApp::init: Initially using %d(~%d)KB\n",
		memUsed(), memoryAccountedFor() );
#endif

	MEM_TRACE_BEGIN( "CanvasApp::init" )

	DataSectionPtr configSection = AppConfig::instance().pRoot();

	EnviroMinder::init();

	// Initialise the consoles
	ConsoleManager & mgr = ConsoleManager::instance();

	XConsole * pPythonConsole = new PythonConsole();
	XConsole * pStatusConsole = new XConsole();

	mgr.add( pPythonConsole,		"Python" );
	mgr.add( pStatusConsole,		"Status" );

	this->setPythonConsoleHistoryNow(this->history_);
	BigWorldClientScript::setPythonConsoles( pPythonConsole, pPythonConsole );

	Vector3 colour = configSection->readVector3( "ui/loadingText", Vector3(255,255,255) );
	pStatusConsole->setConsoleColour( Colour::getUint32( colour, 255 ) );
	pStatusConsole->setScrolling( true );
	pStatusConsole->setCursor( 0, pStatusConsole->visibleHeight()-2 );

	// print some status information
	loadingText( std::string( "Resource path:   " ) +
		BWResource::getDefaultPath() );

	loadingText(std::string( "App config file: " ) + s_configFileName);

	// Initialise the adaptive lod controller
	lodController_.minimumFPS( 10.f );
	lodController_.addController( "clod", &CLODPower, 10.f, 15.f, 50.f );

	MF_WATCH( "Client Settings/LOD/FPS",
		lodController_,
		&AdaptiveLODController::effectiveFPS,
		"Effective fps as seen by the adaptive Level-of-detail controller." );
	MF_WATCH( "Client Settings/LOD/Minimum fps",
		lodController_,
		MF_ACCESSORS( float, AdaptiveLODController, minimumFPS ),
		"Minimum fps setting for the adaptive level-of-detail controller.  FPS "
		"below this setting will cause adaptive lodding to take place." );
	MF_WATCH(
		"Client Settings/Sky Dome2/Render static sky dome",
		&CanvasApp_getStaticSkyToggle, &CanvasApp_setStaticSkyToggle,
		"Toggles rendering of the static sky dome" );

	for ( int i = 0; i < lodController_.numControllers(); i++ )
	{
		AdaptiveLODController::LODController & controller = lodController_.controller( i );
		std::string watchPath = "Client Settings/LOD/" + controller.name_;
		std::string watchName = watchPath + "/current";
		MF_WATCH( watchName.c_str(), controller.current_, Watcher::WT_READ_ONLY );
		watchName = "Client Settings/LOD/";
		watchName = watchName + controller.name_;
		watchName = watchName + " curr";
		MF_WATCH( watchName.c_str(), controller.current_, Watcher::WT_READ_ONLY );
		watchName = watchPath + "/default";
		MF_WATCH( watchName.c_str(), controller, MF_ACCESSORS( float, AdaptiveLODController::LODController, defaultValue ));
		watchName = watchPath + "/worst";
		MF_WATCH( watchName.c_str(), controller, MF_ACCESSORS( float, AdaptiveLODController::LODController, worst ));
		watchName = watchPath + "/speed";
		MF_WATCH( watchName.c_str(), controller, MF_ACCESSORS( float, AdaptiveLODController::LODController, speed ));
		watchName = watchPath + "/importance";
		MF_WATCH( watchName.c_str(), controller.relativeImportance_ );
	}

	// and some fog stuff
	Moo::rc().fogNear( 0 );
	Moo::rc().fogFar( 500 );
	Moo::rc().fogColour( 0x102030 );

	// Renderer settings
	MF_WATCH( "Render/waitForVBL", Moo::rc(),
		MF_ACCESSORS( bool, Moo::RenderContext, waitForVBL ),
		"Enable locking of frame presentation to the vertical blank signal" );
	MF_WATCH( "Render/tripleBuffering", Moo::rc(),
		MF_ACCESSORS( bool, Moo::RenderContext, tripleBuffering ),
		"Enable triple-buffering, including the front-buffer and 2 back buffers" );

	gammaCorrectionOutside_ = configSection->readFloat(
		"renderer/gammaCorrectionOutside", configSection->readFloat(
		"renderer/gammaCorrection", gammaCorrectionOutside_ ) );
	gammaCorrectionInside_ = configSection->readFloat(
		"renderer/gammaCorrectionInside", configSection->readFloat(
		"renderer/gammaCorrection", gammaCorrectionInside_ ) );
	gammaCorrectionSpeed_ = configSection->readFloat(
		"renderer/gammaCorrectionSpeed", gammaCorrectionSpeed_ );

	MF_WATCH( "Render/Gamma Correction Outside",
		gammaCorrectionOutside_,
		Watcher::WT_READ_WRITE,
		"Gama correction factor when the camera is in outside chunks" );
	MF_WATCH( "Render/Gamma Correction Inside",
		gammaCorrectionInside_,
		Watcher::WT_READ_WRITE,
		"Gamma correction factor when the camera is in indoor chunks" );
	MF_WATCH( "Render/Gamma Correction Now", Moo::rc(),
		MF_ACCESSORS( float, Moo::RenderContext, gammaCorrection),
		"Current gama correction factor" );

	Moo::rc().gammaCorrection( gammaCorrectionOutside_ );

	MF_WATCH( "Render/Enviro draw",
		(int&)drawSkyCtrl_,
		Watcher::WT_READ_WRITE,
		"Enable / Disable various environment features such as sky, "
		"sun, moon and clouds." );

	// misc stuff

	ActionMatcher::globalEntityCollision_ =
		configSection->readBool( "entities/entityCollision", false );

	ParticleSystemManager::instance().active( configSection->readBool(
		"entities/particlesActive",
		ParticleSystemManager::instance().active() ) );

	Physics::setMovementThreshold( configSection->readFloat(
		"entities/movementThreshold", 0.25 ) );

	bool ret = DeviceApp::s_pStartupProgTask_->step(APP_PROGRESS_STEP);

	if (distortion_ == NULL)
	{
		if (Distortion::isSupported())
		{
			// initialised at first use
			distortion_ = Distortion::pInstance();
		}
		else
		{
			INFO_MSG( "Distortion is not supported on this hardware\n" );
		}
	}

	if (heatFilter_ == NULL)
	{
		if (HeatShimmer::isSupported())
		{
			heatFilter_ = HeatShimmer::pInstance();
			if (!heatFilter_->init())
			{
				ERROR_MSG( "Heat Shimmer failed to initialise\n" );
				heatFilter_->fini();
				heatFilter_ = NULL;
			}
		}
		else
		{
			INFO_MSG( "Heat Shimmer is not supported on this hardware\n" );
		}
	}

	if (playerFader_ == NULL)
	{
		playerFader_ = PlayerFader::pInstance();
		playerFader_->init();
	}

	if (bloomFilter_ == NULL)
	{
		bloomFilter_ = Bloom::pInstance();
		if (!bloomFilter_->init())
		{
			ERROR_MSG( "Blooming failed to initialise\n" );
			bloomFilter_->fini();
			bloomFilter_ = NULL;
		}
	}

	if (flashBang_ == NULL)
	{
		flashBang_ = new FlashBangEffect;
	}

	MEM_TRACE_END()

	return ret;
}


void CanvasApp::fini()
{
	BW_GUARD;

	if (distortion_)
	{
		distortion_->finz();
		distortion_ = NULL;	
	}

	if (heatFilter_)
	{
		heatFilter_->finz();
		heatFilter_ = NULL;
	}

	if (playerFader_)
	{
		playerFader_->fini();
		playerFader_ = NULL;
	}

	if (bloomFilter_)
	{
		bloomFilter_->fini();
		bloomFilter_ = NULL;
	}

	delete flashBang_;
	flashBang_ = NULL;

	EnviroMinder::fini();
}


void CanvasApp::tick( float dTime )
{
	BW_GUARD;
	dTime_ = dTime;

	// Update the animating textures
	Moo::AnimatingTexture::tick( dTime );
	Moo::Material::tick( dTime );
	Moo::EffectVisualContext::instance().tick( dTime );

	//Adaptive degradation section
	lodController_.fpsTick( 1.f / dTime );

	// set the values
	int controllerIdx = 0;

	/*float recommendedDistance = lodController_.controller( controllerIdx++ ).current_;
	if ( this->getFarPlane() > recommendedDistance )
	{
		this->setFarPlane( recommendedDistance );
	}*/
	Moo::rc().lodPower( lodController_.controller( controllerIdx++ ).current_ );

	if (flashBang_)
	{
		if (flashBangAnimations_.size())
		{
			Vector4 v( 0, 0, 0, 0 );
			for (uint i = 0; i <  flashBangAnimations_.size(); i++)
			{
				Vector4 vt;
				flashBangAnimations_[i]->tick( dTime );
				flashBangAnimations_[i]->output( vt );
				v.x = max( vt.x, v.x );
				v.y = max( vt.y, v.y );
				v.z = max( vt.z, v.z );
				v.w = max( vt.w, v.w );
			}

			flashBang_->fadeValues( v );
		}
		else
		{
			flashBang_->fadeValues( Vector4( 0, 0, 0, 0 ) );
		}
	}

	if (distortion_)
		distortion_->tick(dTime);
}


void CanvasApp::draw()
{
	BW_GUARD_PROFILER( AppDraw_Canvas );

	if(!::gWorldDrawEnabled)
		return;

	// set the gamma level
	float desiredGamma = isCameraOutside() ?
		gammaCorrectionOutside_ : gammaCorrectionInside_;
	float currentGamma = Moo::rc().gammaCorrection();
	if (currentGamma != desiredGamma)
	{
		currentGamma += Math::clamp( gammaCorrectionSpeed_ * dTime_,
			desiredGamma - currentGamma );
		Moo::rc().gammaCorrection( currentGamma );
	}

	// note we need to call pf->update before asking the FSBB if it is enabled.
	playerFader_->update();

	FullScreenBackBuffer::beginScene();

	// render the backdrop
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (pSpace)
	{
		pSpace->enviro().drawHind( dTime_, drawSkyCtrl_ );
	}
}


void CanvasApp::updateDistortionBuffer()
{
	BW_GUARD;
	if (distortion() && distortion()->drawCount())
	{
		distortion()->copyBackBuffer();
		if (distortion()->pushRT())
		{
		
			// if player isnt visible in main buffer, draw it to the copy...
			// a player is not visible if it's being faded by PlayerFader
			if (Player::instance().entity() != NULL &&
				Player::instance().entity()->pPrimaryModel() != NULL &&
				!Player::instance().entity()->pPrimaryModel()->visible() )
			{
				Player::instance().entity()->pPrimaryModel()->visible(true);
				DX::Surface* oldDepth = NULL;
				Moo::rc().device()->GetDepthStencilSurface( &oldDepth );
				Moo::rc().device()->SetDepthStencilSurface( NULL );
				playerFader_->doPostTransferFilter();
				Moo::rc().device()->SetDepthStencilSurface( oldDepth );
				oldDepth->Release();
				Player::instance().entity()->pPrimaryModel()->visible(false);
			}
			ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
			if (pSpace)
			{
				pSpace->enviro().drawFore( dTime_, true, false, false, true, false );
			}
			Moo::SortedChannel::draw(false);
			distortion()->popRT();
			distortion()->drawScene();
		}
	}
}


const CanvasApp::StringVector CanvasApp::pythonConsoleHistory() const
{
	BW_GUARD;
	PythonConsole * console = static_cast<PythonConsole *>(
		ConsoleManager::instance().find("Python"));
		
	return console != NULL
		? console->history()
		: this->history_;
}


void CanvasApp::setPythonConsoleHistory(const StringVector & history)
{
	BW_GUARD;
	if (!this->setPythonConsoleHistoryNow(history))
	{
		this->history_ = history;
	}
}


bool CanvasApp::setPythonConsoleHistoryNow(const StringVector & history)
{
	BW_GUARD;
	PythonConsole * console = static_cast<PythonConsole *>(
		ConsoleManager::instance().find("Python"));
		
	bool result = console != NULL;
	if (result)
	{
		console->setHistory(history);
	}
	return result;
}


void CanvasApp::finishFilters()
{
	BW_GUARD;
	if(!::gWorldDrawEnabled)
		return;

	FullScreenBackBuffer::endScene();

	HistogramProvider::instance().update();

	if (flashBang_)
	{
		flashBang_->draw();
	}

	// python console may die. Save command 
	// history for future reference.
	PythonConsole * console = static_cast<PythonConsole *>(
		ConsoleManager::instance().find("Python"));
		
	if (console != NULL)
	{
		this->history_ = console->history();
	}
}


// canvas_app.
