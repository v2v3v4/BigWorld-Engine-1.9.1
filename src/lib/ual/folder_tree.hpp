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
 *	FolderTree: Inherits from CTreeCtrl to make a folder tree control with
 *	drag & drop support.
 */

#ifndef FOLDER_TREE_HPP
#define FOLDER_TREE_HPP

#include "atlimage.h"
#include "cstdmf/smartpointer.hpp"
#include "asset_info.hpp"
#include "xml_item_list.hpp"
#include "filter_holder.hpp"
#include "thumbnail_manager.hpp"


// Datatypes
class VFolder;
typedef SmartPointer<VFolder> VFolderPtr;

class VFolderItemData;
typedef SmartPointer<VFolderItemData> VFolderItemDataPtr;

class VFolderProvider;
typedef SmartPointer<VFolderProvider> VFolderProviderPtr;

class FolderTree;

class ListProvider;
typedef SmartPointer<ListProvider> ListProviderPtr;


// VFolderProvider
class VFolderProvider : public ReferenceCount
{
public:
	VFolderProvider() : filterHolder_( 0 ), listProvider_( 0 )
		{}
	virtual ~VFolderProvider()
		{}

	virtual bool startEnumChildren( const VFolderItemDataPtr parent ) = 0;
	virtual VFolderItemDataPtr getNextChild( ThumbnailManager& thumbnailManager, CImage& img ) = 0;
	virtual void setFolderTree( FolderTree* folderTree )
		{ folderTree_ = folderTree; }
	virtual void setFilterHolder( FilterHolder* filterHolder )
		{ filterHolder_ = filterHolder; }
	virtual void setListProvider( ListProviderPtr listProvider )
		{ listProvider_ = listProvider; }
	virtual ListProviderPtr getListProvider()
		{ return listProvider_; }
	virtual void getThumbnail( ThumbnailManager& thumbnailManager, VFolderItemDataPtr data, CImage& img ) = 0;
	virtual const std::string getDescriptiveText( VFolderItemDataPtr data, int numItems, bool finished ) = 0;
	virtual bool getListProviderInfo(
		VFolderItemDataPtr data,
		std::string& retInitIdString,
		ListProviderPtr& retListProvider,
		bool& retItemClicked ) = 0;

protected:
	enum ItemGroup {
		GROUP_FOLDER,
		GROUP_ITEM
	};

	FolderTree* folderTree_;
	FilterHolder* filterHolder_;
	ListProviderPtr listProvider_;
};


// Virtual Folder
class VFolder : public ReferenceCount
{
public:
	VFolder(
		VFolderPtr parent,
		std::string name, HTREEITEM item,
		VFolderProviderPtr provider,
		bool expandable,
		bool sortSubFolders,
		XmlItemVec* customItems,
		void* data,
		bool subVFolders ) :
		parent_( parent ),
		name_( name ),
		item_( item ),
		provider_( provider ),
		sortSubFolders_( sortSubFolders ),
		expandable_( expandable ),
		customItems_( customItems ),
		data_( data ),
		subVFolders_( subVFolders )
	{};

	std::string getName() { return name_; };
	HTREEITEM getItem() { return item_; };
	VFolderProviderPtr getProvider() { return provider_; };
	bool isExpandable() { return expandable_; };
	XmlItemVec* getCustomItems() { return customItems_; };
	void* getData() { return data_; };
	void setSortSubFolders( bool sortSubFolders ) { sortSubFolders_ = sortSubFolders; };
	bool getSortSubFolders() { return sortSubFolders_; };
	bool subVFolders() { return subVFolders_; };

private:
	VFolderPtr parent_;
	std::string name_;
	HTREEITEM item_;
	VFolderProviderPtr provider_;
	bool expandable_;
	bool sortSubFolders_;
	XmlItemVec* customItems_;
	void* data_;
	bool subVFolders_;
};


// additional item data per item. Default behaviour = vfolder
class VFolderItemData : public ReferenceCount
{
public:
	VFolderItemData(
		VFolderProviderPtr provider,
		const AssetInfo& assetInfo,
		int group,
		bool expandable ) :
		provider_( provider ),
		assetInfo_( assetInfo ),
		group_( group ),
		expandable_( expandable ),
		custom_( false ),
		item_( 0 ),
		vfolder_( 0 )
		{};
	virtual ~VFolderItemData() {};
	virtual bool handleDuplicate( VFolderItemDataPtr data ) { return false; }; // accepts duplicates
	virtual VFolderProviderPtr getProvider() { return provider_; };
	virtual AssetInfo& assetInfo() { return assetInfo_; };
	virtual int getGroup() { return group_; };
	virtual bool getExpandable() { return expandable_; };
	virtual bool isCustomItem() { return custom_; };
	virtual void isCustomItem( bool custom ) { custom_ = custom; };
	virtual bool isVFolder() const { return !!vfolder_; }; // derived classes should return false here
	virtual HTREEITEM getTreeItem() { return item_; };
	virtual void setTreeItem( HTREEITEM item ) { item_ = item; };
	virtual VFolderPtr getVFolder() { return vfolder_; };
	virtual void setVFolder( VFolderPtr vfolder ) { vfolder_ = vfolder; };

private:
	VFolderProviderPtr provider_;
	AssetInfo assetInfo_;
	HTREEITEM item_;
	int group_;
	bool expandable_;
	bool custom_;
	VFolderPtr vfolder_;
};

