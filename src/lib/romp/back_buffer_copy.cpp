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
#include "back_buffer_copy.hpp"
#include "full_screen_back_buffer.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 0 );


// -----------------------------------------------------------------------------
// Section: class BackBufferCopy
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
BackBufferCopy::BackBufferCopy():
	pTexture_( NULL ),
	inited_( false )
{
}

void BackBufferCopy::finz()
{
	//pTexture is managed by BackBufferExposer.  we don't need to clean up.
	pTexture_ = 0;
}


bool BackBufferCopy::init()
{
	//Create the material
	material_.clearTextureStages();
	material_.zBufferRead(false);
	material_.zBufferWrite(false);
	material_.doubleSided(true);
	material_.alphaBlended( false );
	material_.fogged( false );
	material_.destBlend( Moo::Material::ZERO );
	material_.srcBlend( Moo::Material::ONE );
	material_.textureFactor( 0xffffffff );
	Moo::TextureStage ts;
	ts.textureWrapMode( Moo::TextureStage::CLAMP );
	ts.colourOperation( Moo::TextureStage::SELECTARG1 );
	ts.alphaOperation( Moo::TextureStage::SELECTARG1, Moo::TextureStage::TEXTURE_FACTOR, Moo::TextureStage::DIFFUSE );
	material_.addTextureStage( ts );
	Moo::TextureStage ts2;
	material_.addTextureStage( ts2 );

	inited_ = true;
	return true;
}

void BackBufferCopy::setupBackBufferHeader()
{
	multisample_ = 1.f;
	pTexture_ = FullScreenBackBuffer::renderTarget().pTexture();
}


// -----------------------------------------------------------------------------
// Section: class RectBackBufferCopy
// -----------------------------------------------------------------------------
RectBackBufferCopy::RectBackBufferCopy()
:screenCopyMesh_( D3DPT_TRIANGLESTRIP )
{
	//create the screen copy mesh
	Moo::VertexTUV v;
	v.pos_ = Vector4( 0.f, 0.f, 0.f, 1.f );
	screenCopyMesh_.resize(4);
	for (uint i = 0; i < 4; i++) screenCopyMesh_[i] = v;
}


/**
 *	This method draws a rectangle using the back buffer as a texture.
 */
void RectBackBufferCopy::draw( const Vector2& fromTL, const Vector2& fromBR, const Vector2& toTL, const Vector2& toBR, bool useEffect )
{
	MF_ASSERT( inited_ );

	setupBackBufferHeader();

	//mesh position is to ( where do we render to in the render target )
	*(Vector2*)(&screenCopyMesh_[0].pos_) = toTL;
	screenCopyMesh_[1].pos_.x = toBR.x;
	screenCopyMesh_[1].pos_.y = toTL.y;
	screenCopyMesh_[2].pos_.x = toTL.x;
	screenCopyMesh_[2].pos_.y = toBR.y;
	*(Vector2*)(&screenCopyMesh_[3].pos_) = toBR;

	//mesh UV is from ( where in the back buffer do we render from )
	screenCopyMesh_[0].uv_ = fromTL;
	screenCopyMesh_[1].uv_[0] = fromBR[0];
	screenCopyMesh_[1].uv_[1] = fromTL[1];
	screenCopyMesh_[2].uv_[0] = fromTL[0];
	screenCopyMesh_[2].uv_[1] = fromBR[1];
	screenCopyMesh_[3].uv_ = fromBR;
	float width = fabsf( fromTL.x - toBR.x );
	float height = fabsf( fromTL.y - toBR.y );

	for (int i=0; i<4; i++)
	{
		screenCopyMesh_[i].uv_.x /= width;	
		screenCopyMesh_[i].uv_.y /= height;
	}	

	//adjust for antialiasing
	for ( int i=0; i<4; i++ )
	{
		screenCopyMesh_[i].uv_[0] *= multisample_;
		// offset mesh coordinates to ensure that texture sampling
		// occurs at the "correct" points.
		//
		//vertices are transformed, screen units so always offset by 1/2 pixel.
		screenCopyMesh_[i].pos_.x -= 0.5f;
		screenCopyMesh_[i].pos_.y -= 0.5f;
	}

	if ( !useEffect )
	{
		material_.set();
		Moo::rc().setTexture( 0, pTexture_ );
		Moo::rc().setSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
		Moo::rc().setSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
		Moo::rc().setSamplerState( 0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP );
		Moo::rc().setSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
		Moo::rc().setSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
		Moo::rc().setSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE ); 
	}

	if (useEffect)
		screenCopyMesh_.drawEffect();
	else
		screenCopyMesh_.draw();
}