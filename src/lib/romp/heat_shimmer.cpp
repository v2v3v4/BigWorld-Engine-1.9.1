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
#include "moo/render_context.hpp"
#include "moo/visual_manager.hpp"
#include "moo/visual_channels.hpp"
#include "heat_shimmer.hpp"
#include "full_screen_back_buffer.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/auto_config.hpp"
#include "cstdmf/debug.hpp"
#include "geometrics.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 0 );

static bool s_debugTex = false;
static float s_speed = 121.f;
static float s_spreadX = 0.f;
static float s_spreadY = 0.4f;
static float s_freqS = 0.f;
static float s_freqT = 0.7f;
static float s_uFixup = -0.025f;
static float s_vFixup = 2.f;
Vector4ProviderPtr HeatShimmer::s_alphaProvider = NULL;

static AutoConfigString s_mfmName( "fx/shimmerMaterial" );
static AutoConfigString s_visualName( "fx/shimmerVisual" );

#define SETTINGS_ENABLED (this->shimmerSettings_->activeOption() == 0)

BW_SINGLETON_STORAGE( HeatShimmer );
HeatShimmer s_heatShimmer;

// -----------------------------------------------------------------------------
// Section: class HeatShimmer
// -----------------------------------------------------------------------------
HeatShimmer::HeatShimmer():
	inited_(false),
	watcherEnabled_(true)
#ifdef EDITOR_ENABLED
	,editorEnabled_(true)
#endif
{
	FullScreenBackBuffer::addUser( this );

	{
		MF_WATCH( "Client Settings/fx/Heat/enable",
			watcherEnabled_,
			Watcher::WT_READ_WRITE,
			"Enable the full-screen heat shimmer effect." );
		MF_WATCH( "Client Settings/fx/Heat/speed",
			s_speed,
			Watcher::WT_READ_WRITE,
			"Speed at which the shimmer noise ripples the back buffer." );
		MF_WATCH( "Client Settings/fx/Heat/spread x",
			s_spreadX,
			Watcher::WT_READ_WRITE,
			"Amplitude of the shimmer noise in texels on the X axis." );		
		MF_WATCH( "Client Settings/fx/Heat/spread y",
			s_spreadY,
			Watcher::WT_READ_WRITE,
			"Amplitude of the shimmer noise in texels on the Y axis." );
		MF_WATCH( "Client Settings/fx/Heat/S noise freq",
			s_freqS,
			Watcher::WT_READ_WRITE,
			"Frequency of the shimmer noise in seconds on the X axis" );
		MF_WATCH( "Client Settings/fx/Heat/T noise freq",
			s_freqT,
			Watcher::WT_READ_WRITE,
			"Frequency of the shimmer noise in seconds on the Y axis" );	
		MF_WATCH( "Client Settings/fx/Heat/u fix up",
			s_uFixup,
			Watcher::WT_READ_WRITE,
			"Texel offset in the x axis applied to the shimmered back buffer "
			"transfer." );
		MF_WATCH( "Client Settings/fx/Heat/v fix up",
			s_vFixup,
			Watcher::WT_READ_WRITE,
			"Texel offset in the y axis applied to the shimmered back buffer "
			"transfer." );
		MF_WATCH( "Client Settings/fx/Heat/debug texture",
			s_debugTex,
			Watcher::WT_READ_WRITE,
			"Display or hide the alpha channel of the back buffer, which "
			"represents the amount of shimmered back buffer to copy back "
			"over itself." );
	}
}


HeatShimmer::~HeatShimmer()
{
	this->finz();

	FullScreenBackBuffer::removeUser( this );
}


bool HeatShimmer::isSupported()
{
	if (Moo::rc().vsVersion() < 0x101)
	{
		INFO_MSG( "Heat Shimmer is not supported because the vertex shader version is not sufficient\n" );
		return false;
	}

	const Moo::DeviceInfo& di = Moo::rc().deviceInfo( Moo::rc().deviceIndex() );

	//TODO : relax this constraint and support shimmer using next-power-of-2-up textures.
	if (di.caps_.TextureCaps & D3DPTEXTURECAPS_POW2 && !(di.caps_.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL))
	{
		INFO_MSG( "Heat Shimmer is not supported because non-power of 2 textures are not supported\n" );
		return false;
	}

	DataSectionPtr pSection = BWResource::openSection( s_mfmName );
	if (!pSection)
	{
		INFO_MSG( "Heat Shimmer is not supported because the material could not be found\n" );
		return false;
	}

	return true;
}


