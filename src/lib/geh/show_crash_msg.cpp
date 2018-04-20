/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "show_crash_msg.hpp"
#include <windows.h>
#include <shlwapi.h>
#pragma comment( lib, "shlwapi.lib" )

static const DWORD TICK_TO_WAIT = 4000;
static char title[512];
static char msg[1024];

static void centerWindow( HWND hwnd )
{
	RECT parentRect;
	RECT selfRect;
	HWND parent = GetParent( hwnd );
	if( !parent )
		parent = GetDesktopWindow();
	GetWindowRect( parent, &parentRect );
	GetWindowRect( hwnd, &selfRect );
	LONG x = ( parentRect.right + parentRect.left ) / 2 - ( selfRect.right - selfRect.left ) / 2;
	LONG y = ( parentRect.bottom + parentRect.top ) / 2 - ( selfRect.bottom - selfRect.top ) / 2;
	MoveWindow( hwnd, x, y, selfRect.right - selfRect.left, selfRect.bottom - selfRect.top, TRUE );
}

static void createItems( HWND hwnd )
{
	// setup the window
	MoveWindow( hwnd, 0, 0, 300, 160, FALSE );
	HWND wndStatic = CreateWindow( "STATIC", msg, WS_CHILD | WS_VISIBLE, 20, 20, 260, 120, hwnd, NULL, GetModuleHandle( NULL ), NULL );

	// set font
	HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	if( font && wndStatic )
		SendMessage( wndStatic, WM_SETFONT, (WPARAM)font, FALSE );

	// texts
	SetWindowText( hwnd, title );
	if( wndStatic )
		SetWindowText( wndStatic, msg );
}

INT_PTR CALLBACK dialogProc( HWND hwnd, UINT msg, WPARAM w, LPARAM l )
{
	switch( msg )
	{
	case WM_INITDIALOG:
		createItems( hwnd );
		SetWindowPos( hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
		SetTimer( hwnd, 1, TICK_TO_WAIT, NULL );
		centerWindow( hwnd );
		break;
	case WM_TIMER:
		EndDialog( hwnd, 0 );
		break;
	}
	return FALSE;
}

void showDumpMsg()
{
	char pathName[256];
	GetModuleFileName( NULL, pathName, sizeof( pathName ) );

	if( StrRChr( pathName, NULL, '.' ) )
		*StrRChr( pathName, NULL, '.' ) = 0;

	char* appName;
	if( StrRChr( pathName, NULL, '\\' ) )
		appName = StrRChr( pathName, NULL, '\\' ) + 1;
	else
	{
		StrCpy( pathName, "Application" );
		appName = pathName;
	}

	StrCpy( title, "BigWorld - " );
	StrCat( title, appName );

	StrCpy( msg, appName );
	StrCat( msg, " crashed unexpectedly.\n\n"
		"We are sending debug information back to BigWorld..." );

	DLGTEMPLATE dlg[ 2 ] = { { 0 } };
	dlg->style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_VISIBLE | WS_SYSMENU;
	dlg->dwExtendedStyle = 0;
	dlg->cdit = 0;
	dlg->x = 0;
	dlg->y = 0;
	dlg->cx = 400;
	dlg->cy = 300;

	DialogBoxIndirectParam( GetModuleHandle( NULL ), dlg, NULL, dialogProc, 0 );
}
