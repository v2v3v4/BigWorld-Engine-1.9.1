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

#include "pch.hpp"
#include <vector>
#include <string>
#include "ual_resource.h"
#include "folder_tree.hpp"
#include "list_cache.hpp"
#include "smart_list_ctrl.hpp"

#include "common/string_utils.hpp"
#include "resmgr/bwresource.hpp"

// for flicker free redrawing
#include "controls/memdc.hpp"


// FolderTree

FolderTree::FolderTree( ThumbnailManagerPtr thumbnailManager ) :
	initialised_( false ),
	sortVFolders_( true ),
	sortSubFolders_( true ),
	thumbnailManager_( thumbnailManager ),
	eventHandler_( 0 ),
	dragImgList_( 0 ),
	dragging_( false ),
	vfolderIcon_( -1 ),
	vfolderIconSel_( -1 ),
	itemIcon_( -1 ),
	itemIconSel_( -1 ),
	firstImageIndex_( 0 )
{
	MF_ASSERT( thumbnailManager_ != NULL );
}

FolderTree::~FolderTree()
{
	thumbnailManager_->resetPendingRequests( this );

	imgList_.DeleteImageList();
}

void FolderTree::setEventHandler( FolderTreeEventHandler* eventHandler )
{
	eventHandler_ = eventHandler;
}

int FolderTree::addIcon( HICON image )
{
	if ( !unusedImages_.empty() )
	{
		int i = *unusedImages_.begin();
		unusedImages_.erase( unusedImages_.begin() );
		imgList_.Replace( i, image );
		return i;
	}
	else
		return imgList_.Add( image );
}

int FolderTree::addBitmap( HBITMAP image )
{
	if ( !unusedImages_.empty() )
	{
		int i = *unusedImages_.begin();
		unusedImages_.erase( unusedImages_.begin() );
		CImage mask;
		mask.Create( IMAGE_SIZE, IMAGE_SIZE, 24 );
		CDC* dc = CDC::FromHandle( mask.GetDC() );
		dc->FillSolidRect( 0, 0, IMAGE_SIZE, IMAGE_SIZE, RGB( 0, 0, 0 ) );
		dc->Detach();
		mask.ReleaseDC();
		imgList_.Replace( i, CBitmap::FromHandle( image ), CBitmap::FromHandle( mask ) );
		return i;
	}
	else
		return imgList_.Add( CBitmap::FromHandle( image ), (CBitmap*)0 );
}

void FolderTree::removeImage( int index )
{
	if ( index >= 0 && index >= firstImageIndex_ && index < imgList_.GetImageCount() )
		unusedImages_.push_back( index );
}

void FolderTree::init()
{
	if ( initialised_ )
		return;

	imgList_.Create( IMAGE_SIZE, IMAGE_SIZE, ILC_COLOR24|ILC_MASK, 2, 32 );
	imgList_.SetBkColor( ( GetBkColor() == -1 ) ? GetSysColor( COLOR_WINDOW ) : GetBkColor() );
	addIcon( AfxGetApp()->LoadIcon( IDI_UALFOLDER ) );
	addIcon( AfxGetApp()->LoadIcon( IDI_UALFOLDERSEL ) );
	addIcon( AfxGetApp()->LoadIcon( IDI_UALFILE ) );
	addIcon( AfxGetApp()->LoadIcon( IDI_UALFILESEL ) );

	SetImageList( &imgList_, TVSIL_NORMAL );

	int idx = 0;
	vfolderIcon_ = idx++;
	vfolderIconSel_ = idx++;
	itemIcon_ = idx++;
	itemIconSel_ = idx++;
	firstImageIndex_ = idx;
}

void FolderTree::setGroupIcons( int group, HICON icon, HICON iconSel )
{
	std::vector<GroupIcons>::iterator i;
	for( i = groupIcons_.begin(); i != groupIcons_.end(); ++i )
	{
		if ( (*i).group == group )
			break;
	}

	if ( i == groupIcons_.end() )
	{
		GroupIcons gi;
		gi.group = group;
		gi.icon = vfolderIcon_;
		gi.iconSel = vfolderIconSel_;
		groupIcons_.push_back( gi );
		i = groupIcons_.end();
		--i;
	}
	if ( icon )
		(*i).icon = addIcon( icon );

	if ( iconSel )
		(*i).iconSel = addIcon( iconSel );
}

