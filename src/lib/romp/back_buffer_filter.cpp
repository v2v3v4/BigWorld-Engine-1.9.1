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
#include "back_buffer_filter.hpp"

#include "moo/render_context.hpp"
#include "moo/render_target.hpp"
#include "moo/material.hpp"
#include "moo/dynamic_vertex_buffer.hpp"
#include "moo/dynamic_index_buffer.hpp"
#include "resmgr/bwresource.hpp"
#include "cstdmf/debug.hpp"

#ifndef CODE_INLINE
#include "back_buffer_filter.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "Romp", 0 );

BackBufferFilter* BackBufferFilter::instance_ = NULL;

inline float POW2( float x )
{
	return x*x;
}

// -----------------------------------------------------------------------------
// Section: Construction/Destruction
// -----------------------------------------------------------------------------


/**
 *	Constructor.
 */
BackBufferFilter::BackBufferFilter()
: currentRenderTarget_( 0 ),
  pixelShader_( NULL ),
  copyToScreenMesh_( D3DPT_TRIANGLESTRIP ),
  uSinOffset_( 0 ),
  uCosOffset_( 0 ),
  vSinOffset_( 0 ),
  vCosOffset_( 0 ),
  renderTargetWidth_( 0 ),
  renderTargetHeight_( 0 )
{
	rt_[0] = new Moo::RenderTarget( "RT0" );
	rt_[1] = new Moo::RenderTarget( "RT1" );

	material1_ = new Moo::Material;
	material1_->zBufferRead(false);
	material1_->zBufferWrite(false);
	material1_->doubleSided(true);
	material1_->alphaBlended( true );
	material1_->fogged( false );
	material1_->destBlend( Moo::Material::INV_SRC_ALPHA );
	material1_->srcBlend( Moo::Material::SRC_ALPHA );
	material1_->textureFactor( 0xd0000000 );
	Moo::TextureStage ts;
	ts.textureWrapMode( Moo::TextureStage::CLAMP );
	ts.colourOperation( Moo::TextureStage::SELECTARG1 );
	ts.alphaOperation( Moo::TextureStage::SELECTARG1, Moo::TextureStage::TEXTURE_FACTOR, Moo::TextureStage::DIFFUSE );

	material1_->addTextureStage( ts );
	Moo::TextureStage ts2;
	material1_->addTextureStage( ts2 );

	material2_ = new Moo::Material();
	*material2_ = *material1_;
	material2_->alphaBlended( false );
	material2_->destBlend( Moo::Material::ZERO );
	material2_->srcBlend( Moo::Material::ONE );

	Moo::VertexTUV v;
	v.pos_.z = 0;
	v.pos_.w = 1;

	copyToScreenMesh_.resize( 4 );
	for (uint i = 0; i < 4; i++) copyToScreenMesh_[i] = v;
}


/**
 *	Destructor.
 */
BackBufferFilter::~BackBufferFilter()
{
	delete rt_[0];
	delete rt_[1];
}

void BackBufferFilter::initInstance()
{
	instance_ = new BackBufferFilter;
}

void BackBufferFilter::deleteInstance()
{
	if (instance_)
	{
		delete instance_;
		instance_ = NULL;
	}
}

void BackBufferFilter::deleteUnmanagedObjects()
{
	rt_[0]->release();
	rt_[1]->release();
	if (pixelShader_ && Moo::rc().device())
	{
		pixelShader_->Release();
		pixelShader_ = NULL;
	}

	indexBuffer_ = NULL;
}