// event handling
class FolderTreeEventHandler
{
public:
	virtual void folderTreeSelect( VFolderItemData* data ) = 0;
	virtual void folderTreeStartDrag( VFolderItemData* data ) = 0;
	virtual void folderTreeItemDelete( VFolderItemData* data ) = 0;
	virtual void folderTreeRightClick( VFolderItemData* data ) = 0;
	virtual void folderTreeDoubleClick( VFolderItemData* data ) = 0;
};

// main folder tree class
class FolderTree : public CTreeCtrl, public ThumbnailUpdater
{
public:
	explicit FolderTree( ThumbnailManagerPtr thumbnailManager );
	virtual ~FolderTree();

	int addIcon( HICON image );
	int addBitmap( HBITMAP image );
	void removeImage( int index );

	void init();
	VFolderPtr addVFolder(
		const std::string& displayName,
		VFolderProviderPtr provider,
		VFolderPtr parent, HICON icon, HICON iconSel,
		bool show,
		bool expandable,
		XmlItemVec* customItems,
		void* data,
		bool subVFolders );
	void removeVFolder( const std::string& displayName, HTREEITEM curItem = TVI_ROOT );
	void removeVFolder( HTREEITEM item, HTREEITEM curItem = TVI_ROOT );
	void clear();

	void setSortVFolders( bool sort );
	void setSortSubFolders( bool sort );

	void setEventHandler( FolderTreeEventHandler* eventHandler );

	void setGroupIcons( int group, HICON icon, HICON iconSel );
	void setExtensionsIcons( std::string extensions, HICON icon, HICON iconSel );

	VFolderPtr getVFolder( VFolderItemData* data );

	HTREEITEM getItem( VFolderItemData* data, HTREEITEM item = TVI_ROOT );

	std::string getVFolderOrder( const std::string orderStr = "", HTREEITEM item = TVI_ROOT );
	void setVFolderOrder( const std::string orderStr );
	void moveVFolder( VFolderPtr vf1, VFolderPtr vf2 );

	int getLevelCount( HTREEITEM item = TVI_ROOT );

	void freeSubtreeData( HTREEITEM item );
	void refreshVFolder( VFolderPtr vfolder );
	void refreshVFolders( VFolderProviderPtr provider = 0, HTREEITEM item = TVI_ROOT );
	VFolderPtr getVFolder( const std::string& name, bool strict = true, HTREEITEM item = TVI_ROOT );
	void getVFolders( std::vector<HTREEITEM>& items, HTREEITEM item = TVI_ROOT );
	void selectVFolder( const std::string& name );
	typedef bool (*ItemTestCB)( HTREEITEM item, void* testData );
	VFolderPtr getVFolderCustom( ItemTestCB test, void* testData, bool strict = true, HTREEITEM item = TVI_ROOT );
	void selectVFolderCustom( ItemTestCB test, void* testData );

	bool isDragging();
	void showDrag( bool show );
	void updateDrag( int x, int y );
	void endDrag();

	bool updateItem( const AssetInfo& assetInfo, HTREEITEM item = TVI_ROOT );

	// ThumbnailUpdater interface implementation
	void thumbManagerUpdate( const std::string& longText );

private:
	static const int IMAGE_SIZE = 16;
	struct GroupIcons {
		int group;
		int icon;
		int iconSel;
	};
	struct ExtensionsIcons {
		std::vector<std::string> extensions;
		int icon;
		int iconSel;
	};

	bool initialised_;
	bool sortVFolders_;
	bool sortSubFolders_;
	ThumbnailManagerPtr thumbnailManager_;
	CImageList imgList_;
	int vfolderIcon_;
	int vfolderIconSel_;
	int itemIcon_;
	int itemIconSel_;
	int firstImageIndex_;
	std::vector<VFolderItemDataPtr> itemDataHeap_;
	std::vector<int> unusedImages_;
	std::vector<ExtensionsIcons> extensionsIcons_;
	std::vector<GroupIcons> groupIcons_;
	FolderTreeEventHandler* eventHandler_;
	CImageList* dragImgList_;
	bool dragging_;

	void setItemData( HTREEITEM item, VFolderItemDataPtr data );

	void buildTree( HTREEITEM parentItem, VFolderProviderPtr provider );

	bool isStockIcon( int icon );
	void getGroupIcons( int group, int& icon, int& iconSel, bool expandable );
	void getExtensionIcons( const std::string& name, int& icon, int& iconSel );
	void sortSubTree( HTREEITEM item );
	bool expandItem( HTREEITEM item );

	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg void OnPaint();
	afx_msg void OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnSelChanged( NMHDR * pNotifyStruct, LRESULT* result );
	afx_msg void OnRightClick( NMHDR * pNotifyStruct, LRESULT* result );
	afx_msg void OnItemExpanding( NMHDR * pNotifyStruct, LRESULT* result );
	afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLButtonDblClk( UINT nFlags, CPoint point );
	DECLARE_MESSAGE_MAP()
};


#endif // FOLDER_TREE_HPP
