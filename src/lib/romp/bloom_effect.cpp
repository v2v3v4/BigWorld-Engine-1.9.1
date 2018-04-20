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
#include "bloom_effect.hpp"
#include "transfer_mesh.hpp"
#include "back_buffer_copy.hpp"
#include "resmgr/auto_config.hpp"

#include "resmgr/bwresource.hpp"
#include "texture_feeds.hpp"
#include "cstdmf/debug.hpp"
#include "full_screen_back_buffer.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 0 );

AutoConfigString s_downSampleEffect( "system/bloom/downSample" );
AutoConfigString s_colourScaleEffect( "system/bloom/colourScale" );
AutoConfigString s_gaussianBlurEffect( "system/bloom/gaussianBlur" );
AutoConfigString s_transferEffect( "system/bloom/transfer" );
AutoConfigString s_downSampleColourScaleEffect( "system/bloom/downSampleColourScale" );

BW_SINGLETON_STORAGE( Bloom );
Bloom s_bloom;


static FilterSample Filter4[] = {        // 12021 4-tap filter
	{ 1.f/6.f, -2.5f },
	{ 2.f/6.f, -0.5f },
	{ 2.f/6.f,  0.5f },
	{ 1.f/6.f,  2.5f },
};

static FilterSample Filter24[] = {
	{ 0.3327f,-10.6f },
	{ 0.3557f, -9.6f },
	{ 0.3790f, -8.6f },
	{ 0.4048f, -7.6f },
	{ 0.4398f, -6.6f },
	{ 0.4967f, -5.6f },
	{ 0.5937f, -4.6f },
	{ 0.7448f, -3.6f },
	{ 0.9418f, -2.6f },
	{ 1.1414f, -1.6f },
	{ 1.2757f, -0.6f },
	{ 1.2891f,  0.4f },
	{ 1.1757f,  1.4f },
	{ 0.9835f,  2.4f },
	{ 0.7814f,  3.4f },
	{ 0.6194f,  4.4f },
	{ 0.5123f,  5.4f },
	{ 0.4489f,  6.4f },
	{ 0.4108f,  7.4f },
	{ 0.3838f,  8.4f },
	{ 0.3603f,  9.4f },
	{ 0.3373f, 10.4f },
	{ 0.0000f,  0.0f },
	{ 0.0000f,  0.0f },
};

// If we've turned off bloom, but we still want blur, we still need to be enabled.
#define SETTINGS_ENABLED (this->bloomSettings_->activeOption() == 0 || !bloomBlur_)

Bloom::Bloom():
    renderTargetWidth_(0),
	renderTargetHeight_(0),
	transferMesh_(NULL),
	inited_(false),
	watcherEnabled_(true),
	bbc_(NULL),
	rt0_(NULL),
	rt1_(NULL),
	wasteOfMemory_(NULL),
	filterMode_(1),
	colourAttenuation_(1.f,1.f,1.f,0.9f),
	scalePower_(8.f),
	cutoff_(0.6f),
	width_(1.f),
	bbWidth_(0),
	bbHeight_(0),
	bloomBlur_( true ),
	nPasses_(2),
	downSample_( NULL ),
	downSampleColourScale_( NULL ),
	gaussianBlur_( NULL ),
	colourScale_( NULL ),
	controller_( NULL ),
	colourAttenuationController_( NULL ),
	transfer_( NULL ),
	bloomSettings_( NULL )
#ifdef EDITOR_ENABLED
	,editorEnabled_(true)
