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
 *	XML File Thumbnail Provider (particles, lights, etc)
 */


#include "pch.hpp"
#include <string>
#include <vector>
#include "xml_thumb_prov.hpp"
#include "ual_manager.hpp"
#include "chunk/chunk_light.hpp"
#include "particle/meta_particle_system.hpp"
#include "particle/particle_system.hpp"
#include "resmgr/datasection.hpp"
#include "resmgr/bwresource.hpp"
#include "moo/render_context.hpp"
#include "common/string_utils.hpp"

int XmlThumbProv_token;


// XML File Provider
IMPLEMENT_THUMBNAIL_PROVIDER( XmlThumbProv )

// method used to find out if the xml file is a particle system
bool XmlThumbProv::isParticleSystem( const std::string& file )
{
	return MetaParticleSystem::isParticleSystem( file );
}

// method used to find out if the xml file is a light
bool XmlThumbProv::isLight( const std::string& file )
{
	DataSectionPtr ds = BWResource::openSection( file );
	if ( !ds )
		return false; // file is not a datasection or doesn't exist

	// hardcoding light's section names because of a lack of better way at 
	// the moment:
	if ( ds->openSection( "ambientLight" ) == NULL &&
		ds->openSection( "directionalLight" ) == NULL &&
		ds->openSection( "omniLight" ) == NULL &&
		ds->openSection( "spotLight" ) == NULL &&
		ds->openSection( "pulseLight" ) == NULL &&
		ds->openSection( "flare" ) == NULL )
		return false;

	return true;
}

std::string XmlThumbProv::particleImageFile()
{
	return BWResource::getFilePath( UalManager::instance().getConfigFile() ) +
		"icon_particles.bmp";
}

std::string XmlThumbProv::lightImageFile()
{
	return BWResource::getFilePath( UalManager::instance().getConfigFile() ) +
		"icon_light.bmp";
}

bool XmlThumbProv::isValid( const ThumbnailManager& manager, const std::string& file )
{
	if ( file.empty() )
		return false;

	int dot = (int)file.find_last_of( '.' );
	std::string ext = file.substr( dot + 1 );
	StringUtils::toLowerCase( ext );

	return ext == "xml";
}

bool XmlThumbProv::needsCreate( const ThumbnailManager& manager, const std::string& file, std::string& thumb, int& size )
{
	if ( file.empty() || thumb.empty() )
		return false; // invalid input params, return false

	// try to load the xml file as each of the known formats
	// if known, set the thumb filename to the icon's filename
	if ( isParticleSystem( file ) )
	{
		// it's a particle system
		thumb = particleImageFile();
	}
	else if ( isLight( file ) )
	{
		// it's a light system
		thumb = lightImageFile();
	}

	// return false to load the thumb directly
	return false;
}

bool XmlThumbProv::prepare( const ThumbnailManager& manager, const std::string& file )
{
	// should never get called
	return false;
}

bool XmlThumbProv::render( const ThumbnailManager& manager, const std::string& file, Moo::RenderTarget* rt )
{
	// should never get called
	return false;
}
