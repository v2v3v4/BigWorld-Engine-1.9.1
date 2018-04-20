/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

//---------------------------------------------------------------------------
#include "pch.hpp"

#include "cstdmf/debug.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/string_provider.hpp"
#include "romp_harness.hpp"
#include "romp/fog_controller.hpp"
#include "romp/lens_effect_manager.hpp"
#include "romp/engine_statistics.hpp"
#include "romp/console_manager.hpp"
#include "romp/resource_manager_stats.hpp"
#include "romp/rain.hpp"
#include "romp/sky.hpp"
#include "romp/time_of_day.hpp"
#include "romp/weather.hpp"
#include "romp/bloom_effect.hpp"
#include "romp/heat_shimmer.hpp"
#include "romp/distortion.hpp"
#include "romp/full_screen_back_buffer.hpp"
#include "romp/geometrics.hpp"
#include "romp/enviro_minder.hpp"
#include "moo/render_context.hpp"
#include "moo/moo_math.hpp"
#include "moo/visual_channels.hpp"
#include "romp/water.hpp"
#include "romp/sky_gradient_dome.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk.hpp"
#include "gizmo/tool_manager.hpp"
#include "appmgr/options.hpp"
#include "romp/histogram_provider.hpp"
#include "terrain/base_terrain_renderer.hpp"

#ifndef STATIC_WATER
#include "worldeditor/world/items/editor_chunk_water.hpp"
#endif

PY_TYPEOBJECT( RompHarness )

PY_BEGIN_METHODS( RompHarness )
	PY_METHOD( setTime )
	PY_METHOD( setSecondsPerHour )
	PY_METHOD( setRainAmount )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( RompHarness )
	PY_ATTRIBUTE( fogEnable )
PY_END_ATTRIBUTES()

DECLARE_DEBUG_COMPONENT2( "WorldEditor", 2 )

RompHarness::RompHarness( PyTypePlus * pType ) :
	PyObjectPlus( pType ),
    dTime_( 0.033f ),
    inited_( false ),
	bloom_( NULL ),
	useBloom_( true ),	
	shimmer_( NULL ),
	distortion_( NULL ),
	useShimmer_( true )	
{
	waterMovement_[0] = Vector3(0,0,0);
	waterMovement_[1] = Vector3(0,0,0);

	if ( ChunkManager::instance().cameraSpace() )
	{
		MF_WATCH_REF( L("COMMON/ROMP_HARNESS/TIME_OF_DAY"), *enviroMinder().timeOfDay(),
			&TimeOfDay::getTimeOfDayAsString, &TimeOfDay::setTimeOfDayAsString );
		MF_WATCH( L("COMMON/ROMP_HARNESS/SECS_PER_HOUR"), *enviroMinder().timeOfDay(),
			MF_ACCESSORS( float, TimeOfDay, secondsPerGameHour ) );
	}
}

RompHarness::~RompHarness()
{
	if (bloom_)
	{
		bloom_->fini();
		bloom_ = NULL;
	}

	if (shimmer_)
	{
		shimmer_->finz();
		shimmer_=NULL;
	}

	if (distortion_)
	{
		distortion_->finz();
		distortion_=NULL;
	}

	Waters::instance().fini();

	EnviroMinder::fini();

	ResourceManagerStats::fini();
}


bool
RompHarness::init()
{
    inited_ = true;

	EnviroMinder::init();

	setSecondsPerHour( 0.f );
	DataSectionPtr pWatcherSection = Options::pRoot()->openSection( "romp/watcherValues" );
	if (pWatcherSection)
	{
		pWatcherSection->setWatcherValues();
	}

	if ( HeatShimmer::isSupported() )
	{
		shimmer_ = HeatShimmer::pInstance();
		if ( !shimmer_->init() )
		{			
			ERROR_MSG( L("COMMON/ROMP_HARNESS/SHIMMER_FAILED") );
			shimmer_->finz();
			shimmer_ = NULL;
		}
	}
	else
	{
		ERROR_MSG( L("COMMON/ROMP_HARNESS/SHIMMER_NOT_SUPPORTED")  );		
	}

	if ( Bloom::isSupported() )
	{
		bloom_ = Bloom::pInstance();
		if (!bloom_->init() )
		{
			ERROR_MSG( L("COMMON/ROMP_HARNESS/BLOOM_FAILED") );
			bloom_->fini();
			bloom_ = NULL;
		}
	}
	else
	{
		ERROR_MSG( L("COMMON/ROMP_HARNESS/BLOOM_NOT_SUPPORTED") );		
	}	

	if (Distortion::isSupported())
	{
		// Initialised on the first draw
		distortion_ = Distortion::pInstance();
	}
	else
	{
		INFO_MSG( "Distortion is not supported on this hardware\n" );
	}

	Waters::instance().init();

	return true;
}

void RompHarness::changeSpace()
{
	setSecondsPerHour( 0.f );
}

