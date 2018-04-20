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
 *	GUI Tearoff panel framework - Dock class
 */

#ifndef GUITABS_DOCK_HPP
#define GUITABS_DOCK_HPP

namespace GUITABS
{

/**
 *  This class manages all panel layout functionality, keeping track of
 *  the tree structure of the nested splitter windows that contain docked
 *  panels as well as keeping a list of all panels, docked and floating.
 *  There is only one Dock in the current implementation, but it should be
 *  easy to extend Manager in order to get multiple Docks in an application,
 *  with the restriction that there can only be one Dock per app frame window.
 */
typedef std::list<PanelPtr>::iterator PanelItr;

class Dock : public ReferenceCount
{
public:

	Dock( CFrameWnd* mainFrame, CWnd* mainView );
	~Dock();

	CFrameWnd* getMainFrame();
	CWnd* getMainView();

	bool empty();

	void dockTab( PanelPtr panel, TabPtr tab, CWnd* destPanel, InsertAt insertAt, int srcX, int srcY, int dstX, int dstY );

	/*
	 *  Dock a panel into another panel, or float it if insertAt = FLOATING, or
	 *  insert it as a tab if insertAt = TAB.
	 *  @param panel the panel to insert.
	 *  @param destPanel the node of the destination panel to dock panel into.
	 *  @param insertAt insert position in the layout.
	 *  @param srcX x position of the cursor when started to drag.
	 *  @param srcY y position of the cursor when started to drag.
	 *  @param dstX x position of the cursor when ended dragging.
	 *  @param dstY y position of the cursor when ended to dragging.
	 *  @see Dock, Panel, Floater, DragManager
	 */
	void dockPanel( PanelPtr panel, CWnd* destPanel, InsertAt insertAt, int srcX, int srcY, int dstX, int dstY );

	void floatPanel( PanelPtr panel, int srcX, int srcY, int dstX, int dstY );
	
	void attachAsTab( PanelPtr panel, CWnd* destPanel );

	PanelPtr insertPanel( const std::string contentID, PanelHandle destPanel, InsertAt insertAt );
	const PanelItr removePanel( PanelPtr panel );

	/**
	 *	This removes all tabs/panels that contain the specified contentID
	 */
	void removePanel( const std::string contentID );

	PanelPtr getPanelByWnd( CWnd* ptr );
	PanelPtr getPanelByHandle( PanelHandle handle );

	bool load( DataSectionPtr section );
	bool save( DataSectionPtr section );

	/*
	 *  Set the current active panel.
	 *  @see Dock, Panel
	 */
	void setActivePanel( PanelPtr panel );

	/*
	 *  Show or Hide a panel.
	 *  @see Dock, Panel
	 */
	void showPanel( PanelPtr panel, bool show );

	/**
	 *	This shows/hides all tabs/panels that contain the specified contentID
	 */
	void showPanel( const std::string contentID, bool show );

	/**
	 *	This shows/hides the tab/panel that contains the specified Content
	 */
	void showPanel( ContentPtr content, bool show );

	ContentPtr getContent( const std::string contentID, int index = 0 );

	bool isContentVisible( const std::string contentID );

	/*
	 *  Get a DockNode by a point on the screen.
	 *  @see Dock, DockNode
	 */
	DockNodePtr getNodeByPoint( int x, int y );

	/*
	 *  Toggle the panel's docked/floating state, using the last saved dock
	 *  or floating layouts.
	 *  @see Dock, Panel
	 */
	void togglePanelPos( PanelPtr panel );

	/*
	 *  Toggle the tab's docked/floating state, using the last saved dock
	 *  or floating layouts of the panel.
	 *  @see Dock, Panel, Tab
	 */
	void toggleTabPos( PanelPtr panel, TabPtr tab );

	/*
	 *  Get the Floater a panel is in, by the panel's CWnd.
	 *  @see Dock, Floater
	 */
	FloaterPtr getFloaterByWnd( CWnd* ptr );
	
	bool isDockVisible();

	void showDock( bool show );

	void showFloaters( bool show );

	/*
	 *  Destroy a floater. Needed when destroying a floater with multiple
	 *  panels.
	 *  @see Dock, Floater
	 */
	void destroyFloater( FloaterPtr floater );

	void broadcastMessage( UINT msg, WPARAM wParam, LPARAM lParam );

	void sendMessage( const std::string contentID, UINT msg, WPARAM wParam, LPARAM lParam );

	int getContentCount( const std::string contentID );
	PanelPtr detachTabToPanel( PanelPtr panel, TabPtr tab );

	void rollupPanel( PanelPtr panel );

	// indices are used when loading/saving to reference a panel in the file
	int getPanelIndex( PanelPtr panel );
	PanelPtr getPanelByIndex( int index );

	// A DockNode factory that reads a section and creates the appropriate
	// docknode object accordingly. Public because it's used by SplitterNode too
	DockNodePtr nodeFactory( DataSectionPtr section );

private:
	bool dockVisible_;
	DockNodePtr dockTreeRoot_;
	std::list<PanelPtr> panelList_;
	std::list<FloaterPtr> floaterList_;
	typedef std::list<FloaterPtr>::iterator FloaterItr;
	CFrameWnd* mainFrame_;
	int originalMainViewID_;
	CWnd* mainView_;

	void insertPanelIntoPanel( PanelPtr panel, CWnd* destPanel, InsertAt insertAt );
	bool getNodeByWnd( CWnd* ptr, DockNodePtr& childNode, DockNodePtr& parentNode );
	void removeNodeByWnd( CWnd* ptr );
	void getLeaves( DockNodePtr node, std::vector<DockNodePtr>& leaves );
	bool buildPanelPosList( bool docked, DockNodePtr node, PanelPtr panel );
	void savePanelDockPos( PanelPtr panel );
	void restorePanelDockPos( PanelPtr panel );
	void copyPanelRestorePosToTab( PanelPtr src, PanelPtr dstTab );
};


} // namespace

#endif // GUITABS_DOCK_HPP