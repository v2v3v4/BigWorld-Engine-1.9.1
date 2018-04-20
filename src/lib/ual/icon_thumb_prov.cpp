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


#include "pch.hpp"
#include <string>
#include <vector>
#include "icon_thumb_prov.hpp"
#include "ual_manager.hpp"
#include "resmgr/bwresource.hpp"
#include "moo/render_context.hpp"
#include "common/string_utils.hpp"


int IconThumbProv_token;


// Icon File Provider
IMPLEMENT_THUMBNAIL_PROVIDER( IconThumbProv )

// reads the list of extensions with generic icons from the ual_config.xml file
void IconThumbProv::init()
{
	inited_ = false;

	DataSectionPtr ds = BWResource::instance().openSection( UalManager::instance().getConfigFile() );
	if ( !ds )
		return;
	
	ds = ds->openSection( "IconThumbnailProvider" );
	if ( !ds )
		return;

	std::vector<DataSectionPtr> sections;
	ds->openSections( "Thumbnail", sections );
	for( std::vector<DataSectionPtr>::iterator i = sections.begin();
		i != sections.end(); ++i )
	{
		std::string ext = (*i)->readString( "extension" );
		std::string match = (*i)->readString( "match" );
		std::string image = (*i)->readString( "image" );
		if ( ext.empty() || image.empty() )
			continue;
		// use windows-style slashes when matching directories
		std::replace( match.begin(), match.end(), '/', '\\' );
		iconData_.push_back( IconData( ext, match, image ) );
	}

	inited_ = true;
}

// returns the image file name according to the data in iconData_ that matches
// criteria with the filename, or an empty string if no element of iconData_
// matches.
std::string IconThumbProv::imageFile( const std::string& file )
{
	int dot = (int)file.find_last_of( '.' );
	std::string ext = file.substr( dot + 1 );
	StringUtils::toLowerCase( ext );

	for( std::vector<IconData>::iterator i = iconData_.begin();
		i != iconData_.end(); ++i )
	{
		if ( (*i).extension == ext &&
			( (*i).match.empty() ||
				PathMatchSpec( file.c_str(), (*i).match.c_str() ) ) )
		{
			return (*i).image;
		}
	}
	return "";
}

bool IconThumbProv::isValid( const ThumbnailManager& manager, const std::string& file )
{
	if ( !inited_ )
		init();

	if ( file.empty() )
		return false;

	imageFile_ = imageFile( file );

	return !imageFile_.empty();
}

bool IconThumbProv::needsCreate( const ThumbnailManager& manager, const std::string& file, std::string& thumb, int& size )
{
	if ( file.empty() || thumb.empty() )
		return false; // invalid input params, return false

	// set the thumb filename to the icon's filename, and return false so it
	// loads the thumb directly
	thumb = BWResource::getFilePath( UalManager::instance().getConfigFile() ) +
			imageFile_;
	return false;
}

bool IconThumbProv::prepare( const ThumbnailManager& manager, const std::string& file )
{
	// should never get called
	return false;
}

bool IconThumbProv::render( const ThumbnailManager& manager, const std::string& file, Moo::RenderTarget* rt )
{
	// should never get called
	return false;
}
