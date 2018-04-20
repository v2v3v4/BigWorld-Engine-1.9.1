/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


#ifdef CODE_INLINE
#define INLINE inline
#else
#define INLINE
#endif

namespace Moo
{

INLINE DX::BaseTexture*	RenderTarget::pTexture( )
{
	return pRenderTarget_.pComObject();
}

INLINE DX::Surface*	RenderTarget::depthBuffer( )
{
	return pDepthStencilTarget_.pComObject();
}

INLINE uint32 RenderTarget::width( ) const
{
	return width_;
}

INLINE uint32 RenderTarget::height( ) const
{
	return height_;
}

INLINE D3DFORMAT RenderTarget::format( ) const
{
	return pixelFormat_;
}

INLINE uint32 RenderTarget::textureMemoryUsed( )
{
	//we use A8R8G8B8 format, with one surface level
	//todo : factor in the depthStencil surface too.
	return (width_ * height_ * 4);
}

INLINE const std::string& RenderTarget::resourceID( ) const
{
	return resourceID_;
}

}

/*render_target.ipp*/