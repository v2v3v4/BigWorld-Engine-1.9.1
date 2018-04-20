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
 *	ContentContainer class.
 *	Implements a panel that contains other Content-inheriting classes, useful
 *  in making switching between different contents modal. It basically behaves
 *  as a single tab that changes its content dynamically.
 */


#include "pch.hpp"
#include <string>
#include "resmgr/datasection.hpp"
#include "resmgr/string_provider.hpp"
#include "content_container.hpp"
#include "manager.hpp"

namespace GUITABS
{

const std::string ContentContainer::contentID = "GUITABS::ContentContainer";


ContentContainer::ContentContainer() :
	currentContent_( 0 )
{}

ContentContainer::~ContentContainer()
{
}

void ContentContainer::createContentWnd( ContentPtr content )
{
	if ( !content )
		return;

	CWnd* wnd = content->getCWnd();

	if ( !wnd )
		ASSERT( 0 );

	if ( !IsWindow( wnd->GetSafeHwnd() ) )
	{
		wnd->Create(
			AfxRegisterWndClass( CS_OWNDC, ::LoadCursor(NULL, IDC_ARROW), (HBRUSH)::GetSysColorBrush(COLOR_BTNFACE) ),
			"GUITABS-Created-CWnd",
			WS_CHILD,
			CRect( 0, 0, 300, 400 ),
			this,
			0,
			0);
		if ( !IsWindow( wnd->GetSafeHwnd() ) )
			ASSERT( 0 );
	}
	else
	{
		wnd->SetParent( this );
	}

	wnd->UpdateData( FALSE );
}

void ContentContainer::addContent( ContentPtr content )
{
	if ( !content )
		return;

	createContentWnd( content );

	contents_.push_back( content );

	if ( !currentContent_ )
		currentContent( content );
}

void ContentContainer::addContent( std::string subcontentID )
{
	addContent( Manager::instance().createContent( subcontentID ) );
}

void ContentContainer::currentContent( ContentPtr content )
{
	ContentVecIt i = std::find( contents_.begin(), contents_.end(), content );
	if ( i == contents_.end() )
		return;

	CWnd* oldContent = 0;
	if ( !!currentContent_ && currentContent_ != content )
		oldContent = currentContent_->getCWnd();

	currentContent_ = content;
	CRect rect;
	GetClientRect( &rect );
	currentContent_->getCWnd()->SetWindowPos( 0, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER );
	currentContent_->getCWnd()->ShowWindow( SW_SHOW );
	if ( oldContent )
		oldContent->ShowWindow( SW_HIDE );
	CWnd* parent = GetParent();
	if ( parent )
		parent->RedrawWindow( 0, 0, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN );
}

void ContentContainer::currentContent( std::string subcontentID )
{
	for( ContentVecIt i = contents_.begin(); i != contents_.end(); ++i )
	{
		if ( (*i)->getContentID() == subcontentID )
		{
			currentContent( *i );
			break;
		}
	}
}

void ContentContainer::currentContent( int index )
{
	int count = 0;
	for( ContentVecIt i = contents_.begin(); i != contents_.end(); ++i )
	{
		if ( count == index )
		{
			currentContent( *i );
			break;
		}
		count++;
	}
}

ContentPtr ContentContainer::currentContent()
{
	return currentContent_;
}

bool ContentContainer::contains( ContentPtr content )
{
	for( ContentVecIt i = contents_.begin(); i != contents_.end(); ++i )
		if ( (*i) == content )
			return true;

	return false;
}

int ContentContainer::contains( const std::string subcontentID )
{
	int cnt = 0;
	for( ContentVecIt i = contents_.begin(); i != contents_.end(); ++i )
		if ( (*i)->getContentID() == subcontentID )
			cnt++;

	return cnt;
}


ContentPtr ContentContainer::getContent( const std::string subcontentID )
{
	int index = 0;
	return getContent( subcontentID, index );
}

ContentPtr ContentContainer::getContent( const std::string subcontentID, int& index )
{
	for( ContentVecIt i = contents_.begin(); i != contents_.end(); ++i )
	{
		if ( subcontentID.compare( (*i)->getContentID() ) == 0 )
		{
			if ( index <= 0 )
				return *i;
			else
				index--;
		}
	}

	return 0;
}



std::string ContentContainer::getContentID()
{
	return contentID;
}

void ContentContainer::broadcastMessage( UINT msg, WPARAM wParam, LPARAM lParam )
{
	for( ContentVecIt i = contents_.begin(); i != contents_.end(); ++i )
		(*i)->getCWnd()->SendMessage( msg, wParam, lParam );
}

bool ContentContainer::load( DataSectionPtr section )
{
	if ( !section )
		return false;

	std::vector<DataSectionPtr> sections;
	section->openSections( "Subcontent", sections );
	if ( sections.empty() )
		return true;

	ContentPtr firstContent = 0;

	for( std::vector<DataSectionPtr>::iterator s = sections.begin();
		s != sections.end(); ++s )
	{
		ContentPtr content = Manager::instance().createContent( (*s)->asString() );
		if ( !content )
			continue;

		DataSectionPtr subsec = (*s)->openSection( "SubcontentData" );
		if ( !subsec )
			continue;

		addContent( content );
		if ( !firstContent || (*s)->readBool( "current" ) )
			firstContent = content;

		content->load( subsec );
	}

	if ( !!firstContent )
	{
		currentContent( firstContent );
	}

	return true;
}

bool ContentContainer::save( DataSectionPtr section )
{
	if ( !section )
		return false;

	for( ContentVecIt i = contents_.begin(); i != contents_.end(); ++i )
	{
		DataSectionPtr sec = section->newSection( "Subcontent" );
		if ( !sec )
			continue;

		sec->setString( (*i)->getContentID() );
		if ( currentContent_ == (*i) )
			sec->writeBool( "current", true );
		DataSectionPtr subsec = sec->newSection( "SubcontentData" );
		if ( !subsec )
			continue;

		(*i)->save( subsec );
	}

	return true;
}

std::string ContentContainer::getDisplayString()
{
	if ( !currentContent_ )
		return L("GUITABS/CONTENT_CONTAINER/NO_CONTENT");

	return currentContent_->getDisplayString();
}

std::string ContentContainer::getTabDisplayString()
{
	if ( !currentContent_ )
		return L("GUITABS/CONTENT_CONTAINER/NO_CONTENT");

	return currentContent_->getTabDisplayString();
}

HICON ContentContainer::getIcon()
{
	if ( !currentContent_ )
		return 0;

	return currentContent_->getIcon();
}

CWnd* ContentContainer::getCWnd()
{
	return this;
}

void ContentContainer::getPreferredSize( int& width, int& height )
{
	width = 0;
	height = 0;
	for( ContentVecIt i = contents_.begin(); i != contents_.end(); ++i )
	{
		int w;
		int h;

		(*i)->getPreferredSize( w, h );

		width = max( width, w );
		height = max( height, h );
	}
}

Content::OnCloseAction ContentContainer::onClose( bool isLastContent )
{
	// never destroy, only hide
	return CONTENT_HIDE;
}

void ContentContainer::handleRightClick( int x, int y )
{
	if ( !currentContent_ )
		return;

	currentContent_->handleRightClick( x, y );
}

BEGIN_MESSAGE_MAP(ContentContainer, CDialog)
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
END_MESSAGE_MAP()

void ContentContainer::OnSize( UINT nType, int cx, int cy )
{
	CDialog::OnSize( nType, cx, cy );

	if ( !!currentContent_ )
		currentContent_->getCWnd()->SetWindowPos( 0, 0, 0, cx, cy, SWP_NOZORDER );
}

void ContentContainer::OnSetFocus( CWnd* pOldWnd )
{
	CDialog::OnSetFocus( pOldWnd );

	if ( !!currentContent_ )
		currentContent_->getCWnd()->SetFocus();
}


} // namespace