#endif
{
	{
		MF_WATCH( "Client Settings/fx/Bloom/enable",
			watcherEnabled_,
			Watcher::WT_READ_WRITE,
			"Enable the full-screen blooming effect," );
		MF_WATCH( "Client Settings/fx/Bloom/filter mode", filterMode_,
			Watcher::WT_READ_WRITE,
			"Gaussian blur filter kernel mode, either 0 (4x4 kernel, "
			"faster) or 1 (24x24 kernel, slower)." );
		MF_WATCH( "Client Settings/fx/Bloom/colour attenuation",
			colourAttenuation_,
			Watcher::WT_READ_WRITE,
			"Colour attenuation per-pass.  Should be set much lower if using "
			"the 24x24 filter kernel." );
		MF_WATCH( "Client Settings/fx/Bloom/bloom and blur",
			bloomBlur_,
			Watcher::WT_READ_WRITE,
			"If set to true, then blooming AND blurring occur.  If set to false"
			", only the blur takes place (and is not overlaid on the screen.)" );
		MF_WATCH( "Client Settings/fx/Bloom/num passes",
			nPasses_,
			Watcher::WT_READ_WRITE,
			"Set the number of blurring passes applied to the bloom texture." );		
		MF_WATCH( "Client Settings/fx/Bloom/scale power",
			scalePower_,
			Watcher::WT_READ_WRITE,
			"power of colour scaling function for shader 2 and above hardware." );		
		MF_WATCH( "Client Settings/fx/Bloom/hi-pass cutoff",
			cutoff_,
			Watcher::WT_READ_WRITE,
			"cutoff point for luminance when calculating bloom region." );
		MF_WATCH( "Client Settings/fx/Bloom/width",
			width_,
			Watcher::WT_READ_WRITE,
			"Multiplier on the filter width." );
	}

	FullScreenBackBuffer::addUser( this );
}


Bloom::~Bloom()
{
	FullScreenBackBuffer::removeUser( this );
}


/**
 *	This method is called in response to the shader version cap
 *	graphics setting being changed.  We need to know if we should
 *	disable ourselves, or if we should switch to using the old
 *	style bloom render target.
 */
void Bloom::onSelectPSVersionCap(int psVerCap)
{
	//this eventually means finz, init are called.
	//can't finz here because that deletes effects, and you can't
	//do that inside this callback function, or you hit an assert
	//to do with mutices
	bbWidth_ = -1;
}


bool Bloom::isSupported()
{
	if (Moo::rc().vsVersion() < 0x101)
	{
		INFO_MSG( "Blooming is not supported because the vertex shader version is not sufficient\n" );
		return false;
	}
	if (Moo::rc().psVersion() < 0x101)
	{
		INFO_MSG( "Blooming is not supported because the pixel shader version is not sufficient\n" );
		return false;
	}
	if (!BWResource::openSection( s_downSampleEffect ))
	{
		INFO_MSG( "Blooming is not supported because the down sample effect could not be found\n" );
		return false;
	}
	if (!BWResource::openSection( s_downSampleColourScaleEffect ))
	{
		INFO_MSG( "Blooming is not supported because the down sample colour scale effect could not be found\n" );
		return false;
	}
	if (!BWResource::openSection( s_gaussianBlurEffect ))
	{
		INFO_MSG( "Blooming is not support because the gaussian blur effect could not be found\n" );
		return false;
	}
	if (!BWResource::openSection( s_colourScaleEffect ))
	{
		INFO_MSG( "Blooming is not supported because the gaussian blur effect could not be found\n" );
		return false;
	}	
	if (!BWResource::openSection( s_transferEffect ) )
	{
		INFO_MSG( "Blooming is not supported because the transfer effect could not be found\n" );
		return false;
	}

	const Moo::DeviceInfo& di = Moo::rc().deviceInfo( Moo::rc().deviceIndex() );

	//TODO : relax this constraint and support blooming using next-power-of-2-up textures.
	if (di.caps_.TextureCaps & D3DPTEXTURECAPS_POW2 && !(di.caps_.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL))
	{
		INFO_MSG( "Blooming is not supported because non-power of 2 textures are not supported\n" );
		return false;
	}

	return true;
}


bool Bloom::init()
{
	if ( !Bloom::isSupported() )
	{
		INFO_MSG( "Blooming is not supported on this hardware\n" );
		return false;
	}


	// bloom filter settings
	this->bloomSettings_ = 
		Moo::makeCallbackGraphicsSetting(
			"BLOOM_FILTER", "Bloom Filter", *this, 
			&Bloom::setBloomOption, 
			Bloom::isSupported() ? 0 : 1,
			false, false);

	Moo::EffectManager::instance().addListener( this );
				
	Bloom::pInstance()->bloomSettings_->addOption("ON", "On", Bloom::isSupported());
	Bloom::pInstance()->bloomSettings_->addOption("OFF", "Off", true);
	Moo::GraphicsSetting::add(this->bloomSettings_);

	return this->initInternal();
}


void Bloom::fini()
{
	this->finzInternal();

	Moo::EffectManager::instance().delListener( this );

	this->rt0_ = NULL;
	this->rt1_ = NULL;
	this->wasteOfMemory_ = NULL;
}