void RompHarness::initWater( DataSectionPtr pProject )
{
}


void RompHarness::setTime( float t )
{
	enviroMinder().timeOfDay()->gameTime( t );
}


void RompHarness::setSecondsPerHour( float sph )
{
	enviroMinder().timeOfDay()->secondsPerGameHour( sph );
}


void RompHarness::setRainAmount( float r )
{
	enviroMinder().rain()->amount( r );
}


void RompHarness::update( float dTime, bool globalWeather )
{
	dTime_ = dTime;

	if ( inited_ )
    {
		this->fogEnable( !!Options::getOptionInt( "render/environment/drawFog", 0 ));

		Chunk * pCC = ChunkManager::instance().cameraChunk();
		bool outside = (pCC == NULL || pCC->isOutsideChunk());
		enviroMinder().tick( dTime_, outside );
        FogController::instance().tick();        

		if (distortion_)
			distortion_->tick(dTime);
    }

    //Intersect tool with water
	this->disturbWater();
}

void RompHarness::disturbWater()
{
#ifndef STATIC_WATER
	if ( ToolManager::instance().tool() && ToolManager::instance().tool()->locator() )
	{
		waterMovement_[0] = waterMovement_[1];
		waterMovement_[1] = ToolManager::instance().tool()->locator()->
			transform().applyToOrigin();

		SimpleMutexHolder holder(EditorChunkWater::instancesMutex());
		const std::vector<EditorChunkWater*>& waterItems = EditorChunkWater::instances();
		std::vector<EditorChunkWater*>::const_iterator i = waterItems.begin();
		for (; i != waterItems.end(); ++i)
		{
			(*i)->sway( waterMovement_[0], waterMovement_[1], 1.f );
		}

		//s_phloem.addMovement( waterMovement_[0], waterMovement_[1] );
	}
#endif
}



void RompHarness::drawPreSceneStuff( bool sparkleCheck /* = false */, bool renderEnvironment /* = true */ )
{
	useShimmer_ = !!Options::getOptionInt("render/environment/drawShimmer", 1 );
	useBloom_ = !!Options::getOptionInt("render/environment/drawBloom", 1 );

	//set the actual values
	Options::setOptionInt("render/environment/drawShimmer", useShimmer_ ? 1 : 0 );
	Options::setOptionInt("render/environment/drawBloom",  useBloom_ ? 1 : 0 );

	useShimmer_ = !sparkleCheck && useShimmer_;
	useBloom_ = !sparkleCheck && useBloom_;

	//and modify useBloom temporarily.
	useShimmer_ &= !!Options::getOptionInt("render/environment", 0 );
	useBloom_ &= !!Options::getOptionInt("render/environment", 0 );

	if (bloom_)
		bloom_->setEditorEnabled( useBloom_ );

	if (shimmer_)
		shimmer_->setEditorEnabled( useShimmer_ );

	FullScreenBackBuffer::beginScene();	

	EnviroMinder::DrawSelection flags;
	flags.value = 0;
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawSunAndMoon", 0 ) ? EnviroMinder::DrawSelection::sunAndMoon : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawSky", 0 ) ? EnviroMinder::DrawSelection::skyGradient : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawClouds", 0 ) ? EnviroMinder::DrawSelection::clouds : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawSunAndMoon", 0 ) ? EnviroMinder::DrawSelection::sunFlare : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawStaticSky", 0 ) ? EnviroMinder::DrawSelection::staticSky : 0 );

	flags = renderEnvironment && !!Options::getOptionInt( "render/environment", 1 ) ? flags : 0;
	
	enviroMinder().drawHind( dTime_, flags, renderEnvironment );
}


void RompHarness::drawDelayedSceneStuff( bool renderEnvironment /* = true */ )
{
	EnviroMinder::DrawSelection flags;
	flags.value = 0;
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawSunAndMoon", 0 ) ? EnviroMinder::DrawSelection::sunAndMoon : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawSky", 0 ) ? EnviroMinder::DrawSelection::skyGradient : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawClouds", 0 ) ? EnviroMinder::DrawSelection::clouds : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawSunAndMoon", 0 ) ? EnviroMinder::DrawSelection::sunFlare : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawStaticSky", 0 ) ? EnviroMinder::DrawSelection::staticSky : 0 );
		
	flags = renderEnvironment && !!Options::getOptionInt( "render/environment", 1 ) ? flags : 0;

	enviroMinder().drawHindDelayed( dTime_, flags );
}