void BackBufferFilter::createUnmanagedObjects()
{
	/* Create render targets.
	 */

	float width = Moo::rc().screenWidth() - 1;
	float height = Moo::rc().screenHeight() - 1;
	renderTargetWidth_ = 4 << (*(int*)(&width) >> 23);
	renderTargetHeight_ = 4 << (*(int*)(&height) >> 23);

//	dprintf( "rt W %d, rt H %d\n", renderTargetWidth_, renderTargetHeight_ );
	rt_[0]->create( renderTargetWidth_, renderTargetHeight_ );
	rt_[1]->create( renderTargetWidth_, renderTargetHeight_ );

	/* Set up viewport.
	 */
	vp_.X = 0;
	vp_.Y = 0;
	vp_.MinZ = 0;
	vp_.MaxZ = 1;
	vp_.Width = DWORD((Moo::rc().screenWidth() < renderTargetWidth_) ?
			Moo::rc().screenWidth() : renderTargetWidth_);
	vp_.Height = DWORD((Moo::rc().screenHeight() < renderTargetHeight_) ?
			Moo::rc().screenHeight() : renderTargetHeight_);

	/* Set up copy to screen mesh.
	 */

	float u = float(vp_.Width) / float(renderTargetWidth_);
	float v = float(vp_.Height) / float(renderTargetHeight_);

	copyToScreenMesh_[0].pos_.x = 0;						copyToScreenMesh_[0].pos_.y = 0;
	copyToScreenMesh_[1].pos_.x = Moo::rc().screenWidth();	copyToScreenMesh_[1].pos_.y = 0;
	copyToScreenMesh_[2].pos_.x = 0;						copyToScreenMesh_[2].pos_.y = Moo::rc().screenHeight();
	copyToScreenMesh_[3].pos_.x = Moo::rc().screenWidth();	copyToScreenMesh_[3].pos_.y = Moo::rc().screenHeight();
	copyToScreenMesh_[0].uv_.set( 0, 0 );
	copyToScreenMesh_[1].uv_.set( u, 0 );
	copyToScreenMesh_[2].uv_.set( 0, v );
	copyToScreenMesh_[3].uv_.set( u, v );

	/* Create the pixel shader.
	 */
	BinaryPtr pPixelShader = BWResource::instance().rootSection()->readBinary( 
		"shaders/pixelshaders/makegrayscale.pso" );
	if (pPixelShader)
	{
		if (FAILED( Moo::rc().device()->CreatePixelShader( (const DWORD*)pPixelShader->data(),
									&pixelShader_ )))
		{
			CRITICAL_MSG( "BackBufferFilter::createUnmanagedObjects - couldn't create pixelshader!\n" );
		}
	}
	else
	{
		CRITICAL_MSG( "BackBufferFilter::createUnmanagedObjects - couldn't open pixelshader!\n" );
	}

	/* Create the index buffer.
	 */

	DX::IndexBuffer* ib;
	Moo::rc().device()->CreateIndexBuffer( 66 * 48 * 2 * 2, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
		D3DPOOL_DEFAULT, &ib, NULL );
	uint16* pIndices = NULL;
	if (SUCCEEDED(ib->Lock( 0, 66 * 48 * 2 * 2, (void**)&pIndices, 0 )))
	{

		uint16 lastIndex = 0;
		for (int y = 0; y < 48; y++)
		{
			uint16 rowIndex = y * 65;
			*(pIndices++) = lastIndex;
			*(pIndices++) = rowIndex + 65;
			for (int x = 0; x < 65; x++)
			{
				lastIndex = x + rowIndex;
				*(pIndices++) = lastIndex + 65;
				*(pIndices++) = lastIndex;
			}
		}

		ib->Unlock();
		indexBuffer_ = ib;
		ib->Release();
		ib = NULL;
	}
	else
	{
		CRITICAL_MSG( "BackBufferFilter::CreateUnmanagedObjects: Unable to lock index buffer" );
	}

}

void BackBufferFilter::beginScene()
{
	DX::Viewport fullRTVP = vp_;
	vp_.Width = renderTargetWidth_;
	vp_.Height = renderTargetHeight_;

	//Note rt_->push sets the width and height of the render context, but not the viewport.
	rt_[currentRenderTarget_]->push();
	Moo::rc().device()->SetViewport( &vp_ );
	Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, 0xff000010, 1, 0 );

	//Now set the viewport to be screen sized.
	Moo::rc().screenWidth( vp_.Width );
	Moo::rc().screenHeight( vp_.Height );
	Moo::rc().device()->SetViewport( &vp_ );

}

