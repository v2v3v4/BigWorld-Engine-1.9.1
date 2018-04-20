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
 *	Model Thumbnail Provider
 */


#ifndef MODEL_THUMB_PROV_HPP
#define MODEL_THUMB_PROV_HPP

#include <set>

#include "thumbnail_manager.hpp"


// Model Provider
class ModelThumbProv : public ThumbnailProvider
{
public:
	ModelThumbProv();
	~ModelThumbProv();
	virtual bool isValid( const ThumbnailManager& manager, const std::string& file );
	virtual bool needsCreate( const ThumbnailManager& manager, const std::string& file, std::string& thumb, int& size );
	virtual bool prepare( const ThumbnailManager& manager, const std::string& file );
	virtual bool render( const ThumbnailManager& manager, const std::string& file, Moo::RenderTarget* rt );

private:
	DECLARE_THUMBNAIL_PROVIDER()

	Moo::LightContainerPtr lights_;
	Moo::VisualPtr visual_;
	std::set< std::string > errorModels_;
};

#endif // MODEL_THUMB_PROV_HPP
