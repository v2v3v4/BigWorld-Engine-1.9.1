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
 *	GUI Tearoff panel framework - Manager class
 */

#ifndef GUITABS_MANAGER_HPP
#define GUITABS_MANAGER_HPP


#include "datatypes.hpp"
#include "cstdmf/singleton.hpp"
#include <list>


namespace GUITABS
{


/**
 *  This singleton class is the only class accesible by the user of the tearoff
 *  panel framework. The programmer must first register it's content factories
 *  and insert a dock into his mainFrame (using registerFactory and insertDock)
 *  in any order, and the the user can load a configuration file or insert
 *  panels manually to actually create and display the desired panels on the
 *  screen (using load or insertPanel). It is required that the programmer
 *  calls removeDock() on exit, before destroying it's main frame and view
 *  windows. It is recommended that the mainView window doesn't have a border.
 *  For more information, please see content.hpp and content_factory.hpp
 */
class Manager : public Singleton<Manager>, public ReferenceCount
{
public:
	Manager();
	~Manager();

	/**
	 *  Register a factory object that will be responsible for creating the
	 *  actual content panel.
	 *  @param factory the ContentFactory-derived object.
	 *  @return true if the factory was successfully registered, false otherwise.
	 *  @see ContentFactory
	 */
	bool registerFactory( ContentFactoryPtr factory );

	/**
	 *  Insert/register the MainFrame and the main view windows. The Manager
	 *  will dock panels in the client area of the MainFrame window, and will
	 *  resize/reposition the main view window to make room for docked panels.
	 *  The main frame window must be the parent of the main view window.
	 *  @param mainFrame the Main Frame window of the application. Must inherit from CFrameWnd.
	 *  @param mainFrame the Main View window (ie. the 3D window in WorldEditor). Must inherit from CWnd.
	 *  @return true if the windows were successfully registered, false otherwise.
	 */
	bool insertDock( CFrameWnd* mainFrame, CWnd* mainView );

	/**
	 *  Unregister the MainFrame and the main view windows. The Manager
	 *  will destroy all panels, tabs, content panels, and all other resources,
	 *  and will set the Main Frame window as the parent of the Main View window.
	 */
	void removeDock();

	/**
	 *  Utility method to create a panel displaying the content corresponding
	 *  to a registered factory with the code contentID. The contentID will be
	 *  searched for in the list of ContentFactory factories.
	 *  Ideally, panels should be created with the load() method and managed
	 *  automatically by the framework, but this method allows manual use.
	 *  IMPORTANT NOTE: Panel handles can be no longer valid in some situations
	 *  such as when the related panel is destroyed (i.e. after a remove dock).
	 *  To know if a Panel Handle is is still valid, use the isValid() method.
	 *  @param contentID ID of the content to display in the new panel.
	 *  @param insertAt enum indicating where the panel must be inserted.
	 *  @param destPanel destination panel to insert the panel into. If ommitted, the manager will layout the panel automatically.
	 *  @return a PanelHandle to the panel that was created, 0 otherwise.
	 *  @see ContentFactory, Content, Manager::isValid(), Manager::load(), Manager::save()
	 */
	PanelHandle insertPanel( const std::string contentID, InsertAt insertAt, PanelHandle destPanel = 0 );

	/**
	 *  Utility method to remove previously created panel by Panel Handle
	 *  Ideally, panels should be created with the load() method and managed
	 *  automatically by the framework, but this method allows manual use.
	 *  @param contentID ID of the content to display in the new panel.
	 *  @return true if the panel was removed, false otherwise.
	 *  @see ContentFactory, Content, Manager::load(), Manager::save()
	 */
	bool removePanel( PanelHandle panel );

	/**
	 *  Utility method to remove previously created panel by content ID.
	 *  Ideally, panels should be created with the load() method and managed
	 *  automatically by the framework, but this method allows manual use.
	 *  IMPORTANT NOTE: If there are several content objects with the same
	 *  contentID (for instance, panels that are cloned), this method will
	 *	remove them all.
	 *  @param contentID ID of the content to display in the new panel.
	 *  @return true if the panel was removed, false otherwise.
	 *  @see ContentFactory, Content, Manager::load(), Manager::save()
	 */
	bool removePanel( const std::string contentID );

	/**
	 *  Remove all panels.
	 *  @return true if the panels were removed, false otherwise.
	 *  @see ContentFactory, Content
	 */
	void removePanels();

	/*
	 *  Show or Hide a panel by it's Panel Handle.
	 *  @param panel the panel handle of the panel to show/hide
	 *  @param show true to show the panel, false to hide it
	 *  @see Manager::insertPanel(), Manager::removePanel()
	 */
	void showPanel( PanelHandle panel, bool show );

