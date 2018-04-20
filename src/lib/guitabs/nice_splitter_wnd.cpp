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
 *	GUI Tearoff panel framework - NiceSplitterWnd class implementation
 */


#include "pch.hpp"
#include "nice_splitter_wnd.hpp"



NiceSplitterWnd::NiceSplitterWnd() :
	eventHandler_( 0 ),
	lastWidth_( 0 ),
	lastHeight_( 0 ),
	allowResize_( true ),
	minRowSize_( 0 ),
	minColSize_( 0 )
{
}

void NiceSplitterWnd::setEventHandler( SplitterEventHandler* handler )
{
	eventHandler_ = handler;
}

void NiceSplitterWnd::allowResize( bool allow )
{
	allowResize_ = allow;
}

void NiceSplitterWnd::setMinRowSize( int minSize )
{
	minRowSize_ = minSize;
}

void NiceSplitterWnd::setMinColSize( int minSize )
{
	minColSize_ = minSize;
}


// Overrides
void NiceSplitterWnd::OnDrawSplitter( CDC* pDC, ESplitType nType, const CRect& rectArg )
{
	// this code was copied and modified from winsplit.cpp from the mfc source code.
	// this is the way microsoft sugests it (http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vcmfc98/html/_mfcnotes_tn029.asp).

	// if pDC == NULL, then just invalidate
	if (pDC == NULL)
	{
		RedrawWindow(rectArg, NULL, RDW_INVALIDATE|RDW_NOCHILDREN);
		return;
	}
	ASSERT_VALID(pDC);

	// otherwise, actually draw
	CRect rect = rectArg;
	DWORD bodyCol = ::GetSysColor(COLOR_BTNFACE);
	DWORD shadowCol = ::GetSysColor(COLOR_BTNSHADOW);
	switch (nType)
	{
	case splitBorder:
		pDC->Draw3dRect(rect, bodyCol, bodyCol );
		rect.InflateRect( -::GetSystemMetrics(SM_CXBORDER), -::GetSystemMetrics(SM_CYBORDER) );
		pDC->Draw3dRect(rect, shadowCol, shadowCol );
		return;

	case splitIntersection:
		break;

	case splitBox:
		pDC->Draw3dRect(rect, bodyCol, bodyCol );
		rect.InflateRect( -::GetSystemMetrics(SM_CXBORDER), -::GetSystemMetrics(SM_CYBORDER) );
		pDC->Draw3dRect(rect, bodyCol, bodyCol );
		rect.InflateRect( -::GetSystemMetrics(SM_CXBORDER), -::GetSystemMetrics(SM_CYBORDER) );
		break;

	case splitBar:
		break;

	default:
		ASSERT(FALSE);  // unknown splitter type
	}

	// fill the middle
	pDC->FillSolidRect(rect, bodyCol );
}

void NiceSplitterWnd::SetSplitCursor(int ht)
{
	if ( !allowResize_ )
		return;

	CSplitterWnd::SetSplitCursor( ht );
}

void NiceSplitterWnd::StartTracking(int ht)
{
	if ( !allowResize_ )
		return;

	CSplitterWnd::StartTracking( ht );
}

void NiceSplitterWnd::TrackRowSize(int y, int row)
{
	// Trick "y" by substracting the non client area (caption) of the pane,
	// so the CSplitterWnd method works properly with panes with caption bar.
	CRect paneRect;
	GetPane(row, 0)->GetWindowRect( &paneRect );
	CPoint pt(0, paneRect.top);
	GetPane(row, 0)->ScreenToClient(&pt);
	y -= pt.y; // diff between the window's top and the top of the client area

	CSplitterWnd::TrackRowSize( y, row );
	if ( m_pRowInfo[row].nIdealSize < minRowSize_ )
		m_pRowInfo[row].nIdealSize = minRowSize_;
}

void NiceSplitterWnd::TrackColumnSize(int x, int col)
{
	CSplitterWnd::TrackColumnSize( x, col );
	if ( m_pColInfo[col].nIdealSize < minColSize_ )
		m_pColInfo[col].nIdealSize = minColSize_;
}


// Messages
BEGIN_MESSAGE_MAP(NiceSplitterWnd,CSplitterWnd)
	ON_WM_SIZE()
END_MESSAGE_MAP()

void NiceSplitterWnd::OnSize( UINT nType, int cx, int cy )
{
	if ( cx == 0 || cy == 0 )
		return;

	if ( eventHandler_ )
		eventHandler_->resizeSplitter( lastWidth_, lastHeight_, cx, cy );

	CSplitterWnd::OnSize( nType, cx, cy );

	lastWidth_ = cx;
	lastHeight_ = cy;
}