bool Bloom::isEnabled()
{
	bool enabled = (inited_ && SETTINGS_ENABLED && watcherEnabled_ && 
				(Moo::EffectManager::instance().PSVersionCap() >= 1));
#ifdef EDITOR_ENABLED
	enabled &= editorEnabled_;
#endif
	return enabled;
}


/**
 *	This method allocates pointer rt if it is null, and then calls
 *	render target create.  For non-editors it registers the texture
 *	map as a texture feed as well.
 *
 *	@return true if the render target texture was created succesfully.
 */
bool Bloom::safeCreateRenderTarget(
	Moo::RenderTargetPtr& rt,
	int width,
	int height,
	bool reuseZ,
	const std::string& name )
{
	if ( !rt ) rt = new Moo::RenderTarget( name );
	if (rt->create( width, height, reuseZ ))
	{
		PyTextureProvider* texProv = new PyTextureProvider(NULL,rt);
		TextureFeeds::addTextureFeed(name,texProv);
		Py_DecRef(texProv);
	}
	return (rt->pTexture() != NULL);
}


/**
 *	This method allocates pointer mat if it is null, and initialises
 *	the material from the given effect name.
 *	If anything fails, the material pointer is freed.
 *
 *	@return true if the effect material was successfully initialised.
 */
bool Bloom::safeCreateEffect( Moo::EffectMaterialPtr& mat, const std::string& effectName )
{
	DataSectionPtr pSection = BWResource::openSection(effectName);
	if ( pSection )
	{
		if (!mat)
		{
			mat = new Moo::EffectMaterial;
		}
		
		if (mat->initFromEffect(effectName))
		{
			return true;
		}
		mat = NULL;
	}
	return false;
}


bool Bloom::initInternal()
{
	if ( inited_ )
		return true;

	bbWidth_ = (int)Moo::rc().screenWidth();
	bbHeight_ = (int)Moo::rc().screenHeight();
    renderTargetWidth_ = std::max(1, bbWidth_ >> 2);
	renderTargetHeight_ = std::max(1, bbHeight_ >> 2);

	if ( bbWidth_ == 0 || bbHeight_ == 0 )
		return false;

	transferMesh_ = new SimpleTransfer;
	bbc_ = new RectBackBufferCopy;
	bbc_->init();
	
	bool shader2 = (Moo::EffectManager::instance().PSVersionCap() >= 2);
	if ( !shader2 )
	{
		//Only need 'waste of memory' render target on shader 1 hardware; shader 2
		//and above can down-sample and colour scale at the same time.
		//TODO : find out another way to better use memory
		if (!safeCreateRenderTarget( wasteOfMemory_, bbWidth_, bbHeight_, true, "wasteOfMemory" ))
		{
			ERROR_MSG( "Could not create texture pointer for bloom render target W.O.M\n" );
			return false;
		}
	}
	
	//Render target 0 is a quarter size target.
	if (!safeCreateRenderTarget( rt0_, renderTargetWidth_, renderTargetHeight_, false, "bloom" ))	
	{
		ERROR_MSG( "Could not create texture pointer for bloom render target 0\n" );
		return false;
	}

	//Render target 1 is also a quarter size target.
	if (!safeCreateRenderTarget( rt1_, renderTargetWidth_, renderTargetHeight_, false, "bloom2" ))
	{
		ERROR_MSG( "Could not create texture pointer for bloom render target 1\n" );
		return false;
	}

	// Create the shaders
	if (!safeCreateEffect( downSample_, s_downSampleEffect ) )
	{
		ERROR_MSG( "Could not load effect material for the downsample effect\n" );		
		return false;
	}

	if (!safeCreateEffect( downSampleColourScale_, s_downSampleColourScaleEffect ) )
	{
		ERROR_MSG( "Could not load effect material for the downsample colourscale effect\n" );		
		return false;
	}	

	if (!safeCreateEffect( gaussianBlur_, s_gaussianBlurEffect ) )
	{
		ERROR_MSG( "Could not load effect material for the gaussian blur effect\n" );		
		return false;
	}

	if (!safeCreateEffect( colourScale_, s_colourScaleEffect ) )
	{
		ERROR_MSG( "Could not load effect material for the colour scale effect\n" );		
		return false;
	}

	if (!safeCreateEffect( transfer_, s_transferEffect ) )
	{
		ERROR_MSG( "Could not load effect material for the transfer effect\n" );		
		return false;
	}

	downSampleParameters_.effect( downSample_->pEffect()->pEffect() );
	downSampleColourScaleParameters_.effect( downSampleColourScale_->pEffect()->pEffect() );
	colourScaleParameters_.effect( colourScale_->pEffect()->pEffect() );
	gaussianParameters_.effect( gaussianBlur_->pEffect()->pEffect() );
	transferParameters_.effect( transfer_->pEffect()->pEffect() );	

	inited_ = true;
	return true;
}