bool FolderTree::isStockIcon( int icon )
{
	if ( icon < 2 )
		return true;
	for( std::vector<GroupIcons>::iterator i = groupIcons_.begin();
		i != groupIcons_.end(); ++i )
		if ( (*i).icon == icon || (*i).iconSel == icon )
			return true;
	for( std::vector<ExtensionsIcons>::iterator i = extensionsIcons_.begin();
		i != extensionsIcons_.end(); ++i )
		if ( (*i).icon == icon || (*i).iconSel == icon )
			return true;
	return false;
}

void FolderTree::getGroupIcons( int group, int& icon, int& iconSel, bool expandable )
{
	for( std::vector<GroupIcons>::iterator i = groupIcons_.begin(); i != groupIcons_.end(); ++i )
	{
		if ( (*i).group == group )
		{
			icon = (*i).icon;
			iconSel = (*i).iconSel;
			return;
		}
	}
	if ( expandable )
	{
		icon = vfolderIcon_;
		iconSel = vfolderIconSel_;
	}
	else
	{
		icon = itemIcon_;
		iconSel = itemIconSel_;
	}
}

void FolderTree::setExtensionsIcons( std::string extensions, HICON icon, HICON iconSel )
{
	ExtensionsIcons extIcons;

	StringUtils::vectorFromString( extensions, extIcons.extensions );
	
	extIcons.icon = vfolderIcon_;
	extIcons.iconSel = vfolderIconSel_;

	if ( icon )
		extIcons.icon = addIcon( icon );

	if ( iconSel )
		extIcons.iconSel = addIcon( iconSel );

	extensionsIcons_.push_back( extIcons );
}

void FolderTree::getExtensionIcons( const std::string& name, int& icon, int& iconSel )
{
	icon = vfolderIcon_;
	iconSel = vfolderIconSel_;

	std::string ext = BWResource::getExtension( name );

	if ( !ext.length() )
		return;

	StringUtils::toLowerCase( ext );

	for( std::vector<ExtensionsIcons>::iterator i = extensionsIcons_.begin(); i != extensionsIcons_.end(); ++i )
	{
		for( std::vector<std::string>::iterator j = (*i).extensions.begin(); j != (*i).extensions.end(); ++j )
		{
			if ( (*j) == ext )
			{
				icon = (*i).icon;
				iconSel = (*i).iconSel;
				return;
			}
		}
	}
}

VFolderPtr FolderTree::getVFolder( VFolderItemData* data )
{
	while ( data && !data->isVFolder() )
	{
		HTREEITEM item = GetParentItem( data->getTreeItem() );
		if ( item )
			data = (VFolderItemData*)GetItemData( item );
		else
			data = 0;
	}

	if ( data )
		return data->getVFolder();	

	return 0;
}

HTREEITEM FolderTree::getItem( VFolderItemData* data, HTREEITEM item )
{
	if ( item != TVI_ROOT )
	{
		if ( (VFolderItemData*)GetItemData( item ) == data )
			return item;
	}

	HTREEITEM child = GetChildItem( item );
	while ( child )
	{
		HTREEITEM next = GetNextItem( child, TVGN_NEXT );
		item = getItem( data, child );
		if ( item )
			return item;
		child = next;
	}

	return 0;
}

std::string FolderTree::getVFolderOrder( const std::string orderStr, HTREEITEM item )
{
	std::string accum = orderStr;

	if ( !item )
		return accum;

	if ( item != TVI_ROOT )
	{
		VFolderItemData* data = (VFolderItemData*)GetItemData( item );
		if ( data && data->isVFolder() )
		{
			if ( !accum.empty() )
				accum += ";";
			accum += data->assetInfo().text();
		}
		else
			return accum;
	}

	HTREEITEM child = GetChildItem( item );
	while ( child )
	{
		accum = getVFolderOrder( accum, child );
		child = GetNextItem( child, TVGN_NEXT );
	}

	return accum;
}

