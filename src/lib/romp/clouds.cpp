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
#include "clouds.hpp"
#include "sky_light_map.hpp"
#include "weather.hpp"
#include "photon_occluder.hpp"
#include "cstdmf/bgtask_manager.hpp"
#include "cstdmf/concurrency.hpp"
#include "moo/visual_manager.hpp"

#ifndef CODE_INLINE
#include "clouds.ipp"
#endif

#include "resmgr/auto_config.hpp"

DECLARE_DEBUG_COMPONENT2( "romp", 0 );

// -----------------------------------------------------------------------------
// Section: Photon occluder for clouds
//	//TODO : create photon occluder
// -----------------------------------------------------------------------------
/**
 * TODO: to be documented.
 */
class CloudsPhotonOccluder : public PhotonOccluder
{
public:
	CloudsPhotonOccluder()
	{
	};

	virtual float collides(
			const Vector3 & lightSourcePosition,
			const Vector3 & cameraPosition,
			const LensEffect& le )
	{
		return 1.f;
	}
};

// -----------------------------------------------------------------------------
// Section: Texture Setter
// -----------------------------------------------------------------------------
/**
 *	This class sets cloud textures on the device.  It is also multi-threaded.
 *	When it is told to use a new texture, it uses the background loading thread
 *	to do so.  While it is doing this, the textureName refers to the new
 *	texture, but isLoading() will return true.  And in this state, it will be
 *	sneakily using the pre-existing texture until the new one is ready.
 */
class CloudsTextureSetter : public Moo::EffectConstantValue
{
public:
	CloudsTextureSetter():
	  pTexture_( NULL ),
	  bgLoader_( NULL ),
	  textureName_( "" )
	{		
	}


	/**
	 *	This method is called by the effect system when a material needs
	 *	to draw using a cloud texture.
	 */
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		SimpleMutexHolder holder( mutex_ );

		if (pTexture_ && pTexture_->pTexture())
			pEffect->SetTexture(constantHandle, pTexture_->pTexture());
		else
			pEffect->SetTexture(constantHandle, NULL);

		return true;
	}


	/**
	 *	This method sets our texture.  If the texture is different then
	 *	the existing one, we schedule the new one for loading, and set
	 *	the textureName and the isLoading() flag.  In an unspecified amount
	 *	of time, the new texture will be loaded and used.
	 */
	void texture( const std::string& texName )
	{
		if (textureName_ == texName)
			return;

		if (this->isLoading())
			return;

		textureName_ = texName;		

		bgLoader_ = new CStyleBackgroundTask(
						&CloudsTextureSetter::loadTexture, this,
						&CloudsTextureSetter::onLoadComplete, this );

#ifndef EDITOR_ENABLED
		BgTaskManager::instance().addBackgroundTask( bgLoader_ );
#else
		CloudsTextureSetter::loadTexture( this );
		CloudsTextureSetter::onLoadComplete( this );
#endif
	}


	/**
	 *	This class-static method is called by the background loading thread
	 *	and allows us to load the texture resource in a blocking manner.
	 */
	static void loadTexture( void* s )
	{
		CloudsTextureSetter* setter = static_cast<CloudsTextureSetter*>(s);
		Moo::BaseTexturePtr pTex = 
			Moo::TextureManager::instance()->get( setter->textureName(), true, true, true, "texture/environment" );
		setter->pTexture(pTex);
	}


	/**
	 *	This class-static method is called when the background loading thread
	 *	has finished.
	 */
	static void onLoadComplete( void* s )
	{
		CloudsTextureSetter* setter = static_cast<CloudsTextureSetter*>(s);
		setter->onBgLoadComplete();
	}


	/**
	 *	This method returns the name of the texture we are currently
	 *	drawing with.  If isLoading() is true, then the textureName
	 *	refers to the texture we would like to draw with (however we
	 *	will be actually drawing with the previous texture ptr).
	 */
	const std::string& textureName() const
	{
		return textureName_;
	}


	/**
	 *	This method returns true if we are currently waiting for the
	 *	background loading thread to load our texture.
	 */
	bool isLoading()
	{
		SimpleMutexHolder holder( mutex_ );
		return (bgLoader_ != NULL);
	}

private:
	//only called by the background loading thread
	void pTexture( Moo::BaseTexturePtr pTex )
	{		
		SimpleMutexHolder holder( mutex_ );
		pTexture_ = pTex;		
	}


	//only called by the background loading thread
	void onBgLoadComplete()
	{
		SimpleMutexHolder holder( mutex_ );
		bgLoader_ = NULL;
	}

	Moo::BaseTexturePtr pTexture_;
	std::string textureName_;	//store a copy for use while loading.
	BackgroundTaskPtr bgLoader_;	
	SimpleMutex		mutex_;
};