void Bloom::finzInternal()
{
	if ( !inited_ )
		return;

	if (Moo::rc().device())
	{
		colourScale_ = NULL;
		gaussianBlur_ = NULL;
		downSample_ = NULL;
		downSampleColourScale_ = NULL;
		transfer_ = NULL;
	}

	if(bbc_)
	{
		bbc_->finz();
		delete bbc_;
		bbc_ = NULL;
	}

	if (rt0_)
	{
		rt0_->release();
	}

	if (rt1_)
	{
		rt1_->release();
	}

	if (wasteOfMemory_)
	{
		wasteOfMemory_->release();
	}

	if (transferMesh_)
	{
		delete transferMesh_;
		transferMesh_ = NULL;
	}

	TextureFeeds::delTextureFeed("wasteOfMemory");
	TextureFeeds::delTextureFeed("bloom");
	TextureFeeds::delTextureFeed("bloom2");

	inited_ = false;
}


void Bloom::deleteUnmanagedObjects()
{
	downSampleParameters_.effect( NULL );	
	colourScaleParameters_.effect( NULL );	
	gaussianParameters_.effect( NULL );	
	transferParameters_.effect( NULL );	
	downSampleColourScaleParameters_.effect( NULL );
}


void Bloom::applyPreset( bool blurOnly, int filterMode, float colourAtten, int nPasses )
{
	bloomBlur_ = !blurOnly;
	filterMode_ = filterMode;
	colourAttenuation_ = Vector4(1.f,1.f,1.f,colourAtten);
	nPasses_ = nPasses;
}