static int CALLBACK FolderTreeOrderStrFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	if ( lParam1 == lParam2 )
		return 0;

	VFolderItemData* data1 = (VFolderItemData*)lParam1;
	VFolderItemData* data2 = (VFolderItemData*)lParam2;

	if ( !data1 || !data1->isVFolder() )
		return -1;
	if ( !data2 || !data2->isVFolder() )
		return 1;

	std::vector<std::string>* oi = (std::vector<std::string>*)lParamSort;

	int cnt1 = 0;
	int cnt2 = 0;
	for( int i = 0; i < (int)oi->size(); ++i )
		if ( oi->at(i) == data1->assetInfo().text() )
		{
			cnt1 = i;
			break;
		}
	for( int i = 0; i < (int)oi->size(); ++i )
		if ( oi->at(i) == data2->assetInfo().text() )
		{
			cnt2 = i;
			break;
		}

	return cnt1-cnt2;
}

void FolderTree::setVFolderOrder( const std::string orderStr )
{
	if ( orderStr.empty() )
		return;
	std::vector<std::string> oi;
	StringUtils::vectorFromString( orderStr, oi );
	TVSORTCB sortCB;
	sortCB.hParent = TVI_ROOT;
	sortCB.lpfnCompare = FolderTreeOrderStrFunc;
	sortCB.lParam = (LPARAM)&oi;
	SortChildrenCB( &sortCB );
}

void FolderTree::moveVFolder( VFolderPtr vf1, VFolderPtr vf2 )
{
	if ( !vf1 || vf1 == vf2 )
		return;

	std::vector<std::string> oi;
	StringUtils::vectorFromString( getVFolderOrder(), oi );

	for( std::vector<std::string>::iterator i = oi.begin(); i != oi.end(); ++i )
		if ( (*i) == vf1->getName() )
		{
			oi.erase( i );
			break;
		}

	if ( !vf2 )
		oi.push_back( vf1->getName() );
	else
	{
		bool added = false;
		for( std::vector<std::string>::iterator i = oi.begin(); i != oi.end(); ++i )
			if ( (*i) == vf2->getName() )
			{
				oi.insert( i, vf1->getName() );
				added = true;
				break;
			}
		if ( !added )
			oi.push_back( vf1->getName() );
	}

	setVFolderOrder( StringUtils::vectorToString( oi ) );
}


int FolderTree::getLevelCount( HTREEITEM item )
{
	int cnt = 0;
	HTREEITEM child = GetChildItem( item );
	while ( child )
	{
		cnt++;
		child = GetNextItem( child, TVGN_NEXT );
	}
	return cnt;
}

void FolderTree::freeSubtreeData( HTREEITEM item )
{
	HTREEITEM child = GetChildItem( item );
	while ( child )
	{
		freeSubtreeData( child );
		VFolderItemData* data = (VFolderItemData*)GetItemData( child );
		int icon = 0;
		int iconSel = 0;
		GetItemImage( child, icon, iconSel );
		if ( !isStockIcon( icon ) )
			removeImage( icon );
		if ( iconSel != icon && !isStockIcon( iconSel ))
			removeImage( iconSel );
		for( std::vector<VFolderItemDataPtr>::iterator i = itemDataHeap_.begin();
			i != itemDataHeap_.end();
			++i )
		{
			if ( (*i).getObject() == data )
			{
				itemDataHeap_.erase( i );
				SetItemData( child, 0 );
				break;
			}
		}
		child = GetNextItem( child, TVGN_NEXT );
	}
}

