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
#include "about_box.hpp"

#include "common/compile_time.hpp"
#include "common/tools_common.hpp"

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	mBackground.LoadBitmap( IDB_ABOUTBOX );
	mFont.CreatePointFont( 90, "Arial", NULL );
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

BOOL CAboutDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	BITMAP bitmap;
	mBackground.GetBitmap( &bitmap );
	RECT rect = { 0, 0, bitmap.bmWidth, bitmap.bmHeight };
	AdjustWindowRect( &rect, GetWindowLong( GetSafeHwnd(), GWL_STYLE ), FALSE );
	MoveWindow( &rect, FALSE );
	CenterWindow();

	SetCapture();

	return TRUE;
}

void CAboutDlg::OnPaint()
{
	CPaintDC dc(this); // device context for painting
	CDC memDC;
	memDC.CreateCompatibleDC( &dc);
	CBitmap* saveBmp = memDC.SelectObject( &mBackground );
	CFont* saveFont = memDC.SelectObject( &mFont );

	RECT client;
	GetClientRect( &client );

	dc.SetTextColor( 0x00808080 );
	dc.BitBlt( 0, 0, client.right, client.bottom, &memDC, 0, 0, SRCCOPY );

	memDC.SelectObject( &saveBmp );
	memDC.SelectObject( &saveFont );

	CString builtOn = CString( "Version " ) + aboutVersionString;
	if (ToolsCommon::isEval())
		builtOn += CString( " Eval" );
#ifdef _DEBUG
    builtOn += CString( " Debug" );
#endif
	builtOn += CString( ": built " ) + aboutCompileTimeString;

	dc.SetBkMode( TRANSPARENT );
	dc.ExtTextOut( 72, 290, 0, NULL, builtOn, NULL );
}

void CAboutDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	CDialog::OnLButtonDown(nFlags, point);
	OnOK();
}

void CAboutDlg::OnRButtonDown(UINT nFlags, CPoint point)
{
	CDialog::OnRButtonDown(nFlags, point);
	OnOK();
}