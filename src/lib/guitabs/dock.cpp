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
 *	GUI Tearoff panel framework - Dock class implementation
 */


#include "pch.hpp"
#include "guitabs.hpp"


namespace GUITABS
{


Dock::Dock( CFrameWnd* mainFrame, CWnd* mainView ) :
	dockVisible_( true ),
	dockTreeRoot_( 0 ),
	mainFrame_( mainFrame ),
	mainView_( mainView )
{
	originalMainViewID_ = mainView_->GetDlgCtrlID();
	dockTreeRoot_ = new MainViewNode( mainView );
}

Dock::~Dock()
{
	showDock( false );

	// dettach the mainView_ window from the dock tree, and put it as a child
	// of mainFrame_. Also sets the active view so we don't get any strange MFC
	// asserts on exit.
	if ( IsWindow( mainView_->GetSafeHwnd() ) && IsWindow( mainFrame_->GetSafeHwnd() ) )
	{
		mainView_->SetDlgCtrlID( originalMainViewID_ );
		mainView_->SetParent( mainFrame_ );
		mainView_->SetFocus();
	}
	if ( IsWindow( mainFrame_->GetSafeHwnd() ) )
	{
		mainFrame_->SetActiveView( 0 );
		mainFrame_->RecalcLayout();
	}

	// destroy tree in leaf to parent order, to avoid MFC destroy calls on
	// children when the parent is destroyed.
	dockTreeRoot_->destroy();
	for(FloaterItr i = floaterList_.begin(); i != floaterList_.end(); ++i )
	{
		(*i)->getRootNode()->destroy();
	}
}

bool Dock::empty()
{
	return panelList_.empty();
}

CFrameWnd* Dock::getMainFrame()
{
	return mainFrame_;
}

CWnd* Dock::getMainView()
{
	return mainView_;
}

DockNodePtr Dock::getNodeByPoint( int x, int y )
{
	DockNodePtr ret = 0;
	
	for( FloaterItr i = floaterList_.begin(); i != floaterList_.end() && !ret; ++i )
	{
		if ( (*i)->IsWindowVisible() &&
			::IsChild( (*i)->getCWnd()->GetSafeHwnd(), ::WindowFromPoint( CPoint( x, y ) ) ) )
		{
			ret = (*i)->getRootNode()->getNodeByPoint( x, y );
			if ( ret )
				break;
		}
	}

	if ( ! ret )
		ret = dockTreeRoot_->getNodeByPoint( x, y );

	return ret;
}

void Dock::dockTab( PanelPtr panel, TabPtr tab, CWnd* destPanel, InsertAt insertAt, int srcX, int srcY, int dstX, int dstY )
{
	CRect rect;
	panel->GetWindowRect( &rect );

	panel->detachTab( tab );

	PanelPtr newPanel = new Panel( mainFrame_ );
	panelList_.push_back( newPanel );

	newPanel->addTab( tab );

	newPanel->setLastPos( rect.left, rect.top );

	CRect mainRect;
	mainFrame_->GetWindowRect( &mainRect );
	rect.OffsetRect( -mainRect.left, -mainRect.top );
	newPanel->SetWindowPos( 0, rect.left, rect.top, 0, 0, SWP_NOSIZE|SWP_NOZORDER );

	copyPanelRestorePosToTab( panel, newPanel );

	dockPanel( newPanel, destPanel, insertAt, srcX, srcY, dstX, dstY );

	newPanel->activate();
}

void Dock::dockPanel( PanelPtr panel, CWnd* destPanel, InsertAt insertAt, int srcX, int srcY, int dstX, int dstY )
{
	if ( !panel ) 
		return;

	if ( insertAt == FLOATING )
	{
		floatPanel( panel, srcX, srcY, dstX, dstY );
	}
	else if ( destPanel )
	{
		insertPanelIntoPanel( panel, destPanel, insertAt );
	}
}

void Dock::floatPanel( PanelPtr panel, int srcX, int srcY, int dstX, int dstY )
{
	CRect rect;
	panel->GetWindowRect( &rect );
	dstX = dstX - ( srcX - rect.left );
	dstY = dstY - ( srcY - rect.top );

	int w = rect.Width() + GetSystemMetrics( SM_CXFIXEDFRAME  ) * 2;
	int h = rect.Height() + GetSystemMetrics( SM_CYFIXEDFRAME  ) * 2;

	if ( !panel->isFloating() )
	{
		showDock( true );
		panel->getPreferredSize( w, h );
		w += GetSystemMetrics( SM_CXFIXEDFRAME  ) * 2;
		h += GetSystemMetrics( SM_CYFIXEDFRAME  ) * 2;
	}

	Floater::validatePos( dstX, dstY, w, h );

	if ( panel->isFloating() )
	{
		FloaterPtr floater = getFloaterByWnd( panel->getCWnd() );

		if ( floater && floater->getRootNode()->isLeaf() )
		{
			floater->SetWindowPos( 0, dstX, dstY, 0, 0, SWP_NOZORDER|SWP_NOSIZE );
			floater->ShowWindow( SW_SHOW );
			return;
		}
	}

	panel->setLastPos( dstX, dstY );

	removeNodeByWnd( panel->getCWnd() );

	FloaterPtr floater = new Floater( mainFrame_ );
	floater->SetWindowPos( 0, dstX, dstY, w, h, SWP_NOZORDER );
	floaterList_.push_back( floater );

	DockedPanelNodePtr panelNode = new DockedPanelNode( panel );

	floater->setRootNode( panelNode );

	panelNode->setParentWnd( floater->getCWnd() );
	panelNode->getCWnd()->SetDlgCtrlID( AFX_IDW_PANE_FIRST );
	floater->RecalcLayout();

	panel->setFloating( true );

	floater->ShowWindow( SW_SHOW );

	// update current docking positions
	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
		savePanelDockPos( *i );
}

void Dock::attachAsTab( PanelPtr panel, CWnd* destPanel )
{
	TabPtr tab;
	PanelPtr dest = 0;

	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
	{
		if ( (*i)->getCWnd() == destPanel )
		{
			dest = (*i);
			break;
		}
	}

	if ( !dest )
		return;

	while ( tab = panel->detachFirstTab() )
		dest->addTab( tab );

	dest->activate();

	removePanel( panel );
}

bool Dock::isDockVisible()
{
	return dockVisible_;
}

void Dock::showDock( bool show )
{
	if ( dockVisible_ == show )
		return;

	dockVisible_ = show;

	if ( dockTreeRoot_->getCWnd() == mainView_ )
		return;

	// show/hide mainFrame docked panels
	CWnd* wnd = dockTreeRoot_->getCWnd();

	if ( show )
	{
		DockNodePtr node;
		DockNodePtr parent;
		getNodeByWnd( mainView_, node, parent );

		int id = 0;
		int side = 0;
		if ( parent->getRightChild()->getCWnd() == mainView_ )
			side = 1;

		if ( parent->getSplitOrientation() == HORIZONTAL )
			id = ((CSplitterWnd*)parent->getCWnd())->IdFromRowCol( 0, side );
		else
			id = ((CSplitterWnd*)parent->getCWnd())->IdFromRowCol( side, 0 );

		mainView_->SetDlgCtrlID( id );

		mainView_->SetParent( parent->getCWnd() );

		wnd->SetDlgCtrlID( originalMainViewID_ );
		dockTreeRoot_->setParentWnd( mainFrame_ );
		dockTreeRoot_->recalcLayout();
		wnd->ShowWindow( SW_SHOW );
	}
	else
	{
		wnd->SetDlgCtrlID( 0 );
		dockTreeRoot_->setParentWnd( 0 );
		wnd->ShowWindow( SW_HIDE );

		mainView_->SetDlgCtrlID( originalMainViewID_ );
		mainView_->SetParent( mainFrame_ );
		mainView_->SetFocus();
	}
	mainFrame_->RecalcLayout();
}

void Dock::showFloaters( bool show )
{
	for( FloaterItr i = floaterList_.begin(); i != floaterList_.end(); ++i )
		(*i)->ShowWindow( show?SW_SHOW:SW_HIDE );
}


void Dock::insertPanelIntoPanel( PanelPtr panel, CWnd* destPanel, InsertAt insertAt )
{
	if ( !panel || !destPanel )
		return;

	showDock( true );

	// remove panel from it's old docking position
	removeNodeByWnd( panel->getCWnd() );

	// find the destination panel's node and parent node
	DockNodePtr childNode = 0;
	DockNodePtr parentNode = 0;

	getNodeByWnd( destPanel, childNode, parentNode );

	if ( !childNode )
		return;

	// try to find out if it's in a floating window, and return the floater
	FloaterPtr floater = getFloaterByWnd( destPanel );

	// we now know destPanel is valid, so if it's tab, insert it in
	if ( insertAt == TAB )
	{
		attachAsTab( panel, destPanel );
		getNodeByWnd( destPanel, childNode, parentNode );
		if ( floater )
		{
			if ( !floater->getRootNode()->isLeaf() && childNode )
			{
				floater->RecalcLayout();
				floater->getRootNode()->adjustSizeToNode( childNode, false );
				floater->getRootNode()->recalcLayout();
				floater->RecalcLayout();
			}
		}
		else
		{
			if ( !dockTreeRoot_->isLeaf() && childNode )
			{
				mainFrame_->RecalcLayout();
				dockTreeRoot_->adjustSizeToNode( childNode, false );
				dockTreeRoot_->recalcLayout();
				mainFrame_->RecalcLayout();
			}
		}
		return;
	}

	// create a new splitter to insert the panel in
	Orientation dir;

	if ( insertAt == TOP || insertAt == BOTTOM )
		dir = VERTICAL;
	else
		dir = HORIZONTAL;

	CWnd* parentWnd;

	if ( parentNode )
		parentWnd = parentNode->getCWnd();
	else
	{
		if ( floater )
			parentWnd = floater->getCWnd();
		else
			parentWnd = mainFrame_;
	}

	CRect destRect;
	int destID;

	destPanel->GetWindowRect( &destRect );
	destID = destPanel->GetDlgCtrlID();

	SplitterNodePtr newSplitter = new SplitterNode( dir, parentWnd, destID );
	DockedPanelNodePtr newNode = new DockedPanelNode( panel );

	int w;
	int h;

	panel->getPreferredSize( w, h );

	// set the splitter's childs
	int leftChildSize = 0;
	int rightChildSize = 0;

	if ( insertAt == LEFT || insertAt == TOP ) 
	{
		newSplitter->setLeftChild( newNode );
		newSplitter->setRightChild( childNode );
		if ( dir == VERTICAL )
			leftChildSize = h;
		else
			leftChildSize = w;
	}
	else
	{
		newSplitter->setLeftChild( childNode );
		newSplitter->setRightChild( newNode );
		if ( dir == VERTICAL )
			rightChildSize = h;
		else
			rightChildSize = w;
	}

	// set the splitter's parent
	if ( parentNode )
	{
		if ( parentNode->getLeftChild() == childNode )
		{
			parentNode->setLeftChild( newSplitter );
		}
		else
		{
			parentNode->setRightChild( newSplitter );
		}
	}
	else
	{
		if ( floater )
		{
			floater->setRootNode( newSplitter );
		}
		else
			dockTreeRoot_ = newSplitter;
	}

	// finish splitter window required operations
	newSplitter->finishInsert( &destRect, leftChildSize, rightChildSize );

	// recalc layout, needed by splitter, miniframe and frame windows
	if ( floater )
	{
		floater->ShowWindow( SW_SHOW );

		panel->setFloating( true );
		floater->adjustSize();
		floater->getRootNode()->recalcLayout();
		floater->RecalcLayout();
	}
	else
	{
		panel->setFloating( false );
		dockTreeRoot_->recalcLayout();
		mainFrame_->RecalcLayout();
		dockTreeRoot_->adjustSizeToNode( newNode );
		dockTreeRoot_->recalcLayout();
		mainFrame_->RecalcLayout();
	}

	// update current docking positions
	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
		savePanelDockPos( *i );
}

bool Dock::getNodeByWnd( CWnd* ptr, DockNodePtr& childNode, DockNodePtr& parentNode )
{
	childNode = 0;
	parentNode = 0;

	dockTreeRoot_->getNodeByWnd( ptr, childNode, parentNode );

	FloaterItr i = floaterList_.begin();
	while( i != floaterList_.end() && !childNode )
		(*i++)->getRootNode()->getNodeByWnd( ptr, childNode, parentNode );

	return !!childNode;
}

FloaterPtr Dock::getFloaterByWnd( CWnd* ptr )
{
	DockNodePtr childNode = 0;
	DockNodePtr parentNode = 0;

	for( FloaterItr i = floaterList_.begin(); i != floaterList_.end() && !childNode; ++i )
	{
		(*i)->getRootNode()->getNodeByWnd( ptr, childNode, parentNode );
		if ( childNode )
			return *i;
	}

	return 0;
}

void Dock::removeNodeByWnd( CWnd* ptr )
{
	DockNodePtr childNode = 0;
	DockNodePtr parentNode = 0;
	FloaterPtr floater;

	getNodeByWnd( ptr, childNode, parentNode );
	floater = getFloaterByWnd( ptr );

	if ( floater )
	{
		// Hack: To avoid that the CFrameWnd floater still points to a deleted
		// panel as its view, which later results in assertion failed/crash.
		floater->SetActiveView( 0 );
	}

	if ( !childNode )
		return; // node not found, it's already removed from trees

	childNode->getCWnd()->ShowWindow( SW_HIDE );
	childNode->setParentWnd( 0 );
	childNode->getCWnd()->SetWindowPos( 0, 0, 0, 0, 0, SWP_NOZORDER|SWP_NOSIZE );

	// set last floating position
	if ( floater )
	{
		CRect rect;
		floater->GetWindowRect( &rect );
		PanelPtr panel = getPanelByWnd( ptr );
		if ( panel )
			panel->setLastPos( rect.left, rect.top );
	}

	if ( !parentNode )
	{
		if ( floater )
		{
			for( FloaterItr i = floaterList_.begin(); i != floaterList_.end(); ++i )
			{
				if ( (*i) == floater )
				{
					floaterList_.erase( i );
					break;
				}
			}
		}
		return;
	}

	DockNodePtr grandParentNode = 0;

	getNodeByWnd( parentNode->getCWnd(), parentNode, grandParentNode );

	if ( !parentNode )
		return; // at this point, it should always find the parentNode

	DockNodePtr otherChildNode;

	parentNode->setParentWnd( 0 );

	if ( parentNode->getLeftChild() == childNode )
		otherChildNode = parentNode->getRightChild();
	else
		otherChildNode = parentNode->getLeftChild();

	if ( grandParentNode )
	{
		if ( grandParentNode->getLeftChild() == parentNode )
			grandParentNode->setLeftChild( otherChildNode );
		else
			grandParentNode->setRightChild( otherChildNode );
	}
	else
	{
		int id = parentNode->getCWnd()->GetDlgCtrlID();
		otherChildNode->getCWnd()->SetDlgCtrlID( id );
		otherChildNode->setParentWnd( mainFrame_ );

		if ( floater )
			floater->setRootNode( otherChildNode );
		else
			dockTreeRoot_ = otherChildNode;
	}

	if ( floater )
	{
		floater->getRootNode()->recalcLayout();
		floater->adjustSize();
		floater->RecalcLayout();
	}
	else
	{
		dockTreeRoot_->recalcLayout();
		dockTreeRoot_->adjustSizeToNode( otherChildNode );
		mainFrame_->RecalcLayout();
	}
}


PanelPtr Dock::insertPanel( const std::string contentID, PanelHandle destPanel, InsertAt insertAt )
{
	DockNodePtr childNode = 0;
	DockNodePtr parentNode = 0;

	if ( insertAt == FLOATING )
		destPanel = 0;

	PanelPtr dest = getPanelByHandle( destPanel );

	if ( destPanel )
	{
		if ( !dest )
			destPanel = 0;
		else
			showPanel( dest, true );
	}

	if ( !destPanel )
	{
		if ( insertAt == TAB )
		{
			insertAt = FLOATING;
		}
		else if ( insertAt != FLOATING )
		{
			getNodeByWnd( mainView_, childNode, parentNode );

			if ( !childNode )
				insertAt = FLOATING;
		}
	}

	if ( insertAt == SUBCONTENT && !!destPanel && !!dest )
	{
		ContentContainerPtr cc = (ContentContainer*)destPanel;
		cc->addContent( contentID );

		FloaterPtr floater = getFloaterByWnd( dest->getCWnd() );
		getNodeByWnd( dest->getCWnd(), childNode, parentNode );

		if ( floater )
		{
			floater->getRootNode()->adjustSizeToNode( childNode );
			floater->getRootNode()->recalcLayout();
			floater->RecalcLayout();
			floater->adjustSize( true );
		}
		else
		{
			dockTreeRoot_->adjustSizeToNode( childNode );
			dockTreeRoot_->recalcLayout();
			mainFrame_->RecalcLayout();
		}
		return dest;
	}

	PanelPtr panel = new Panel( mainFrame_ );
	panelList_.push_back( panel );

	panel->addTab( contentID );

	if ( dest )
	{
		insertPanelIntoPanel( panel, dest->getCWnd(), insertAt );

		if ( insertAt == TAB )
			return getPanelByHandle( destPanel );
		else
			return panel;
	}
	else if ( insertAt == FLOATING )
	{
		floatPanel( panel, 0, 0, 300, 200 );
		return panel;
	}

	CWnd* destWnd = 0;

	DockNodePtr node = dockTreeRoot_;

	if ( insertAt == LEFT )
	{
		while( !node->isLeaf() ) node = node->getLeftChild();

		if ( node != childNode )
			insertAt = BOTTOM;

		destWnd = node->getCWnd();
	}
	else if ( insertAt == RIGHT )
	{
		while( !node->isLeaf() ) node = node->getRightChild();

		if ( node != childNode )
			insertAt = TOP;

		destWnd = node->getCWnd();
	}
	else if ( childNode )
	{
		destWnd = childNode->getCWnd();
	}

	dockPanel( panel, destWnd, insertAt, 0, 0, 0, 0 );

	return panel;
}

const PanelItr Dock::removePanel( PanelPtr panel )
{
	removeNodeByWnd( panel->getCWnd() );

	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
		if ( (*i) == panel )
			return panelList_.erase( i );

	return panelList_.end();
}

void Dock::removePanel( const std::string contentID )
{
	for( PanelItr i = panelList_.begin(); i != panelList_.end(); )
	{
		(*i)->detachTab( contentID );
		if ( (*i)->tabCount() == 0 ) 
			i = removePanel( *i );
		else
			++i;
	}
}

PanelPtr Dock::getPanelByWnd( CWnd* ptr )
{
	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
	{
		if ( (*i)->getCWnd() == ptr )
		{
			return *i;
		}
	}
	return 0;
}

PanelPtr Dock::getPanelByHandle( PanelHandle handle )
{
	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
	{
		if ( (*i)->contains( handle ) )
		{
			return *i;
		}
	}
	return 0;
}

DockNodePtr Dock::nodeFactory( DataSectionPtr section )
{
	if ( !section )
		return 0;

	if ( !!section->openSection( "MainView" ) )
		return new MainViewNode( mainView_ );
	else if ( !!section->openSection( "DockedPanel" ) )
		return new DockedPanelNode();
	else if ( !!section->openSection( "Splitter" ) )
		return new SplitterNode();

	return false;
}

bool Dock::load( DataSectionPtr section )
{
	if ( !section )
		return false;

	std::vector<DataSectionPtr> sections;
	section->openSections( "Panel", sections );
	if ( sections.empty() )
		return false;
	for( std::vector<DataSectionPtr>::iterator i = sections.begin(); i != sections.end(); ++i )
	{
		PanelPtr newPanel = new Panel( mainFrame_ );
		if ( !newPanel->load( *i ) )
			return false;
		panelList_.push_back( newPanel );
	}

	DataSectionPtr treeSec = section->openSection( "Tree" );
	if ( !treeSec )
		return false;

	DockNodePtr node = nodeFactory( treeSec );
	if ( !node )
		return false;

	if ( !node->load( treeSec, mainFrame_, originalMainViewID_ ) )
		return false;

	dockTreeRoot_ = node;
	dockTreeRoot_->recalcLayout();
	mainFrame_->RecalcLayout();

	sections.clear();
	section->openSections( "Floater", sections );
	for( std::vector<DataSectionPtr>::iterator i = sections.begin(); i != sections.end(); ++i )
	{
		FloaterPtr newFloater = new Floater( mainFrame_ );
		if ( !newFloater->load( *i ) )
			return false;
		floaterList_.push_back( newFloater );
	}

	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
	{
		savePanelDockPos( *i );
		if ( getFloaterByWnd( (*i)->getCWnd() ) )
			(*i)->setFloating( true );
	}

	showDock( section->readBool( "visible", dockVisible_ ) );
	return true;
}

bool Dock::save( DataSectionPtr section )
{
	if ( !section )
		return false;

	section->writeBool( "visible", dockVisible_ );

	section->deleteSections( "Panel" );
	section->deleteSections( "Tree" );
	section->deleteSections( "Floater" );

	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
		if ( !(*i)->save( section->newSection( "Panel" ) ) )
			return false;

	if ( !dockTreeRoot_->save( section->newSection( "Tree" ) ) )
		return false;

	for( FloaterItr i = floaterList_.begin(); i != floaterList_.end(); ++i )
		if ( !(*i)->save( section->newSection( "Floater" ) ) )
			return false;

	return true;
}

void Dock::setActivePanel( PanelPtr panel )
{
	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
	{
		if ( panel != (*i) )
		{
			(*i)->deactivate();
		}
	}
}

void Dock::showPanel( PanelPtr panel, bool show )
{
	if ( show == false )
	{
		removeNodeByWnd( panel->getCWnd() );
	}
	else
	{
		restorePanelDockPos( panel );
		panel->activate();
	}
}

void Dock::showPanel( ContentPtr content, bool show )
{
	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
	{
		if ( (*i)->contains( content ) )
		{
			if ( show )
				showPanel( *i, show );
			(*i)->showTab( content, show );
			if ( show )
				(*i)->activate();
			break;
		}
	}
}

void Dock::showPanel( const std::string contentID, bool show )
{
	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
	{
		if ( (*i)->contains( contentID ) )
		{
			if ( show )
				showPanel( *i, show );
			(*i)->showTab( contentID, show );
			if ( show )
				(*i)->activate();
		}
	}
}

ContentPtr Dock::getContent( const std::string contentID, int index )
{
	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
		if ( (*i)->contains( contentID ) )
		{
			ContentPtr content = (*i)->getContent( contentID, index );
			if ( content )
				return content;
		}

	return 0;
}

bool Dock::isContentVisible( const std::string contentID )
{
	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
	{
		if ( (*i)->contains( contentID ) )
		{
			DockNodePtr node;
			DockNodePtr parent;
			getNodeByWnd( (*i)->getCWnd(), node, parent );

			if ( node )
			{
				if ( (*i)->isFloating() )
				{
					FloaterPtr floater = getFloaterByWnd( (*i)->getCWnd() );

					if ( !floater || !floater->IsWindowVisible() )
						return false;
				}

				if ( (*i)->isTabVisible( contentID ) )
					return true;
			}
		}
	}
	return false;
}



void Dock::getLeaves( DockNodePtr node, std::vector<DockNodePtr>& leaves )
{
	if ( node->isLeaf() )
	{
		leaves.push_back( node );
		return;
	}

	getLeaves( node->getLeftChild(), leaves );
	getLeaves( node->getRightChild(), leaves );
}

bool Dock::buildPanelPosList( bool docked, DockNodePtr node, PanelPtr panel )
{
	if ( node->isLeaf() )
	{
		if ( node->getCWnd() == panel->getCWnd() )
			return true;
		else 
			return false;
	}

	bool inLeft = buildPanelPosList( docked, node->getLeftChild(), panel );
	bool inRight = buildPanelPosList( docked, node->getRightChild(), panel );

	if ( inLeft )
	{
		std::vector<DockNodePtr> leaves;
		getLeaves( node, leaves );

		for( std::vector<DockNodePtr>::iterator i = leaves.begin(); i != leaves.end(); ++i )
		{
			InsertAt ins;

			if ( node->getSplitOrientation() == HORIZONTAL )
				ins = LEFT;
			else
				ins = TOP;

			panel->insertPos( docked, PanelPos( ins, (*i)->getCWnd() ) );
		}
	}
	else if ( inRight )
	{
		std::vector<DockNodePtr> leaves;
		getLeaves( node, leaves );

		for( std::vector<DockNodePtr>::reverse_iterator i = leaves.rbegin(); i != leaves.rend(); ++i )
		{
			InsertAt ins;

			if ( node->getSplitOrientation() == HORIZONTAL )
				ins = RIGHT;
			else
				ins = BOTTOM;

			panel->insertPos( docked, PanelPos( ins, (*i)->getCWnd() ) );
		}
	}

	return inLeft || inRight;
}

void Dock::savePanelDockPos( PanelPtr panel )
{
	FloaterPtr floater = getFloaterByWnd( panel->getCWnd() );
	if ( floater )
	{
		panel->clearPosList( false );
		buildPanelPosList( false, floater->getRootNode(), panel );
	}
	else
	{
		panel->clearPosList( true );
		buildPanelPosList( true, dockTreeRoot_, panel );
	}
}

void Dock::restorePanelDockPos( PanelPtr panel )
{
	DockNodePtr node;
	DockNodePtr parent;
	getNodeByWnd( panel->getCWnd(), node, parent );

	if ( node )
		return;	// already visible

	bool docked;

	docked = !panel->isFloating();

	panel->resetPosList( docked );

	PanelPos pos;

	while( panel->getNextPos( docked, pos ) )
	{
		if ( panel->isFloating() )
		{
			for( FloaterItr i = floaterList_.begin(); i != floaterList_.end(); ++i )
			{
				DockNodePtr node;
				DockNodePtr parent;
				(*i)->getRootNode()->getNodeByWnd( pos.destPanel, node, parent );
				if ( node )
				{
					insertPanelIntoPanel( panel, pos.destPanel, pos.insertAt );
					(*i)->ShowWindow( SW_SHOW );
					return;
				}
			}
		}
		else
		{
			DockNodePtr node;
			DockNodePtr parent;
			dockTreeRoot_->getNodeByWnd( pos.destPanel, node, parent );
			if ( node )
			{
				insertPanelIntoPanel( panel, pos.destPanel, pos.insertAt );
				return;
			}
		}
	}
	
	if ( panel->isFloating() )
	{
		int x;
		int y;
		panel->getLastPos( x, y );
		floatPanel( panel, 0, 0, x, y );
	}
	else
		insertPanelIntoPanel( panel, mainView_, RIGHT );

}


void Dock::togglePanelPos( PanelPtr panel )
{
	removeNodeByWnd( panel->getCWnd() );
	
	if ( !panel->isFloating() )
	{
		int w;
		int h;
		panel->getPreferredSize( w, h );
		panel->SetWindowPos( 0, 0, 0, w, h, SWP_NOZORDER );
	}

	panel->setFloating( !panel->isFloating() );
	restorePanelDockPos( panel );
}

void Dock::toggleTabPos( PanelPtr panel, TabPtr tab )
{
	// create a new panel from the tab
	PanelPtr newPanel = detachTabToPanel( panel, tab );

	// call restore to put this panel in the previous state of the original panel
	newPanel->setFloating( !panel->isFloating() );
	int w;
	int h;
	newPanel->getPreferredSize( w, h );
	newPanel->SetWindowPos( 0, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER );
	restorePanelDockPos( newPanel );
}


void Dock::destroyFloater( FloaterPtr floater )
{
	// remove all panels from floater
	if ( IsWindow( floater->GetSafeHwnd() ) )
	{
		// still a window, remove the rest of the panels
		CRect rect;
		floater->GetWindowRect( &rect );

		for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
		{
			DockNodePtr node;
			DockNodePtr parent;
			floater->getRootNode()->getNodeByWnd( (*i)->getCWnd(), node, parent );
			if ( node )
			{
				(*i)->setLastPos( rect.left, rect.top );
				removeNodeByWnd( node->getCWnd() );
			}
		}
	}

	// remove all panels from floater
	for( FloaterItr i = floaterList_.begin(); i != floaterList_.end(); ++i )
	{
		if ( (*i) == floater )
		{
			floaterList_.erase( i );
			break;
		}
	}
}


void Dock::broadcastMessage( UINT msg, WPARAM wParam, LPARAM lParam )
{
	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
		(*i)->broadcastMessage( msg, wParam, lParam );
}

void Dock::sendMessage( const std::string contentID, UINT msg, WPARAM wParam, LPARAM lParam )
{
	int index = 0;
	while( true )
	{
		ContentPtr content = getContent( contentID, index++ );
		if ( !content )
			break;
		content->getCWnd()->SendMessage( msg, wParam, lParam );
	}
}


int Dock::getContentCount( const std::string contentID )
{
	int cnt = 0;

	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i )
		cnt += (*i)->contains( contentID );