void RompHarness::drawPostSceneStuff( bool showWeather /* = true */, bool showFlora /* = true */, bool showFloraShadowing /* = false */ )
{
	// check if flora can be seen
	bool canDrawFlora = showFlora && Terrain::BaseTerrainRenderer::instance()->canSeeTerrain();

	Waters::instance().tick( dTime_ );

	// update any dynamic textures
	TextureRenderer::updateCachableDynamics( dTime_ );	

	bool drawWater = Options::getOptionInt( "render/scenery", 1 ) &&
		Options::getOptionInt( "render/scenery/drawWater", 1 );
	Waters::drawWaters( drawWater );

	bool drawWaterReflection = drawWater &&
		Options::getOptionInt( "render/scenery/drawWater/reflection", 1 );
	Waters::drawReflection( drawWaterReflection );

	bool drawWaterSimulation = drawWater &&
		Options::getOptionInt( "render/scenery/drawWater/simulation", 1 );
	Waters::simulationEnabled( drawWaterSimulation );

	bool drawWire = Options::getOptionInt( "render/scenery/wireFrame", 0 )==1;
	Waters::instance().drawWireframe(drawWire);

	if( drawWater )
	{
		Waters::instance().rainAmount( enviroMinder().rain()->amount() );
		Waters::instance().updateSimulations( dTime_ );
	}

	enviroMinder().drawFore( dTime_, showWeather, canDrawFlora, showFloraShadowing, false, true );

	if (distortion_ && distortion_->drawCount() > 0 && 
		distortion_->isEnabled())
	{
		distortion_->copyBackBuffer();
		if (distortion_->pushRT())
		{
			// draw tools
			ToolPtr spTool = ToolManager::instance().tool();
			if ( spTool )
			{
				spTool->render();
			}
			enviroMinder().drawFore( dTime_, showWeather, false, false, true, false );

			Moo::SortedChannel().draw(false);

			distortion_->popRT();
			distortion_->drawScene();
		}
	}
	else
		Waters::instance().drawDrawList( dTime_ );
		
	enviroMinder().drawFore( dTime_, showWeather, canDrawFlora, showFloraShadowing, true, false );

	Moo::SortedChannel::draw();

	LensEffectManager::instance().tick( dTime_ );	

	FullScreenBackBuffer::endScene();	

	HistogramProvider::instance().update();	

	LensEffectManager::instance().draw();
}


void RompHarness::propensity( const std::string& weatherSystemName, float amount )
{
	if ( enviroMinder().weather() )
    {
    	WeatherSystem * ws = enviroMinder().weather()->system( weatherSystemName );
        if ( ws )
        {
        	float args[4];
            args[0] = 1.f;
            args[1] = 1.f;
            args[2] = 1.f;
            args[3] = 1.f;
        	ws->direct( amount, args, 0.f );
        }
    }
}


TimeOfDay* RompHarness::timeOfDay() const
{
	return enviroMinder().timeOfDay();
}

EnviroMinder& RompHarness::enviroMinder() const
{
	return ChunkManager::instance().cameraSpace()->enviro();
}

/**
 *	Get an attribute for python
 */
PyObject * RompHarness::pyGetAttribute( const char * attr )
{
	// try our normal attributes
	PY_GETATTR_STD();

	// ask our base class
	return PyObjectPlus::pyGetAttribute( attr );
}


/**
 *	Set an attribute for python
 */
int RompHarness::pySetAttribute( const char * attr, PyObject * value )
{
	// try our normal attributes
	PY_SETATTR_STD();

	// ask our base class
	return PyObjectPlus::pySetAttribute( attr, value );
}


/**
 *	The (non-static) python setTime method
 */
PyObject * RompHarness::py_setTime( PyObject * args )
{
	float t;

	if (!PyArg_ParseTuple( args, "f", &t ))
	{
		PyErr_SetString( PyExc_TypeError, "RompHarness.setTime() "
			"expects a float time" );
		return NULL;
	}

	enviroMinder().timeOfDay()->gameTime( t );

	Py_Return;
}


/**
 *	The (non-static) python setSecondsPerHour method
 */
PyObject * RompHarness::py_setSecondsPerHour( PyObject * args )
{
	float t;

	if (!PyArg_ParseTuple( args, "f", &t ))
	{
		PyErr_SetString( PyExc_TypeError, "RompHarness.setSecondsPerHour() "
			"expects a float time" );
		return NULL;
	}

	enviroMinder().timeOfDay()->secondsPerGameHour( t );

	Py_Return;
}


/**
 *	The (non-static) python setRainAmount method
 */
PyObject * RompHarness::py_setRainAmount( PyObject * args )
{
	float a;

	if (!PyArg_ParseTuple( args, "f", &a ))
	{
		PyErr_SetString( PyExc_TypeError, "RompHarness.setRainAmount() "
			"expects a float amount between 0 and 1" );
		return NULL;
	}

	enviroMinder().rain()->amount( a );

	Py_Return;
}


/**
 *	This method enables or disables global fogging
 */
void RompHarness::fogEnable( bool state )
{
	FogController::instance().enable( state );

	Options::setOptionInt( "render/environment/drawFog", state ? 1 : 0 );
}


/**
 *	This method returns the global fogging state.
 */
bool RompHarness::fogEnable() const
{
	return FogController::instance().enable();
}

//---------------------------------------------------------------------------