void FolderTree::refreshVFolder( VFolderPtr vfolder )
{
	if ( !vfolder || !vfolder->isExpandable() || !vfolder->getItem() )
		return;
	
	SetRedraw( FALSE );

	int scrollX = GetScrollPos( SB_HORZ );
	int scrollY = GetScrollPos( SB_VERT );

	HTREEITEM item = vfolder->getItem();

	// save selected item's parents
	std::vector<std::string> lastSelected;
	HTREEITEM sel = GetSelectedItem();
	while ( sel )
	{
		if ( sel == item )
			break; // found the vfolder, so the vfolder's subtree includes the selected item
		lastSelected.push_back( (LPCTSTR)GetItemText( sel ) );
		sel = GetParentItem( sel );
	}
	if ( !sel )
		lastSelected.clear(); // selected item not in the vfolder's subtree, so don't care

	UINT state = GetItemState( item, TVIS_EXPANDED );
//	if ( state & TVIS_EXPANDED )
//		Expand( item, TVE_COLLAPSE );
//	SetItemState( item, 0, TVIS_EXPANDEDONCE );

	// Erase subtree item's their mem gets released
	freeSubtreeData( item );

	// Erase the actual items from the subtree
	HTREEITEM child = GetChildItem( item );
	while ( child )
	{
		HTREEITEM next = GetNextItem( child, TVGN_NEXT );
		DeleteItem( child );
		child = next;
	}

	// Rebuild tree
	InsertItem( "***dummychild***", 0, 0, item );
//	if ( state & TVIS_EXPANDED )
//		Expand( item, TVE_EXPAND );
	if ( state & TVIS_EXPANDED )
		expandItem( item );

	if ( !lastSelected.empty() )
	{
		// the selected item was inside the vfolder's tree before reconstruction, so reselect
		child = item;
		HTREEITEM deepestNull = item;
		for( std::vector<std::string>::reverse_iterator i = lastSelected.rbegin();
			i != lastSelected.rend();
			++i )
		{
			// find the child by name
			state = GetItemState( child, TVIS_EXPANDED );
			if ( !(state & TVIS_EXPANDED) )
				Expand( child, TVE_EXPAND );
			child = GetChildItem( child );
			while ( child )
			{
				if ( (*i) == (LPCTSTR)GetItemText( child ) )
					break;
				child = GetNextItem( child, TVGN_NEXT );
			}
			if ( !child )
				break;
			deepestNull = child;
		}
		if ( deepestNull )
			SelectItem( deepestNull ); // select the deepest level of the tree found ( hopefully, the old selected item )
	}

	SetScrollPos( SB_HORZ, scrollX );
	SetScrollPos( SB_VERT, scrollY );

	SetRedraw( TRUE );
	Invalidate();
	UpdateWindow();
}

void FolderTree::refreshVFolders( VFolderProviderPtr provider, HTREEITEM item )
{
	if ( !item )
		return;

	if ( item != TVI_ROOT )
	{
		VFolderItemData* data = (VFolderItemData*)GetItemData( item );
		if ( !data || !data->isVFolder() )
			return;
		if ( !provider || data->getProvider() == provider )
		{
			refreshVFolder( data->getVFolder() );
			if ( data->getProvider() == provider )
				return; // no need to keep looking
		}
	}

	HTREEITEM child = GetChildItem( item );
	while ( child )
	{
		HTREEITEM next = GetNextItem( child, TVGN_NEXT );
		refreshVFolders( provider, child );
		child = next;
	}
}

VFolderPtr FolderTree::getVFolder( const std::string& name, bool strict, HTREEITEM item )
{
	if ( !item )
		return 0;

	if ( name.empty() )
		return 0;

	if ( item != TVI_ROOT )
	{
		VFolderItemData* data = (VFolderItemData*)GetItemData( item );
		if ( data && name == (LPCTSTR)GetItemText( item ) )
		{
			if ( data->isVFolder() )
				return data->getVFolder();
			else if ( !strict )
				return getVFolder( data );
		}
	}

	HTREEITEM child = GetChildItem( item );
	while ( child )
	{
		VFolderPtr vfolder = getVFolder( name, strict, child );
		if ( !!vfolder )
			return vfolder;
		child = GetNextItem( child, TVGN_NEXT );
	}
	return 0;
}

