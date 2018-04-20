/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#pragma once

#include "ual_resource.h"
#include "ual_vfolder_loader.hpp"
#include "cstdmf/smartpointer.hpp"
#include "resmgr/datasection.hpp"
#include "guitabs/nice_splitter_wnd.hpp"
#include "guitabs/guitabs_content.hpp"
#include "folder_tree.hpp"
#include "list_cache.hpp"
#include "filter_spec.hpp"
#include "smart_list_ctrl.hpp"
#include "list_file_provider.hpp"
#include "list_xml_provider.hpp"
#include "vfolder_file_provider.hpp"
#include "vfolder_xml_provider.hpp"
#include "filters_ctrl.hpp"
#include "xml_item_list.hpp"
#include "ual_manager.hpp"
#include "common/popup_menu.hpp"
#include "search_edit.hpp"

// Forward
class FilterHolder;

// UalFolderData
class UalFolderData : public ReferenceCount
{
public:
	std::string internalTag_;	// used to refer to vFolders from within the code
	int thumbSize_;
	int originalThumbSize_;
	bool showInList_;
	bool multiItemDrag_;
	std::vector<std::string> disabledFilters_;
	XmlItemVec customItems_;
	std::string idleText_;
};
typedef SmartPointer<UalFolderData> UalFolderDataPtr;


// UalDialog dialog
class UalDialog :
	public CDialog,
	public GUITABS::Content,
	public FolderTreeEventHandler,
	public SmartListCtrlEventHandler,
	public FiltersCtrlEventHandler
{
public: // GUITABS Content Methods:
	static const std::string contentID;
	std::string getContentID() { return contentID; };
	std::string getDisplayString() { return dlgLongCaption_; };
	std::string getTabDisplayString() { return dlgShortCaption_; };
	HICON getIcon() { return hicon_; };
	CWnd* getCWnd() { return this; };
	void getPreferredSize( int& w, int& h )
	{
		w = preferredWidth_;
		h = preferredHeight_;
	};
	bool isClonable() { return true; };
	GUITABS::ContentPtr clone();
	void handleRightClick( int x, int y );
	void OnOK() { };
	void OnCancel() { };
	void PostNcDestroy() { };
	bool load( DataSectionPtr section );
	bool save( DataSectionPtr section );
	OnCloseAction onClose( bool isLastContent ) { return isLastContent?CONTENT_HIDE:CONTENT_DESTROY; };

public:
	UalDialog( const std::string& configFile = "" );   // standard constructor
	virtual ~UalDialog();

	static void registerVFolderLoader( UalVFolderLoaderPtr loader );
	static void fini();

	bool loadConfig( const std::string fname = "" );
	void saveConfig();

	void setListStyle( SmartListCtrl::ViewStyle style );
	void setLayout( bool vertical, bool resetLastSize = false );

	void setShortCaption( std::string caption ) { dlgShortCaption_ = caption; };
	void setLongCaption( std::string caption ) { dlgLongCaption_ = caption; };
	void setIcon( HICON hicon ) { hicon_ = hicon; };

	void updateItem( const std::string& longText );
	static bool vfolderFindByTag( HTREEITEM item, void* testData );
	void showItem( const std::string& vfolder, const std::string& longText );

// Dialog Data
	enum { IDD = IDD_UAL };

	// controls event handling
	void favouritesChanged();
	void historyChanged();
	void folderTreeSelect( VFolderItemData* data );
	void folderTreeStartDrag( VFolderItemData* data );
	void folderTreeItemDelete( VFolderItemData* data );
	void folderTreeRightClick( VFolderItemData* data );
	void folderTreeDoubleClick( VFolderItemData* data );
	void listLoadingUpdate();
	void listLoadingFinished();
	void listItemSelect();
	void listItemDelete();
	void listDoubleClick( int index );
	void listStartDrag( int index );
	void listItemRightClick( int index );
	void listItemToolTip( int index, std::string& info );
	void filterClicked( const char* name, bool pushed, void* data );

	// used by the loaders
	ListFileProviderPtr fileListProvider() { return fileListProvider_; }
	ListXmlProviderPtr xmlListProvider() { return xmlListProvider_; }
	ListXmlProviderPtr historyListProvider() { return historyListProvider_; }
	ListXmlProviderPtr favouritesListProvider() { return favouritesListProvider_; }
	VFolderXmlProvider* historyFolderProvider() { return historyFolderProvider_.getObject(); }
	VFolderXmlProvider* favouritesFolderProvider() { return favouritesFolderProvider_.getObject(); }
	void historyFolderProvider( VFolderXmlProvider* prov ) { historyFolderProvider_ = prov; }
	void favouritesFolderProvider( VFolderXmlProvider* prov ) { favouritesFolderProvider_ = prov; }

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	BOOL OnInitDialog();
	BOOL PreTranslateMessage( MSG* msg );

	afx_msg HBRUSH OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor );
	afx_msg void OnSetFocus( CWnd* pOldWnd );
	afx_msg void OnKillFocus( CWnd* pNewWnd );
	afx_msg void OnDestroy();
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnSearchChange();
	afx_msg void OnSearchFilters();
	afx_msg void OnSearchClose();
	afx_msg void OnGUIManagerCommand(UINT nID);
	DECLARE_MESSAGE_MAP()

