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
 *	GUI Tearoff panel framework - Tab class implementation
 */



#include "pch.hpp"
#include "guitabs.hpp"
#include "resmgr/string_provider.hpp"


namespace GUITABS
{


Tab::Tab( CWnd* parentWnd, std::string contentID ) :
	isVisible_( false )
{
	content_ = Manager::instance().createContent( contentID );

	construct( parentWnd );
}

Tab::Tab( CWnd* parentWnd, ContentPtr content ) :
	isVisible_( false ),
	content_( content )
{
	construct( parentWnd );
}

void Tab::construct( CWnd* parentWnd )
{
	if ( content_ )
	{
		CWnd* wnd = content_->getCWnd();

		if ( !wnd )
			ASSERT( 0 );

		if ( !IsWindow( wnd->GetSafeHwnd() ) )
		{
			wnd->Create(
				AfxRegisterWndClass( CS_OWNDC, ::LoadCursor(NULL, IDC_ARROW), (HBRUSH)::GetSysColorBrush(COLOR_BTNFACE) ),
				"GUITABS-Created-CWnd",
				WS_CHILD,
				CRect( 0, 0, 300, 400 ),
				parentWnd,
				0,
				0);
			if ( !IsWindow( wnd->GetSafeHwnd() ) )
				ASSERT( 0 );
		}
		else
		{
			wnd->SetParent( parentWnd );
		}

		wnd->UpdateData( FALSE );
	}
}

Tab::~Tab()
{
	if ( content_ && content_->getCWnd() )
		content_->getCWnd()->DestroyWindow();
}

bool Tab::load( DataSectionPtr section )
{
	if ( !section )
		return false;

	DataSectionPtr contentSec = section->openSection( "ContentData" );
	if ( !contentSec )
		return false;

	// for now, ignore if content returns false
	content_->load( contentSec );

	return true;
}

bool Tab::save( DataSectionPtr section )
{
	if ( !section )
		return false;

	DataSectionPtr contentSec = section->openSection( "ContentData", true );
	if ( !contentSec )
		return false;

	// for now, ignore if content returns false
	content_->save( contentSec );

	return true;
}

std::string Tab::getDisplayString()
{
	if (content_)
		return content_->getDisplayString();

	return L("GUITABS/TAB/NO_CONTENT");
}

std::string Tab::getTabDisplayString()
{
	if (content_)
		return content_->getTabDisplayString();

	return L("GUITABS/TAB/NO_CONTENT");
}

HICON Tab::getIcon()
{
	if (content_)
		return content_->getIcon();

	return 0;
}


CWnd* Tab::getCWnd()
{
	if (content_)
		return content_->getCWnd();

	return 0;
}

bool Tab::isClonable()
{
	if (content_)
		return content_->isClonable();

	return false;
}

void Tab::getPreferredSize( int& width, int& height )
{
	width = 0;
	height = 0;

	if (content_)
		content_->getPreferredSize( width, height );	
}

bool Tab::isVisible()
{
	if ( !content_ )
		return false;

	return isVisible_;
}

void Tab::setVisible( bool visible )
{
	isVisible_ = visible;
}

void Tab::show( bool show )
{
	if ( !content_ )
		return;

	isVisible_ = show;
	content_->getCWnd()->ShowWindow( show?SW_SHOW:SW_HIDE );
}



ContentPtr Tab::getContent()
{
	return content_;
}

void Tab::handleRightClick( int x, int y )
{
	if ( content_ )
		content_->handleRightClick( x, y );
}


}	// namespace
