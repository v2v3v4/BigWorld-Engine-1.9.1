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
 *	FiltersCtrl: Manages a set of push-like checkbox buttons.
 */


#include "pch.hpp"
#include <string>
#include <vector>
#include "filters_ctrl.hpp"


// FiltersCtrl
FiltersCtrl::FiltersCtrl() :
	eventHandler_( 0 ),
	lines_( 1 ),
	separatorWidth_( 10 ),
	butSeparation_( 4 ),
	pushlike_( false )
{
}

FiltersCtrl::~FiltersCtrl()
{
	clear();
}

void FiltersCtrl::clear()
{
	filters_.clear();
	lines_ = 1;
}

bool FiltersCtrl::empty()
{
	return filters_.empty();
}

void FiltersCtrl::add( const char* name, bool pushed, void* data )
{
	FilterPtr newFilter = new Filter();

	int id = filters_.size() + FILTERCTRL_ID_BASE;

	CWindowDC dc( this );
	CFont* oldFont = dc.SelectObject( GetParent()->GetFont() );
	CSize textSize = dc.GetTextExtent( name, strlen( name ) );

	newFilter->name = name;
	newFilter->data = data;
	newFilter->button.Create(
		name,
		(pushlike_?BS_PUSHLIKE:0) | BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE,
		CRect( 0, 0, textSize.cx + (pushlike_?14:26), 20 ),
		this,
		id );
	if ( pushlike_ )
	{
		LONG style = GetWindowLong( newFilter->button.GetSafeHwnd(), GWL_EXSTYLE );
		SetWindowLong( newFilter->button.GetSafeHwnd(), GWL_EXSTYLE, style | WS_EX_STATICEDGE );
	}
	newFilter->button.SetFont( GetParent()->GetFont() );
	newFilter->button.SetWindowPos( 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED );

	CRect rect;
	GetWindowRect( &rect );
	recalcWidth( rect.Width() );

	if ( pushed )
		newFilter->button.SetCheck( BST_CHECKED );

	filters_.push_back( newFilter );

	dc.SelectObject( oldFont );
}

void FiltersCtrl::addSeparator()
{
	FilterPtr newSep = new Filter();

	newSep->name = "";
	newSep->data = 0;
	newSep->separator.Create(
		"",
		WS_CHILD | WS_VISIBLE | WS_DISABLED,
		CRect( 0, 0, 2, 20 ),
		this,
		0 );
	LONG style = GetWindowLong( newSep->separator.GetSafeHwnd(), GWL_EXSTYLE );
	SetWindowLong( newSep->separator.GetSafeHwnd(), GWL_EXSTYLE, style | WS_EX_STATICEDGE );
	newSep->separator.SetWindowPos( 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED );
	filters_.push_back( newSep );
}

int FiltersCtrl::getHeight()
{
	return lines_*22;
}

void FiltersCtrl::recalcWidth( int width )
{
	lines_ = 1;
	int x = 0;
	int y = 0;

	for( FilterItr i = filters_.begin(); i != filters_.end(); ++i )
	{
		if ( (*i)->button.GetSafeHwnd() )
		{
			CRect brect;
			(*i)->button.GetWindowRect( brect );
			if ( x && x + brect.Width() > width )
			{
				x = 0;
				y += 22;
				lines_++;
			}

			(*i)->button.SetWindowPos( 0, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
			(*i)->button.RedrawWindow();

			x += brect.Width() + butSeparation_;
		}
		else
		{
			CRect brect;
			(*i)->separator.GetWindowRect( brect );
			(*i)->separator.SetWindowPos( 0,
				x + (separatorWidth_ - butSeparation_ - brect.Width() )/2, y,
				0, 0, SWP_NOSIZE | SWP_NOZORDER );
			(*i)->separator.RedrawWindow();
			x += separatorWidth_;
		}

	}
}

void FiltersCtrl::enableAll( bool enable )
{
	for( FilterItr i = filters_.begin(); i != filters_.end(); ++i )
		if ( (*i)->button.GetSafeHwnd() )
			(*i)->button.EnableWindow( enable?TRUE:FALSE );
}

void FiltersCtrl::enable( const std::string& name, bool enable )
{
	if ( name.empty() )
		return;

	for( FilterItr i = filters_.begin(); i != filters_.end(); ++i )
		if ( (*i)->name == name && (*i)->button.GetSafeHwnd() )
		{
			(*i)->button.EnableWindow( enable?TRUE:FALSE );
			break;
		}
}

void FiltersCtrl::setEventHandler( FiltersCtrlEventHandler* eventHandler )
{
	eventHandler_ = eventHandler;
}

BEGIN_MESSAGE_MAP(FiltersCtrl, CWnd)
	ON_WM_SIZE()
	ON_COMMAND_RANGE( FILTERCTRL_ID_BASE, FILTERCTRL_ID_BASE + 100, OnFilterClicked )
END_MESSAGE_MAP()

void FiltersCtrl::OnFilterClicked( UINT nID )
{
	if ( !eventHandler_ )
		return;

	nID -= FILTERCTRL_ID_BASE;

	if ( nID >= 0 && nID < filters_.size() )
	{
		eventHandler_->filterClicked(
			filters_[ nID ]->name.c_str(),
			(filters_[ nID ]->button.GetCheck() == BST_CHECKED),
			filters_[ nID ]->data
			);
	}
}

void FiltersCtrl::OnSize( UINT nType, int cx, int cy )
{
	CWnd::OnSize( nType, cx, cy );

	recalcWidth( cx );
}
