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
 *	ThumbnailManager: Thumbnail generator class
 *
 *	Brief explanation of the way it works:
 *	- a thumbnail is requested by the app by calling create
 *	- create looks for the thumb of the file in the thread results
 *		(returns result if found)
 *	- if not found, it adds the file to the pending list
 *	- in the thread, if there are pending requests, it starts processing the
 *		most recent request
 *	- if the thread finds a thumb for item, it pushes it to the results (that
 *		is, if needsCreate returns false).
 *	- if needsCreate returns true, the thread will tell the provider to prepare
 *	- after the prov is prepared in the thread, the thread requests a render in
 *		the main thread, and waits until it's finished (calling renderDone).
 *	- in the tick in the main thread, if renderRequested_ is true the prepared
 *		asset is rendered and renderRequested_ set to false (effectivelly
 *		telling the thread the render is ready)
 *	- the thread waits for renderRequested_ to be false, and when it happens,
 *		it saves the render target as a texture to the thumb file and loads it
 *		in the ThreadResult object currentResult_.
 *	- in the tick in the main thread, so the results_ should now contain the
 *		item's thumb image. It stores the results in the ready_ list and calls
 *		updateItem, which forces a redraw of the item and in turn calls create
 *		again (but now create finds the item's thumb in ready_ and return it).
 */

#include "pch.hpp"

#include <string>
#include <vector>

#include "common/string_utils.hpp"

#include "moo/render_context.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/multi_file_system.hpp"

#include "math/boundbox.hpp"

#include "thumbnail_manager.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

// Default providers tokens to ensure that they get compiled
extern int ImageThumbProv_token;
extern int ModelThumbProv_token;
extern int XmlThumbProv_token;
extern int IconThumbProv_token;
static int s_chunkTokenSet = 0
	| ImageThumbProv_token
	| ModelThumbProv_token
	| XmlThumbProv_token
	| IconThumbProv_token
	;


// ThumbnailProvider

bool ThumbnailProvider::needsCreate( const ThumbnailManager& manager, const std::string& file, std::string& thumb, int& size )
{
	if ( file.empty() || thumb.empty() )
		return false; // invalid input params, return false

	if ( !BWResource::fileExists( thumb ) )
		return true; // no thumbnail created yet, so create it

	return BWResource::isFileOlder( thumb, file, 60 ); // checks if it's 1 minute older or more
}

void ThumbnailProvider::zoomToExtents( const BoundingBox& bb, const float scale /* = 1.f */ )
{
	Vector3 bounds = bb.maxBounds() - bb.minBounds();

	float radius = bounds.length() / 2.f;
	
	if (radius < 0.01f) 
	{
		return;
	}

	float dist = radius / tanf( Moo::rc().camera().fov() / 2.f );

	// special case to avoid near 
	// plane clipping of small objects
	if (Moo::rc().camera().nearPlane()  > dist - radius)
	{
		dist = Moo::rc().camera().nearPlane() + radius;
	}
	
	Matrix view = Moo::rc().view();
	view.invert();
	Vector3 forward = view.applyToUnitAxisVector( 2 );
	view.invert();

	Vector3 centre(
		(bb.minBounds().x + bb.maxBounds().x) / 2.f,
		(bb.minBounds().y + bb.maxBounds().y) / 2.f,
		(bb.minBounds().z + bb.maxBounds().z) / 2.f );

	Vector3 pos = centre - scale * dist * forward;
	view.lookAt( pos, forward, Vector3( 0.f, 1.f, 0.f ) );
	Moo::rc().view( view );
}


// ThumbnailManager

static const int THUMBPROV_MAX_RESULTS = 400;

/*static*/ std::vector<ThumbnailProviderPtr> * ThumbnailManager::s_providers_ = NULL;
static bool s_providersFinalised = false;


ThumbnailManager::ThumbnailManager() :
	postfix_( ".thumbnail.jpg" ),
	folder_( ".bwthumbs" ),
	size_( 64 ),
	backColour_( RGB( 255, 255, 255 ) ),
	thread_( 0 ),
	renderRT_( "ThumbnailManager" ),
	renderSize_( 64 ),
	stopThreadRequested_( false )
{
	startThread();
}

ThumbnailManager::~ThumbnailManager()
{
	renderRT_.release();
	stop();
	delete s_providers_;
	s_providers_ = NULL;
	s_providersFinalised = true;
}

/*static*/ void ThumbnailManager::registerProvider( ThumbnailProviderPtr provider )
{
	if (provider)
	{
		MF_ASSERT( !s_providersFinalised );
		if (s_providers_ == NULL)
		{
			s_providers_ = new std::vector< ThumbnailProviderPtr >();
		}
		s_providers_->push_back( provider );
	}
}

void ThumbnailManager::recalcSizeKeepAspect( int w, int h, int& origW, int& origH ) const
{
	float k = 1;
	if ( origW > origH && origW > 0 )
		k = (float)w / origW;
	else if ( origH > 0 )
		k = (float)h / origH;
	origW = (int)( origW * k );
	origH = (int)( origH * k );
}

void ThumbnailManager::stretchImage( CImage& img, int w, int h, bool highQuality ) const
{
	if ( img.IsNull() )
		return;
	int origW = img.GetWidth();
	int origH = img.GetHeight();
	
	recalcSizeKeepAspect( w, h, origW, origH );

	CImage image;
	image.Create( w, h, 32 );

	CDC* pDC = CDC::FromHandle( image.GetDC() );
	pDC->FillSolidRect( 0, 0, w, h, backColour_ );
	if ( highQuality )
		pDC->SetStretchBltMode( HALFTONE );
	img.StretchBlt( pDC->m_hDC, ( w - origW ) / 2, ( h - origH ) / 2, origW, origH );
	image.ReleaseDC();

	img.Destroy();
	img.Create( w, h, 32 );

	pDC = CDC::FromHandle( img.GetDC() );
	image.BitBlt( pDC->m_hDC, 0, 0 );
	img.ReleaseDC();
}

void ThumbnailManager::create( const std::string& file, CImage& img, int w, int h,
							  ThumbnailUpdater* updater, bool loadDirectly )
{
	if ( !loadDirectly )
	{
		std::string fname = file;
		std::replace( fname.begin(), fname.end(), '/', '\\' );
		// check if the image is already in the thread results list
		for( std::list<ThreadResultPtr>::iterator i = ready_.begin();
			i != ready_.end(); ++i )
		{
			if ( (*i)->updater_ == updater && (*i)->image_ &&
				(*i)->image_->GetWidth() == w && (*i)->image_->GetHeight() == h &&
				(*i)->file_ == fname )
			{
				// blit the image to the result image and erase from the list
				img.Create( w, h, 32 );
				CDC* pDC = CDC::FromHandle( img.GetDC() );
				(*i)->image_->BitBlt( pDC->m_hDC, 0, 0 );
				img.ReleaseDC();
				ready_.erase( i );
				return;
			}
		}

		// check if the file produced errors before, and if so, ignore it
		if ( errorFiles_.find( fname ) != errorFiles_.end() )
			return;

		// now check if it's in the thread results not yet copied to ready
		{
			SimpleMutexHolder smh( resultsMutex_ );
			for( std::list<ThreadResultPtr>::iterator i = results_.begin();
				i != results_.end(); ++i )
			{
				if ( (*i)->updater_ == updater && (*i)->image_ &&
					(*i)->image_->GetWidth() == w && (*i)->image_->GetHeight() == h &&
					(*i)->file_ == fname )
				{
					// simply return with no image, to avoid adding another
					// pending request
					return;
				}
			}
		}

		std::string path;
		std::string thumb;

		int slash = (int)fname.find_last_of( '\\' );
		if ( slash > 0 )
		{
			path = fname.substr( 0, slash ) + "\\" + folder_;
			thumb = path + fname.substr( slash );
		}
		else
		{
			path = folder_;
			thumb = path + '\\' + fname;
		}
		thumb += postfix_;

		// look for pending requests for a thumb of the same name, size and
		// updater
		pendingMutex_.grab();
		for( std::list<ThreadDataPtr>::iterator i = pending_.begin();
			i != pending_.end(); ++i )
		{
			if ( (*i)->w_ == w && (*i)->h_ == h &&
				(*i)->updater_ == updater && (*i)->file_ == fname )
			{
				// there's a request already, so remove it
				pending_.erase( i );
				break;
			}
		}
		pending_.push_back( new ThreadData( fname, thumb, path, w, h, updater ) );
		pendingMutex_.give();
	}
	else if ( loadDirectly )
	{
		// load the specified file directly, and resize if needed
		img.Load( file.c_str() );

		if ( !img.IsNull() )
		{
			// Is rescale needed? this if will evaluate to true when the
			// caller's requested size is different than the size of the
			// generated thumb.
			if ( img.GetWidth() != w || img.GetHeight() != h )
				stretchImage( img, w, h, true );
		}
	}
}

void ThumbnailManager::tick()
{
	if ( renderRequested() )
	{
		// The thumb-generating thread has requested a rendering in the main
		// thread, so render.
		render();
	}
	else if ( resultsAvailable() )
	{
		// Thumbs were processed in the thread, so check the result.

		// Block the main thread for a maximum of 1/50th of a second
		clock_t maxTime = clock() + CLOCKS_PER_SEC / 50;

		while ( true )
		{
			if ( clock() >= maxTime )
				break;

			resultsMutex_.grab();
			// variable to test if the results queue got empty between this point and
			// the previous call to resultsAvailable() or since the last iteration
			bool resultsEmpty = results_.empty();
			ThreadResultPtr result;
			if ( !resultsEmpty )
			{
				result = results_.back();
				results_.pop_back();
			}
			resultsMutex_.give();
			if ( resultsEmpty || result == NULL )
				break; // no more results, return.

			if ( result->image_ && !result->image_->IsNull() )
			{
				if ( result->updater_ )
				{
					// There's a result and the data has an updater object, so
					// tell the updater the thumb for that file is ready, which
					// should eventually call 'create' for this file.
					if ( ready_.size() >= THUMBPROV_MAX_RESULTS )
					{
						// discard old results, keeping the list lean and fast
						ready_.pop_front();
					}
					ready_.push_back( result );
					result->updater_->thumbManagerUpdate( result->file_ );
				}
			}
			else
			{
				// something ocurred while generating the thumb, tag as error so
				// it doesn't try to generate it again each time the item is
				// redrawn
				errorFiles_.insert( result->file_ );
			}
		}
	}
}

// thread methods
bool ThumbnailManager::pendingAvailable()
{
	pendingMutex_.grab();
	bool res = !pending_.empty();
	pendingMutex_.give();
	return res;
}

bool ThumbnailManager::resultsAvailable()
{
	resultsMutex_.grab();
	bool res = !results_.empty();
	resultsMutex_.give();
	return res;
}

void ThumbnailManager::requestRender( int size )
{
	mutex_.grab();
	renderRequested_ = true;
	renderSize_ = size;
	mutex_.give();
}

bool ThumbnailManager::renderRequested()
{
	mutex_.grab();
	bool res = renderRequested_;
	mutex_.give();
	return res;
}

bool ThumbnailManager::renderDone()
{
	mutex_.grab();
	bool res = !renderRequested_;
	mutex_.give();
	return res;
}

void ThumbnailManager::render()
{
	bool res = false;

	if ( renderData_ && renderData_->provider_ &&
		Moo::rc().checkDevice() )
	{
		if ( renderRT_.pTexture() != NULL &&
				( renderRT_.width() != renderSize_ ||
				renderRT_.height() != renderSize_ ) )
		{
			renderRT_.release();
		}
		if ( renderRT_.pTexture() == NULL )
		{
			renderRT_.create( renderSize_, renderSize_ );
		}
		if ( renderRT_.pTexture() != NULL && renderRT_.push() )
		{
			Moo::LightContainerPtr pOldLights = Moo::rc().lightContainer();
			if (!pNewLights_)
			{
				Moo::DirectionalLightPtr pDirLight = 
					new Moo::DirectionalLight( Moo::Colour( 0.5f, 0.5f, 0.5f, 1.f ), Vector3( 0, 0, -1.f ) );
				pNewLights_ = new Moo::LightContainer();
				pNewLights_->ambientColour( Moo::Colour( Vector4( 0.75, 0.75, 0.75, 1.f ) ) );
				pNewLights_->addDirectional( pDirLight );
			}
			Moo::rc().lightContainer( pNewLights_ );

			Moo::rc().beginScene();
			Moo::rc().setVertexShader( NULL );
			Moo::rc().setPixelShader( NULL );
			Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA
				| D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );

			res = renderData_->provider_->render( *this, renderData_->file_, &renderRT_ );
			
			Moo::rc().endScene();

			Moo::rc().lightContainer( pOldLights );
			
			renderRT_.pop();
		}
		if ( res && renderRT_.pTexture() )
		{
			// render ok, so save the render target to a thumb file
			if ( !PathIsDirectory( renderData_->path_.c_str() ) &&
				renderData_->thumb_.find( folder_ ) != std::string::npos )
			{
				// create folder if it doesn't exist and if the
				// thumbs file name requires it
				CreateDirectory( renderData_->path_.c_str(), 0 );
				DWORD att = GetFileAttributes( renderData_->path_.c_str() );
				SetFileAttributes( renderData_->path_.c_str(), att | FILE_ATTRIBUTE_HIDDEN );
			}
			D3DXIMAGE_FILEFORMAT format = D3DXIFF_JPG;
			std::string ext = renderData_->thumb_.substr(
				renderData_->thumb_.find_last_of( '.' ) + 1 );
			StringUtils::toLowerCase( ext );
			// match the extension to the appropriate DX format
			if ( ext == "bmp" )
				format = D3DXIFF_BMP;
			else if ( ext == "jpg" )
				format = D3DXIFF_JPG;
			else if ( ext == "png" )
				format = D3DXIFF_PNG;
			else
				ASSERT( 0 ); // format not supported by CImage::Load
			// and save
			D3DXSaveTextureToFileInMemory(
				&(renderData_->memFile_),
				format,
				renderRT_.pTexture(), NULL );
		}
	}
	// Done rendering to the thread's render target. Now set the flags so
	// the thread nows the render is ready
	mutex_.grab();
	renderRequested_ = false;
	mutex_.give();
}

