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

#include "ual_drop_manager.hpp"


/*static*/ CRect UalDropManager::HIT_TEST_NONE = CRect( -1, -1, -1, -1 );
/*static*/ CRect UalDropManager::HIT_TEST_MISS = CRect(  0,  0,  0,  0 );


UalDropManager::UalDropManager():
	ext_(""),
	pen_( PS_SOLID, DRAG_BORDER, DRAG_COLOUR )
{}

void UalDropManager::add( SmartPointer< UalDropCallback > dropping )
{
	if (dropping == NULL || dropping->cWnd() == NULL)
	{
		WARNING_MSG( "UalDropManager::add: Tried to add a NULL drop target.\n" );
		return;
	}
	droppings_.insert( DropType( dropping->hWnd(), dropping ));
}

void UalDropManager::start( const std::string& ext )
{
	ext_ = ext;
	std::transform( ext_.begin(), ext_.end(), ext_.begin(), tolower );
	desktop_ = CWnd::GetDesktopWindow();
	dc_ = desktop_->GetDCEx( NULL, DCX_WINDOW | DCX_CACHE );
	oldPen_ = dc_->SelectObject( &pen_ );
	oldROP_ = dc_->SetROP2( R2_NOTXORPEN );
	highlighted_ = false;
}

void UalDropManager::highlight( const CRect& rect, bool light )
{
	static CRect s_oldRect = CRect(0,0,0,0);
	if (light && highlighted_ && (rect == s_oldRect)) return;

	if (highlighted_)
		dc_->Rectangle( s_oldRect );

	if (light)
		dc_->Rectangle( rect );

	s_oldRect = rect;
	highlighted_ = light;
}

SmartPointer< UalDropCallback > UalDropManager::test( HWND hwnd, UalItemInfo* ii )
{
	std::pair<DropMapIter,DropMapIter> drops = droppings_.equal_range( hwnd );
	for (DropMapIter i=drops.first; i!=drops.second; ++i)
	{
		if (i->second->ext() == ext_)
		{
			CRect temp = i->second->test( ii );
			if (temp == HIT_TEST_NONE) // No test
			{
				i->second->cWnd()->GetClientRect( &highlightRect_ );
			}
			else if (temp == HIT_TEST_MISS) // Test failed
			{
				return NULL;
			}
			else // Success
			{
				highlightRect_ = temp;
			}
			
			i->second->cWnd()->ClientToScreen ( &highlightRect_ );
			return i->second;
		}
	}
	return NULL;
}

SmartPointer< UalDropCallback > UalDropManager::test( UalItemInfo* ii )
{
	HWND hwnd = ::WindowFromPoint( CPoint( ii->x(), ii->y() ) );
	
	SmartPointer< UalDropCallback > drop = test( hwnd, ii );
	if (drop == NULL)
	{
		drop = test( ::GetParent( hwnd ), ii );
	}

	highlight( highlightRect_, drop != NULL );

	return drop;
}

bool UalDropManager::end( UalItemInfo* ii )
{
	SmartPointer< UalDropCallback > res = test( ii );

	highlight( highlightRect_, false );
	
	if ( dc_ != NULL )
	{
		dc_->SetROP2( oldROP_ );
		dc_->SelectObject( oldPen_ );
		desktop_->ReleaseDC( dc_ );
		dc_ = NULL;
	}

	ext_ = "";
	
	if (res)
		return res->execute( ii );

	return false;
}
