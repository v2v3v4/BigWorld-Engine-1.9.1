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
#include "back_buffer_fx.hpp"

#include "moo/render_context.hpp"
#include "moo/render_target.hpp"
#include "moo/material.hpp"
#include "moo/dynamic_vertex_buffer.hpp"
#include "moo/dynamic_index_buffer.hpp"
#include "resmgr/bwresource.hpp"
#include "cstdmf/debug.hpp"

#ifndef CODE_INLINE
#include "back_buffer_fx.ipp"
#endif


DECLARE_DEBUG_COMPONENT2( "Romp", 0 );



/**
 *	Constructor.
 */
BackBufferEffect::BackBufferEffect( int w, int h )
: pixelShader_( 0 ),
  renderTargetWidth_( w ),
  renderTargetHeight_( h ),
  transferMesh_( NULL ),
  vertexShader_( 0 ),
  bbc_( NULL ),
  rt0_( NULL ),
  inited_( false )
{
	material_ = new Moo::Material;
	material_->zBufferRead(false);
	material_->zBufferWrite(false);
	material_->doubleSided(true);
	material_->alphaBlended( true );
	material_->fogged( false );
	material_->destBlend( Moo::Material::ONE );
	material_->srcBlend( Moo::Material::SRC_ALPHA );
	material_->textureFactor( 0xffffffff );
	Moo::TextureStage ts;
	ts.textureWrapMode( Moo::TextureStage::CLAMP );
	ts.colourOperation( Moo::TextureStage::SELECTARG1 );
	ts.alphaOperation( Moo::TextureStage::SELECTARG1, Moo::TextureStage::TEXTURE_FACTOR, Moo::TextureStage::DIFFUSE );

	material_->addTextureStage( ts );
	Moo::TextureStage ts2;
	material_->addTextureStage( ts2 );
}


/**
 *	Destructor.
 */
BackBufferEffect::~BackBufferEffect()
{
	this->finz();
}


bool BackBufferEffect::init()
{
	if ( inited_ )
		return true;

	//create transfer mesh
	if ( !transferMesh_ )
		transferMesh_ = new DistortionMesh;

	//create render target
	if ( !rt0_ )
		rt0_ = new Moo::RenderTarget( "RT0" );

	MF_ASSERT( renderTargetWidth_ != 0 );
	MF_ASSERT( renderTargetHeight_ != 0 );

	rt0_->create( renderTargetWidth_, renderTargetHeight_, false );

	/* Set up viewport.
	 */
	vp_.X = 0;
	vp_.Y = 0;
	vp_.MinZ = 0;
	vp_.MaxZ = 1;
	vp_.Width = renderTargetWidth_;
	vp_.Height = renderTargetHeight_;

	//create shaders
	vertexShader_ = NULL;
	pixelShader_ = NULL;

	//create back buffer copy
	bbc_ = new RectBackBufferCopy;
	bbc_->init();

	this->finalInit();
	inited_ = true;
	return true;
}


void BackBufferEffect::finz()
{
	if ( !inited_ )
		return;

	if ( rt0_ )
	{
		rt0_->release();
		rt0_ = 0;
	}

	if (pixelShader_ && Moo::rc().device())
	{
		pixelShader_->Release();
		pixelShader_ = 0;
	}

	if (vertexShader_ && Moo::rc().device())
	{
		vertexShader_->Release();
		vertexShader_ = 0;
	}

	if (transferMesh_)
	{
		delete transferMesh_;
		transferMesh_ = NULL;
	}

	if ( bbc_ )
	{
		bbc_->finz();
		delete bbc_;
		bbc_ = NULL;
	}

	inited_ = false;
}


bool BackBufferEffect::loadVertexShader( const std::string& resourceStub, DX::VertexShader** result )
{
	BinaryPtr pVertexShader = BWResource::instance().rootSection()->readBinary( resourceStub + "_pc.vso" );

	if( pVertexShader )
	{
		// Try to create the shader.
		if (FAILED( Moo::rc().device()->CreateVertexShader( (const DWORD*)pVertexShader->data(), result ) ) )
		{
			ERROR_MSG( "BackBufferEffect::loadVertexShader - couldn't create vertexshader %s!\n", resourceStub.c_str() );
			return false;
		}
	}
	else
	{
		ERROR_MSG( "BackBufferEffect::loadVertexShader - couldn't open vertexshader %s!\n", resourceStub.c_str() );
		return false;
	}

	return true;
}


bool BackBufferEffect::loadPixelShader( const std::string& resourceStub, DX::PixelShader** result )
{
	BinaryPtr pPixelShader = BWResource::instance().rootSection()->readBinary( resourceStub + "_pc.pso" );

	if (pPixelShader)
	{
		if (FAILED( Moo::rc().device()->CreatePixelShader( (const DWORD*)pPixelShader->data(), result )))
		{
			WARNING_MSG( "BackBufferFX::loadPixelShader- couldn't create pixelshader %s!\n", resourceStub.c_str() );
			return false;
		}
	}
	else
	{
		WARNING_MSG( "BackBufferFX::loadPixelShader- couldn't open pixelshader %s!\n", resourceStub.c_str() );
		return false;
	}
	return true;
}


void BackBufferEffect::draw()
{
	if ( !inited_ )
	{
		if ( !init() )
			return;
	}

	this->grabBackBuffer();
	this->setRenderState();
	this->applyEffect();
	this->endDraw();
}


void BackBufferEffect::grabBackBuffer()
{
	// do this each frame since the back buffer will change
	// when flipping if we don't use antialiasing...
	bbc_->setupBackBufferHeader();

	//Note rt0_->push sets the width and height of the render context, but not the viewport.
	rt0_->push();
	Moo::rc().setViewport( &vp_ );

	//copy the appropriate back buffer area
	bbc_->draw( tl_, dimensions_ + tl_, Vector2(0.f,0.f), Vector2((float)renderTargetWidth_,(float)renderTargetHeight_));

	//and set the render target back to be the back buffer
	rt0_->pop();
}


void BackBufferEffect::setRenderState()
{
	//and set the rendering state
	material_->set();
	Moo::rc().setPixelShader( pixelShader_ );
	Moo::rc().setTexture( 0, rt0_->pTexture() );
	transferMesh_->setIndices();
	Moo::rc().setVertexShader( vertexShader_ );
	Moo::rc().setFVF( Moo::VertexXYZNUV::fvf() );
}


void BackBufferEffect::applyEffect()
{
	Vector2 offset;

	// offset mesh coordinates to ensure that texture sampling
	// occurs at the "correct" points
	offset.x = -0.5f;
	offset.y = 0.5f;
	transferMesh_->draw( tl_+offset, dimensions_, Vector2(1.f,1.f) );
}


void BackBufferEffect::endDraw()
{
	Moo::rc().setPixelShader( 0 );
}

// back_buffer_fx.cpp