void Bloom::doPostTransferFilter()
{
	// TODO : use StretchBlt to capture the backbuffer. 
	// The symptom of this at the moment is the blooming does not move
	// via heat shimmer, and also the player transparency creates a visual
	// discrepancy because the blooming ignores it.
	// if (alreadyTransferred)
	//		copy back buffer back to the full screen surface.
	//
	if (controller_)
	{
		controller_->tick( 0.033f );
		Vector4 values;
		controller_->output( values );
		
		nPasses_ = (uint32)max( 0.f, values.x + 0.5f );
		scalePower_ = values.y;
		width_ = values.z;
		cutoff_ = values.w;
	}

	if (colourAttenuationController_)
	{
		colourAttenuationController_->tick( 0.033f );		
		colourAttenuationController_->output( colourAttenuation_ );		
	}


	// Check to see if the temporary buffers need to be re-generated
	// due to a change in the frame buffer sizes..
	if(bbWidth_!= (int)Moo::rc().screenWidth() || bbHeight_ != Moo::rc().screenHeight())
	{
		this->finzInternal();
		this->initInternal();
	}

	if (!inited_)
		return;

	MF_ASSERT( isEnabled() )

	if (!downSampleParameters_.hasEffect())
	{
		downSampleParameters_.effect( downSample_->pEffect()->pEffect() );
		downSampleColourScaleParameters_.effect( downSampleColourScale_->pEffect()->pEffect() );
		colourScaleParameters_.effect( colourScale_->pEffect()->pEffect() );
		gaussianParameters_.effect( gaussianBlur_->pEffect()->pEffect() );
		transferParameters_.effect( transfer_->pEffect()->pEffect() );
	}

	static DogWatch bloomTimer( "Bloom" );
	ScopedDogWatch btWatcher( bloomTimer );	
	
	Moo::rc().device()->SetTransform( D3DTS_WORLD, &Matrix::identity );
	Moo::rc().device()->SetTransform( D3DTS_VIEW, &Matrix::identity );
	Moo::rc().device()->SetTransform( D3DTS_PROJECTION, &Matrix::identity );
	Moo::rc().setPixelShader( 0 );

	DX::BaseTexture *pSource = NULL;

	bool shader2 = (Moo::EffectManager::instance().PSVersionCap() >= 2);
			
	if( bloomBlur_ && !shader2 )
	{
		// If we're blooming and we're on shader 1 hardware then
		// we need to colour scale the back buffer into the
		//'waste of memory' render target.
		this->captureBackBuffer();		
		pSource = wasteOfMemory_->pTexture();
		sourceDimensions_.x = (float)wasteOfMemory_->width();
		sourceDimensions_.y = (float)wasteOfMemory_->height();		
	}
	else
	{
		//If we're only blurring, or we are on shader 2 hardware
		//we can just use the texture in the Fullscreen back buffer.
		pSource = FullScreenBackBuffer::renderTarget().pTexture();
		sourceDimensions_.x = (float)bbWidth_;
		sourceDimensions_.y = (float)bbHeight_;
	}

	// Early out if there are missing textures.
	if(!pSource)
		return;
		 
	srcWidth_ = bbWidth_;
	srcHeight_ = bbHeight_;	

	rt1_->push();
	if (!bloomBlur_ || !shader2)
	{
		//downsample the current texture using a 16-tap single pass fetch.
		//The colour scale has already been done.
		this->downSample( pSource, *downSample_, downSampleParameters_ );
	}
	else
	{
		// Downsample and colour scale the current texture using a
		//16-tap single pass fetch.
		this->downSample( pSource, *downSampleColourScale_, downSampleColourScaleParameters_ );
	}
	rt1_->pop();
	
	
	// Get ready to do n passes on the blurs.
	srcWidth_ = renderTargetWidth_;
	srcHeight_ = renderTargetHeight_;

	uint sampleCount;
	FilterSample* pSamples;
	if( filterMode_ == GAUSS_24X24)
	{
		sampleCount = 24;
		pSamples = &Filter24[0];
	}
	else
	{
		sampleCount = 4;
		pSamples = &Filter4[0];
	}
	
	// Guassian blur
	for ( int p=0; p<nPasses_; p++ )
	{			
		// Apply subsequent filter passes using the selected filter
		// kernel - for different effects just add different kernels
		// NOTE: Number of entrries in the kernel must be a multiple
		// of 4.

		rt0_->push();	
		sourceDimensions_.x = (float)rt1_->width();
		sourceDimensions_.y = (float)rt1_->height();;	
		this->filterCopy( rt1_->pTexture(), sampleCount, pSamples, true );
		rt0_->pop();
		
		rt1_->push();	
		sourceDimensions_.x = (float)rt0_->width();
		sourceDimensions_.y = (float)rt0_->height();;	
		this->filterCopy( rt0_->pTexture(), sampleCount, pSamples, false );
		rt1_->pop();
	}
	
	// If we are just creating a blur texture, instead of blooming,
	// then we don't perform a full screen transfer. This will be
	// done at another time.
	if( bloomBlur_ )
	{
		transferParameters_.setTexture( "diffuseMap", rt1_->pTexture() );
		if (transfer_->begin())
		{
			for (uint32 i=0; i<transfer_->nPasses(); i++)
			{
				transfer_->beginPass(i);
				Vector2 tl( -0.5f, -0.5f );
				Vector2 dimensions((float)bbWidth_, (float)bbHeight_);
				transferMesh_->draw( tl, dimensions, Vector2(1.f,1.f), true );
				transfer_->endPass();
			}
			transfer_->end();
		}		
	}

	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );
	Moo::rc().setTexture(0, NULL);
	Moo::rc().setTexture(1, NULL);
	Moo::rc().setTexture(2, NULL);
	Moo::rc().setTexture(3, NULL);
	Moo::rc().setPixelShader( NULL );
}


void Bloom::captureBackBuffer()
{
	DX::Viewport vp;

	vp.X = vp.Y = 0;
	vp.MinZ = 0;
	vp.MaxZ = 1;
	vp.Width = bbWidth_;
	vp.Height = bbHeight_;

	if ( wasteOfMemory_->valid() && wasteOfMemory_->push() )
	{
		Moo::rc().setViewport( &vp );
		colourScaleParameters_.setTexture( "diffuseMap", FullScreenBackBuffer::renderTarget().pTexture() );	

		if (colourScale_->begin())
		{
			for ( uint32 i=0; i<colourScale_->nPasses(); i++ )
			{
				colourScale_->beginPass(i);
				// note bbc_ always applies pixel-texel alignment correction
				Vector2 tl(0,0);
				Vector2 dimensions((float)bbWidth_,(float)bbHeight_);
				bbc_->draw( tl, dimensions, tl, dimensions, true);
				colourScale_->endPass();
			}
			colourScale_->end();
		}
		
			
		wasteOfMemory_->pop();
	}
}