	/*
	 *  Show or Hide a panel/tab by it's contentID
	 *  IMPORTANT NOTE: If there are several content objects with the same
	 *  contentID (for instance, panels that are cloned), this method will
	 *	show/hide them all.
	 *  @param panel the panel handle of the panel to show/hide
	 *  @param show true to show the panel, false to hide it
	 *  @see Manager::insertPanel()
	 */
	void showPanel( const std::string contentID, bool show );

	/*
	 *  Query if a content is visible in one or more panels.
	 *  IMPORTANT NOTE: There can bee several content objects with the same
	 *  contentID (for instance, panels that are cloned).
	 *  @return true to show the panel, false to hide it
	 *  @see Manager::insertPanel()
	 */
	bool isContentVisible( const std::string contentID );

	/*
	 *  Return's a Content object pointer by it's contentID, or NULL if no
	 *	Panel/Tab has been inserted with that contentID. The application can
	 *	then cast the pointer back to the original type declared in the
	 *	corresponding ContentFactory derived class.
	 *  IMPORTANT NOTE: this method returns the first instance of the Content,
	 *	so in the case of multiple instances of the same content you should use
	 *	the getContents method.
	 *  @param contentID identifier of the desired Content.
	 *  @param index index, when there's more than one 'contentID' panel.
	 *  @return a pointer to the Content, or NULL if no contentID panel exists.
	 *  @see Manager::insertPanel()
	 */
	Content* getContent( const std::string contentID, int index = 0 );

	/*
	 *  Finds if a Panel Handle is still valid or not. For more info, read the
	 *  insertPanel important notes section.
	 *  @param panel the panel handle of the panel to test
	 *  @see Manager::insertPanel()
	 */
	bool isValid( PanelHandle panel );

	/**
	 *  Utility method to see if the dock is visible.
	 *  @param show true if the dock is visible, false otherwise.
	 */
	bool isDockVisible();

	/**
	 *  Utility method to show or hide all docked panels.
	 *  @param show true to show the dock and all panels, false to hide.
	 */
	void showDock( bool show );

	/**
	 *  Utility method to show or hide all floating panel windows.
	 *  @param show true to show all panels, false to hide all panels.
	 */
	void showFloaters( bool show );

	/**
	 *  Utility method to send a message to all content windows.
	 *  @param msg message id.
	 *  @param WPARAM additional message-dependent information.
	 *  @param LPARAM additional message-dependent information.
	 */
	void broadcastMessage( UINT msg, WPARAM wParam, LPARAM lParam );

	/**
	 *  Utility method to send a message to content windows with ID contentID.
	 *  @param contentID content identifier.
	 *  @param msg message id.
	 *  @param WPARAM additional message-dependent information.
	 *  @param LPARAM additional message-dependent information.
	 */
	void sendMessage( const std::string contentID, UINT msg, WPARAM wParam, LPARAM lParam );

	/**
	 *  Load all panels previously saved. This will reload the last saved panels
	 *  with their corresponding insert position, floating state, visibility, etc.
	 *  This function is tipically called on app's startup, after creating the
	 *  mainframe window and the main 3D view window.
	 *  @param fname file to load layout from.
	 *  @return true if successful, false otherwise 
	 */
	bool load( const std::string fname = "" );

	/**
	 *  Load all panels previously saved. This will reload the las panels
	 *  with their corresponding insert position, floating state, etc.
	 *  This function is tipically called on app's exit, before destroying the
	 *  mainframe window and the main 3D view window.
	 *  @param fname file to save into, or "" to save to the last loaded file.
	 *  @return true if successful, false otherwise 
	 */
	bool save( const std::string fname = "" );

	/**
	 *  Clone a tab ( a content/panelhandle ) to a new dialog.
	 *  @param content pointer to the panel handle (content) to be cloned
	 *  @param x desired X position of the newly created panel
	 *  @param y desired Y position of the newly created panel
	 *  @return panel handle of the new panel, 0 if no panel could be created.
	 */
	PanelHandle clone( PanelHandle content, int x, int y );

private:
	DockPtr dock_;
	DragManagerPtr dragMgr_;
	std::list<ContentFactoryPtr> factoryList_;
	typedef std::list<ContentFactoryPtr>::iterator ContentFactoryItr;
	std::string lastLayoutFile_;

	// Utility methods, for friend classes internal use only.
	// In the case of the dock() method, this was done in order to avoid
	// having a pointer to the Dock inside each of the friend classes.
	friend Tab;
	friend DragManager;
	friend Panel;
	friend SplitterNode;
	friend Floater;
	friend DockedPanelNode;
	friend ContentContainer;
	/*
	 *  Create a Content derived-object from a previously registered
	 *  ContentFactory-derived object matching the contentID.
	 *  @see Tab
	 */
	ContentPtr createContent( const std::string contentID );

	/*
	 *  Get the main Dock object of the Manager.
	 *  @see Dock
	 */
	DockPtr dock();

	/*
	 *  Get the DragManager object.
	 *  @see DragManager
	 */
	DragManagerPtr dragManager();
};


} // namespace

#endif // GUITABS_MANAGER_HPP