VFolderPtr FolderTree::getVFolderCustom( ItemTestCB test, void* testData, bool strict, HTREEITEM item )
{
	if ( !item )
		return 0;

	if ( !test )
		return 0;

	if ( item != TVI_ROOT )
	{
		VFolderItemData* data = (VFolderItemData*)GetItemData( item );
		if ( data && test( item, testData ) )
		{
			if ( data->isVFolder() )
				return data->getVFolder();
			else if ( !strict )
				return getVFolder( data );
		}
	}

	HTREEITEM child = GetChildItem( item );
	while ( child )
	{
		VFolderPtr vfolder = getVFolderCustom( test, testData, strict, child );
		if ( !!vfolder )
			return vfolder;
		child = GetNextItem( child, TVGN_NEXT );
	}
	return 0;
}

void FolderTree::getVFolders( std::vector<HTREEITEM>& items, HTREEITEM item )
{
	if ( !item )
		return;

	if ( item != TVI_ROOT )
	{
		VFolderItemData* data = (VFolderItemData*)GetItemData( item );
		if ( data && data->isVFolder() )
			items.push_back( item );
	}

	HTREEITEM child = GetChildItem( item );
	while ( child )
	{
		getVFolders( items, child );
		child = GetNextItem( child, TVGN_NEXT );
	}
	return;
}

void FolderTree::selectVFolder( const std::string& name )
{
	if ( name.empty() )
		return;

	VFolderPtr vfolder = getVFolder( name );
	if ( vfolder && vfolder->getItem() )
		SelectItem( vfolder->getItem() );
}

void FolderTree::selectVFolderCustom( ItemTestCB test, void* testData )
{
	if ( !test )
		return;

	VFolderPtr vfolder = getVFolderCustom( test, testData );
	if ( vfolder && vfolder->getItem() )
		SelectItem( vfolder->getItem() );
}


static int CALLBACK FolderTreeCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	VFolderItemData* data1 = (VFolderItemData*)lParam1;
	VFolderItemData* data2 = (VFolderItemData*)lParam2;

	if ( !data1 ) 
		return -1;

	if ( !data2 ) 
		return 1;

	if ( data1->getGroup() > data2->getGroup() )
		return 1;
	else if ( data1->getGroup() < data2->getGroup() )
		return -1;

	return _stricmp( data1->assetInfo().text().c_str(), data2->assetInfo().text().c_str() );
}

void FolderTree::sortSubTree( HTREEITEM item )
{
	TVSORTCB sortCB;
	sortCB.hParent = item;
	sortCB.lpfnCompare = FolderTreeCompareFunc;
	sortCB.lParam = 0;
	SortChildrenCB( &sortCB );
}

void FolderTree::setItemData( HTREEITEM item, VFolderItemDataPtr data )
{
	SetItemData( item, (DWORD_PTR)data.getObject() );

	if ( data )
	{
		data->setTreeItem( item );
		itemDataHeap_.push_back( data );
	}
}

void FolderTree::buildTree( HTREEITEM parentItem, VFolderProviderPtr provider )
{
	if ( !provider )
		return;

	VFolderItemDataPtr parentData = (VFolderItemData*)GetItemData( parentItem );
	VFolderItemDataPtr data;

	if ( !provider->startEnumChildren( parentData ) )
		return;

	CWaitCursor wait;

	CImage img;
	// get children
	while( data = provider->getNextChild( *thumbnailManager_, img ) )
	{
		int icon;
		int iconSel;
		if ( !img.IsNull() )
		{
			icon = addBitmap( (HBITMAP)img );
			iconSel = icon;
			img.Destroy();
		}
		else
			getGroupIcons( data->getGroup(), icon, iconSel, data->getExpandable() );

		std::string name = data->assetInfo().text().c_str();
		HTREEITEM child = GetChildItem( parentItem );
		while ( child )
		{
			if ( _stricmp( name.c_str(), GetItemText( child ) ) == 0 )
				break;
			child = GetNextItem( child, TVGN_NEXT );
		}

		bool dontAdd = false;
		if ( child )
		{
			VFolderItemDataPtr oldData = (VFolderItemData*)GetItemData( child );
			dontAdd = oldData->handleDuplicate( data );
		}
		if ( !dontAdd )
		{
			HTREEITEM item = InsertItem( data->assetInfo().text().c_str(), icon, iconSel, parentItem );
			setItemData( item, data );
			if ( getVFolder( data.getObject() )->getSortSubFolders() )
				sortSubTree( parentItem );
			if ( data->getExpandable() )
				InsertItem( "***dummychild***", 0, 0, item ); // later removed when expanded.
		}
	}
	if ( parentData && parentData->isVFolder() )
	{
		// custom items
		XmlItemVec* items = parentData->getVFolder()->getCustomItems();
		if ( items )
		{
			HTREEITEM topItem = TVI_FIRST;
			for( XmlItemVec::iterator i = items->begin();
				i != items->end(); ++i )
			{
				int icon = itemIcon_;
				int iconSel = itemIconSel_;
				thumbnailManager_->create( (*i).assetInfo().thumbnail(),
									img, IMAGE_SIZE, IMAGE_SIZE, this, true );
				data = new VFolderItemData( provider, (*i).assetInfo(), 0, false );
				data->isCustomItem( true );
				if ( !img.IsNull() )
				{
					icon = addBitmap( (HBITMAP)img );
					iconSel = icon;
					img.Destroy();
				}

				HTREEITEM item = InsertItem( data->assetInfo().text().c_str(),
					icon, iconSel, parentItem,
					(*i).position() == XmlItem::TOP ? topItem : TVI_LAST );
				if ( (*i).position() == XmlItem::TOP )
					topItem = item;
				setItemData( item, data );
			}
		}
	}
}

