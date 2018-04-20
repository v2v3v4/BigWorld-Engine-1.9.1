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
 *	GUI Tearoff panel framework - MainViewNode class implementation
 */



#include "pch.hpp"
#include "guitabs.hpp"


namespace GUITABS
{

MainViewNode::MainViewNode( CWnd* mainView ) :
	mainView_( mainView )
{
}

CWnd* MainViewNode::getCWnd()
{
	return mainView_;
}

bool MainViewNode::load( DataSectionPtr section, CWnd* parent, int wndID )
{
	if ( !section )
		return false;

	DataSectionPtr nodeSec = section->openSection( "MainView" );
	if ( !nodeSec )
		return false;

	mainView_->SetDlgCtrlID( wndID );
	mainView_->SetParent( parent );

	return true;
}

bool MainViewNode::save( DataSectionPtr section )
{
	if ( !section )
		return false;

	DataSectionPtr nodeSec = section->openSection( "MainView", true );
	if ( !nodeSec )
		return false;

	nodeSec->setString( "Main Application View Window" );
	return true;
}


}