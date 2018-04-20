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
 *	Icon Thumbnail Provider (for files without preview, such as prefabs)
 */


#ifndef ICON_THUMB_PROV_HPP
#define ICON_THUMB_PROV_HPP

#include "thumbnail_manager.hpp"


// Icon File Provider
class IconThumbProv : public ThumbnailProvider
{
public:
	IconThumbProv() : inited_( false ) {};
	virtual bool isValid( const ThumbnailManager& manager, const std::string& file );
	virtual bool needsCreate( const ThumbnailManager& manager, const std::string& file, std::string& thumb, int& size );
	virtual bool prepare( const ThumbnailManager& manager, const std::string& file );
	virtual bool render( const ThumbnailManager& manager, const std::string& file, Moo::RenderTarget* rt  );

private:
	bool inited_;
	std::string imageFile_;
	struct IconData
	{
		IconData(
			const std::string& e, const std::string& m, const std::string& i
			) :
		extension( e ),
		match( m ),
		image( i )
		{};
		std::string extension;
		std::string match;
		std::string image;
	};
	std::vector<IconData> iconData_;

	DECLARE_THUMBNAIL_PROVIDER()

	void init();
	std::string imageFile( const std::string& file );
};

#endif // ICON_THUMB_PROV_HPP