static CloudsTextureSetter* s_cloudsTextureSetter[3];


// -----------------------------------------------------------------------------
// Section: Clouds Blend Setter
// -----------------------------------------------------------------------------
/**
 * TODO: to be documented.
 */
class CloudsBlendSetter : public Moo::EffectConstantValue
{
public:
	CloudsBlendSetter():
	  value_( 1.f,1.f,0.f,0.f )
	{		
	}
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetVector(constantHandle, &value_);
		return true;
	}
	void value( const Vector4& value )
	{
		value_ = value;
	}
private:
	Vector4 value_;
};

static CloudsBlendSetter* s_cloudsBlendSetter = NULL;


// -----------------------------------------------------------------------------
// Section: Clouds Use Blend Pixel Shader Setter
// -----------------------------------------------------------------------------
/**
 * TODO: to be documented.
 */
class CloudsUseBlendSetter : public Moo::EffectConstantValue
{
public:
	CloudsUseBlendSetter():
	  value_( false )
	{		
	}
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetInt(constantHandle, value_ ? 1 : 0);
		return true;
	}
	void value( bool value )
	{
		value_ = value;
	}
private:
	bool value_;
};

static CloudsUseBlendSetter* s_cloudsUseBlendSetter = NULL;


// -----------------------------------------------------------------------------
// Section: Clouds
// -----------------------------------------------------------------------------
static AutoConfigString s_cloudsEffect( "environment/cloudsEffect" );
static AutoConfigString s_cloudsVisual( "environment/skyDomeVisual" );
static Moo::ManagedEffectPtr s_cloudsManagedEffect;

/**
 *	Constructor.
 */
Clouds::Clouds()
:avgCover_( 0 ),
 precipitation_( 0.f, 0.f, 0.f ),
 settings_( NULL ),
 material_( NULL ),
 cloudsTransition_( 0.f ),
 cloudsTransitionTime_( 5.f ),
 lightingTransition_( 0.f ), 
 lightingTransitionTime_( 10.f ),
 lightDim_( 1.f, 1.f, 1.f ),
 lightDimAnimation_( false, 5.f ),
 fog_( 1.f ),
 fogAnimation_( false, 5.f )
{	
	pPhotonOccluder_ = new CloudsPhotonOccluder;

	// watch some stuff
	static bool watchingClouds = false;
	if (!watchingClouds)
	{		
		//TODO : PJ put watchers back in, once we have decided about the interoperability
		//of the "clouds" and "sky" classes.
		//MF_WATCH( "Client Settings/Clouds/cur cover", avgCover_, Watcher::WT_READ_ONLY, "Average coulds cover" );
		//MF_WATCH( "Client Settings/Clouds/cur density", *this, &Clouds::avgDensity, "Average coulds density" );
		//MF_WATCH( "Client Settings/Clouds/precipitation", precipitation_, Watcher::WT_READ_ONLY, "Clouds precipitation" );
		MF_WATCH( "Client Settings/Clouds/transition time", cloudsTransitionTime_, Watcher::WT_READ_WRITE, "Time to transition between states" );

		watchingClouds = true;
	}
}


/**
 *	Destructor.
 */
Clouds::~Clouds( )
{
	if (pPhotonOccluder_)
	{
		delete pPhotonOccluder_;
	}	
}


/*static*/ void Clouds::init()
{
	char constantName[] = "CloudTextureX\0";
	for (uint i=0; i<3; i++)
	{			
		s_cloudsTextureSetter[i] = new CloudsTextureSetter;
		constantName[12] = (char)('1' + i);			
		*Moo::EffectConstantValue::get( constantName ) = s_cloudsTextureSetter[i];
	}

	s_cloudsBlendSetter = new CloudsBlendSetter;
	*Moo::EffectConstantValue::get( "CloudsBlendAmount" ) = s_cloudsBlendSetter;

	s_cloudsUseBlendSetter = new CloudsUseBlendSetter;
	*Moo::EffectConstantValue::get( "CloudsUseBlend" ) = s_cloudsUseBlendSetter;
	
	s_cloudsManagedEffect = Moo::EffectManager::instance().get( s_cloudsEffect );
}

/*static*/ void Clouds::fini()
{
	s_cloudsManagedEffect = NULL;
	s_cloudsUseBlendSetter = NULL;
	s_cloudsBlendSetter = NULL;		
	for (uint i=0; i<3; i++)
	{			
		s_cloudsTextureSetter[i] = NULL;
	}
}


