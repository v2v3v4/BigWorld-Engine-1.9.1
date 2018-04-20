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

#include "pch.hpp"
#include <string>

#include "moo/render_context.hpp"
#include "moo/effect_manager.hpp"
#include "moo/light_container.hpp"
#include "moo/visual_channels.hpp"
#include "moo/visual_manager.hpp"
#include "moo/visual.hpp"

#include "math/boundbox.hpp"
#include "math/matrix.hpp"

#include "model_thumb_prov.hpp"

#include "common/string_utils.hpp"

int ModelThumbProv_token;

DECLARE_DEBUG_COMPONENT( 0 )

// Image Provider
IMPLEMENT_THUMBNAIL_PROVIDER( ModelThumbProv )

ModelThumbProv::ModelThumbProv():
	lights_(NULL),
	visual_(NULL)
{}

ModelThumbProv::~ModelThumbProv()
{
	lights_ = NULL;
}

bool ModelThumbProv::isValid( const ThumbnailManager& manager, const std::string& file )
{
	if ( file.empty() )
		return false;

	std::string ext = file.substr( file.find_last_of( '.' ) + 1 );
	StringUtils::toLowerCase( ext );
	return ext == "model"
		|| ext == "visual";
}

bool ModelThumbProv::needsCreate( const ThumbnailManager& manager, const std::string& file, std::string& thumb, int& size )
{
	size = 128; // Models want 128 x 128 thumbnails
	
	std::string basename = BWResource::removeExtension( file );
	if ( BWResource::getExtension( basename ) == "static" )
	{
		// it's a visual with two extensions, so remove the remaining extension
		basename = BWResource::removeExtension( basename );
	}

	std::string thumbName = basename + ".thumbnail.jpg";

	thumb = thumbName;

	if ( PathFileExists( thumbName.c_str() ) )
	{
		return false;
	}
	else
	{
		thumbName = basename + ".thumbnail.bmp";

		if ( PathFileExists( thumbName.c_str() ) )
		{
			// change the thumbnail path to the legacy one
			thumb = thumbName;
			return false;
		}
	}
	return true;
}

bool ModelThumbProv::prepare( const ThumbnailManager& manager, const std::string& file )
{
	std::string visualName = file;
	std::string modelName = BWResource::dissolveFilename( file );
	bool errored = (errorModels_.find( modelName ) != errorModels_.end());

	if ( BWResource::getExtension( file ) != "visual" )
	{
		DataSectionPtr model = BWResource::openSection( modelName, false );

		if (model == NULL) 
		{
			if (errored) return false;
			ERROR_MSG( "ModelThumbProv::create: Could not open model file"
				" \"%s\"\n", modelName.c_str() );
			errorModels_.insert( modelName );
			return false;
		}

		visualName = model->readString("nodefullVisual","");

		if (visualName == "")
		{
			visualName = model->readString("nodelessVisual","");
			if (visualName == "")
			{
				visualName = model->readString("billboardVisual","");
				if (visualName == "")
				{
					if (errored) return false;
					ERROR_MSG( "ModelThumbProv::create: Could not determine type of model"
						" in file \"%s\"\n", modelName.c_str() );
					errorModels_.insert( modelName );
					return false;
				}
			}
		}
		visualName += ".visual";
	}

	visual_ = Moo::VisualManager::instance()->get( visualName );
	if (visual_ == NULL)
	{
		if (errored) return false;
		ERROR_MSG( "ModelThumbProv::create: Couldn't load visual \"%s\"\n", visualName.c_str() );
		errorModels_.insert( modelName );
		return false;
	}

	return true;
}

bool ModelThumbProv::render( const ThumbnailManager& manager, const std::string& file, Moo::RenderTarget* rt )
{
	if ( !visual_ )
		return false;

	Matrix rotation;
	
	if (lights_ == NULL)
	{
		lights_ = new Moo::LightContainer;

		lights_->ambientColour( Moo::Colour( 0.75f, 0.75f, 0.75f, 1.f ));

		Matrix dir (Matrix::identity);
		rotation.setRotateX( - MATH_PI / 4.f );
		dir.preMultiply( rotation );
		rotation.setRotateY( + MATH_PI / 4.f );
		dir.preMultiply( rotation );

		Moo::DirectionalLightPtr pDir = new Moo::DirectionalLight( Moo::Colour( 0.75f, 0.75f, 0.5f, 1.f ), dir[2] );
		pDir->worldTransform( Matrix::identity );
		lights_->addDirectional( pDir );

		dir = Matrix::identity;
		rotation.setRotateX( + MATH_PI / 8.f );
		dir.preMultiply( rotation );
		rotation.setRotateY( - MATH_PI / 4.f );
		dir.preMultiply( rotation );

		pDir = new Moo::DirectionalLight( Moo::Colour( 0.75f, 0.75f, 0.75f, 1.f ), dir[2] );
		pDir->worldTransform( Matrix::identity );
		lights_->addDirectional( pDir );
	}
	/* Flush any events queued by prepare so they are available to 
		 * render the thumbnails
		 */
	Moo::EffectManager::instance().finishEffectInits();
	//Make sure we set this before we try to draw
	Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

	Moo::rc().device()->Clear( 0, NULL,
		D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, RGB( 192, 192, 192 ), 1, 0 );

	// Set the projection matrix
	Moo::Camera cam = Moo::rc().camera();
	cam.aspectRatio( 1.f );
	Moo::rc().camera( cam );
	Moo::rc().updateProjectionMatrix();

	// Set a standard view
	Matrix view (Matrix::identity);
	Moo::rc().world( view );
	rotation.setRotateX( - MATH_PI / 8.f );
	view.preMultiply( rotation );
	rotation.setRotateY( + MATH_PI / 8.f );
	view.preMultiply( rotation );
	Moo::rc().view( view );

	// Zoom to the models bounding box
	zoomToExtents( visual_->boundingBox() );

	// Set up the lighting
	Moo::LightContainerPtr oldLights = Moo::rc().lightContainer();
	Moo::rc().lightContainer( lights_ );
	
	// Draw the model
	visual_->draw();

	// Draw any sorted channels
	Moo::SortedChannel::draw();

	Moo::rc().lightContainer( oldLights );

	//Make sure we restore this after we are done
	Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

	visual_ = NULL;

	return true;
}