void Bloom::downSample(DX::BaseTexture* pSrc, 
	Moo::EffectMaterial& mat, EffectParameterCache& matCache )
{
	Moo::rc().setFVF( D3DFVF_XYZRHW | D3DFVF_TEX4 );

	struct FILTER_VERTEX { float x, y, z, w; struct uv { float u, v; } tex[4]; };
	//size determines the subset of the dest. render target to draw into,
	//and is basically width/ height in clip coordinates.
	Vector2 size( (float)Moo::rc().screenWidth(),
					(float)(Moo::rc().screenHeight()));

	//fixup is the geometric offset required for exact pixel-texel alignment
	Vector2 fixup( -0.5f, -0.5f );

	FILTER_VERTEX v[4] =
	{ //   X					Y							Z			W
		{fixup.x,			fixup.y,			1.f ,		1.f},
		{size.x + fixup.x,	fixup.y,			1.f ,		1.f },
		{size.x + fixup.x,	size.y + fixup.y,	1.f ,		1.f },
		{fixup.x,			size.y + fixup.y,	1.f ,		1.f }
	};

	// Set uvs + pixel shader constant	
	matCache.setTexture( "diffuseMap", pSrc );
	matCache.setFloat( "scalePower", scalePower_ );

	if (mat.begin())
	{
		for (uint p=0; p<mat.nPasses(); p++)
		{
			mat.beginPass(p);
	
			float xOff[] = 
			{
				-1.f,1.f,1.f,-1.f
			};

			float yOff[] = 
			{	1.f,1.f,-1.f,-1.f
			};

			float srcWidth = (float)srcWidth_;
			float srcHeight = (float)srcHeight_;

			for ( int i=0; i<4; i++ )
			{				
				v[0].tex[i].u = xOff[i];
				v[0].tex[i].v = yOff[i] + srcHeight;
				v[1].tex[i].u = xOff[i] + srcWidth;
				v[1].tex[i].v = yOff[i] + srcHeight;
				v[2].tex[i].u = xOff[i] + srcWidth;
				v[2].tex[i].v = yOff[i];
				v[3].tex[i].u = xOff[i];
				v[3].tex[i].v = yOff[i];

				//convert linear texture coordinates to standard.
				for ( int j=0; j<4; j++ )
				{
					v[j].tex[i].u /= sourceDimensions_.x;
					v[j].tex[i].v /= -sourceDimensions_.y;
					v[j].tex[i].v += 1.f;		
				}
			}

			Moo::rc().drawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, &v, sizeof(FILTER_VERTEX));
			mat.endPass();
		}
		mat.end();
	}
}