/*static*/ void ThumbnailManager::s_startThread( void *extraData )
{
	ThumbnailManager* manager = (ThumbnailManager*)(extraData);
	MF_ASSERT( manager != NULL );

	while ( true )
	{
		while( !manager->stopThreadRequested() && !manager->pendingAvailable() )
			Sleep( 100 );

		if ( manager->stopThreadRequested() )
			return;

		manager->pendingMutex_.grab();
		// variable to test if the pending queue got empty between this point and
		// the previous call to pendingAvailable()
		bool pendingEmpty = manager->pending_.empty();
		ThreadDataPtr data;
		if ( !pendingEmpty )
		{
			data = manager->pending_.back();
			manager->pending_.pop_back();
		}
		manager->pendingMutex_.give();
		if ( pendingEmpty || data == NULL )
			continue;

		class DebugDialogDisable
		{
		public:
			DebugDialogDisable() { DebugMsgHelper::showErrorDialogs( false ); }
			~DebugDialogDisable() { DebugMsgHelper::showErrorDialogs( true ); }
		};
		DebugDialogDisable dbgDlgDisable;
			
		ThumbnailProviderPtr prov = NULL;
		for( std::vector<ThumbnailProviderPtr>::iterator i = manager->s_providers_->begin();
			i != manager->s_providers_->end(); ++i )
		{
			if ( (*i)->isValid( *manager, data->file_ ) )
			{
				prov = *i;
				break;
			}
		}
		int size = manager->size_;
		if ( prov && prov->needsCreate( *manager, data->file_, data->thumb_, size ) )
		{
			// known type, so do it!
			if ( prov->prepare( *manager, data->file_ ) )
			{
				if ( manager->stopThreadRequested() )
					return;

				// the asset was loaded/prepared, so attempt to render it
				// request render in the main thread, and wait for it
				data->provider_ = prov;
				manager->renderData_ = data;	// needed by the main thread to render
				manager->requestRender( size );
				while( !manager->renderDone() &&
					!manager->stopThreadRequested() )
				{
					Sleep( 0 );
				}

				if ( manager->stopThreadRequested() )
				{
					return;
				}

				if ( data->memFile_ != NULL )
				{
					BinaryPtr bin = new BinaryBlock(
						data->memFile_->GetBufferPointer(),
						data->memFile_->GetBufferSize(),
						"BinaryBlock/ThumbnailManager" );

					BWResource::instance().fileSystem()->writeFile(
						data->thumb_.c_str(),
						bin, true );

					data->memFile_->Release();
				}

				manager->renderData_ = NULL;	// no longer needed
			}
		}
		ThreadResultPtr result = new ThreadResult( data->file_, new CImage(), data->updater_ );
		result->image_->Load( data->thumb_.c_str() );

		if ( !result->image_->IsNull() )
		{
			// Is rescale needed? this one should only get called if the caller
			// requests a size different than "size_"
			if ( result->image_->GetWidth() != data->w_ ||
				result->image_->GetHeight() != data->h_ )
			{
				manager->stretchImage( *result->image_, data->w_, data->h_, true );
			}
		}
		manager->resultsMutex_.grab();
		if ( manager->results_.size() >= THUMBPROV_MAX_RESULTS )
		{
			// discard old results, keeping the list lean and fast.
			manager->results_.pop_front();
		}
		manager->results_.push_back( result );
		manager->resultsMutex_.give();
	}
}

