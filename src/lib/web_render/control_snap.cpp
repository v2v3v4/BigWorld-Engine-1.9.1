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
#include "control_snap.hpp"

ControlSnap::ControlSnap( ControlSnap::DrawMethod drawMethod )
	: viewObject_( NULL ),
	width_( 0 ),
	height_( 0 ),
	drawMethod_( drawMethod )
{}

ControlSnap::~ControlSnap()
{
	if (viewObject_)
	{
		viewObject_->Release();
	}
}

BOOL ControlSnap::create( int width, int height, REFCLSID clsid )
{
	if (container_.create( width, height, clsid ))
	{
		if (SUCCEEDED( container_.embededUnknown()->QueryInterface(
			IID_IViewObject, (LPVOID*)&viewObject_ )))
		{
			width_ = width;
			height_ = height;

			HDC desktopDC = GetDC( NULL );

			dc_ = CreateCompatibleDC( desktopDC );
			BITMAPINFO info;
			info.bmiHeader.biSize = sizeof( info.bmiHeader );
			info.bmiHeader.biWidth = width_;
			info.bmiHeader.biHeight = -height_;
			info.bmiHeader.biPlanes = 1;
			info.bmiHeader.biBitCount = 32;
			info.bmiHeader.biCompression = BI_RGB;
			info.bmiHeader.biSizeImage = 0;
			info.bmiHeader.biXPelsPerMeter = 0;
			info.bmiHeader.biYPelsPerMeter = 0;
			info.bmiHeader.biClrUsed = 0;
			info.bmiHeader.biClrImportant = 0;

			char* bits;
			bitmap_ = CreateDIBSection( dc_, &info, DIB_RGB_COLORS, (LPVOID*)&bits, NULL, NULL );

			ReleaseDC( NULL, desktopDC );
			mono_ = (HBITMAP)SelectObject( dc_, bitmap_ );

			return TRUE;
		}
	}
	return FALSE;
}

void ControlSnap::dispatchMouseMessage( int mouseX, int mouseY, UINT msg, WPARAM wParam )
{
	if (mouseX >= 0 && mouseX < width() &&
		mouseY >= 0 && mouseY < height())
	{
		HWND hwnd;

		if (SUCCEEDED( container_.embededOleInPlaceObject()->GetWindow( &hwnd )))
		{
			POINT point = { mouseX, mouseY };

			for (;;)
			{
				HWND child = ChildWindowFromPoint( hwnd, point );

				if (child != hwnd)
				{
					ClientToScreen( hwnd, &point );
					ScreenToClient( child, &point );
					hwnd = child;
				}
				else
				{
					break;
				}
			}

			SendMessage( hwnd, msg, wParam, point.x + point.y * 65536 );
		}
	}
}

void ControlSnap::update( int mouseX, int mouseY )
{
	dispatchMouseMessage( mouseX, mouseY, WM_MOUSEMOVE, 0 );

	if (drawMethod_ == CCDM_VIEWOBJECT)
	{
		RECTL rect = { 0, 0, width(), height() };
		viewObject_->Draw( DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc(), &rect, NULL, NULL, NULL );
	}
	else if (drawMethod_ == CCDM_PRINT)
	{
		HWND hwnd;

		if (SUCCEEDED( container_.embededOleInPlaceObject()->GetWindow( &hwnd )))
		{
			SendMessage( hwnd, WM_PRINT, (WPARAM)dc(),
				PRF_CHILDREN | PRF_CLIENT | PRF_ERASEBKGND | PRF_NONCLIENT | PRF_OWNED );
		}
	}
	else if (drawMethod_ == CCDM_BITBLT)
	{
		HWND hwnd;

		if (SUCCEEDED( container_.embededOleInPlaceObject()->GetWindow( &hwnd )))
		{
			HDC hdc = GetWindowDC( hwnd );
			BitBlt( dc(), 0, 0, width(), height(), hdc, 0, 0, SRCCOPY );
			ReleaseDC( hwnd, hdc );
		}
	}
}

void ControlSnap::click( int mouseX, int mouseY )
{
	dispatchMouseMessage( mouseX, mouseY, WM_LBUTTONDOWN, MK_LBUTTON );
	dispatchMouseMessage( mouseX, mouseY, WM_LBUTTONUP, 0 );
}
