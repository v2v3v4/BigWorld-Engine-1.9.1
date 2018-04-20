/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WEB_BROWSER_SNAP_HPP__
#define WEB_BROWSER_SNAP_HPP__

#include "control_snap.hpp"

class WebBrowserSnap : public ControlSnap
{
public:
	BOOL create( int width, int height );
	BOOL load( LPCTSTR resource );
};

#endif//WEB_BROWSER_SNAP_HPP__
