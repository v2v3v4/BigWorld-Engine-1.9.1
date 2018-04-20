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
 *	GUI Tearoff panel framework - Manager class implementation
 */


#include "pch.hpp"
#include "guitabs.hpp"


/// GUITabs Manager Singleton
BW_SINGLETON_STORAGE( GUITABS::Manager )


namespace GUITABS
{


Manager::Manager() :
	dragMgr_( new DragManager() ),
	lastLayoutFile_( "" )
{
	registerFactory( new ContentContainerFactory() );
}


Manager::~Manager()
{
	removeDock();

	factoryList_.clear();
}


bool Manager::registerFactory( ContentFactoryPtr factory )
{
	if ( !factory )
		return false;

	factoryList_.push_back( factory );
	return true;
}

bool Manager::insertDock( CFrameWnd* mainFrame, CWnd* mainView )
{
	if ( !mainFrame || !mainView )
		return false;

	if ( dock_ )
		return false;

	dock_ = new Dock( mainFrame, mainView );

	return true;
}

void Manager::removeDock()
{
	if ( !dock_ )
		return;

	dock_ = 0;
}


PanelHandle Manager::insertPanel( const std::string contentID, InsertAt insertAt, PanelHandle destPanel )
{
	if ( !dock_ )
		return 0;

	PanelPtr panel = dock_->insertPanel( contentID, destPanel, insertAt );

	// this code should be improved so in the rare case of having two contents
	// with the same contentID in one panel, it returns the last inserted one.
	// (probably using the getContent method that takes an 'index' param?)
	return panel->getContent( contentID ).getObject();
}

bool Manager::removePanel( PanelHandle panel )
{
	if ( !dock_ )
		return false;

	PanelPtr p = dock_->getPanelByHandle( panel );

	if ( p )
		dock_->removePanel( p );

	return ( !!p );
}

bool Manager::removePanel( const std::string contentID )
{
	if ( !dock_ )
		return false;

	dock_->removePanel( contentID );

	return true;
}

void Manager::removePanels()
{
	if ( !dock_ )
		return;
	CFrameWnd* mainFrame = dock_->getMainFrame();
	CWnd* mainView = dock_->getMainView();
	dock_ = 0;
	dock_ = new Dock( mainFrame, mainView );
}

void Manager::showPanel( PanelHandle panel, bool show )
{
	if ( !dock_ )
		return;

	ContentPtr content = panel;
	dock_->showPanel( content, show );
}

void Manager::showPanel( const std::string contentID, bool show )
{
	if ( !dock_ )
		return;

	dock_->showPanel( contentID, show );
}

bool Manager::isContentVisible( const std::string contentID )
{
	if ( !dock_ )
		return false;

	return dock_->isContentVisible( contentID );
}

Content* Manager::getContent( const std::string contentID, int index /*=0*/ )
{
	if ( !dock_ )
		return 0;

	return dock_->getContent( contentID, index ).getObject();
}

bool Manager::isValid( PanelHandle panel )
{
	if ( !dock_ )
		return false;

	if ( dock_->getPanelByHandle( panel ) )
		return true;
	else
		return false;
}

bool Manager::isDockVisible()
{
	if ( !dock_ )
		return false;

	return dock_->isDockVisible();
}

void Manager::showDock( bool show )
{
	if ( !dock_ )
		return;

	dock_->showDock( show );
}

void Manager::showFloaters( bool show )
{
	if ( !dock_ )
		return;

	dock_->showFloaters( show );
}

void Manager::broadcastMessage( UINT msg, WPARAM wParam, LPARAM lParam )
{
	if ( !dock_ )
		return;

	dock_->broadcastMessage( msg, wParam, lParam );
}

void Manager::sendMessage( const std::string contentID, UINT msg, WPARAM wParam, LPARAM lParam )
{
	if ( !dock_ )
		return;

	dock_->sendMessage( contentID, msg, wParam, lParam );
}

bool Manager::load( const std::string fname )
{
	if ( !dock_ )
		return false;

	std::string loadName = fname;
	if ( fname.empty() )
	{
		char buffer[MAX_PATH+1];
		GetCurrentDirectory( sizeof( buffer ), buffer );
		strcat( buffer, "\\layout.xml" );
		loadName = buffer;
		std::replace( loadName.begin(), loadName.end(), '\\', '/' );
	}
	lastLayoutFile_ = loadName;

	DataResource file( loadName, RESOURCE_TYPE_XML );
	DataSectionPtr section = file.getRootSection();

	if ( !section )
		return false;

	if ( !dock_->empty() )
		removePanels();

	if ( !dock_->load( section->openSection( "Dock" ) ) )
	{
		removePanels();
		return false;
	}

	return true;
}

bool Manager::save( const std::string fname )
{
	if ( !dock_ )
		return false;

	std::string saveName = fname;
	if ( fname.empty() )
		saveName = lastLayoutFile_;
	if ( saveName.empty() )
		return false;

	DataResource file( saveName, RESOURCE_TYPE_XML );
	DataSectionPtr section = file.getRootSection();

	if ( !section )
		return false;

	if ( !dock_->save( section->openSection( "Dock", true ) ) )
		return false;

	if ( file.save( saveName ) != DataHandle::DHE_NoError )
		return false;
	return true;
}

PanelHandle Manager::clone( PanelHandle content, int x, int y  )
{
	if ( !dock_ )
		return 0;

	PanelPtr panel = dock_->getPanelByHandle( content );
	if ( !panel )
		return 0;

	return panel->cloneTab( content, x, y  ).getObject();
}


// Utility methods used internally

ContentPtr Manager::createContent( const std::string contentID )
{
	for( ContentFactoryItr i = factoryList_.begin(); i != factoryList_.end(); ++i )
	{
		if ( contentID.compare( (*i)->getContentID() ) == 0 )
			return (*i)->create();
	}

	return 0;
}

DockPtr Manager::dock()
{
	return dock_;
}

DragManagerPtr Manager::dragManager()
{
	return dragMgr_;
}


} // namespace
