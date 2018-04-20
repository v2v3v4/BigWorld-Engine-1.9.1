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
#include "web_browser_snap.hpp"

#pragma warning( disable: 4192 )
#import <shdocvw.dll>


BOOL WebBrowserSnap::create( int width, int height )
{
	static CLSID const CLSID_InternetExplorer
		= { 0x8856F961, 0x340A, 0x11D0, { 0xA9, 0x6B, 0x0, 0xC0, 0x4F, 0xD7, 0x5, 0xA2 } };
	return ControlSnap::create( width, height, CLSID_InternetExplorer );
}

BOOL WebBrowserSnap::load( LPCTSTR resource )
{
	SHDocVw::IWebBrowser2* wb2;
	if (SUCCEEDED( viewObject_->QueryInterface( __uuidof( SHDocVw::IWebBrowser2 ), (LPVOID*)&wb2 )))
	{
		if (SUCCEEDED( wb2->Navigate( resource )))
		{
			return TRUE;
		}
		wb2->Release();
	}
	return FALSE;
}
