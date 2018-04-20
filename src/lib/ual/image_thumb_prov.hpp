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


#ifndef IMAGE_THUMB_PROV_HPP
#define IMAGE_THUMB_PROV_HPP

#include "thumbnail_manager.hpp"


// Image Provider
class ImageThumbProv : public ThumbnailProvider
{
public:
	ImageThumbProv() : pTexture_( 0 ) {};
	virtual bool isValid( const ThumbnailManager& manager, const std::string& file );
	virtual bool prepare( const ThumbnailManager& manager, const std::string& file );
	virtual bool render( const ThumbnailManager& manager, const std::string& file, Moo::RenderTarget* rt  );

private:
	ComObjectWrap<DX::Texture> pTexture_;
	POINT size_;

	DECLARE_THUMBNAIL_PROVIDER()
};

#endif // IMAGE_THUMB_PROV_HPP
