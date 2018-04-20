/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *	Image Thumbnail Provider
 */


#include "pch.hpp"
#include <string>
#include <vector>
#include "image_thumb_prov.hpp"
#include "moo/render_context.hpp"
#include "common/string_utils.hpp"


int ImageThumbProv_token;


// Image Provider
IMPLEMENT_THUMBNAIL_PROVIDER( ImageThumbProv )

bool ImageThumbProv::isValid( const ThumbnailManager& manager, const std::string& file )
{
	if ( file.empty() )
		return false;

	int dot = (int)file.find_last_of( '.' );
	std::string ext = file.substr( dot + 1 );
	StringUtils::toLowerCase( ext );

	return ext == "bmp"
		|| ext == "png"
		|| ext == "jpg"
		|| ext == "ppm"
		|| ext == "dds"
		|| ext == "tga"
		|| ext == "dib"
		|| ext == "hdr"
		|| ext == "pfm";
}

bool ImageThumbProv::prepare( const ThumbnailManager& manager, const std::string& file )
{
	if ( file.empty() || !PathFileExists( file.c_str() ) )
		return false;

	//load an image into a texture
	D3DXIMAGE_INFO imgInfo;
	if ( !SUCCEEDED( D3DXGetImageInfoFromFile( file.c_str(), &imgInfo ) ) )
		return false;

	int s = manager.size();

	int origW = imgInfo.Width;
	int origH = imgInfo.Height;
	manager.recalcSizeKeepAspect( s, s, origW, origH );
	origW = ( origW >> 2 ) << 2; // align to 4-pixel boundary
	origH = ( origH >> 2 ) << 2; // align to 4-pixel boundary

	size_.x = ( ( s - origW ) >> 3 ) << 2;
	size_.y = ( ( s - origH ) >> 3 ) << 2;

	return !!( pTexture_ = Moo::rc().createTextureFromFileEx(
			file.c_str(),
			origW, origH,
			1, 0, D3DFMT_A8R8G8B8,
			D3DPOOL_SYSTEMMEM,
			D3DX_FILTER_TRIANGLE,
			D3DX_DEFAULT,
			0, NULL, NULL ) );
}


bool ImageThumbProv::render( const ThumbnailManager& manager, const std::string& file, Moo::RenderTarget* rt )
{
	if ( !pTexture_ )
		return false;
	Moo::rc().device()->Clear( 0, NULL,
		D3DCLEAR_TARGET, RGB( 255, 255, 255 ), 1, 0 );

	DX::BaseTexture* pBaseTex = rt->pTexture();
	DX::Texture* pDstTexture;
	if( pBaseTex && pBaseTex->GetType() == D3DRTYPE_TEXTURE )
	{
		pDstTexture = (DX::Texture*)pBaseTex;
		pDstTexture->AddRef();
	}
	else
		return false;
	
	DX::Surface* pSrcSurface;
	if ( !SUCCEEDED( pTexture_->GetSurfaceLevel( 0, &pSrcSurface ) ) )
	{
		pDstTexture->Release();
		return false;
	}

	DX::Surface* pDstSurface;
	if ( !SUCCEEDED( pDstTexture->GetSurfaceLevel( 0, &pDstSurface ) ) )
	{
		pSrcSurface->Release();
		pDstTexture->Release();
		return false;
	}

	// divide dest point by two and align to 4-pixel boundary
	Moo::rc().device()->UpdateSurface(
		pSrcSurface, NULL,
		pDstSurface, &size_ );

	pDstSurface->Release();
	pSrcSurface->Release();

	pDstTexture->Release();

	pTexture_ = 0;
	return true;
}