VFolderPtr FolderTree::addVFolder(
	const std::string& displayName,
	VFolderProviderPtr provider,
	VFolderPtr parent, HICON icon, HICON iconSel,
	bool show,
	bool expandable,
	XmlItemVec* customItems,
	void* data,
	bool subVFolders )
{
	if ( initialised_ )
		return 0;

	HTREEITEM item = 0;
	HTREEITEM parentItem = 0;
	if ( !!parent )
		parentItem = parent->getItem();

	if ( show )
	{
		int i;
		int is;

		getGroupIcons( 0, i, is, expandable );

		if ( icon )
		{
			i = addIcon( icon );
			if ( !iconSel )
				is = i;
		}
		if ( iconSel )
		{
			is = addIcon( iconSel );
		}

		item = InsertItem( displayName.c_str(), i, is, parentItem );
		setItemData( item, new VFolderItemData(
			provider, AssetInfo( "", displayName.c_str(), "" ), 0, true ) );
		if ( sortVFolders_ )
			sortSubTree( parentItem ); // sort siblings
	
		if ( expandable )
			InsertItem( "***dummychild***", 0, 0, item ); // later removed when expanded.
	}

	VFolderPtr newVFolder = new VFolder( parent, displayName, item, provider, expandable, sortSubFolders_, customItems, data, subVFolders );
	if ( item )
	{
		VFolderItemData* data = (VFolderItemData*)GetItemData( item );
		data->setVFolder( newVFolder );
	}

	return newVFolder;
}

void FolderTree::removeVFolder( const std::string& displayName, HTREEITEM curItem )
{
	HTREEITEM child = GetChildItem( curItem );
	while ( child )
	{
		HTREEITEM next = GetNextItem( child, TVGN_NEXT );
		VFolderItemData* data = (VFolderItemData*)GetItemData( child );
		if ( data && data->isVFolder() )
		{
			if ( displayName == (LPCTSTR)GetItemText( child ) )
			{
				freeSubtreeData( child );
				DeleteItem( child );
				return;
			}
			else
				removeVFolder( displayName, child );
		}
		child = next;
	}
}

void FolderTree::removeVFolder( HTREEITEM item, HTREEITEM curItem )
{
	HTREEITEM child = GetChildItem( curItem );
	while ( child )
	{
		HTREEITEM next = GetNextItem( child, TVGN_NEXT );
		if ( child == item )
		{
			freeSubtreeData( child );
			DeleteItem( child );
			return;
		}
		else
			removeVFolder( item, child );
		child = next;
	}
}

void FolderTree::clear()
{
	freeSubtreeData( TVI_ROOT );
	DeleteAllItems();
}

void FolderTree::setSortVFolders( bool sort )
{
	sortVFolders_ = sort;
}

void FolderTree::setSortSubFolders( bool sort )
{
	sortSubFolders_ = sort;
}

