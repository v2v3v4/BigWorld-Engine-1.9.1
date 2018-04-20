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

#include "brush_thumb_provider.hpp"

#include "worldeditor/terrain/terrain_paint_brush.hpp"

#include "common/string_utils.hpp"


int BrushThumbProvider_token;


// Brush Provider implementation
IMPLEMENT_THUMBNAIL_PROVIDER( BrushThumbProvider )


/*
 *	This tests whether the given file is something that this provider
 *	is responsible for, in this case any file with the extension
 *	"brush".
 *
 *	See ThumbnailProvider::isValid for more details.
 */
/*virtual*/ bool BrushThumbProvider::isValid( 
	const ThumbnailManager &		manager, 
	const std::string &				file )
{
	if (file.empty())
	{
		return false;
	}

	std::string ext = BWResource::getExtension( file );
	StringUtils::toLowerCase( ext );
	return ext == "brush";
}


/*
 *	This prepares for rendering a brush thumbnail for the UAL.  Here we extract
 *	the texture used by the brush and use a ImageThumbProv to do the actual
 *	preparing.
 *
 *	See BrushThumbProvider::prepare and ImageThumbProv::prepare for more 
 *	details.
 */
/*virtual*/ bool BrushThumbProvider::prepare( 
	const ThumbnailManager &		manager, 
	const std::string &				file )
{
	std::string textureFile = getTextureFileForBrush( file );
	return imageProvider_.prepare( manager, textureFile );
}


/*
 *	This renders a brush thumbnail for the UAL.  Here we extract the texture 
 *	used by the brush and use a ImageThumbProv to do the actual preparing.
 *
 *	See BrushThumbProvider::prepare and ImageThumbProv::prepare for more 
 *	details.
 */
/*virtual*/ bool BrushThumbProvider::render( 
	const ThumbnailManager &		manager, 
	const std::string &				file, 
	Moo::RenderTarget *				rt )
{
	std::string textureFile = getTextureFileForBrush( file );
	return imageProvider_.render( manager, textureFile, rt );
}


/**
 *	This takes a brush file and returns the absolute location of the texture
 *	that it uses.
 *
 *	@param file			The brush file.
 *	@return				The texture that the brush uses.
 */
std::string BrushThumbProvider::getTextureFileForBrush( 
	const std::string &			file 
) const
{
	DataSectionPtr pBrushDataSection = BWResource::openSection( file );

	// If the file does not exist return the empty string:
	if (pBrushDataSection)
	{
		std::string texFile = TerrainPaintBrush::texture( pBrushDataSection );
		return BWResource::resolveFilename( texFile );
	}
	else
	{
		return std::string();
	}
		
}
