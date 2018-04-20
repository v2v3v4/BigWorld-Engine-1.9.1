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
 *	GUI Tearoff panel framework - Panel class implementation
 */



#include "pch.hpp"
#include "guitabs.hpp"
#include "resmgr/string_provider.hpp"


namespace GUITABS
{


Panel::Panel( CWnd* parent ) :
	activeTab_( 0 ),
	isFloating_( false ),
	isExpanded_( true ),
	expandedSize_( 100 ),
	isActive_( false ),
	buttonDown_( 0 ),
	lastX_( 300 ),
	lastY_( 200 ),
	tempTab_( 0 )
{
	CreateEx(
		0,
		AfxRegisterWndClass( CS_OWNDC, ::LoadCursor(NULL, IDC_ARROW), (HBRUSH)::GetSysColorBrush(COLOR_BTNFACE) ),
		"Panel",
		WS_CHILD | WS_CLIPCHILDREN,
		CRect(0,0,1,1),
		parent,
		0,
		0);

	tabBar_ = new TabCtrl( this, TabCtrl::TOP );

	tabBar_->setEventHandler( this );
	tabBar_->ShowWindow( SW_SHOW );

	NONCLIENTMETRICS metrics;
	metrics.cbSize = sizeof( metrics );
	SystemParametersInfo( SPI_GETNONCLIENTMETRICS, metrics.cbSize, (void*)&metrics, 0 );
	metrics.lfSmCaptionFont.lfWeight = FW_NORMAL;
	captionFont_.CreateFontIndirect( &metrics.lfSmCaptionFont );
}

Panel::~Panel()
{
	if ( tabBar_ )
		tabBar_->DestroyWindow();

	activeTab_ = 0;
	tabList_.clear();

	DestroyWindow();
}

CWnd* Panel::getCWnd()
{
	return this;
}

void Panel::addTab( const std::string contentID )
{
	TabPtr newTab = new Tab( this, contentID );
	tabList_.push_back( newTab );
	tabBar_->insertItem( newTab->getTabDisplayString(), newTab->getIcon(), newTab.getObject() );
	newTab->show( true );
	setActiveTab( newTab );
}

void Panel::addTab( TabPtr tab )
{
	tab->getCWnd()->SetParent( this );
	tabList_.push_back( tab );
	if ( tab->isVisible() )
	{
		tabBar_->insertItem( tab->getTabDisplayString(), tab->getIcon(), tab.getObject() );
		setActiveTab( tab );
	}
}

void Panel::detachTab( TabPtr tab )
{
	for(TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
	{
		if ( (*i) == tab )
		{
			if ( activeTab_ == tab )
				activeTab_ = 0;
			tab->getCWnd()->ShowWindow( SW_HIDE );
			tab->getCWnd()->SetParent( 0 );
			tabBar_->removeItem( tab.getObject() );
			tabList_.erase( i );
			for( i = tabList_.begin(); i != tabList_.end(); ++i )
			{
				if ( tabBar_->contains( (*i).getObject() ) )
				{
					setActiveTab( *i );
					break;
				}
			}
			if ( i == tabList_.end() )
				setActiveTab( 0 );

			break;
		}
	}
}

void Panel::detachTab( const std::string contentID )
{
	for(TabItr i = tabList_.begin(); i != tabList_.end(); )
	{
		if ( contentID.compare( (*i)->getContent()->getContentID() ) == 0 )
		{
			// restart iterator since detachTab will remove the tab from the list
			TabItr old = i;
			++i;
			detachTab( *old );
		}
		else
			++i;
	}
}

TabPtr Panel::detachFirstTab()
{
	if ( tabList_.empty() )
		return 0;

	TabPtr tab = *(tabList_.begin());

	detachTab( *(tabList_.begin()) );

	return tab;
}

bool Panel::load( DataSectionPtr section )
{
	lastX_ = section->readInt( "lastX", lastX_ );
	lastY_ = section->readInt( "lastY", lastY_ );
	isExpanded_ = section->readBool( "expanded", isExpanded_ );
	expandedSize_ = section->readInt( "expandedSize", expandedSize_ );
	isFloating_ = section->readBool( "floating", isFloating_ );

	std::vector<DataSectionPtr> tabs;
	section->openSections( "Tab", tabs );
	if ( tabs.empty() )
		return false;
	activeTab_ = 0;
	TabPtr firstTab = 0;
	for( std::vector<DataSectionPtr>::iterator i = tabs.begin(); i != tabs.end(); ++i )
	{
		std::string contentID = (*i)->readString( "contentID" );
		if ( contentID.empty() )
			continue;

		TabPtr newTab = new Tab( this, contentID );

		if ( !newTab->getContent() )
			continue;

		newTab->setVisible( (*i)->readBool( "visible", true ) );

		// ignoring if loading a tab returns false
		newTab->load( *i );

		addTab( newTab );

		newTab->getCWnd()->ShowWindow( SW_HIDE );

		if ( !firstTab && !!activeTab_ )
			firstTab = activeTab_;
	}
	if ( firstTab )
		setActiveTab( firstTab );

	if ( activeTab_ )
	{
		updateTabBar();

		if ( isExpanded_ )
			activeTab_->getCWnd()->ShowWindow( SW_SHOW );
		else
			activeTab_->getCWnd()->ShowWindow( SW_HIDE );
	}

	int w;
	int h;
	getPreferredSize( w, h );
	SetWindowPos( 0, 0, 0,
		section->readInt( "lastWidth", w ), section->readInt( "lastHeight", h ),
		SWP_NOMOVE | SWP_NOZORDER );

	return true;
}

bool Panel::save( DataSectionPtr section )
{
	if ( !section )
		return false;

	// save properties
	section->writeInt( "lastX", lastX_ );
	section->writeInt( "lastY", lastY_ );
	CRect rect;
	GetWindowRect( &rect );
	section->writeInt( "lastWidth", rect.Width() );
	section->writeInt( "lastHeight", rect.Height() );
	section->writeBool( "expanded", isExpanded_ );
	section->writeInt( "expandedSize", expandedSize_ );
	section->writeBool( "floating", isFloating_ );

	// save tab order in a temporary vector
	std::vector<TabPtr> tabOrder;
	for( int i = 0; i < tabBar_->itemCount(); ++i )
		tabOrder.push_back( (Tab*)tabBar_->getItemData( i ) );
	for( TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
		if ( std::find<std::vector<TabPtr>::iterator,TabPtr>(
				tabOrder.begin(), tabOrder.end(), *i ) == tabOrder.end() )
			tabOrder.push_back( *i );

	// save tabs
	for( std::vector<TabPtr>::iterator i = tabOrder.begin(); i != tabOrder.end(); ++i )
	{
		DataSectionPtr tabSec = section->newSection( "Tab" );
		if ( !tabSec )
			return false;

		// have to save visibility at this level, not in the tab...
		tabSec->writeBool( "visible", (*i)->isVisible() );
		tabSec->writeString( "contentID", (*i)->getContent()->getContentID() );

		if ( !(*i)->save( tabSec ) )
			return false;
	}

	return true;
}

void Panel::activate()
{
	isActive_ = true;
	paintCaptionBar();
	if ( activeTab_ )
		activeTab_->getCWnd()->SetFocus();
	Manager::instance().dock()->setActivePanel( this );
}

void Panel::deactivate()
{
	isActive_ = false;
	paintCaptionBar();
}

bool Panel::isExpanded()
{
	return isExpanded_;
}

void Panel::setExpanded( bool expanded )
{
	isExpanded_ = expanded;
	Manager::instance().dock()->rollupPanel( this );
	if ( activeTab_ )
	{
		updateTabBar();

		if ( isExpanded_ )
		{
			activeTab_->getCWnd()->ShowWindow( SW_SHOW );
			activeTab_->getCWnd()->RedrawWindow();
		}
		else
			activeTab_->getCWnd()->ShowWindow( SW_HIDE );
	}
}



bool Panel::isFloating()
{
	return isFloating_;
}

void Panel::setFloating( bool floating )
{
	isFloating_ = floating;
}


void Panel::getPreferredSize( int& width, int& height )
{
	width = 0;
	height = 0;

	for( TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
	{
		int w;
		int h;

		(*i)->getPreferredSize( w, h );
		if ( w > width )
			width = w;
		if ( h > height )
			height = h;
	}

	if ( !isExpanded() )
		height = PANEL_ROLLUP_SIZE;
}

int Panel::getCaptionSize()
{
	return CAPTION_HEIGHT;
}

int Panel::getTabCtrlSize()
{
	if ( tabBar_->itemCount() > 1 )
		return tabBar_->getHeight();
	else
		return 0;	
}

bool Panel::isTabCtrlAtTop()
{
	if ( tabBar_->getAlignment() == TabCtrl::TOP )
		return true;
	else
		return false;
}


void Panel::clearPosList( bool docked )
{
	if ( docked )
		dockedPosList_.clear();
	else
		floatingPosList_.clear();

	resetPosList( docked );
}

void Panel::resetPosList( bool docked )
{
	if ( docked )
		dockedPosItr_ = dockedPosList_.begin();
	else
		floatingPosItr_ = floatingPosList_.begin();
}

void Panel::insertPos( bool docked, PanelPos pos )
{
	if ( docked )
		dockedPosList_.push_back( pos );
	else
		floatingPosList_.push_back( pos );
}

bool Panel::getNextPos( bool docked, PanelPos& pos )
{
	if ( docked )
	{
		if ( dockedPosItr_ == dockedPosList_.end() || dockedPosList_.empty() )
			return false;

		pos = *dockedPosItr_++;
	}
	else
	{
		if ( floatingPosItr_ == floatingPosList_.end() || floatingPosList_.empty() )
			return false;

		pos = *floatingPosItr_++;
	}

	return true;
}

void Panel::getLastPos( int& x, int& y )
{
	x = lastX_;
	y = lastY_;
}

void Panel::setLastPos( int x, int y )
{
	lastX_ = x;
	lastY_ = y;
}

bool Panel::tabContains( TabPtr t, ContentPtr content )
{
	Content* tcontent = t->getContent().getObject();
	if ( !tcontent )
		return 0;
	
	return tcontent == content ||
			( tcontent->getContentID() == ContentContainer::contentID &&
			((ContentContainer*)tcontent)->contains( content ) );
}

int Panel::tabContains( TabPtr t, const std::string contentID )
{
	Content* tcontent = t->getContent().getObject();
	if ( !tcontent )
		return 0;
	
	int cnt = 0;

	if ( contentID.compare( tcontent->getContentID() ) == 0 )
		++cnt;
	else if ( tcontent->getContentID() == ContentContainer::contentID )
		cnt += ((ContentContainer*)tcontent)->contains( contentID );
	
	return cnt;
}

bool Panel::contains( ContentPtr content )
{
	for( TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
	{
		if ( tabContains( *i, content ) )
			return true;
	}

	return false;
}

int Panel::contains( const std::string contentID )
{
	int cnt = 0;

	for( TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
		cnt += tabContains( *i, contentID );

	return cnt;
}

ContentPtr Panel::getContent( const std::string contentID )
{
	int index = 0;
	return getContent( contentID, index );
}

ContentPtr Panel::getContent( const std::string contentID, int& index )
{
	for( TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
	{
		Content* content = (*i)->getContent().getObject();
		if ( !content )
			continue;
		if ( contentID.compare( content->getContentID() ) == 0 )
		{
			if ( index <= 0 )
				return content;
			else
				index--;
		}
		else if ( content->getContentID() == ContentContainer::contentID &&
			((ContentContainer*)content)->contains( contentID ) )
		{
			content = ((ContentContainer*)content)->getContent( contentID, index ).getObject();
			if ( content )
				return content;
			// index already decremented by ContentContainer::getContent
		}
	}

	return 0;
}

void Panel::broadcastMessage( UINT msg, WPARAM wParam, LPARAM lParam )
{
	for( TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
	{
		Content* content = (*i)->getContent().getObject();
		if ( !content )
			continue;

		if ( content->getContentID() == ContentContainer::contentID )
			((ContentContainer*)content)->broadcastMessage( msg, wParam, lParam );
		else
			(*i)->getCWnd()->SendMessage( msg, wParam, lParam );
	}
}

void Panel::clickedTab( void* itemData, int x, int y )
{
	for( TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
	{
		if ( (*i).getObject() == itemData )
		{
			setActiveTab( *i );
			break;
		}
	}

	if ( activeTab_ )
	{
		CRect rect;
		tabBar_->GetWindowRect( &rect );
		Manager::instance().dragManager()->startDrag( rect.left + x, rect.top + y, this, activeTab_ );
	}
}

void Panel::doubleClickedTab( void* itemData, int x, int y )
{
	for( TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
	{
		if ( (*i).getObject() == itemData )
		{
			setActiveTab( *i );
			break;
		}
	}

	if ( activeTab_ )
	{
		Manager::instance().dock()->toggleTabPos( this, activeTab_ );
		setActiveTab( *tabList_.begin() );
	}
}

void Panel::rightClickedTab( void* itemData, int x, int y )
{
	if ( activeTab_ && activeTab_.getObject() == itemData )
		activeTab_->handleRightClick( x, y );
}


//
// private members
//

void Panel::paintCaptionBar()
{
	paintCaptionBarOnly();
	paintCaptionButtons();
}

void Panel::paintCaptionBarOnly()
{
	CBrush brush;
	COLORREF textColor;

	// Draw the caption bar
	if( isActive_ )
	{
		brush.CreateSolidBrush( GetSysColor(COLOR_ACTIVECAPTION) );
		textColor = GetSysColor(COLOR_CAPTIONTEXT);
	}
	else
	{
		brush.CreateSolidBrush( GetSysColor(COLOR_BTNFACE) );
		textColor = GetSysColor(COLOR_BTNTEXT);
	}

	CWindowDC dc(this);

	CRect rect;
	GetWindowRect(&rect);
	
	rect.right = rect.Width();
	rect.top = 0;
	rect.left = 0;
	rect.bottom = rect.top + CAPTION_HEIGHT;
	dc.FillRect(&rect,&brush);

	// Draw caption text
	int oldBkMode = dc.SetBkMode(TRANSPARENT);
	CFont* oldFont = dc.SelectObject( &captionFont_ );
	COLORREF oldColor = dc.SetTextColor( textColor );

	std::string text;
	if ( !isExpanded_ && visibleTabCount() > 1 )
	{
		text = "";
		for( TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
		{
			if ( tabBar_->contains( (*i).getObject() ) )
			{
				if ( text.length() > 0 )
					text = text + ", ";
				text = text + (*i)->getContent()->getTabDisplayString();
			}
		}
	}
	else
	{
		if ( activeTab_ )
			text = activeTab_->getDisplayString();
		else
			text = L("GUITABS/PANEL/NO_TAB_SELECTED");
	}

	CRect tRect = rect;
	tRect.left += CAPTION_LEFTMARGIN;
	tRect.top += CAPTION_TOPMARGIN;
	if ( activeTab_ && activeTab_->isClonable() )
		tRect.right -= CAPTION_HEIGHT*3;
	else
		tRect.right -= CAPTION_HEIGHT*2;
	dc.DrawText( CString( text.c_str() ) , &tRect, DT_SINGLELINE | DT_LEFT | DT_END_ELLIPSIS );

	if ( !isActive_ && tabBar_->itemCount() > 1 ) 
	{
		CPen btnBottomLine( PS_SOLID, 1, isExpanded_?::GetSysColor(COLOR_BTNSHADOW):GetSysColor(COLOR_BTNFACE) );
		CPen* oldPen = dc.SelectObject( &btnBottomLine );
		dc.MoveTo( rect.left, rect.bottom - 1 );
		dc.LineTo( rect.right, rect.bottom - 1 );
		dc.SelectObject( oldPen );
	}

	// Restore old DC objects
	dc.SelectObject( oldFont );
	dc.SetBkMode( oldBkMode );
	dc.SetTextColor( oldColor );
}

void Panel::paintCaptionButtons( UINT hitButton )
{
	CBrush brush;
	CPen pen;

	CWindowDC dc(this);

	if ( isActive_ )
	{
		brush.CreateSolidBrush( GetSysColor(COLOR_ACTIVECAPTION) );
		pen.CreatePen( PS_SOLID, 1, GetSysColor(COLOR_CAPTIONTEXT) );
	}
	else
	{
		brush.CreateSolidBrush( GetSysColor(COLOR_BTNFACE) );
		pen.CreatePen( PS_SOLID, 1, GetSysColor(COLOR_BTNTEXT) );
	}

	// Draw background
	CRect rect;
	GetWindowRect(&rect);
	
	rect.right = rect.Width();
	if ( activeTab_ && activeTab_->isClonable() )
		rect.left = rect.right - CAPTION_HEIGHT*3;
	else
		rect.left = rect.right - CAPTION_HEIGHT*2;
	rect.top = 0;
	rect.bottom = rect.top + CAPTION_HEIGHT - 1;
	dc.FillRect(&rect,&brush);

	// Draw buttons
	CPen* oldPen = dc.SelectObject( &pen );
	rect.top += CAPTION_TOPMARGIN;

	// Draw the "Close" button
	rect.left = rect.right - CAPTION_HEIGHT;
	CRect butRect( rect );
	butRect.DeflateRect( 0, 0, 1, 1 );
	if ( hitButton == BUT_CLOSE )
		if ( buttonDown_ == hitButton )
			dc.Draw3dRect( &butRect, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNHIGHLIGHT) );
		else
			dc.Draw3dRect( &butRect, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_BTNSHADOW) );

	butRect.DeflateRect( 5, 4, 5, 5 );
	dc.MoveTo( butRect.left, butRect.top );
	dc.LineTo( butRect.right, butRect.bottom+1 );
	dc.MoveTo( butRect.left, butRect.bottom );
	dc.LineTo( butRect.right, butRect.top-1 );
	
	// Draw the "Rollup" button
	rect.OffsetRect( -CAPTION_HEIGHT, 0 );
	butRect = rect;
	butRect.DeflateRect( 0, 0, 1, 1 );
	if ( hitButton == BUT_ROLLUP )
		if ( buttonDown_ == hitButton )
			dc.Draw3dRect( &butRect, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNHIGHLIGHT) );
		else
			dc.Draw3dRect( &butRect, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_BTNSHADOW) );

	butRect.DeflateRect( 5, 4, 5, 5 );
	if ( isExpanded_ )
	{
		dc.MoveTo( ( butRect.left + butRect.right ) /2, butRect.top );
		dc.LineTo( butRect.left, butRect.bottom );
		dc.MoveTo( ( butRect.left + butRect.right ) /2, butRect.top );
		dc.LineTo( butRect.right, butRect.bottom );
		dc.MoveTo( butRect.left, butRect.bottom );
		dc.LineTo( butRect.right, butRect.bottom );
	}
	else
	{
		dc.MoveTo( ( butRect.left + butRect.right ) /2, butRect.bottom );
		dc.LineTo( butRect.left, butRect.top );
		dc.MoveTo( ( butRect.left + butRect.right ) /2, butRect.bottom );
		dc.LineTo( butRect.right, butRect.top );
		dc.MoveTo( butRect.left, butRect.top );
		dc.LineTo( butRect.right, butRect.top );
	}
	
	// Draw the "Clone" button
	if ( activeTab_ && activeTab_->isClonable() )
	{
		rect.OffsetRect( -CAPTION_HEIGHT, 0 );
		butRect = rect;
		butRect.DeflateRect( 0, 0, 1, 1 );
		if ( hitButton == BUT_CLONE )
			if ( buttonDown_ == hitButton )
				dc.Draw3dRect( &butRect, ::GetSysColor(COLOR_BTNSHADOW), ::GetSysColor(COLOR_BTNHIGHLIGHT) );
			else
				dc.Draw3dRect( &butRect, ::GetSysColor(COLOR_BTNHIGHLIGHT), ::GetSysColor(COLOR_BTNSHADOW) );
	
		butRect.DeflateRect( 5, 4, 5, 5 );
		dc.MoveTo( butRect.left, ( butRect.top + butRect.bottom ) / 2 );
		dc.LineTo( butRect.right, ( butRect.top + butRect.bottom ) / 2 );
		dc.MoveTo( ( butRect.left + butRect.right ) /2, butRect.bottom );
		dc.LineTo( ( butRect.left + butRect.right ) /2, butRect.top -1 );
	}

	// Restore old DC objects
	dc.SelectObject( oldPen );

}

UINT Panel::hitTest( const CPoint point )
{
	CRect winRect;
	GetWindowRect(&winRect);
	
	int numBut = 0;

	CRect closeRect = winRect;
	closeRect.DeflateRect( winRect.Width() - CAPTION_HEIGHT, 0, 0, winRect.Height() - CAPTION_HEIGHT );
	numBut++;

	CRect rollupRect = closeRect;
	rollupRect.OffsetRect( -CAPTION_HEIGHT, 0 );
	numBut++;

	CRect cloneRect( 0, 0, 0, 0 );
	if ( activeTab_ && activeTab_->isClonable() )
	{
		cloneRect = rollupRect;
		cloneRect.OffsetRect( -CAPTION_HEIGHT, 0 );
		numBut++;
	}

	// deflate caption to exclude buttons
	winRect.DeflateRect( 0, CAPTION_HEIGHT * numBut, 0, 0 );

	// do the hit test.
	UINT ret = HTCAPTION;

	if( closeRect.PtInRect( point ) )
		ret =  BUT_CLOSE;
	else if( rollupRect.PtInRect( point ) )
		ret = BUT_ROLLUP;
	else if( activeTab_ && activeTab_->isClonable() && cloneRect.PtInRect( point ) )
		ret = BUT_CLONE;
	else if ( winRect.PtInRect( point ) )
		ret = HTCLIENT;
	else
		ret = HTCAPTION;

	return ret;
}

void Panel::updateTabBar()
{
	if ( isExpanded_ && tabBar_->itemCount() > 1 )
		tabBar_->ShowWindow( SW_SHOW );
	else
		tabBar_->ShowWindow( SW_HIDE );
}

void Panel::setActiveTab( TabPtr tab )
{
	if ( activeTab_ ) 
		activeTab_->getCWnd()->ShowWindow( SW_HIDE );

	activeTab_ = tab;

	updateTabBar();

	if ( tab )
	{
		if ( isExpanded_ )
			tab->getCWnd()->ShowWindow( SW_SHOW );
		tabBar_->setCurItem( activeTab_.getObject() );
		tab->getCWnd()->SetFocus();
	}

	recalcSize();

	paintCaptionBar();
}

void Panel::recalcSize()
{
	CRect rect;
	GetClientRect( &rect );
	recalcSize( rect.Width(), rect.Height() );
}

void Panel::recalcSize( int w, int h )
{
	int tabBarHeight = 0;
	if ( tabBar_->GetSafeHwnd() )
	{
		tabBar_->SetWindowPos( 0, 0, 0, w, 1, SWP_NOZORDER );
		tabBar_->recalcHeight(); // force a recalc of the height/number of lines, based on the width
		if ( tabBar_->getAlignment() == TabCtrl::TOP )
			tabBar_->SetWindowPos( 0, 0, 0, w, tabBar_->getHeight(), SWP_NOZORDER );
		else
			tabBar_->SetWindowPos( 0, 0, h - tabBar_->getHeight(), w, tabBar_->getHeight(), SWP_NOZORDER );

		if ( tabBar_->itemCount() > 1 )
			tabBarHeight = tabBar_->getHeight() + 3;
	}

	if ( activeTab_ )
	{
		if ( tabBar_->getAlignment() == TabCtrl::TOP )
			activeTab_->getCWnd()->SetWindowPos( 0, 0, tabBarHeight, w, h - tabBarHeight, SWP_NOZORDER );
		else
			activeTab_->getCWnd()->SetWindowPos( 0, 0, 0, w, h - tabBarHeight, SWP_NOZORDER );
	}
}

void Panel::insertTempTab( TabPtr tab )
{
	if ( activeTab_ && isExpanded_ )
		activeTab_->getCWnd()->ShowWindow( SW_HIDE );
	tempTab_ = tab;
	tabBar_->insertItem( tab->getTabDisplayString(), tab->getIcon(), tab.getObject() );
	updateTabBar();
	recalcSize();
	UpdateWindow();
}

void Panel::updateTempTab( int x, int y )
{
	if ( !tempTab_ )
		return;
	CRect rect;
	tabBar_->GetWindowRect( &rect );
	tabBar_->updateItemPosition( tempTab_.getObject(), x - rect.left, y - rect.top );
}

void Panel::removeTempTab()
{
	if ( !tempTab_ )
		return;
	if ( activeTab_ && isExpanded_ )
		activeTab_->getCWnd()->ShowWindow( SW_SHOW );
	tabBar_->removeItem( tempTab_.getObject() );
	if ( activeTab_ )
		tabBar_->setCurItem( activeTab_.getObject() );
	updateTabBar();
	recalcSize();
	UpdateWindow();
	tempTab_ = 0;
}

TabPtr Panel::getActiveTab()
{
	return activeTab_;
}


void Panel::showTab( TabPtr tab, bool show )
{
	TabItr i;
	for( i = tabList_.begin(); i != tabList_.end(); ++i )
		if ( (*i) == tab )
			break;

	if ( i == tabList_.end() )
		return;

	if ( show )
	{
		if ( !tabBar_->contains( tab.getObject() ) )
		{
			tab->setVisible( true );
			tabBar_->insertItem( tab->getTabDisplayString(), tab->getIcon(), tab.getObject() );
		}

		setActiveTab( tab );
	}
	else
	{
		if ( tabBar_->contains( tab.getObject() ) )
		{
			tab->show( false );
			tabBar_->removeItem( tab.getObject() );
		}

		if ( activeTab_ == tab )
		{
			for( i = tabList_.begin(); i != tabList_.end(); ++i )
			{
				if ( tabBar_->contains( (*i).getObject() ) )
				{
					setActiveTab( (*i) );
					break;
				}
			}
		}
	}

	updateTabBar();

	if ( tabBar_->itemCount() == 0 )
		Manager::instance().dock()->showPanel( this, false );
}

void Panel::showTab( const std::string contentID, bool show )
{
	for( TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
	{
		if ( tabContains( *i, contentID ) )
		{
			showTab( *i, show );
			Content* tcontent = (*i)->getContent().getObject();
			if ( tcontent && tcontent->getContentID() == ContentContainer::contentID )
				((ContentContainer*)tcontent)->currentContent( contentID );
		}
	}
}

void Panel::showTab( ContentPtr content, bool show )
{
	for( TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
	{
		if ( tabContains( *i, content ) )
		{
			showTab( *i, show );
			Content* tcontent = (*i)->getContent().getObject();
			if ( tcontent && tcontent->getContentID() == ContentContainer::contentID )
				((ContentContainer*)tcontent)->currentContent( content );
			break;
		}
	}
}

bool Panel::isTabVisible( const std::string contentID )
{
	for( TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
		if ( contentID.compare( (*i)->getContent()->getContentID() ) == 0 )
			return tabBar_->contains( (*i).getObject() );

	return false;
}

ContentPtr Panel::cloneTab( ContentPtr content, int x, int y )
{
	if ( !content )
		return 0;

	content = content->clone(); 

	TabPtr tab = new Tab( this, content );
	tab->show( true );
	addTab( tab );
	CRect rect;
	GetWindowRect( &rect );
	Manager::instance().dock()->dockTab( this, tab, 0, FLOATING,
		rect.left, rect.top, x, y );

	return content;
}

int Panel::tabCount()
{
	return (int)tabList_.size();
}

int Panel::visibleTabCount()
{
	return tabBar_->itemCount();
}

void Panel::updateTabPosition( int x, int y )
{
	if ( !activeTab_ )
		return;

	CRect rect;
	tabBar_->GetWindowRect( &rect );
	tabBar_->updateItemPosition( activeTab_.getObject(), x - rect.left, y - rect.top );
}

int Panel::getTabInsertionIndex( int x, int y )
{
	CRect rect;
	tabBar_->GetWindowRect( &rect );
	return tabBar_->getItemIndex( 0, x - rect.left, y - rect.top );
}

void Panel::setActiveTabIndex( int index )
{
	if ( !activeTab_ )
		return;

	tabBar_->updateItemPosition( activeTab_.getObject(), index );
}

int Panel::getActiveTabIndex()
{
	if ( !activeTab_ )
		return -1;

	return tabBar_->getItemIndex( activeTab_.getObject() );
}


bool Panel::onTabClose( TabPtr tab )
{
	bool doClose = false;

	int cnt = Manager::instance().dock()->getContentCount( tab->getContent()->getContentID() );
	ASSERT( cnt != 0 );

	Content::OnCloseAction response = tab->getContent()->onClose( (cnt == 1) );
	if ( response == Content::CONTENT_KEEP )
	{
		doClose = false;
	}
	else if ( response == Content::CONTENT_HIDE )
	{
		showTab( tab, false );
		doClose = true;
	}
	else if ( response == Content::CONTENT_DESTROY )
	{
		detachTab( tab );
		doClose = true;
	}
	else
		ASSERT( 0 );

	return doClose;
}

bool Panel::onClose()
{
	bool doClose;

	FloaterPtr floater = Manager::instance().dock()->getFloaterByWnd( getCWnd() );
	
	if ( floater )
	{
		// Hack: To avoid that the CFrameWnd floater still points to a deleted
		// panel as its view, which later results in assertion failed/crash.
		floater->SetActiveView( 0 );
	}

	// Hack: needed to avoid that the MainFrame thinks a FormView tab
	// contained in the panel is the active view, which results in
	// windows no longer receiving messages properly for some reason.
	Manager::instance().dock()->getMainFrame()->SetActiveView(
		(CView*)Manager::instance().dock()->getMainView()
		);

	if ( isFloating_ && ( !floater || floater->getRootNode()->getCWnd() == getCWnd() )  )
	{
		doClose = true;

		for( TabItr i = tabList_.begin(); i != tabList_.end(); )
		{
			// save iterator since onTabClose can potentially remove the tab from the list
			TabItr old = i;
			++i;
			if ( !onTabClose( (*old) ) )
				doClose = false;
		}
	}
	else
		doClose = onTabClose( activeTab_ );

	if ( doClose && tabBar_->itemCount() == 0 ) 
		Manager::instance().dock()->showPanel( this, false );

	return doClose;
}

int Panel::getIndex()
{
	return Manager::instance().dock()->getPanelIndex( this );
}


//
// Windows events
//

BEGIN_MESSAGE_MAP(Panel, CWnd)
	ON_WM_NCCALCSIZE()
	ON_WM_PAINT()
	ON_WM_NCPAINT()
	ON_WM_TIMER()
	ON_WM_NCHITTEST()
	ON_WM_NCLBUTTONDOWN()
	ON_WM_NCLBUTTONDBLCLK()
	ON_WM_NCLBUTTONUP()
	ON_WM_NCRBUTTONDOWN()
	ON_WM_NCMOUSEMOVE()
	ON_WM_MOUSEACTIVATE()
	ON_WM_SIZE()
END_MESSAGE_MAP()


void Panel::OnNcCalcSize( BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp )
{
	if( !tabList_.empty() )
		lpncsp->rgrc[0].top += CAPTION_HEIGHT;
}

void Panel::OnNcPaint()
{
	if( !tabList_.empty() )
		paintCaptionBar();
}

void Panel::OnPaint()
{
	// refresh tab names, just in case
	for( TabItr i = tabList_.begin(); i != tabList_.end(); ++i )
		tabBar_->updateItemData( (*i).getObject(), (*i)->getTabDisplayString(), (*i)->getIcon() );
	recalcSize();
	CWnd::OnPaint();
}

void Panel::OnTimer( UINT_PTR nIDEvent )
{
	CPoint point;

	GetCursorPos( &point );

	UINT ht = hitTest( point );

	if ( ht != BUT_CLOSE && ht != BUT_ROLLUP && ht != BUT_CLONE )
	{	
		paintCaptionButtons( ht );
		KillTimer( HOVER_TIMERID );
	}
	else
	{
		SetTimer( HOVER_TIMERID, HOVER_TIMERMILLIS, 0 );
	}
}

HITTESTRESULT Panel::OnNcHitTest( CPoint point )
{
	HITTESTRESULT ht = hitTest( point );

	if ( ht == BUT_CLOSE || ht == BUT_ROLLUP || ht == BUT_CLONE )
	{	
		paintCaptionButtons( ht );
		SetTimer( HOVER_TIMERID, HOVER_TIMERMILLIS, 0 );
	}

	return ht;
}

void Panel::OnNcLButtonDown( UINT nHitTest, CPoint point )
{
	buttonDown_ = nHitTest;

	switch ( nHitTest )
	{
	case HTCAPTION:
		activate();
		paintCaptionButtons( nHitTest );
		Manager::instance().dragManager()->startDrag( point.x, point.y, this, 0 );
		return;
	}
	paintCaptionButtons( nHitTest );
}

void Panel::OnNcLButtonUp( UINT nHitTest, CPoint point )
{
	int lastBut = buttonDown_;
	buttonDown_ = 0;
	paintCaptionButtons( nHitTest );

	switch ( nHitTest )
	{
	case BUT_CLOSE:
		if ( lastBut != nHitTest ) break;
		if ( onClose() )
		{
			if ( tabCount() == 0 ) 
				Manager::instance().dock()->removePanel( this );
			return;
		}
		break;

	case BUT_ROLLUP:
		if ( lastBut != nHitTest ) break;
		setExpanded( !isExpanded_ );
		SetFocus();
		activate();
		break;

	case BUT_CLONE:
		if ( lastBut != nHitTest ) break;
		if ( activeTab_ )
		{
			CRect rect;
			GetWindowRect( &rect );
			// position is hand-hacked, but should work well in all cases
			cloneTab(
				activeTab_->getContent(),
				( rect.left + 10 ) % (GetSystemMetrics( SM_CXMAXIMIZED ) - 64),
				( rect.top ) % (GetSystemMetrics( SM_CYMAXIMIZED ) - 64) );
		}
		break;
	}
}

void Panel::OnNcLButtonDblClk( UINT nHitTest, CPoint point )
{
	if ( nHitTest == HTCAPTION )
	{
		Manager::instance().dock()->togglePanelPos( this );
	}
}

void Panel::OnNcRButtonDown( UINT nHitTest, CPoint point )
{
	if ( nHitTest == HTCAPTION && activeTab_ )
		activeTab_->handleRightClick( point.x, point.y );
}

void Panel::OnNcMouseMove( UINT nHitTest, CPoint point )
{
	if ( buttonDown_ )
	{
		short mouseButton;

		if ( GetSystemMetrics( SM_SWAPBUTTON ) )
			mouseButton = GetAsyncKeyState( VK_RBUTTON );
		else
			mouseButton = GetAsyncKeyState( VK_LBUTTON );

		if ( mouseButton >= 0 )
			buttonDown_ = 0;
	}
}

int Panel::OnMouseActivate( CWnd* pDesktopWnd, UINT nHitTest, UINT message )
{
	if ( Manager::instance().dock()->getMainFrame() )
	{
		if ( isFloating_ )
			Manager::instance().dock()->getMainFrame()->SetForegroundWindow();
	}
	if ( nHitTest == HTCAPTION || ( !isActive_ && nHitTest == HTCLIENT ) )
	{
		activate();
		if ( Manager::instance().dock()->getMainFrame() )
		{
			// Hack: needed to avoid that the MainFrame thinks a FormView tab
			// contained in the panel is the active view, which results in
			// windows no longer receiving messages properly for some reason.
			Manager::instance().dock()->getMainFrame()->SetActiveView(
				(CView*)Manager::instance().dock()->getMainView()
				);
		}
		return MA_ACTIVATE;
	}

	return CWnd::OnMouseActivate( pDesktopWnd, nHitTest, message );
}

void Panel::OnSize( UINT nType, int cx, int cy )
{
	CWnd::OnSize( nType, cx, cy );
	recalcSize( cx, cy );
}




}	// namespace