void Bloom::filterCopy(DX::BaseTexture* pSrc,
					   DWORD dwSamples,
					   FilterSample rSample[],
					   bool filterX )
{
	Moo::rc().setFVF( D3DFVF_XYZRHW | D3DFVF_TEX4 );

	struct FILTER_VERTEX { float x, y, z, w; struct uv { float u, v; } tex[4]; };
	//size determines the subset of the dest. render target to draw into,
	//and is basically width/ height in clip coordinates.
	Vector2 size( (float)(Moo::rc().screenWidth()),
					(float)(Moo::rc().screenHeight()) );

	//fixup is the geometric offset required for exact pixel-texel alignment
	Vector2 fixup( -0.5f, -0.5f );

	FILTER_VERTEX v[4] =
	{ //   X					Y				Z		W
		{fixup.x,			fixup.y,			1.f, 1.f },
		{size.x + fixup.x,	fixup.y,			1.f, 1.f },
		{size.x + fixup.x,	size.y + fixup.y,	1.f, 1.f },
		{fixup.x,			size.y + fixup.y,	1.f, 1.f }
	};


	float srcWidth = (float)srcWidth_;
	float srcHeight = (float)srcHeight_;
	Vector4 colourAttenuation = colourAttenuation_ * colourAttenuation_.w;
	colourAttenuation.w = 1.f;

	gaussianParameters_.setTexture( "diffuseMap", pSrc );

	if ( gaussianBlur_->begin() )
	{
		for ( uint p=0; p<gaussianBlur_->nPasses(); p++ )
		{
			gaussianBlur_->beginPass(p);			

			for( uint32 i = 0; i < dwSamples; i += 4 )
			{	
				Vector4 vWeights[4];
				for (uint32 iStage = 0; iStage < 4; iStage++)
				{
					// Set filter coefficients
					vWeights[iStage] = colourAttenuation;
					vWeights[iStage].scale( rSample[i+iStage].fCoefficient );

					if(filterX)
					{
						v[0].tex[iStage].u = rSample[i+iStage].fOffset * width_;
						v[0].tex[iStage].v = 0.f;
						v[1].tex[iStage].u = srcWidth + rSample[i+iStage].fOffset * width_;
						v[1].tex[iStage].v = 0.f;
						v[2].tex[iStage].u = srcWidth + rSample[i+iStage].fOffset * width_;
						v[2].tex[iStage].v = srcHeight;
						v[3].tex[iStage].u = rSample[i+iStage].fOffset * width_;
						v[3].tex[iStage].v = srcHeight;
					}
					else
					{
						v[0].tex[iStage].u = 0.f;
						v[0].tex[iStage].v = rSample[i+iStage].fOffset * width_;
						v[1].tex[iStage].u = srcWidth;
						v[1].tex[iStage].v = rSample[i+iStage].fOffset * width_;
						v[2].tex[iStage].u = srcWidth;
						v[2].tex[iStage].v = srcHeight + rSample[i+iStage].fOffset * width_;
						v[3].tex[iStage].u = 0.f;
						v[3].tex[iStage].v = srcHeight + rSample[i+iStage].fOffset * width_;
					}

					//convert linear texture coordinates to standard.
					for ( int i=0; i<4; i++ )
					{
						v[i].tex[iStage].u /= sourceDimensions_.x;
						v[i].tex[iStage].v /= -sourceDimensions_.y;
						v[i].tex[iStage].v += 1.f;
					}
				}
			
				//  only 1st pass is opaque
				gaussianParameters_.setBool( "AlphaBlendPass", i < 4 ? false : true );
				gaussianParameters_.setVectorArray( "FilterCoefficents", &vWeights[0], 4 );
				gaussianParameters_.commitChanges();

				// Render 1 pass of the filter
				Moo::rc().drawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, &v, sizeof(FILTER_VERTEX));
			}
			gaussianBlur_->endPass();
		}
		gaussianBlur_->end();
	}
}


/** This method registers a vector4 provider to provider further control
 *	over the blooming.
 *
 *	It is interpreted as (colour attenuation, colour scale power, width, ignored)
 *
 *	@param p the Vector4Provider to set
 */
void Bloom::bloomController( Vector4ProviderPtr p )
{
	if (Bloom::pInstance())	
		Bloom::pInstance()->controller_ = p;
}


/*~ function BigWorld.bloomController
 *	@components{ client }
 *
 *	This function registers a vector4 provider to provider further control
 *	over the blooming.
 *
 *	It is interpreted as (nPasses, power, width, cutoff)
 *	nPasses is rounded to the nearest int when used.
 *
 *	@param p the Vector4Provider to set
 */

PY_MODULE_STATIC_METHOD( Bloom, bloomController, BigWorld )



/** This method registers a vector4 provider to provider further control
 *	over the colour attenuation of the blooming.  As there are more bloom
 *	passes, differences in the colour attenuation values will become more
 *	pronounced. 
 *
 *	The colour attenuation is interpreted as (r,g,b,luminance)
 *	e.g. (1,1,1,0.9) would be a neutral colour, with 0.9 luminance.
 *	e.g. (1.01,1,1,0.95) would be slightly redder, and a little brighter.
 *
 *	@param p the Vector4Provider to set
 */
void Bloom::bloomColourAttenuation( Vector4ProviderPtr p )
{
	if (Bloom::pInstance())
		Bloom::pInstance()->colourAttenuationController_ = p;
}


/*~ function BigWorld.bloomColourAttenuation
 *	@components{ client }
 *
 *	This function registers a vector4 provider to provider further control
 *	over the blooming's colour attenuation.
 *
 *	It is interpreted as (colour attenuation, colour scale power, width, cutoff)
 *
 *	@param p the Vector4Provider to set
 */

PY_MODULE_STATIC_METHOD( Bloom, bloomColourAttenuation, BigWorld )