bool HeatShimmer::init()
{
	if ( inited_ )
		return true;

	if ( !HeatShimmer::isSupported() )
		return false;


	this->shimmerSettings_ = 
		Moo::makeCallbackGraphicsSetting(
			"HEAT_SHIMMER", "Heat Shimmer", *this, 
			&HeatShimmer::setShimmerOption, 
			HeatShimmer::isSupported() ? 0 : 1,
			false, false);
				
	this->shimmerSettings_->addOption("ON", "On", HeatShimmer::isSupported());
	this->shimmerSettings_->addOption("OFF", "Off", true);
	Moo::GraphicsSetting::add(this->shimmerSettings_);



	bool ok = false;

	if ( s_mfmName.value() == "" )
	{
		ERROR_MSG( "No MFM was specified for heat shimmer\n" );
		return false;
	}

	if ( s_visualName.value() == "" )
	{
		ERROR_MSG( "No Visual was specified for heat shimmer\n" );
		return false;
	}

	//create transfer mesh
	visual_ = Moo::VisualManager::instance()->get( s_visualName );
	if ( !visual_ )
	{
		ERROR_MSG( "Could not find visual file %s\n", s_visualName.value().c_str() );
		return false;
	}

	//create material
	effectMaterial_ = new Moo::EffectMaterial();
	DataSectionPtr pSection = BWResource::openSection( s_mfmName );
	if (pSection)
	{
		effectMaterial_->load( pSection );
		parameters_.effect( effectMaterial_->pEffect()->pEffect() );
		ok = true;
	}
	else
	{
		effectMaterial_ = NULL;
		ok = false;
	}

	this->setShimmerStyle(2);

	inited_ = ok;
	return ok;
}


void HeatShimmer::finz()
{
	if ( !inited_ )
		return;	

	if (effectMaterial_)
	{
		effectMaterial_ = NULL;
	}

	visual_ = NULL;	
	inited_ = false;
}


/**
 *	This method returns true iff heat shimmer is available for this frame.
 */
bool HeatShimmer::isEnabled()
{
	bool enabled = (inited_ && SETTINGS_ENABLED && watcherEnabled_);
#ifdef EDITOR_ENABLED
	enabled &= editorEnabled_;
#endif
	return enabled;
}


void HeatShimmer::deleteUnmanagedObjects()
{
	//set to NULL so that we re-acquire handles later on.
	//don't re-acquire in createUnmanaged because we cannot
	//depend on the recreation order.
	parameters_.effect( NULL );
}


void HeatShimmer::beginScene()
{
	MF_ASSERT( isEnabled() );

	Moo::rc().setRenderState( 
		D3DRS_COLORWRITEENABLE, 
			D3DCOLORWRITEENABLE_RED | 
			D3DCOLORWRITEENABLE_GREEN | 
			D3DCOLORWRITEENABLE_BLUE );

	this->setShimmerMaterials( true );
}


void HeatShimmer::endScene()
{
	MF_ASSERT( isEnabled() );

	this->drawShimmerChannel();
		
	Moo::rc().setRenderState( 
		D3DRS_COLORWRITEENABLE, 
			D3DCOLORWRITEENABLE_RED | 
			D3DCOLORWRITEENABLE_GREEN | 
			D3DCOLORWRITEENABLE_BLUE | 
			D3DCOLORWRITEENABLE_ALPHA );
}


bool HeatShimmer::doTransfer( bool fsbbTransferredAlready )
{
	this->setShimmerMaterials(false);
	this->draw(1.f, 1.f);
	return true;
}


void HeatShimmer::setRenderState()
{
	if ( !parameters_.hasEffect() )
	{
		parameters_.effect( effectMaterial_->pEffect()->pEffect() );
	}

	DX::Device* device = Moo::rc().device();

	Moo::rc().setVertexShader( NULL );
	Moo::rc().setPixelShader( NULL );
	Moo::rc().setFVF( Moo::VertexXYZNUV::fvf() );	

	parameters_.setTexture( "BackBuffer", FullScreenBackBuffer::renderTarget().pTexture() );

	//SHADER CONSTANTS ...

	//geometric offset for precise pixel/texel alignment
	//pc shader works in clip coordinates, so we just offset the mesh by
	//the pixel/texel alignment factor.  y is +ve because clip space is the right way up	
	float width = Moo::rc().screenWidth();
	float height = Moo::rc().screenHeight();
	Vector4 geometricOffset( 1.f, 1.f, -1.f/width, 1.f/height );
	parameters_.setVector( "SCREEN_FACTOR_OFFSET", &geometricOffset );

	//10 - animation constants
	static float s_animationT = 0.f;
	s_animationT += (0.03f * (s_speed/10.f));
	float t = s_animationT;
	Vector4 animationCsts( s_spreadX / width, s_spreadY / height, t, 3.141592654f * 2.f );
	parameters_.setVector( "ANIMATION", &animationCsts );

	//11 - time offset
	float timeOffset(0.2f);
	parameters_.setFloat( "TIME_OFFSET", timeOffset );

	//14/15 - s/t wave directions
	float hw = width/2.f;
	float hh = height/2.f;
	Vector4 SDir( s_freqS * 0.25f * hw, s_freqS * 0.f * hh, s_freqS * -0.7f * hw, s_freqS * -0.8f * hh );
	Vector4 TDir( s_freqT * 0.f * hw, s_freqT * 0.015f * hh, s_freqT * -0.7f * hw, s_freqT * 0.1f * hh );
	parameters_.setVector( "NOISE_FREQ_S", &SDir );
	parameters_.setVector( "NOISE_FREQ_T", &TDir );	

	//19 - more fixups
	Vector4 uvFix( s_uFixup/ width, s_vFixup / height, 0.f, 0.f );
	parameters_.setVector( "UVFIX", &uvFix );	
	
	//the overall alpha
	Vector4 fullScreenAlpha(0,0,0,0);
	if ( s_alphaProvider ) s_alphaProvider->output( fullScreenAlpha );
	parameters_.setFloat( "FULLSCREEN_ALPHA", fullScreenAlpha.w );

	//debug
	static bool s_lastDebugTex = false;
	if (s_debugTex != s_lastDebugTex)
	{
		s_lastDebugTex = s_debugTex;
		effectMaterial_->hTechnique( s_debugTex ? "debug" : "standard" );
	}
}


