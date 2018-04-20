/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WEB_PAGE_HPP
#define WEB_PAGE_HPP

#include "moo/forward_declarations.hpp"
#include "moo/device_callback.hpp"
#include "web_browser_snap.hpp"
#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"
#include "romp/texture_feeds.hpp"

/**
 *	This class controls a web page using the WebBrowserSnap interface
 */
class WebPage : public Moo::DeviceCallback, public Moo::BaseTexture
{
public:
	WebPage(uint32 width, uint32 height, const std::string& url);
	~WebPage();

	void navigate( const std::string& url );
	void update();
	void updateBrowser();
	void updateTexture();


	const std::string& url() { return url_; }

	void createUnmanagedObjects();
	void deleteUnmanagedObjects();

	void createTexture();
	void destroyTexture();

	DX::BaseTexture*	pTexture();
	uint32				width() const;
	uint32				height() const;
	D3DFORMAT			format() const;
	uint32				textureMemoryUsed();

	void				finishInit( WebBrowserSnap* pBrowser );
private:
	std::string						url_;
	WebBrowserSnap*					pBrowser_;

	ComObjectWrap<DX::Texture> pTexture_;
};


/**
 *	This class is the web page provider class.
 *	It implements the python interface to a web page
 */
class WebPageProvider : public PyObjectPlus
{
	Py_Header( WebPageProvider, PyObjectPlus )
public:
	WebPageProvider(uint32 width, uint32 height, 
		const std::string& url, PyTypePlus * pType = &s_type_);
	~WebPageProvider();

	PY_FACTORY_DECLARE()

	void navigate( const std::string& url );
	const std::string& url();

	PyTextureProvider* texture();

	void update();

	PY_AUTO_METHOD_DECLARE( RETVOID, navigate, ARG( std::string, END ) )
	PY_AUTO_METHOD_DECLARE( RETDATA, url, END )
	PY_AUTO_METHOD_DECLARE( RETDATA, texture, END )
	PY_AUTO_METHOD_DECLARE( RETVOID, update, END )

	PyObject * pyGetAttribute( const char * attr );
	int pySetAttribute( const char * attr, PyObject * value );
private:
	SmartPointer<WebPage> webPage_;
};


#endif // WEB_PAGE
