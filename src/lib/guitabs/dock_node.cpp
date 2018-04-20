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
 *	GUI Tearoff panel framework - DockNode class implementation
 */



#include "pch.hpp"
#include "guitabs.hpp"


namespace GUITABS
{

// Default implementation: Leaf behaviour

void DockNode::setLeftChild( DockNodePtr child )
{
	ASSERT( 0 );
}

void DockNode::setRightChild( DockNodePtr child )
{
	ASSERT( 0 );
}

DockNodePtr DockNode::getLeftChild()
{
	ASSERT( 0 );
	return 0;
}

DockNodePtr DockNode::getRightChild()
{
	ASSERT( 0 );
	return 0;
}

bool DockNode::isLeaf()
{
	return true;
}

bool DockNode::hitTest( int x, int y )
{
	CRect rect;

	getCWnd()->GetWindowRect(&rect);

	return ( rect.PtInRect( CPoint( x, y ) ) == TRUE );
}

Orientation DockNode::getSplitOrientation()
{
	return UNDEFINED_ORIENTATION;
}

bool DockNode::isVisible()
{
	return ( getCWnd()->GetStyle() & WS_VISIBLE ) > 0;
}

bool DockNode::isExpanded()
{
	return true;
}

bool DockNode::adjustSizeToNode( DockNodePtr newNode, bool nodeIsNew )
{
	if ( newNode == this )
		return true;
	else
		return false;
}

void DockNode::recalcLayout()
{
	return;
}

void DockNode::setParentWnd( CWnd* parent )
{
	getCWnd()->SetParent( parent );
}

void DockNode::getPreferredSize( int& w, int& h )
{
	w = 0;
	h = 0;
}

bool DockNode::getNodeByWnd( CWnd* ptr, DockNodePtr& childNode, DockNodePtr& parentNode )
{
	if ( getCWnd() == ptr )
	{
		parentNode = 0;
		childNode = this;
		return true;
	}

	return false;
}

DockNodePtr DockNode::getNodeByPoint( int x, int y )
{
	if ( hitTest( x, y ) )
	{
		return this;
	}
	return 0;
}

void DockNode::destroy()
{
	return;
}



}	// namespace