/**
 *	This method is called by the envirominder when it
 *	is activated, e.g. when the camera has moved to a
 *	new space.
 */
void Clouds::activate( const EnviroMinder& em, DataSectionPtr pSpaceSettings )
{
	//This makes sure we match activate / deactivate pairs
	MF_ASSERT( !material_ )

	material_ = new Moo::EffectMaterial;
	if (!material_->initFromEffect(s_cloudsEffect))
	{
		ERROR_MSG( "Clouds::activate - could not load effect file %s", s_cloudsEffect.value().c_str() );
	}

	visual_ = Moo::VisualManager::instance()->get( s_cloudsVisual );
	if (!visual_)
	{
		ERROR_MSG( "Clouds::activate - could not load visual file %s", s_cloudsVisual.value().c_str() );
	}

	DataSectionPtr pClouds;
	std::string cloudsXML = pSpaceSettings->readString( "clouds", "" );
	if (cloudsXML != "")
		pClouds = BWResource::openSection(cloudsXML);
	else
		pClouds = pSpaceSettings->openSection( "clouds" );
	if (pClouds)
	{
		std::vector<DataSectionPtr>	ruleSections;
		pClouds->openSections( "clouds", ruleSections );
		for (uint i=0; i<ruleSections.size(); i++)
		{
			rules_.push_back( new Rule( ruleSections[i] ) );			
		}
	}
}


/**
 *	This method is called by the envirominder when it
 *	is deactivated, e.g. when the camera has moved to a
 *	new space and this environment is no longer in use.
 */
void Clouds::deactivate( const EnviroMinder& em )
{
	if (material_)
	{
		material_ = NULL;
		visual_ = NULL;

		for (uint i=0; i<rules_.size(); i++)
		{
			delete rules_[i];		
		}
		rules_.clear();
	}
}


/**
 *	Update our internal parameters based on the input weather settings
 */
void Clouds::update( const struct WeatherSettings & ws, float dTime, Vector3 sunDir, uint32 sunCol, float sunAngle )
{
	//no cloud rules, so no clouds.
	if (!rules_.size())
		return;

	//TODO : what do we do about this
	/*cloudStrata[0].colourMin = ws.colourMin;
	cloudStrata[0].colourMax = ws.colourMax;
	cloudStrata[0].cover = ws.cover;
	cloudStrata[0].cohesion = ws.cohesion;
	conflict_ = ws.conflict;
	temperature_ = ws.temperature;*/

	if (cloudsTransition_ == 0.f)
	{
		this->chooseBestMatch(ws);		
	}

	if (cloudsTransition_ > 0.f)
	{
		this->doCloudsTransition(dTime);		
	}

	if (lightingTransition_ > 0.f)
	{
		lightingTransition_ -= dTime;
		if (lightingTransition_ < 0.f)
			lightingTransition_ = 0.f;
		lightDim_ = lightDimAnimation_.animate( lightingTransition_ );
		fog_ = fogAnimation_.animate( lightingTransition_ );
	}

	s_cloudsUseBlendSetter->value( (bool)(cloudsTransition_ > 0.f) );
}


void Clouds::updateLightMap( SkyLightMap* lightMap )
{
	if (lightMap)
	{
		//TODO : redo the way the light map works		
		//lightMap_->readyForUpdate();
		//this->draw();				
		//lightMap_->finishedUpdate();
	}
}


/** 
 *	This method chooses the cloud rule that best matches
 *	the current weather settings.  If the rule is different
 *	to the rule currently in use, the cloudsTransition_ value is
 *	set to the standard cloudsTransitionTime_, indicating the clouds
 *	will begin blending to match the current weather.
 */
void Clouds::chooseBestMatch(const WeatherSettings& ws)
{
	//make sure we are not currently under transition
	MF_ASSERT( cloudsTransition_ == 0.f );

	//choose new best cloud maps and settings
	float bestMatch = FLT_MAX;
	Clouds::Rule* best = NULL;

	for (uint32 i=0; i<rules_.size(); i++)
	{
		Clouds::Rule& rule = *rules_[i];
		float match = rule.correlation(ws);
		if (match < bestMatch)
		{
			bestMatch = match;
			best = &rule;
		}
	}

	MF_ASSERT( best != NULL );
	if (best != current_)
	{
		DEBUG_MSG( "Weather is changing...\n" );
		current_ = best;
		cloudsTransition_ = cloudsTransitionTime_;

		lightingTransition_ = lightingTransitionTime_;
		lightDimAnimation_.clear();
		lightDimAnimation_.addKey( 0.f, current_->avgColourDim() );
		lightDimAnimation_.addKey( lightingTransitionTime_, lightDim_ );
		fogAnimation_.clear();
		fogAnimation_.addKey( 0.f, current_->fog() );
		fogAnimation_.addKey( lightingTransitionTime_, fog_ );
	}
}