bool FolderTree::expandItem( HTREEITEM item )
{
	HTREEITEM child = GetChildItem( item );
	if ( child && GetItemText( child ).Compare("***dummychild***") == 0 ) {
		// it's the first time it's expanded, remove dummy item and build
		DeleteItem( child );
		VFolderItemDataPtr data = (VFolderItemData*)GetItemData( item );
		
		if ( !!data->getProvider() )
		{
			buildTree( item, data->getProvider() );

			HTREEITEM child = GetChildItem( item );
			if ( !child )
				return true; // avoid expanding
		}
	}
	return false;
}


// MFC messages
BEGIN_MESSAGE_MAP(FolderTree,CTreeCtrl)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_KEYDOWN()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_NOTIFY_REFLECT( TVN_SELCHANGED, OnSelChanged )
	ON_NOTIFY_REFLECT( NM_RCLICK, OnRightClick )
	ON_NOTIFY_REFLECT( TVN_ITEMEXPANDING, OnItemExpanding )
	ON_NOTIFY_REFLECT( TVN_BEGINDRAG, OnBeginDrag )
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

BOOL FolderTree::OnEraseBkgnd(CDC* pDC) 
{
	return FALSE;
}

void FolderTree::OnPaint() 
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect( &rect );
    controls::MemDC memDC;
	controls::MemDCScope memDCScope( memDC, dc, &rect );
	CWnd::DefWindowProc( WM_PAINT, (WPARAM)memDC.m_hDC, 0 );
}

void FolderTree::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags )
{
	if ( nChar == VK_DELETE && eventHandler_ )
	{
		HTREEITEM item = GetSelectedItem();
		if ( item )
			eventHandler_->folderTreeItemDelete( (VFolderItemData*)GetItemData( item ) );
		return;
	}
	CTreeCtrl::OnKeyDown( nChar, nRepCnt, nFlags );
}

void FolderTree::OnLButtonDown(UINT nFlags, CPoint point) 
{
	//select item
	HTREEITEM item = GetSelectedItem();
	CTreeCtrl::OnLButtonDown(nFlags, point);
	// send a synthetic select message when the user clicks an item and the
	// selection doesn't change
	UINT hitflags;
	if ( eventHandler_ && item && item == GetSelectedItem() && item == HitTest( point, &hitflags ) )
		eventHandler_->folderTreeSelect( (VFolderItemData*)GetItemData( item ) ); 
}

void FolderTree::OnRButtonDown(UINT nFlags, CPoint point) 
{
	//select item
	UINT hitflags;
	HTREEITEM hitem = HitTest( point, &hitflags ) ;
	if ( hitflags & (TVHT_ONITEM | TVHT_ONITEMBUTTON  )) 
		SelectItem( hitem );
	CTreeCtrl::OnRButtonDown(nFlags, point);
}

void FolderTree::OnSelChanged( NMHDR * pNotifyStruct, LRESULT* result )
{
	if ( eventHandler_ )
	{
		HTREEITEM item = GetSelectedItem();
		if ( item )
			eventHandler_->folderTreeSelect( (VFolderItemData*)GetItemData( item ) );
	}
}

void FolderTree::OnRightClick( NMHDR * pNotifyStruct, LRESULT* result )
{
	*result = 1;
	if ( eventHandler_ )
	{
		CPoint pt;
		GetCursorPos( &pt );
		ScreenToClient( &pt );
		UINT hitflags;
		HTREEITEM item = HitTest( pt, &hitflags ) ;
		VFolderItemData* data = 0;
		if ( item )
			data = (VFolderItemData*)GetItemData( item );
		eventHandler_->folderTreeRightClick( data );
	}
}

void FolderTree::OnItemExpanding( NMHDR * pNotifyStruct, LRESULT* result )
{
	LPNMTREEVIEW ns = (LPNMTREEVIEW) pNotifyStruct;

	*result = FALSE;

	if ( ns->action == TVE_EXPAND || ns->action == TVE_EXPANDPARTIAL )
		*result = expandItem( ns->itemNew.hItem ) ? TRUE : FALSE;
}