	return cnt;
}

PanelPtr Dock::detachTabToPanel( PanelPtr panel, TabPtr tab )
{
	CRect rect;
	panel->GetWindowRect( &rect );

	int w;
	int h;
	panel->getPreferredSize( w, h );

	panel->detachTab( tab );

	PanelPtr newPanel = new Panel( mainFrame_ );
	panelList_.push_back( newPanel );

	newPanel->addTab( tab );

	if ( panel->isFloating() )
	{
		newPanel->SetWindowPos( 0, rect.left, rect.top, 0, 0, SWP_NOSIZE|SWP_NOZORDER );
		newPanel->setLastPos( rect.left, rect.top );
	}
	else
	{
		newPanel->SetWindowPos( 0, 0, 0, w, h, SWP_NOZORDER );
	}

	copyPanelRestorePosToTab( panel, newPanel );

	return newPanel;
}

void Dock::copyPanelRestorePosToTab( PanelPtr src, PanelPtr dstTab )
{
	// copy panel's restore positions
	PanelPos pos;
	
	dstTab->clearPosList( false );
	src->resetPosList( false );
	if ( src->isFloating() )
		dstTab->insertPos( false, PanelPos( TAB, src->getCWnd() ) );
	while ( src->getNextPos( false, pos ) )
		dstTab->insertPos( false, pos );

	dstTab->clearPosList( true );
	src->resetPosList( true );
	if ( !src->isFloating() )
		dstTab->insertPos( true, PanelPos( TAB, src->getCWnd() ) );
	while ( src->getNextPos( true, pos ) )
		dstTab->insertPos( true, pos );
}


void Dock::rollupPanel( PanelPtr panel )
{
	DockNodePtr node;
	DockNodePtr parent;

	getNodeByWnd( panel->getCWnd(), node, parent );

	if ( !node )
		return;

	FloaterPtr floater = getFloaterByWnd( panel->getCWnd() );

	if ( floater )
	{
		floater->getRootNode()->adjustSizeToNode( node );
		floater->getRootNode()->recalcLayout();
		floater->RecalcLayout();
		floater->adjustSize( true );
	}
	else
	{
		dockTreeRoot_->adjustSizeToNode( node );
		dockTreeRoot_->recalcLayout();
		mainFrame_->RecalcLayout();
	}
}

int Dock::getPanelIndex( PanelPtr panel )
{	
	int idx = 0;
	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i, ++idx )
		if ( (*i) == panel )
			return idx;

	return -1;
}

PanelPtr Dock::getPanelByIndex( int index )
{
	int idx = 0;
	for( PanelItr i = panelList_.begin(); i != panelList_.end(); ++i, ++idx )
		if ( index == idx )
			return (*i);

	return 0;
}


} // namespace