void ThumbnailManager::startThread()
{
	stopThread();
	thread_ = new SimpleThread( s_startThread, this );
}

void ThumbnailManager::stopThread()
{
	if ( !thread_ )
		return;

	stopThreadRequest( true );

	delete thread_;
	thread_ = NULL;

	stopThreadRequest( false );
}

void ThumbnailManager::stopThreadRequest( bool set )
{
	mutex_.grab();
	stopThreadRequested_ = set;
	mutex_.give();
}

bool ThumbnailManager::stopThreadRequested()
{
	mutex_.grab();
	bool res = stopThreadRequested_;
	mutex_.give();
	return res;
}


void ThumbnailManager::resetPendingRequests( ThumbnailUpdater* updater )
{
	// remove only requests and results by the updater
	pendingMutex_.grab();
	for( std::list<ThreadDataPtr>::iterator i = pending_.begin();
		i != pending_.end(); )
	{
		if ( (*i)->updater_ == updater )
			i = pending_.erase( i );
		else
			++i;
	}
	pendingMutex_.give();

	resultsMutex_.grab();
	for( std::list<ThreadResultPtr>::iterator i = results_.begin();
		i != results_.end(); )
	{
		if ( (*i)->updater_ == updater )
			i = results_.erase( i );
		else
			++i;
	}
	resultsMutex_.give();
	ready_.clear();
	errorFiles_.clear();
}

void ThumbnailManager::stop()
{
	stopThread();
	pending_.clear();
	results_.clear();
	ready_.clear();
	errorFiles_.clear();
}
