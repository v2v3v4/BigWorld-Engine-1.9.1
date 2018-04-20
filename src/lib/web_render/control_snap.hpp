/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CONTROL_SNAP_HPP__
#define CONTROL_SNAP_HPP__

#include "control_container.hpp"

class ControlSnap
{
	int width_;
	int height_;
	HDC dc_;
	HBITMAP bitmap_;
	HBITMAP mono_;

	void dispatchMouseMessage( int mouseX, int mouseY, UINT msg, WPARAM wParam );

protected:
	enum DrawMethod
	{
		CCDM_VIEWOBJECT,
		CCDM_PRINT,
		CCDM_BITBLT
	};
	DrawMethod drawMethod_;

	ControlContainer container_;
	IViewObject* viewObject_;
public:
	ControlSnap( DrawMethod drawMethod = CCDM_VIEWOBJECT);
	virtual ~ControlSnap();
	BOOL create( int width, int height, REFCLSID clsid );

	int width() const	{	return width_;	}
	int height() const	{	return height_;	}
	HDC dc()	{	return dc_;	}
	HBITMAP bitmap()	{	return bitmap_;	}

	void update( int mouseX = -1, int mouseY = -1 );
	void click( int mouseX, int mouseY );

	virtual BOOL create( int width, int height )	{	return FALSE;	}
	virtual BOOL load( LPCTSTR resource )	{	return FALSE;	}
};

#endif//CONTROL_SNAP_HPP__
