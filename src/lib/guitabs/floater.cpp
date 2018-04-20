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
 *	GUI Tearoff panel framework - Floater class implementation
 */



#include "pch.hpp"
#include "guitabs.hpp"


namespace GUITABS
{


static const int MIN_VERTICAL_SIZE = 21;
static const int ROLLEDUP_SIZE = 16;


Floater::Floater( CWnd* parentWnd ) :
	dockTreeRoot_( 0 ),
	lastRollupSize_( 0 )
{
	Create(
		0,
		"",
		WS_POPUP|WS_THICKFRAME|MFS_SYNCACTIVE|WS_CAPTION|WS_SYSMENU,
		CRect( 0, 0, 100, 100 ),
		parentWnd,
		0
		);
}

Floater::~Floater()
{
	DestroyWindow();
}

void Floater::validatePos( int& posX, int& posY, int& width, int& height )
{
	HMONITOR mon = MonitorFromRect(
		CRect( posX, posY, posX + width, posY + height ),
		MONITOR_DEFAULTTONEAREST );
	MONITORINFO mi;
	mi.cbSize = sizeof( MONITORINFO );
	GetMonitorInfo( mon, &mi );

	if ( posX + width < mi.rcWork.left )
		posX = mi.rcWork.left;
	if ( posX > mi.rcWork.right )
		posX = mi.rcWork.right - width;

	if ( posY + height < mi.rcWork.top )
		posY = mi.rcWork.top;
	if ( posY > mi.rcWork.bottom )
		posY = mi.rcWork.bottom - height;
}

bool Floater::load( DataSectionPtr section )
{
	if ( !section )
		return false;

	int posX = section->readInt( "posX", 300 );
	int posY = section->readInt( "posY", 200 );
	int width = section->readInt( "width", 300 );
	int height = section->readInt( "height", 400 );
	lastRollupSize_ = section->readInt( "lastRollupSize", 0 );

	validatePos( posX, posY, width, height );

	SetWindowPos( 0, posX, posY, width, height, SWP_NOZORDER );

	DockNodePtr node = Manager::instance().dock()->nodeFactory( section );
	if ( !node )
		return false;

	if ( !node->load( section, this, AFX_IDW_PANE_FIRST ) )
		return false;

	dockTreeRoot_ = node;
	dockTreeRoot_->recalcLayout();
	RecalcLayout();

	ShowWindow( SW_SHOW );

	updateStyle();

	return true;
}

bool Floater::save( DataSectionPtr section )
{
	if ( !section )
		return false;

	CRect rect( 0, 0, 200, 200 );
	GetWindowRect( &rect );

	section->writeInt( "posX", rect.left );
	section->writeInt( "posY", rect.top );
	section->writeInt( "width", rect.Width() );
	section->writeInt( "height", rect.Height() );
	section->writeInt( "lastRollupSize", lastRollupSize_ );

	if ( !dockTreeRoot_->save( section ) )
		return false;

	return true;
}

CWnd* Floater::getCWnd()
{
	return this;
}

DockNodePtr Floater::getRootNode()
{
	return dockTreeRoot_;
}

void Floater::setRootNode( DockNodePtr node )
{
	dockTreeRoot_ = node;

	if ( node )
	{
		node->setParentWnd( this );
		node->getCWnd()->SetDlgCtrlID( AFX_IDW_PANE_FIRST );
		node->getCWnd()->ShowWindow( SW_SHOW );

		updateStyle();

		RecalcLayout();
	}
}

void Floater::updateStyle()
{
	int count = 0;

	countVisibleNodes( dockTreeRoot_, count );

	if ( count == 1 )
		ModifyStyle( WS_CAPTION|WS_SYSMENU, 0, SWP_NOSIZE|SWP_NOMOVE|SWP_NOZORDER|SWP_FRAMECHANGED|SWP_DRAWFRAME );
	else
		ModifyStyle( 0, WS_CAPTION|WS_SYSMENU, SWP_NOSIZE|SWP_NOMOVE|SWP_NOZORDER|SWP_FRAMECHANGED|SWP_DRAWFRAME );
}

int Floater::getLastRollupSize()
{
	return lastRollupSize_;
}

void Floater::setLastRollupSize( int size )
{
	lastRollupSize_ = size;
}

void Floater::adjustSize( bool rollUp )
{
	int w = 0;
	int h = 0;
	dockTreeRoot_->getPreferredSize( w, h );
	if ( rollUp )
	{
		if ( h > ROLLEDUP_SIZE )
		{
			int fh = getLastRollupSize();
			if ( fh )
				h = fh - GetSystemMetrics( SM_CYFIXEDFRAME ) * 2;
		}
		else
		{
			CRect rect;
			GetWindowRect( &rect );
			setLastRollupSize( rect.Height() );
		}
		CRect rect;
		GetWindowRect( &rect );
		w = rect.Width();
	}
	else
	{
		w += GetSystemMetrics( SM_CXFIXEDFRAME ) * 2;
		setLastRollupSize( 0 );
	}
	if ( !dockTreeRoot_->isLeaf() )
		h += GetSystemMetrics( SM_CYCAPTION );
	h += GetSystemMetrics( SM_CYFIXEDFRAME ) * 2;
	SetWindowPos( 0, 0, 0, w, h, SWP_NOMOVE|SWP_NOZORDER );
}

void Floater::countVisibleNodes( DockNodePtr node, int& count )
{
	if ( node->isLeaf() )
	{
		if ( node->isVisible() )
			count++;
	}
	else
	{
		countVisibleNodes( node->getLeftChild(), count );
		countVisibleNodes( node->getRightChild(), count );
	}
}


void Floater::PostNcDestroy()
{
	// do nothing. The standard behaviour is "delete this", which conflicts with smartpointers.
}

BEGIN_MESSAGE_MAP(Floater,CMiniFrameWnd)
	ON_WM_CLOSE()
	ON_WM_SIZING()
END_MESSAGE_MAP()


bool Floater::onClosePanels( DockNodePtr node )
{
	if ( node->isLeaf() )
	{
		PanelPtr panel = Manager::instance().dock()->getPanelByWnd( node->getCWnd() );
		if ( panel )
			return panel->onClose();
		else
			return true;
	}

	bool l = onClosePanels( node->getLeftChild() );
	bool r = onClosePanels( node->getRightChild() );

	return l && r;
}

void Floater::OnClose()
{
	if ( !onClosePanels( dockTreeRoot_ ) )
		return;

	Manager::instance().dock()->destroyFloater( this );
}

void Floater::OnSizing( UINT nSide, LPRECT rect )
{
	// control the size of a floater, specially if it's rolled up
	int w = 0;
	int h = 0;
	dockTreeRoot_->getPreferredSize( w, h );

	int count = 0;
	countVisibleNodes( dockTreeRoot_, count );
	int minH;
	if ( dockTreeRoot_ && !dockTreeRoot_->isExpanded() )
		minH = h + GetSystemMetrics( SM_CYFIXEDFRAME ) * 2;
	else
		minH = MIN_VERTICAL_SIZE;
	if ( count > 1 )
		minH += GetSystemMetrics( SM_CYCAPTION );

	if ( rect && (
		rect->bottom - rect->top - 1 < minH ||
		( dockTreeRoot_ && !dockTreeRoot_->isExpanded() ) ) )
		if ( nSide == WMSZ_TOP || nSide == WMSZ_TOPLEFT || nSide == WMSZ_TOPRIGHT )
			rect->top = rect->bottom - minH - 1;
		else
			rect->bottom = rect->top + minH + 1;
	CWnd::OnSizing( nSide, rect );
}


}	// namespace