void HeatShimmer::draw( float alpha, float wobbliness )
{
	MF_ASSERT( isEnabled() );

	this->setRenderState();	

	if (effectMaterial_->begin())
	{
		for ( uint32 i=0; i<effectMaterial_->nPasses(); i++ )
		{
			effectMaterial_->beginPass(i);			
			visual_->justDrawPrimitives();
			effectMaterial_->endPass();
		}
		effectMaterial_->end();
	}

	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );
	Moo::rc().setTexture(0, NULL);
	Moo::rc().setTexture(1, NULL);
	Moo::rc().setTexture(2, NULL);
	Moo::rc().setTexture(3, NULL);
	Moo::rc().setPixelShader( NULL );
}


void HeatShimmer::setShimmerAlpha( Vector4ProviderPtr v4 )
{
	s_alphaProvider = v4;
}


void HeatShimmer::drawShimmerChannel()
{
	MF_ASSERT( isEnabled() );
	
	Moo::ShimmerChannel::draw();
}


void HeatShimmer::setShimmerMaterials(bool status)
{
	MF_ASSERT( isEnabled() );
	
	Moo::Material::shimmerMaterials = status;
}


/*~	function BigWorld.setShimmerAlpha
 *
 *	This function affects the heat shimmer that BigWorld produces as a built-in effect.  Depending on the heat 
 *	shimmer style set via BigWorld.setShimmerStyle(), the shimmer alpha setting will affect how noticable the 
 *	shimmer is.  As a VectorProvider, this value can be updated anywhere and be immediately reflected in-game. 
 *
 *	@param	alpha	Adaptable alpha setting, represented by a Vector4Provider
 */
PY_MODULE_STATIC_METHOD( HeatShimmer, setShimmerAlpha, BigWorld )


void HeatShimmer::setShimmerStyle( int style )
{
	switch ( style )
	{
	case 0: //this has an intentional 1-pixel inaccuracy when blending on the screen.
			//this style is used by default, for shimmer suits etc.  1 pixel offset means
			//it is easier to see		
		s_speed = 121.f;
		s_spreadX = 0.1f;
		s_spreadY = 0.3f;
		s_freqS = 1.f;
		s_freqT = 0.7f;		
		s_uFixup = 1.f;
		s_vFixup = 1.f;
		break;
	case 1: //this is the full-on but screen-corrected style.  used for large shockwaves.		
		s_speed = 180.f;
		s_spreadX = 0.4f;
		s_spreadY = 0.68f;
		s_freqS = 2.f;
		s_freqT = 2.7f;		
		s_uFixup = 0.f;
		s_vFixup = 0.f;
		break;
	case 2:	//Note - this is the default heat shimmer style, very subtle + no screen offset		
		s_speed = 121.f;
		s_spreadX = 0.1f;
		s_spreadY = 0.3f;
		s_freqS = 1.f;
		s_freqT = 0.7f;		
		s_uFixup = 0.f;
		s_vFixup = 0.f;
		break;
	case 3: //Note - this is a good setting for water shimmer		
		s_speed = 102.f;
		s_spreadX = 1.f;
		s_spreadY = 1.6f;
		s_freqS = -8.f;
		s_freqT = -6.3f;		
		s_uFixup = 0.f;
		s_vFixup = 0.f;
		break;
	}
}


/*~ function BigWorld.setShimmerStyle
 *
 *	This functions set the shimmer style used by BigWorld.  For example, when looking over water, switch to a water 
 *	shimmer, when standing on terrain, use a heat shimmer, when shimmer is to reflect off Entities, rather than 
 *	environment, use an Entity shimmer and when a large explosion of shockwave is activated, enforce the shockwave 
 *	shimmer, then use BigWorld.setShimmerAlpha() to configure the shimmer.  The available shimmer styles are as 
 *	follows;
 *
 *		0	Suit Shimmer
 *		1	Shockwave Shimmer
 *		2	Heat Shimmer
 *		3	Water Shimmer
 *
 *	@param	style	An integer represeting the one of the styles shown above
 */
PY_MODULE_STATIC_METHOD( HeatShimmer, setShimmerStyle, BigWorld )