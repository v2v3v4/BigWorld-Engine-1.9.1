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
 *	GUI Tearoff panel framework - SplitterNode class implementation
 */



#include "pch.hpp"
#include "guitabs.hpp"


namespace GUITABS
{

static const int MIN_PANE_SIZE = 1;
static const int SPLITTER_PAD = 9;
static const int MIN_ROW_SIZE = 16;


SplitterNode::SplitterNode() :
	dir_( VERTICAL ),
	delayedLeftSize_( -1 ),
	delayedRightSize_( -1 )
{
}

SplitterNode::SplitterNode( Orientation dir, CWnd* parent, int wndID ) :
	delayedLeftSize_( -1 ),
	delayedRightSize_( -1 )
{
	init( dir, parent, wndID );
}

void SplitterNode::init( Orientation dir, CWnd* parent, int wndID )
{
	dir_ = dir;
	leftChild_ = 0;
	rightChild_ = 0;

	int ncols = 1;
	int nrows = 1;

	if ( dir == VERTICAL )
		nrows = 2;
	else
		ncols = 2;

	if ( wndID == 0 )
		wndID = AFX_IDW_PANE_FIRST;

	splitterWnd_.setMinRowSize( MIN_ROW_SIZE );
	splitterWnd_.CreateStatic( parent, nrows, ncols, WS_CHILD, wndID );
	splitterWnd_.setEventHandler( this );
}

SplitterNode::~SplitterNode()
{
	leftChild_ = 0;
	rightChild_ = 0;

	splitterWnd_.DestroyWindow();
}

void SplitterNode::setLeftChild( DockNodePtr child )
{
	leftChild_ = child;

	if ( child )
	{
		int id = splitterWnd_.IdFromRowCol( 0,0 );
		child->setParentWnd( &splitterWnd_ );
		child->getCWnd()->SetDlgCtrlID( id );
		child->getCWnd()->ShowWindow( SW_SHOW );
	}
}

void SplitterNode::setRightChild( DockNodePtr child )
{
	rightChild_ = child;

	if ( child )
	{
		int id;
		if ( dir_ == VERTICAL )
			id = splitterWnd_.IdFromRowCol( 1,0 );
		else
			id = splitterWnd_.IdFromRowCol( 0,1 );

		child->setParentWnd( &splitterWnd_ );
		child->getCWnd()->SetDlgCtrlID( id );
		child->getCWnd()->ShowWindow( SW_SHOW );
	}
}

void SplitterNode::finishInsert( const CRect* destRect, int leftChildSize, int rightChildSize )
{
	if ( dir_ == VERTICAL )
	{
		if ( leftChildSize )
		{
			splitterWnd_.SetRowInfo( 0, min( leftChildSize, destRect->Height()/2 ), MIN_PANE_SIZE + 1 );
			splitterWnd_.SetRowInfo( 1, max( destRect->Height() - leftChildSize, destRect->Height()/2 ), MIN_PANE_SIZE + 1 );
		}
		else if ( rightChildSize )
		{
			splitterWnd_.SetRowInfo( 0, max( destRect->Height() - rightChildSize, destRect->Height()/2 ), MIN_PANE_SIZE + 1 );
			splitterWnd_.SetRowInfo( 1, min( rightChildSize, destRect->Height()/2 ), MIN_PANE_SIZE + 1 );
		}
		else
		{
			splitterWnd_.SetRowInfo( 0, destRect->Height()/2, MIN_PANE_SIZE + 1 );
			splitterWnd_.SetRowInfo( 1, destRect->Height()/2, MIN_PANE_SIZE + 1 );
		}
	}
	else
	{
		if ( leftChildSize )
		{
			splitterWnd_.SetColumnInfo( 0, min( leftChildSize, destRect->Width()/2 ) , MIN_PANE_SIZE + 1 );
			splitterWnd_.SetColumnInfo( 1, max( destRect->Width() - leftChildSize, destRect->Width()/2 ), MIN_PANE_SIZE + 1 );
		}
		else if ( rightChildSize )
		{
			splitterWnd_.SetColumnInfo( 0, max( destRect->Width() - rightChildSize, destRect->Width()/2 ), MIN_PANE_SIZE + 1 );
			splitterWnd_.SetColumnInfo( 1, min( rightChildSize, destRect->Width()/2 ), MIN_PANE_SIZE + 1 );
		}
		else
		{
			splitterWnd_.SetColumnInfo( 0, destRect->Width()/2, MIN_PANE_SIZE + 1 );
			splitterWnd_.SetColumnInfo( 1, destRect->Width()/2, MIN_PANE_SIZE + 1 );
		}
	}

	splitterWnd_.ShowWindow( SW_SHOW );
	splitterWnd_.RecalcLayout();
	splitterWnd_.UpdateWindow();
}

DockNodePtr SplitterNode::getLeftChild()
{
	return leftChild_;
}

DockNodePtr SplitterNode::getRightChild()
{
	return rightChild_;
}

int SplitterNode::getLeftSize()
{
	int size;
	int minSize;

	if ( dir_ == VERTICAL )
		splitterWnd_.GetRowInfo( 0, size, minSize );
	else
		splitterWnd_.GetColumnInfo( 0, size, minSize );

	return size;
}

int SplitterNode::getRightSize()
{
	int size;
	int minSize;

	if ( dir_ == VERTICAL )
		splitterWnd_.GetRowInfo( 1, size, minSize );
	else
		splitterWnd_.GetColumnInfo( 1, size, minSize );

	return size;
}

void SplitterNode::setLeftSize( int size )
{
	if ( dir_ == VERTICAL )
	{
		if ( !getLeftChild()->isExpanded() || !getRightChild()->isExpanded() )
			splitterWnd_.allowResize( false );
		else
			splitterWnd_.allowResize( true );
		splitterWnd_.SetRowInfo( 0, size, MIN_PANE_SIZE );
	}
	else
		splitterWnd_.SetColumnInfo( 0, size, MIN_PANE_SIZE );
}

void SplitterNode::setRightSize( int size )
{
	if ( dir_ == VERTICAL )
	{
		if ( !getLeftChild()->isExpanded() || !getRightChild()->isExpanded() )
			splitterWnd_.allowResize( false );
		else
			splitterWnd_.allowResize( true );
		splitterWnd_.SetRowInfo( 1, size, MIN_PANE_SIZE );
	}
	else
		splitterWnd_.SetColumnInfo( 1, size, MIN_PANE_SIZE );
}



bool SplitterNode::isLeaf()
{
	return false;
}

bool SplitterNode::isExpanded()
{
	return getLeftChild()->isExpanded() || getRightChild()->isExpanded();
}


CWnd* SplitterNode::getCWnd()
{
	return &splitterWnd_;
}

bool SplitterNode::adjustSizeToNode( DockNodePtr newNode, bool nodeIsNew )
{
	DockNodePtr node;
	DockNodePtr parentNode;

	// Find out in which subtree is the node
	bool isInLeft = getLeftChild()->getNodeByWnd( newNode->getCWnd(),  node, parentNode );
	bool isInRight = getRightChild()->getNodeByWnd( newNode->getCWnd(), node, parentNode );

	int w;
	int h;

	// get current splitter size
	CRect rect;
	getCWnd()->GetWindowRect( &rect );

	// get the mainview window, in order to properly calculate preferred sizes
	CWnd* mainViewWnd = 0;
	if ( Manager::instance().dock() )
		mainViewWnd = Manager::instance().dock()->getMainView();

	if ( isInLeft )
	{
		// node in left subtree, proceed to resize the right subtree to fit the node
		if ( !getLeftChild()->getNodeByWnd( mainViewWnd, node, parentNode ) )
		{
			// left subtree does not contain mainView, so preferred size is ok
			getLeftChild()->getPreferredSize( w, h );
		}
		else
		{
			// left subtree contains mainView, so get the current size of the pane
			w = getLeftSize();
			h = getLeftSize();
		}

		if ( getSplitOrientation() == HORIZONTAL )
		{
			if ( getLeftSize() < w || ( getLeftChild() == newNode && nodeIsNew ) )
			{
				// only resize if the node requires more space than already available
				setLeftSize( max( MIN_PANE_SIZE+1, min( w, rect.Width() - MIN_PANE_SIZE - 30 ) ) );
				setRightSize( max( MIN_PANE_SIZE+1, rect.Width() - w - SPLITTER_PAD ) );
			}
		}
		else
		{
			if ( getLeftSize() < h || ( getLeftChild() == newNode  && nodeIsNew ) ||
				!getLeftChild()->isExpanded() )
			{
				// resize if the node requires more space than already available or if it's collapsed
				if ( getLeftChild()->isExpanded() )
				{
					int hh;
					getRightChild()->getPreferredSize( w, hh );
					if ( getRightChild()->isExpanded() )
						hh /= 2;
					h = min( h, rect.Height() - hh - SPLITTER_PAD );
				}
				setLeftSize( max( MIN_PANE_SIZE+1, h ) );
				setRightSize( max( MIN_PANE_SIZE+1, rect.Height() - h - SPLITTER_PAD ) );
			}
		}
	}
	else if ( isInRight )
	{
		// node in right subtree, proceed to resize the left subtree to fit the node
		if ( !getRightChild()->getNodeByWnd( mainViewWnd, node, parentNode ) )
		{
			// right subtree does not contain mainView, so preferred size is ok
			getRightChild()->getPreferredSize( w, h );
		}
		else
		{
			// right subtree contains mainView, so get the current size of the pane
			w = getRightSize();
			h = getRightSize();
		}

		if ( getSplitOrientation() == HORIZONTAL )
		{
			if ( getRightSize() < w || ( getRightChild() == newNode && nodeIsNew ) )
			{
				// only resize if the node requires more space than already available
				setLeftSize( max( MIN_PANE_SIZE+1, rect.Width() - w - SPLITTER_PAD ) );
				setRightSize( max( MIN_PANE_SIZE+1, min( w, rect.Width() - MIN_PANE_SIZE - 30 ) ) );
			}
		}
		else
		{
			if ( getRightSize() < h || ( getRightChild() == newNode && nodeIsNew ) ||
				!getRightChild()->isExpanded() )
			{
				// resize if the node requires more space than already available or if it's collapsed
				if ( !getLeftChild()->isExpanded() )
				{
					// if left subtree is collapsed, minimize left and maximize right
					if ( !getLeftChild()->getNodeByWnd( mainViewWnd, node, parentNode ) )
					{
						// left subtree does not contain mainView, so preferred size is ok
						getLeftChild()->getPreferredSize( w, h );
					}
					else
					{
						// left subtree contains mainView, so calc maximum available size
						w = rect.Width() - w;
						h = rect.Height() - h;
					}

					setLeftSize( max( MIN_PANE_SIZE+1, h ) );
					setRightSize( max( MIN_PANE_SIZE+1, rect.Height() - h - SPLITTER_PAD ) );
				}
				else
				{
					// left not collapsed, proceed resizing node's branch
					if ( getRightChild()->isExpanded() )
					{
						int hh;
						getLeftChild()->getPreferredSize( w, hh );
						if ( getLeftChild()->isExpanded() )
							hh /= 2;
						h = min( h, rect.Height() - hh - SPLITTER_PAD );
					}
					setLeftSize( max( MIN_PANE_SIZE+1, rect.Height() - h - SPLITTER_PAD ) );
					setRightSize( max( MIN_PANE_SIZE+1, h ) );
				}
			}
		}
	}

	splitterWnd_.RecalcLayout();

	// Adjust sizes for the left and right subtrees
	getLeftChild()->adjustSizeToNode( newNode, nodeIsNew );
	getRightChild()->adjustSizeToNode( newNode, nodeIsNew );

	return isInLeft || isInRight;
}

void SplitterNode::recalcLayout()
{
	if ( getSplitOrientation() == VERTICAL && ( !getLeftChild()->isExpanded() || !getRightChild()->isExpanded() ) )
		splitterWnd_.allowResize( false );
	else
		splitterWnd_.allowResize( true );
	splitterWnd_.RecalcLayout();
	getLeftChild()->recalcLayout();
	getRightChild()->recalcLayout();
}


bool SplitterNode::load( DataSectionPtr section, CWnd* parent, int wndID )
{
	if ( !section || !parent )
		return false;

	DataSectionPtr nodeSec = section->openSection( "Splitter" );
	if ( !nodeSec )
		return false;

	DataSectionPtr leftSec = nodeSec->openSection( "Left" );
	DataSectionPtr rightSec = nodeSec->openSection( "Right" );
	if ( !leftSec || !rightSec )
		return false;

	DockNodePtr leftNode = Manager::instance().dock()->nodeFactory( leftSec );
	DockNodePtr rightNode = Manager::instance().dock()->nodeFactory( rightSec );
	if ( !leftNode || !rightNode )
		return false;

	dir_ = nodeSec->readBool( "layoutVertical", dir_ == VERTICAL )?VERTICAL:HORIZONTAL;
	init( dir_, parent, wndID );
	if ( !splitterWnd_.GetSafeHwnd() )
		return false;

	int leftSize = leftSec->readInt( "size", 100 );
	int rightSize = rightSec->readInt( "size", 100 );

	int leftId;
	int rightId;

	leftId = splitterWnd_.IdFromRowCol( 0,0 );
	if ( dir_ == VERTICAL )
		rightId = splitterWnd_.IdFromRowCol( 1,0 );
	else
		rightId = splitterWnd_.IdFromRowCol( 0,1 );

	if ( !leftNode->load( leftSec, &splitterWnd_, leftId ) )
		return false;
	if ( !rightNode->load( rightSec, &splitterWnd_, rightId ) )
		return false;

	setLeftChild( leftNode );
	setRightChild( rightNode );

	setLeftSize( leftSize );
	setRightSize( rightSize );
	splitterWnd_.ShowWindow( SW_SHOW );
	splitterWnd_.RecalcLayout();

	delayedLeftSize_ = leftSize;
	delayedRightSize_ = rightSize;

	return true;
}

bool SplitterNode::save( DataSectionPtr section )
{
	if ( !section )
		return false;

	DataSectionPtr nodeSec = section->openSection( "Splitter", true );
	if ( !nodeSec )
		return false;

	nodeSec->writeBool( "layoutVertical", dir_ == VERTICAL );

	DataSectionPtr leftSec = nodeSec->openSection( "Left", true );
	DataSectionPtr rightSec = nodeSec->openSection( "Right", true );
	if ( !leftSec || !rightSec )
		return false;

	leftSec->writeInt( "size", getLeftSize() );
	rightSec->writeInt( "size", getRightSize() );

	return leftChild_->save( leftSec ) && rightChild_->save( rightSec );
}

Orientation SplitterNode::getSplitOrientation()
{
	return dir_;
}

void SplitterNode::setParentWnd( CWnd* parent )
{
	splitterWnd_.SetParent( parent );
}

void SplitterNode::getPreferredSize( int& w, int& h )
{
	int wL;
	int hL;

	getLeftChild()->getPreferredSize( wL, hL );	

	int wR;
	int hR;
	getRightChild()->getPreferredSize( wR, hR );

	if ( getSplitOrientation() == HORIZONTAL )
	{
		w = wL + wR + 7;
		h = max( hL, hR );
	}
	else
	{
		h = hL + hR + 7;
		w = max( wL, wR );
	}
}


bool SplitterNode::getNodeByWnd( CWnd* ptr, DockNodePtr& childNode, DockNodePtr& parentNode )
{
	if ( getCWnd() == ptr )
	{
		parentNode = 0;
		childNode = this;
		return true;
	}
	else if ( getLeftChild()->getNodeByWnd( ptr, childNode, parentNode ) )
	{
		if ( getLeftChild()->getCWnd() == ptr )
			parentNode = this;
		return true;
	}
	else if ( getRightChild()->getNodeByWnd( ptr, childNode, parentNode ) )
	{
		if ( getRightChild()->getCWnd() == ptr )
			parentNode = this;
		return true;
	}

	return false;
}

DockNodePtr SplitterNode::getNodeByPoint( int x, int y )
{
	if ( hitTest( x, y ) )
	{
		DockNodePtr retNode;
		retNode = getLeftChild()->getNodeByPoint( x, y );
		if ( retNode )
			return retNode;
		retNode = getRightChild()->getNodeByPoint( x, y );
		if ( retNode )
			return retNode;
	}
	return 0;
}


void SplitterNode::destroy()
{
	getLeftChild()->destroy();
	getRightChild()->destroy();

	setLeftChild( 0 );
	setRightChild( 0 );
}


// Private methods
void SplitterNode::resizeTreeDimension( Orientation dir, int lastSize, int size )
{
	CWnd* mainViewWnd = 0;
	if ( Manager::instance().dock() )
		mainViewWnd = Manager::instance().dock()->getMainView();

	if ( getSplitOrientation() == dir )
	{
		DockNodePtr childNode;
		DockNodePtr parentNode;

		if ( getLeftChild()->getNodeByWnd( mainViewWnd, childNode, parentNode ) )
		{
			// if mainView is to the left, only resize the left pane.
			int d = 0;
			if ( mainViewWnd->GetExStyle() & WS_EX_CLIENTEDGE )
				d += GetSystemMetrics( SM_CXEDGE ) * 2;

			if ( !getLeftChild()->isLeaf() )
				d += GetSystemMetrics( SM_CXEDGE ) * 2;

			if ( !getRightChild()->isExpanded() && dir == VERTICAL )
			{
				// if the other pane is not expanded, keep it collapsed
				int w;
				int h;
				getRightChild()->getPreferredSize( w, h );
				h += d;
				setLeftSize( max( MIN_PANE_SIZE+1, size - h - SPLITTER_PAD ) );
				setRightSize( h );
			}
			else
			{
				// keep the expanded size of the other pane
				CRect rect;
				getLeftChild()->getCWnd()->GetWindowRect( &rect );
				int curSize;
				if ( dir == HORIZONTAL )
					curSize = rect.Width();
				else
					curSize = rect.Height();

				setLeftSize( max( MIN_PANE_SIZE+1, curSize + ( size - lastSize ) - d ) );
			}
		}
		else if ( getRightChild()->getNodeByWnd( mainViewWnd, childNode, parentNode ) )
		{
			// if mainView is to the right, only resize the right pane.
			int d = 0;
			if ( mainViewWnd->GetExStyle() & WS_EX_CLIENTEDGE )
				d += GetSystemMetrics( SM_CXEDGE ) * 2;

			if ( !getRightChild()->isLeaf() )
				d += GetSystemMetrics( SM_CXEDGE ) * 2;

			if ( !getLeftChild()->isExpanded() && dir == VERTICAL )
			{
				// if the other pane is not expanded, keep it collapsed
				int w;
				int h;
				getLeftChild()->getPreferredSize( w, h );
				h += d;
				setLeftSize( h );
				setRightSize( max( MIN_PANE_SIZE+1, size - h - SPLITTER_PAD ) );
			}
			else
			{
				// keep the expanded size of the other pane
				CRect rect;
				getRightChild()->getCWnd()->GetWindowRect( &rect );
				int curSize;
				if ( dir == HORIZONTAL )
					curSize = rect.Width();
				else
					curSize = rect.Height();

				setRightSize( max( MIN_PANE_SIZE+1, curSize + ( size - lastSize ) - d ) );
			}
		}
		else
		{
			// if mainView isn't in the subtree, resize both panes proportionally.
			int leftNewSize;

			if ( !getLeftChild()->isExpanded() && dir == VERTICAL )
			{
				// if the left pane is collapsed, left size dominates
				int w;
				int h;
				getLeftChild()->getPreferredSize( w, h );
				leftNewSize = h;
			}
			else
			{
				// if the left pane is expanded
				if ( !getRightChild()->isExpanded() && dir == VERTICAL )
				{
					// if the right pane is collapsed, right size dominates
					int w;
					int h;
					getRightChild()->getPreferredSize( w, h );
					leftNewSize = size - h - SPLITTER_PAD;
				}
				else
				{
					// simply resize both panes proportionally to the change of size of the splitter
					leftNewSize = min( size - MIN_PANE_SIZE - 11, getLeftSize() + ( size - lastSize ) / 2 );
				}
			}

			setLeftSize( max( MIN_PANE_SIZE+1, leftNewSize ) );
			setRightSize( max( MIN_PANE_SIZE+1, size - leftNewSize - SPLITTER_PAD ) );
		}
	}
}

// EventHandler methods
void SplitterNode::resizeSplitter( int lastWidth, int lastHeight, int width, int height )
{
	// Needed to filter out the first resize messages sent from windows, because
	// windows first resizes to a very small window (0,0) and then to the actual size
	if ( width < 7 && height < 7 )
		return;

	if ( delayedLeftSize_ > -1 && delayedRightSize_ > -1 )
	{
		// delayed resize activated. Validate and set sizes. Currently, this is
		// only called from the load method.
		CWnd* mainViewWnd = Manager::instance().dock()->getMainView();
		DockNodePtr node;
		DockNodePtr parentNode;
		bool isInLeft = getLeftChild()->getNodeByWnd( mainViewWnd,  node, parentNode );
		bool isInRight = getRightChild()->getNodeByWnd( mainViewWnd,  node, parentNode );

		int size = (dir_ == VERTICAL) ? height : width;

		if ( isInLeft )
		{
			// MainView to the left, try to keep the right dock size
			int d = GetSystemMetrics( SM_CXEDGE );
			if ( mainViewWnd->GetExStyle() & WS_EX_CLIENTEDGE )
				d += GetSystemMetrics( SM_CXEDGE ) * 2;

			delayedLeftSize_ = size - d - delayedRightSize_ - SPLITTER_PAD;
		}
		else if ( !isInLeft && !isInRight &&
			size < ( delayedLeftSize_ + delayedRightSize_ + SPLITTER_PAD ) )
		{
			// The new size is smaller than the sum of the desired sizes, so
			// adjust the sizes proportionally so no panels get collapsed to
			// size 0.
			int sum = delayedLeftSize_ + delayedRightSize_;
			delayedLeftSize_ = delayedLeftSize_ * size / sum;
			delayedRightSize_ = delayedRightSize_ * size / sum;
		}

		setLeftSize( max( MIN_PANE_SIZE+1, delayedLeftSize_ ) );
		setRightSize( max( MIN_PANE_SIZE+1, delayedRightSize_ ) );

		delayedLeftSize_ = -1;
		delayedRightSize_ = -1;
	}
	else
	{
		if ( width != lastWidth && width > 0 && lastWidth > 0 )
			resizeTreeDimension( HORIZONTAL, lastWidth, width );

		if ( height != lastHeight && height > 0 && lastHeight > 0 )
	 		resizeTreeDimension( VERTICAL, lastHeight, height );
	}
}


}	// namespace