/** 
 *	This method is called when the clouds are in transition from
 *	one set to another.  It sets up the textures and blend values
 *	for the underlying effect.
 *	Note that texture loading must be done in a background thread.
 *	The texture setters are multi-threaded for this purpose.  Thus
 *	when we would like to transit the textures, we first must set
 *	set the texture, then wait until the isLoading() flag is cleared.
 *	While isLoading() is true, we keep the cloudsTransition value
 *	at maximum (so that we always come back to this method, and so
 *	that we don't try and transition to another state in the meantime).
 */
void Clouds::doCloudsTransition( float dTime )
{
	MF_ASSERT( current_ );

	cloudsTransition_ -= dTime;		

	//transit the lower strata texture
	if (s_cloudsTextureSetter[0]->textureName() != current_->lowerStrata())
	{				
		if (cloudsTransition_ > 0.f)	//we are blending out the first texture
		{
			s_cloudsTextureSetter[2]->texture(current_->lowerStrata());
			if (s_cloudsTextureSetter[2]->isLoading())
			{
				//we are still waiting for the texture to load in the backg
				//thread.  keep the cloudsTransition timer stocked up.
				cloudsTransition_ += dTime;				
			}
			float t = cloudsTransition_ / cloudsTransitionTime_;
			//parameters :			( map0, map0..2blend, map1, map1..2blend)
			s_cloudsBlendSetter->value( Vector4(t, 1.f-t, 1.f, 0.f) );			
			return;
		}
		else					// we now should blend out the second texture
		{
			s_cloudsTextureSetter[0]->texture(current_->lowerStrata());
			cloudsTransition_ = cloudsTransitionTime_;			
		}
	}

	//transit the second texture
	if (s_cloudsTextureSetter[1]->textureName() != current_->upperStrata())
	{		
		if (cloudsTransition_ > 0.f)	//we are blending out the second texture
		{		
			s_cloudsTextureSetter[2]->texture(current_->upperStrata());
			if (s_cloudsTextureSetter[2]->isLoading())
			{
				//we are still waiting for the texture to load in the backg
				//thread.  keep the cloudsTransition timer stocked up.
				cloudsTransition_ += dTime;				
			}
			float t = cloudsTransition_ / cloudsTransitionTime_;
			//parameters :			( map0, map0..2blend, map1, map1..2blend)			
			s_cloudsBlendSetter->value( Vector4(1.f, 0.f, t, 1.f-t) );
			return;
		}
		else					//we are now finished
		{
			s_cloudsTextureSetter[1]->texture(current_->upperStrata());
			cloudsTransition_ = 0.f;
		}
	}

	//well, the second texture didn't require blending out. so finish now.
	DEBUG_MSG( "Transited to %s, %s\n", current_->lowerStrata().c_str(), current_->upperStrata().c_str() );
	cloudsTransition_ = 0.f;
}


/**
 * This method draws the cloud dome.
 */
void Clouds::draw()
{
	//no cloud rules, so no clouds.
	if (!rules_.size())
		return;

	if (visual_ && material_ && material_->begin())
	{
		for ( uint32 i=0; i<material_->nPasses(); i++ )
		{
			material_->beginPass(i);
			visual_->justDrawPrimitives();
			material_->endPass();
		}
		material_->end();
	}
}


/**
 *	This method constructs a cloud rule based on the datasection passed in.
 */
Clouds::Rule::Rule( DataSectionPtr pSection )
{
	lowerStrata_ = pSection->readString( "lower", "" );
	upperStrata_ = pSection->readString( "upper", "" );
	position_[0] = pSection->readFloat( "colour", 0.5f );
	position_[1] = pSection->readFloat( "cover", 0.5f );
	position_[2] = pSection->readFloat( "cohesion", 0.5f );
	light_ = pSection->readVector3( "light", Vector3(1.f,1.f,1.f) );
	fog_ = pSection->readFloat( "fog", 1.f );
}


/**
 *	This method returns a value idicating how much this cloud rule
 *	correlates to the passed in weather settings.  The returned
 *	value is not normalised, but can be compared to any other
 *	correlation value.  A value close to 0 indicates greater correlation.
 */
float Clouds::Rule::correlation( const WeatherSettings& ws ) const
{
	Vector3 testPt( ws.colourMin, ws.cover, ws.cohesion );
	testPt -= position_;
	return testPt.lengthSquared();
}
