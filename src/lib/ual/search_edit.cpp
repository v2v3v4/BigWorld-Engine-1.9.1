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
#include <string>
#include "search_edit.hpp"



SearchEdit::SearchEdit() :
	CEdit()
{
}

void SearchEdit::setIdleText( const std::string& idleText )
{
	idleText_ = idleText;
	Invalidate();
	UpdateWindow();
}

bool SearchEdit::idle()
{
	CString str;
	GetWindowText( str );
	if ( str.IsEmpty() && ::GetFocus() != GetSafeHwnd() )
		return true;

	return false;
}

BEGIN_MESSAGE_MAP(SearchEdit, CEdit)
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_PAINT()
END_MESSAGE_MAP()


void SearchEdit::OnSetFocus(CWnd *oldWnd)
{
	Invalidate();
	UpdateWindow();
    CEdit::OnSetFocus(oldWnd);
}


void SearchEdit::OnKillFocus(CWnd *newWnd)
{
	Invalidate();
	UpdateWindow();
    CEdit::OnKillFocus(newWnd);
}

void SearchEdit::OnPaint()
{
	CEdit::OnPaint();
	if ( idle() )
	{
		Invalidate();
		CPaintDC dc( this );
		CRect rect;
		GetRect( &rect );
		CFont* oldFont = dc.SelectObject( GetFont() );
		dc.SetTextColor( GetSysColor( COLOR_GRAYTEXT ) );
		dc.DrawText( idleText_.c_str(), idleText_.length(),
			rect, 0 );
		dc.SelectObject( oldFont );
		ValidateRect( NULL );
	}
}
