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
 *	GUI Tearoff panel framework - DockedPanelNode class implementation
 */


#include "pch.hpp"
#include "guitabs.hpp"


namespace GUITABS
{

DockedPanelNode::DockedPanelNode()
{
}

DockedPanelNode::DockedPanelNode( PanelPtr dockedPanel )
{
	init( dockedPanel );
}

void DockedPanelNode::init( PanelPtr dockedPanel )
{
	dockedPanel_ = dockedPanel;
}

CWnd* DockedPanelNode::getCWnd()
{
	return dockedPanel_->getCWnd();
}

bool DockedPanelNode::load( DataSectionPtr section, CWnd* parent, int wndID )
{
	if ( !section )
		return false;

	DataSectionPtr nodeSec = section->openSection( "DockedPanel", true );
	if ( !nodeSec )
		return false;

	int index = nodeSec->readInt( "index", -1 );
	if ( index < 0 )
		return false;

	PanelPtr panel = Manager::instance().dock()->getPanelByIndex( index );
	if ( !panel )
		return false;

	init( panel );
	panel->SetDlgCtrlID( wndID );
	panel->SetParent( parent );
	panel->ShowWindow( SW_SHOW );

	return true;
}

bool DockedPanelNode::save( DataSectionPtr section )
{
	if ( !section )
		return false;

	DataSectionPtr nodeSec = section->openSection( "DockedPanel", true );
	if ( !nodeSec )
		return false;

	nodeSec->writeInt( "index", dockedPanel_->getIndex() );
    return true;
}

void DockedPanelNode::getPreferredSize( int& w, int& h )
{
	dockedPanel_->getPreferredSize( w, h );
}

bool DockedPanelNode::isExpanded()
{
	return dockedPanel_->isExpanded();
}


}