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
#include "web_page.hpp"
#include "cstdmf/bgtask_manager.hpp"
#include "ashes/simple_gui.hpp"
#include "moo/render_context.hpp"

DECLARE_DEBUG_COMPONENT2( "romp", 0 )

namespace
{
	/*
	 *	This background task helps us manage the browser updates
	 */
	class WebPageBGTask : public BackgroundTask
	{
	public:
		WebPageBGTask( WebPage* webPage ) :
			webPage_( webPage )
		{
		}
		void doBackgroundTask( BgTaskManager & mgr )
		{
			BW_GUARD;
		}
		void doMainThreadTask( BgTaskManager & mgr ) 
		{
			BW_GUARD;
			static DogWatch dwBrowser("Browser");
			ScopedDogWatch sdw( dwBrowser );
			webPage_->updateBrowser();
			webPage_->updateTexture();
			webPage_ = NULL;
		}
		bool finished() const { return !webPage_.hasObject(); }
	private:
		SmartPointer<WebPage> webPage_;
	};

	static SmartPointer<WebPageBGTask> s_webPageTask;
}

/**
 *	Constructor for the WebPage object
 *	@param width the width of the web page texture
 *	@param height the height of the web page texture
 *	@param url the url to use for the web page texture
 */
WebPage::WebPage(uint32 width, uint32 height, const std::string& url)
{
	BW_GUARD;
	pBrowser_ = new WebBrowserSnap();
	pBrowser_->create( width, height );
	this->navigate( url );

	if (Moo::rc().device())
		createUnmanagedObjects();
}

/**
 * Destructor
 */
WebPage::~WebPage()
{
	BW_GUARD;
	deleteUnmanagedObjects();
	delete pBrowser_;
}

/**
 *	This method causes the object to navigate to a url
 *	@url the url to navigate to
 */
void WebPage::navigate( const std::string& url )
{
	BW_GUARD;
	url_ = url;
	pBrowser_->load( url.c_str() );
}

/**
 *	This method causes the browser to update itself
 */
void WebPage::updateBrowser()
{
	BW_GUARD;
	pBrowser_->update();
}

/**
 *	This method updates the texture to the current browser state
 */
void WebPage::updateTexture()
{
	BW_GUARD;
	if (pTexture_.hasComObject())
	{
		HBITMAP bitmap = pBrowser_->bitmap();
		BITMAP bmp;

		GetObject( bitmap, sizeof( bmp ), &bmp );

		IDirect3DSurface9* surface;
		pTexture_->GetSurfaceLevel( 0, &surface );

		RECT rect = { 0, 0, pBrowser_->width(), pBrowser_->height() };
		D3DXLoadSurfaceFromMemory( surface, NULL, NULL,
			bmp.bmBits, D3DFMT_A8R8G8B8, bmp.bmWidthBytes, NULL, &rect, D3DX_DEFAULT, 0 );

		surface->Release();
	}
}

/**
 *	This method updates the browser and texture
 */
void WebPage::update()
{
	BW_GUARD;
	this->updateBrowser();
	this->updateTexture();

}

/**
 *	Callback method for when the device is lost or changed
 */
void WebPage::createUnmanagedObjects()
{
	BW_GUARD;
	createTexture();
}

/**
 *	Callback method for when the device is lost or changed
 */
void WebPage::deleteUnmanagedObjects()
{
	BW_GUARD;
	destroyTexture();
}

/**
 *	Helper method to create the texture for the web page
 */
void WebPage::createTexture()
{
	BW_GUARD;
	pTexture_ = Moo::rc().createTexture( pBrowser_->width(), pBrowser_->height(), 1, 
		D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, "WebPage/WebTexture" );
}

/**
 *	Helper method to destroy the texture for the web page
 */
void WebPage::destroyTexture()
{
	BW_GUARD;
	pTexture_ = NULL;
}