private:
	friend class UalManager;
	friend class UalVFolderLoader;

	std::string configFile_;
	std::string lastLanguage_;
	std::string dlgShortCaption_;
	std::string dlgLongCaption_;
	HICON hicon_;
	int preferredWidth_;
	int preferredHeight_;
	bool layoutVertical_;
	int layoutLastRowSize_;
	int layoutLastColSize_;
	int defaultSize_;
	CToolBarCtrl toolbar_;
	FolderTree folderTree_;
	SmartListCtrl smartList_;
	CStatic searchBk_;
	SearchEdit search_;
	std::string searchIdleText_;
	CStatic searchFilters_;
	CStatic searchClose_;
	CStatic statusBar_;
	// need to have the list providers declared per-ual
	ListFileProviderPtr fileListProvider_;
	ListXmlProviderPtr xmlListProvider_;
	ListXmlProviderPtr historyListProvider_;
	ListXmlProviderPtr favouritesListProvider_;
	// have to have folder providers for history and favourites, for refreshing
	typedef SmartPointer<VFolderXmlProvider> VFolderXmlProviderPtr;
	VFolderXmlProviderPtr historyFolderProvider_;
	VFolderXmlProviderPtr favouritesFolderProvider_;
	std::vector<UalFolderDataPtr> folderData_;
	typedef NiceSplitterWnd SplitterBarType; // Use CSplitterWnd if NiceSplitterWnd (in GUITABS) not available (will need to rewrite code)
	SplitterBarType* splitterBar_;
	FiltersCtrl filtersCtrl_;
	FilterHolder filterHolder_;
	bool showFilters_;
	HWND lastFocus_;
	std::vector<std::string> excludeVFolders_; // override UAL's config file for this folders: don't load them
	DataSectionPtr customVFolders_; // section containing customVFolders data, only used when cloning
	UalItemInfo lastItemInfo_; // stores data from the last drag and drop operation when cloning using drag&drop
	std::string lastListInit_; // used to avoid flickering in the list when clicking items in the tree
	CToolTipCtrl toolTip_;
	std::string delayedListShowItem_;

	HICON iconFromXml( DataSectionPtr section, std::string item );
	void loadMain( DataSectionPtr section );
	void loadToolbar( DataSectionPtr section );
	void loadFilters( DataSectionPtr section );
	void loadVFolders( DataSectionPtr section, const std::string& loadOneName = "", VFolderPtr parent = 0 );
	VFolderPtr loadVFolder( DataSectionPtr section, const std::string& loadOneName, VFolderPtr parent, DataSectionPtr customData = 0 );
	void loadVFolderExcludeInfo( DataSectionPtr section );
	void loadCustomVFolders( DataSectionPtr section, const std::string& loadOneName = "" );
	VFolderPtr loadFromBaseVFolder( DataSectionPtr section, const std::string& baseName, DataSectionPtr customData, VFolderPtr parent = 0 );

	void buildSmartListFilters();

	void buildFiltersCtrl();
	void buildFolderTree();
	void buildSmartList();

	bool guiActionRefresh();
	bool guiActionLayout();

	int createContextMenu( UalPopupMenuItems& menuItems );
	void showItemContextMenu( UalItemInfo* ii );
	void showContextMenu( VFolderItemData* data );
	void fillAssetsVectorFromList( std::vector<AssetInfo>& assets );

	void updateFiltersImage();
	void adjustSplitterSize( int width, int height );
	void adjustSearchSize( int width, int height );
	void adjustFiltersSize( int width, int height );

	void callbackVFolderSelect( VFolderItemData* data );
	void refreshStatusBar();
	void setFolderTreeStatusBar( VFolderItemData* data );

	void getItemInfo( int index, AssetInfo& assetInfo );
	void scrollWindow( CWnd* wnd, CPoint pt );
	void handleDragMouseMove( UalItemInfo& ii, const CPoint& srcPt, bool isScreenCoords = false );
	void dragLoop( std::vector<AssetInfo>& assetsInfo, bool isFolder = false, void* folderExtraData = 0 );
	void stopDrag();
	void cancelDrag();
	void updateSmartListDrag( const UalItemInfo& itemInfo, bool endDrag );
	void updateFolderTreeDrag( const UalItemInfo& itemInfo, bool endDrag );
	bool updateDrag( const UalItemInfo& itemInfo, bool endDrag );
	void resetDragDropTargets();

	void setStatusText( const std::string& text );

	void error( const std::string& msg );
};

// UalDialog Factory class
class UalDialogFactory : public GUITABS::ContentFactory
{
public:
	UalDialogFactory() {};
	GUITABS::ContentPtr create()
	{
		return createUal( UalManager::instance().getConfigFile() );
	};
	UalDialog* createUal( const std::string& configFile )
	{
		UalDialog* newUal = new UalDialog( configFile );
		newUal->Create( UalDialog::IDD );
		return newUal;
	};
	std::string getContentID() { return UalDialog::contentID; };
};