void BackBufferFilter::endScene()
{
	float uso = uSinOffset_;
	float uco = uCosOffset_;
	float vso = vSinOffset_;
	float vco = vCosOffset_;

	static std::vector< Vector3 > horisontal(65);
	static std::vector< Vector3 > vertical(49);

	float u = float(vp_.Width) / float(renderTargetWidth_);
	float v = float(vp_.Height) / float(renderTargetHeight_);
	float toff = u / 64.f;

	std::vector<Vector3>::iterator it = horisontal.begin();
	std::vector<Vector3>::iterator end = horisontal.end();

	float us = (u - toff - toff) / 64.f;
	float uval = toff;

	float ps = 1 / 32.f;
	float pv = -ps * 32;

	while (it != end)
	{
		float ht = (toff / 2.f);
		(it++)->set( uval, (cosf(uco * 0.45f) + sinf( uso * 0.25f ))  * ht, POW2( pv ) );
		pv += ps;
		uval += us;
		uso += 0.8f;
		uco += 1.0f;
	}

	it = vertical.begin();
	end = vertical.end();

	float vs = (v - toff - toff) / 48.f;
	float vval = toff;

	pv = -24 * ps;
	while (it != end)
	{
		float ht = (toff / 2.f);
		(it++)->set( ( sinf( vso * 0.5f ) + cosf( vco * 0.3f ) )  * ht, vval /*+ ( cos(vco) * ht )*/, POW2( pv ) );
		pv += ps;
		vval += vs;
		vso += 1.20f;
		vco += 0.80f;
	}

	Moo::DynamicVertexBuffer< Moo::VertexTUV >& vb = Moo::DynamicVertexBuffer< Moo::VertexTUV >::instance();
	Moo::VertexTUV* pVerts = vb.lock( 65*49 );

	float yv = 0;
	float ys = float( vp_.Height ) / 48.f;
	for (int y = 0; y < 49; y++)
	{
		Vector3 uv = vertical[y];
		float xv = 0;
		float xs = float( vp_.Width ) / 64.f;
		for (int x = 0; x < 65; x++)
		{
			Vector3& uv2 = horisontal[x];
			float weight = min( uv.z + uv2.z,  1.f );
			pVerts->pos_.set( xv, yv, 0, 1 );
			pVerts->uv_.set( uv.x * weight + uv2.x, uv.y + uv2.y * weight );
			pVerts++;
			xv += xs;
		}
		yv += ys;
	}
	vb.unlock();


	material1_->set();
	Moo::rc().setTexture( 0, rt_[(currentRenderTarget_ + 1) & 1]->pTexture() );
	Moo::rc().device()->SetStreamSource( 0, vb.pVertexBuffer(), 0, sizeof( Moo::VertexTUV ) );
	Moo::rc().device()->SetIndices( indexBuffer_.pComObject() );
	Moo::rc().setVertexShader( NULL );
	Moo::rc().setFVF( Moo::VertexTUV::fvf() );
	Moo::rc().drawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, 65 * 49, 0, (66 * 2 * 48) - 2 );

	uSinOffset_ -= 0.1f;
	uCosOffset_ += 0.2f;
	vSinOffset_ -= 0.05f;
	vCosOffset_ += 0.15f;

	rt_[ currentRenderTarget_ ]->pop();
	material2_->set();
	Moo::rc().device()->SetPixelShader( pixelShader_ );
	Moo::rc().setTexture( 0, rt_[currentRenderTarget_]->pTexture() );
	copyToScreenMesh_.draw();
	Moo::rc().device()->SetPixelShader( 0 );
	currentRenderTarget_ = (currentRenderTarget_ + 1 ) & 1;
}

// back_buffer_filter.cpp
