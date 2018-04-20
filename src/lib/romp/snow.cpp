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

#include "romp/snow.hpp"
#include "cstdmf/debug.hpp"
#include "moo/render_context.hpp"

#include "weather.hpp"
#include "particle/py_meta_particle_system.hpp"
#include "particle/particle_system.hpp"
#include "particle/actions/source_psa.hpp"
#include "particle/actions/sink_psa.hpp"
#include "particle/actions/vector_generator.hpp"
#include "particle/renderers/particle_system_renderer.hpp"
#include "particle/renderers/sprite_particle_renderer.hpp"
#include "enviro_minder.hpp"
#include "resmgr/auto_config.hpp"


DECLARE_DEBUG_COMPONENT2( "Romp", 0 )


/**
 * TODO: to be documented.
 */
class CameraMatrixLiaison : public MatrixLiaison, public Aligned
{
public:
	CameraMatrixLiaison():
	  worldTransform_( Matrix::identity )
	{
	}

	virtual const Matrix & getMatrix() const
	{
		return worldTransform_;
	}

	virtual bool setMatrix( const Matrix & m )
	{
		worldTransform_ = m;
		return true;
	}

private:
	Matrix	worldTransform_;
};



/**
 *	Maximum number of particles to generate when snow is heaviest
 */

static AutoConfigString s_snowFlakes( "environment/snowFlakesParticles" );
static AutoConfigString s_coldBreath( "environment/coldBreathParticles" );

/// Constructor
Snow::Snow()
:pSnowFlakes_( NULL ),
 flakesMaxRate_( 100 ),
 cameraLiaison_(NULL),
 wind_( 0, 0, 0 ),
 amount_( 0.f ),
 amountSmallFor_( 5.f ),
 pColdBreath_( NULL ),
 pColdBreathSource_( NULL )
{
	this->createSnowFlakeSystem();
	this->createColdBreathSystem();
}


/// Destructor
Snow::~Snow()
{
	if (cameraLiaison_)
	{
		pSnowFlakes_->detach();
		delete cameraLiaison_;
	}
	
	Py_DECREF( pSnowFlakes_ );
	pSnowFlakes_ = NULL;	

	Py_DECREF( pColdBreath_ );
	pColdBreath_ = NULL;
}


/**
 *	Add attachments to given vector
 */
void Snow::addAttachments( PlayerAttachments & pa )
{
	pa.add( pColdBreath_, "biped Head" );
}


/// Standard tick function
void Snow::tick( float dTime )
{
	if (amount_ < 0.01f)
	{
		amountSmallFor_ += dTime;
		if (amountSmallFor_ >= flakesMaxAge_) return;
	}
	amountSmallFor_ = 0.f;

	// update our position
	cameraLiaison_->setMatrix( Moo::rc().invView() );

	// and update the particle system
	if (pSnowFlakes_ != NULL)
	{
		pSnowFlakes_->tick( dTime );
	}
}


/// Standard draw function
void Snow::draw()
{
	if (amountSmallFor_ >= flakesMaxAge_) return;

	if (pSnowFlakes_ != NULL)
	{
		pSnowFlakes_->draw( Matrix::identity, NULL );
	}
}


/// Updates internal parameters based on the input weather settings
void Snow::update( const struct WeatherSettings & ws, bool playerDead )
{
	wind_ = Vector3( ws.windX, 0, ws.windZ );

	if (pColdBreathSource_ != NULL)
	{
		float rate =
			(1.f - Math::clamp( 0.f, ws.temperature / 8.f, 1.f )) * 40.f;
		//	40.f;
		if (playerDead) rate = 0.f;
		pColdBreathSource_->rate( rate );
	}
}



/// Set the current amount of snow to generate
void Snow::amount( float a )
{
	amount_ = a;

	for (uint32 i=0; i<pSnowFlakes_->nSystems(); i++)
	{
		ParticleSystemPtr ps = pSnowFlakes_->pSystem()->systemFromIndex(i);
		SourcePSA * source = static_cast<SourcePSA*>(&*ps->pAction(PSA_SOURCE_TYPE_ID));
		if (source != NULL)
		{
			float flakesMaxRate = ps->capacity() / flakesMaxAge_;
			source->rate( min( a, 1.f ) * flakesMaxRate );
		}
	}
}


/// Get the current amount of snow being generated
float Snow::amount()
{
	return amount_;
}


/// Create the snow flakes particle system
void Snow::createSnowFlakeSystem()
{	
	pSnowFlakes_ = new PyMetaParticleSystem( new MetaParticleSystem() );
	pSnowFlakes_->pSystem()->load( s_snowFlakes, "" );	

	cameraLiaison_ = new CameraMatrixLiaison;

	ParticleSystemPtr ps = pSnowFlakes_->pSystem()->systemFromIndex(0);
	if(ps)
	{
		SinkPSA * sink = static_cast<SinkPSA*>(&*ps->pAction(PSA_SINK_TYPE_ID));
		if(sink)
		{
			flakesMaxAge_ = sink ? sink->maximumAge() : 1.f;
			flakesMaxAge_ = max( 0.1f, flakesMaxAge_ );
		}
		else
		{
			flakesMaxAge_ = 1.f;
		}
	}

	for (uint32 i=0; i<pSnowFlakes_->nSystems(); i++)
	{
		ps = pSnowFlakes_->pSystem()->systemFromIndex(i);
		
		// set the initial source rate to zero 
		SourcePSA * source = static_cast<SourcePSA*>(&*ps->pAction(PSA_SOURCE_TYPE_ID));
		if(source)
			source->rate(0.f);

		// calculate maximum age
		SinkPSA * sink = static_cast<SinkPSA*>(&*ps->pAction(PSA_SINK_TYPE_ID));
		if(sink)
			flakesMaxAge_ = max( sink->maximumAge(), flakesMaxAge_ );

		// attach the snowflakes to the camera
		ps->attach( cameraLiaison_ );
	}
}


/// Create the cold breath particle system
void Snow::createColdBreathSystem()
{
	pColdBreath_ = new PyMetaParticleSystem( new MetaParticleSystem );
	pColdBreath_->pSystem()->load( s_coldBreath, "" );	
	ParticleSystemPtr ps = pColdBreath_->pSystem()->systemFromIndex(0);
	if (ps)
	{
		pColdBreathSource_ =
			static_cast<SourcePSA*>(&*ps->pAction(PSA_SOURCE_TYPE_ID));
	}
}	


// snow.cpp