/**
 *	This is the overridden pTexture method from the BaseTexture interface,
 *	it return the d3d texture for the web page
 *	It also makes sure the texture is updated.
 */
DX::BaseTexture* WebPage::pTexture( ) 
{ 
	BW_GUARD;
	// Create the update callback as we are using the texture this frame
	if (!s_webPageTask.exists() || s_webPageTask->finished())
	{
		s_webPageTask = new WebPageBGTask( this );
		BgTaskManager::instance().addMainThreadTask( s_webPageTask );
	}
	return pTexture_.pComObject(); 
}

/**
 *	This is the overridden width method from the BaseTexture interface,
 */
uint32 WebPage::width( ) const 
{ 
	return pBrowser_->width(); 
}

/**
 *	This is the overridden height method from the BaseTexture interface,
 */
uint32 WebPage::height( ) const 
{ 
	return pBrowser_->height(); 
}

/**
 *	This is the overridden format method from the BaseTexture interface,
 */
D3DFORMAT WebPage::format( ) const 
{ 
	return D3DFMT_A8R8G8B8; 
}

/**
 *	This is the overridden textureMemoryUsed method from the BaseTexture interface,
 */
uint32 WebPage::textureMemoryUsed( ) 
{ 
	return pBrowser_->width() * pBrowser_->height() * 4;
}



/*
 *	WebPageProvider implementation
 */
PY_TYPEOBJECT( WebPageProvider )

PY_BEGIN_METHODS( WebPageProvider )
	PY_METHOD( navigate )
	PY_METHOD( url )
	PY_METHOD( texture )
	PY_METHOD( update )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( WebPageProvider )
PY_END_ATTRIBUTES()

PY_FACTORY( WebPageProvider, BigWorld )

int PyWebPageProvider_token = 1;

/**
 *	Constructor for the web page provider
 */
WebPageProvider::WebPageProvider(uint32 width, uint32 height, 
									const std::string& url, PyTypePlus * pType) :
	PyObjectPlus( pType )
{
	webPage_ = new WebPage( width, height, url );
}

WebPageProvider::~WebPageProvider()
{

}

/**
 *	Standard get attribute method.
 */
PyObject * WebPageProvider::pyGetAttribute( const char * attr )
{
	BW_GUARD;
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}


/**
 *	Standard set attribute method.
 */
int WebPageProvider::pySetAttribute( const char * attr, PyObject * value )
{
	BW_GUARD;
	PY_SETATTR_STD();

	return PyObjectPlus::pySetAttribute( attr, value );
}

/**
 *	Python construction method for 
 */
PyObject * WebPageProvider::pyNew( PyObject * args )
{
	BW_GUARD;
	int w, h;
	char* url = NULL;

	if (!PyArg_ParseTuple( args, "ii|s", &w, &h, &url ))
	{
		PyErr_SetString( PyExc_TypeError, "BigWorld.WebPageProvider() "
			"expects width and height integer arguments and optional string url argument" );
		return NULL;
	}
	if (w <= 0 || w > 4096 || h <= 0 || h > 4096)
	{
		PyErr_SetString( PyExc_ValueError, "BigWorld.WebPageProvider() "
			"width and heigth must be > 0 and <= 4096" );
		return NULL;
	}

	return new WebPageProvider( w, h, url ? url : "" );
}

/**
 *	This method navigates to a url
 *	@param url the url to navigate to
 */
void WebPageProvider::navigate( const std::string& url )
{
	webPage_->navigate( url );
}

/**
 *	This method returns the current url
 *	@return curren url
 */
const std::string& WebPageProvider::url()
{
	return webPage_->url();
}

/**
 *	Get a texture provider for the webpage
 *	@return a unique texture provider for the webpage
 */
PyTextureProvider* WebPageProvider::texture()
{
	return new PyTextureProvider( NULL, webPage_.get() );
}

void WebPageProvider::update()
{
	webPage_->update();
}