void FolderTree::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	if ( !eventHandler_ )
		return;

	LPNMTREEVIEW info = (LPNMTREEVIEW) pNMHDR;
	TVITEM item = info->itemNew;

	SelectItem( item.hItem );

	POINT pt;
	GetCursorPos( &pt );
	if ( dragImgList_ ) 
		delete dragImgList_;
	dragImgList_ = CreateDragImage( item.hItem );  // get the image list for dragging
	dragImgList_->SetBkColor( imgList_.GetBkColor() );
	CPoint offset( IMAGE_SIZE, IMAGE_SIZE );
	dragImgList_->SetDragCursorImage( 0, offset );
	dragImgList_->BeginDrag( 0, offset );
	dragImgList_->DragEnter( 0, pt );

	dragging_ = true;
	eventHandler_->folderTreeStartDrag( (VFolderItemData*)GetItemData( item.hItem ) );
}

void FolderTree::OnLButtonDblClk( UINT nFlags, CPoint point )
{
	if ( !eventHandler_ )
		return;

	UINT hitflags;
	HTREEITEM item = HitTest( point , &hitflags );
	if ( item && ( hitflags & TVHT_ONITEM ))
		eventHandler_->folderTreeDoubleClick( (VFolderItemData*)GetItemData( item ) );
}

bool FolderTree::isDragging()
{
	return dragging_;
}

void FolderTree::showDrag( bool show )
{
	if ( show )
	{
		POINT pt;
		GetCursorPos( &pt );
		dragImgList_->DragEnter( 0, pt );
	}
	else
		dragImgList_->DragLeave( 0 );
}

void FolderTree::updateDrag( int x, int y )
{
	POINT pt = { x, y };
	dragImgList_->DragMove( pt );
}

void FolderTree::endDrag()
{
	dragImgList_->DragLeave( 0 );
	dragImgList_->EndDrag();
	delete dragImgList_;
	dragImgList_ = 0;
	dragging_ = false;
}

bool FolderTree::updateItem( const AssetInfo& assetInfo, HTREEITEM item )
{
	// Static variable that will contain the thumbnail for the asset, so
	// we can get the thumbnail once and use it for all tree items that
	// point to this same asset.
	static CImage img;

	if ( item != TVI_ROOT )
	{
		VFolderItemData* data = (VFolderItemData*)GetItemData( item );
		if ( data )
		{
			std::string infLongText = data->assetInfo().longText();
			std::string assetInfoLongText = assetInfo.longText();
			StringUtils::toLowerCase( infLongText );
			StringUtils::toLowerCase( assetInfoLongText );

			if ( data &&
				( assetInfo.text() == "" || data->assetInfo().text() == assetInfo.text() ) &&
				infLongText == assetInfoLongText )
			{
				// Found. Just refresh the it's thumbnail
				if ( img.IsNull() )
					data->getProvider()->getThumbnail( *thumbnailManager_, data, img );
				int icon;
				int iconSel;
				if ( !img.IsNull() )
				{
					icon = addBitmap( (HBITMAP)img );
					iconSel = icon;
				}
				else
					getGroupIcons( data->getGroup(), icon, iconSel, data->getExpandable() );
				SetItemImage( item, icon, iconSel );

				return false; // return false so it keeps looking
			}
		}
	}
	else
	{
		// init the static if we are at the root
		if ( !img.IsNull() )
			img.Destroy();
	}

	HTREEITEM child = GetChildItem( item );
	while ( child )
	{
		if ( updateItem( assetInfo, child ) )
			return true;
		child = GetNextItem( child, TVGN_NEXT );
	}

	if ( item == TVI_ROOT && !img.IsNull() )
	{
		// destroy the static image if we are at the root item 
		img.Destroy();
	}

	return false;
}

void FolderTree::thumbManagerUpdate( const std::string& longText )
{
	if ( !GetSafeHwnd() || longText.empty() )
		return;

	std::string longTextTmp = longText;
	std::replace( longTextTmp.begin(), longTextTmp.end(), '/', '\\' );

	updateItem( AssetInfo( "", "", longTextTmp ) );
}
