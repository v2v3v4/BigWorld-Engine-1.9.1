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
 *	SmartList: Inherits from CListCtrl to implement a virtual list optimised
 *	to handle large lists.
 */

#include "pch.hpp"
#include <algorithm>
#include <vector>
#include <string>
#include "time.h"
#include "ual_resource.h"
#include "list_cache.hpp"
#include "filter_spec.hpp"
#include "filter_holder.hpp"
#include "smart_list_ctrl.hpp"

#include "resmgr/string_provider.hpp"

#include "common/string_utils.hpp"

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT( 0 );




// SmartListCtrl

// public methods
SmartListCtrl::SmartListCtrl( ThumbnailManagerPtr thumbnailManager ) :
	style_( SmartListCtrl::BIGICONS ),
	provider_( 0 ),
	thumbnailManager_( thumbnailManager ),
	listCache_( &listCacheBig_ ),
	dragImgList_( 0 ),
	dragging_( false ),
	generateDragListEndItem_( false ),
	lastListDropItem_( -1 ),
	lastItemChanged_( -1 ),
	ignoreSelMessages_( false ),
	listViewIcons_( true ),
	thumbWidthSmall_( 16 ),
	thumbHeightSmall_( 16 ),
	thumbWidthCur_( 0 ),
	thumbHeightCur_( 0 ),
	customItems_( 0 ),
	eventHandler_( 0 ),
	maxSelUpdateMsec_( 50 ),
	delayedSelectionPending_( false ),
	redrawPending_( false ),
	maxItems_( 200 )
{
	MF_ASSERT( thumbnailManager_ != NULL );
	thumbWidth_ = thumbnailManager_->size();
	thumbHeight_ = thumbnailManager_->size();
}

SmartListCtrl::~SmartListCtrl()
{
	thumbnailManager_->resetPendingRequests( this );

	if ( dragImgList_ ) 
		delete dragImgList_;
}

SmartListCtrl::ViewStyle SmartListCtrl::getStyle()
{
	return style_;
}

void SmartListCtrl::setStyle( ViewStyle style )
{
	style_ = style;
	static CImageList dummyImgList;
	static const int IMGLIST_FORMAT = ILC_COLOR24|ILC_MASK;

	thumbnailManager_->resetPendingRequests( this );

	if ( !dummyImgList.GetSafeHandle() )
	{
		dummyImgList.Create( 1, 1, IMGLIST_FORMAT, 0, 0 );
		dummyImgList.SetBkColor( GetBkColor() );
	}

	// delete previous image list
	SetImageList( &dummyImgList, LVSIL_NORMAL );
	SetImageList( &dummyImgList, LVSIL_SMALL );
	SetImageList( &dummyImgList, LVSIL_STATE );

	CImageList* imgListPtr = 0;
	// set thumbnail size according to list style
	DWORD wstyle = GetWindowLong( GetSafeHwnd(), GWL_STYLE );
	// hack: have to force change the list view style so tooltip cache resets.
	SetWindowLong( GetSafeHwnd(), GWL_STYLE, (wstyle & ~LVS_TYPEMASK) | LVS_REPORT );
	wstyle = GetWindowLong( GetSafeHwnd(), GWL_STYLE );
	if ( style_ == BIGICONS )
	{
		SetWindowLong( GetSafeHwnd(), GWL_STYLE, (wstyle & ~LVS_TYPEMASK) | LVS_ICON );
		listViewIcons_ = true;
		thumbWidthCur_ = thumbWidth_;
		thumbHeightCur_ = thumbHeight_;
		listCache_ = &listCacheBig_;
		listCache_->setMaxItems( maxItems_ );
		imgListPtr = &imgListBig_;
	}
	else if ( style_ == SMALLICONS )
	{
		SetWindowLong( GetSafeHwnd(), GWL_STYLE, (wstyle & ~LVS_TYPEMASK) | LVS_LIST );
		listViewIcons_ = true;
		thumbWidthCur_ = thumbWidthSmall_;
		thumbHeightCur_ = thumbHeightSmall_;
		// Since small icons take less space, up the max cache items (by 16 if
		// big thumbs are 64x64 and small thumbs are 16x16, for example) to
		// take advantage of the same memory space.
		int memoryMultiplier =
			( thumbWidth_ * thumbHeight_ ) /
			( thumbWidthSmall_ * thumbHeightSmall_ );
		listCache_ = &listCacheSmall_;
		listCache_->setMaxItems( maxItems_ * memoryMultiplier );
		imgListPtr = &imgListSmall_;
	}
	else
	{
		SetWindowLong( GetSafeHwnd(), GWL_STYLE, (wstyle & ~LVS_TYPEMASK) | LVS_LIST );
		listViewIcons_ = false;
		thumbWidthCur_ = 0;
		thumbHeightCur_ = 0;
		imgListPtr = &dummyImgList;
	}

	// create image list if the style requires it
	if ( style_ != LIST )
	{
		if ( !imgListPtr->GetSafeHandle() )
		{
			imgListPtr->Create( thumbWidthCur_, thumbHeightCur_, IMGLIST_FORMAT, 0, 32 );
			imgListPtr->SetBkColor( GetBkColor() );
			imgListPtr->Add( AfxGetApp()->LoadIcon( IDI_UALFILE ) );
			// clear cache
			listCache_->init( imgListPtr, 1 );
		}
	}

	// set the image list
	if ( style_ == BIGICONS )
		SetImageList( imgListPtr, LVSIL_NORMAL );
	else if ( style_ == SMALLICONS )
		SetImageList( imgListPtr, LVSIL_SMALL );

	// clear and start
	SetItemCount( 0 );
	if ( provider_ )
		SetTimer( SMARTLIST_LOADTIMER_ID, SMARTLIST_LOADTIMER_MSEC, 0 );
}

void SmartListCtrl::PreSubclassWindow()
{
	SetExtendedStyle( GetExtendedStyle() | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER );
}

void SmartListCtrl::init( ListProviderPtr provider, XmlItemVec* customItems, bool clearSelection )
{
	provider_ = provider;
	customItems_ = customItems;

	if ( clearSelection )
	{
		bool oldIgnore = ignoreSelMessages_;
		ignoreSelMessages_ = true;
		SetItemState( -1, 0, LVIS_SELECTED );
		selItems_.clear();
		ignoreSelMessages_ = oldIgnore;
	}

	setStyle( getStyle() );
}

void SmartListCtrl::setMaxCache( int maxItems )
{
	maxItems_ = maxItems;
}

bool SmartListCtrl::getListViewIcons()
{
	return listViewIcons_;
}

void SmartListCtrl::setListViewIcons( bool listViewIcons )
{
	listViewIcons_ = listViewIcons;
}

void SmartListCtrl::refresh()
{
	if ( !provider_ )
		return;

	listCacheBig_.clear();
	listCacheSmall_.clear();
	provider_->refresh();
	init( provider_, customItems_, false );
}

ListProviderPtr SmartListCtrl::getProvider()
{
	return provider_;
}

bool SmartListCtrl::finished()
{
	if ( provider_ )
		return provider_->finished();
	return false;
}

XmlItem* SmartListCtrl::getCustomItem( int& index )
{
	if ( !customItems_ )
		return 0;

	int topIndex = 0;
	for( XmlItemVec::iterator i = customItems_->begin();
		i != customItems_->end();
		++i )
	{
		if ( (*i).position() != XmlItem::TOP )
			continue;
		if ( topIndex == index )
			return &(*i);
		++topIndex;
	}
	int bottomIndex = topIndex + provider_->getNumItems();
	for( XmlItemVec::iterator i = customItems_->begin();
		i != customItems_->end();
		++i )
	{
		if ( (*i).position() != XmlItem::BOTTOM )
			continue;
		if ( bottomIndex == index )
			return &(*i);
		++bottomIndex;
	}
	index -= topIndex;
	return 0;
}

bool SmartListCtrl::isCustomItem( int index )
{
	return getCustomItem( index ) != 0;
}

const AssetInfo SmartListCtrl::getAssetInfo( int index )
{
	XmlItem* item = getCustomItem( index );
	if ( item )
		return item->assetInfo();

	if ( !provider_ || index >= provider_->getNumItems() )
		return AssetInfo();

	return provider_->getAssetInfo( index );
}

void SmartListCtrl::updateItemInternal( int index, const AssetInfo& inf, bool removeFromCache )
{
	if ( !provider_ || index < 0 )
		return;

	if ( removeFromCache )
	{
		listCacheBig_.cacheRemove( inf.text(), inf.longText() );
		listCacheSmall_.cacheRemove( inf.text(), inf.longText() );
	}
	CRect clRect;
	GetClientRect( &clRect );
	CRect rect;
	GetItemRect( index, &rect, LVIR_BOUNDS );
	if ( rect.right >= 0 && rect.bottom >= 0 &&
		rect.left <= clRect.right && rect.top <= clRect.bottom )
	{
		RedrawItems( index, index );
		RedrawWindow( rect, NULL, 0 );
	}
}

void SmartListCtrl::updateItem( int index, bool removeFromCache )
{
	updateItemInternal( index, getAssetInfo( index ), removeFromCache );
}

void SmartListCtrl::updateItem( const AssetInfo& assetInfo, bool removeFromCache )
{
	// remove the item from the cache and schedule a redraw
	if ( removeFromCache )
	{
		listCacheBig_.cacheRemove( assetInfo.text(), assetInfo.longText() );
		listCacheSmall_.cacheRemove( assetInfo.text(), assetInfo.longText() );
	}
	if ( !redrawPending_ )
	{
		// only schedule a redraw if one hasn't been scheduled yet
		redrawPending_ = true;
		SetTimer( SMARTLIST_REDRAWTIMER_ID, SMARTLIST_REDRAWTIMER_MSEC, 0 );
	}
	return;

	// Commented out old code that actually searched for the item in the list
	// because it was too slow in big lists, it's better just to redraw all
/*	if ( !provider_ )
		return;

	int n = GetItemCount();
	int begin = binSearch( n, 0, n-1, assetInfo );
	bool binSearchOk = true;
	if ( begin == -1 )
	{
		// binSearch didn't find it, so try from the beginning and assume
		// the list is not sorted (for instance, 'Favourites' or 'History')
		begin = 0;
		binSearchOk = false;
	}
	for( int i = begin; i < n; ++i )
	{
		AssetInfo inf = getAssetInfo( i );
		if ( binSearchOk &&
			_stricmp( inf.text().c_str(), assetInfo.text().c_str() ) != 0 )
			break;	// break early, as no more duplicates were found

		if ( _stricmp( inf.longText().c_str(), assetInfo.longText().c_str() ) == 0 )
			updateItemInternal( i, inf );
	}*/
}

bool SmartListCtrl::showItem( const AssetInfo& assetInfo )
{
	int n = GetItemCount();
	int begin = binSearch( n, 0, n-1, assetInfo ); 
	if ( begin == -1 )
	{
		// binSearch didn't find it, so try from the beginning
		begin = 0;	
	}
	for( int i = begin; i < n; ++i )
	{
		AssetInfo inf = getAssetInfo( i );
		if ( _stricmp( inf.longText().c_str(), assetInfo.longText().c_str() ) == 0 )
		{
			SetItemState( -1, 0, LVIS_SELECTED );
			SetItemState( i, LVIS_SELECTED, LVIS_SELECTED );
			EnsureVisible( i, FALSE );
			return true;
		}
	}
	return false;
}

void SmartListCtrl::setEventHandler( SmartListCtrlEventHandler* eventHandler )
{
	eventHandler_ = eventHandler;
}

void SmartListCtrl::setDefaultIcon( HICON icon )
{
	if ( icon && imgListBig_.GetSafeHandle() )
		imgListBig_.Replace( 0, icon );
	if ( icon && imgListSmall_.GetSafeHandle() )
		imgListSmall_.Replace( 0, icon );
}

void SmartListCtrl::updateFilters()
{
	if ( !provider_ )
		return;

	provider_->filterItems();
	// hack to force reset of the tooltips
	DWORD wstyle = GetWindowLong( GetSafeHwnd(), GWL_STYLE );
	SetWindowLong( GetSafeHwnd(), GWL_STYLE, (wstyle & ~LVS_TYPEMASK) | LVS_REPORT );
	wstyle = GetWindowLong( GetSafeHwnd(), GWL_STYLE );
	if ( style_ == BIGICONS )
		SetWindowLong( GetSafeHwnd(), GWL_STYLE, (wstyle & ~LVS_TYPEMASK) | LVS_ICON );
	else
		SetWindowLong( GetSafeHwnd(), GWL_STYLE, (wstyle & ~LVS_TYPEMASK) | LVS_LIST );
	// do the actual change
	changeItemCount( provider_->getNumItems() );
}

// private methods
int SmartListCtrl::binSearch( int size, int begin, int end, const AssetInfo& assetInfo )
{
	if ( size == 0 || end == begin - 1 )
		return -1;	// this values can happen under normal circumstances

	if ( size < 0 || begin < 0 || end < 0 || end < begin - 1
		|| begin >= size || end >= size )
	{
		// border cases that should not happen
		WARNING_MSG( "SmartListCtrl::binSearch: bad parameters size (%d),"
			" begin (%d) and/or end (%d), searching for %s (%s)\n",
			size, begin, end, assetInfo.text().c_str(),
			assetInfo.longText().c_str() );
		return -1;
	}

	int index = (begin + end) / 2;

	if ( index < 0 || index >= size )
	{
		// this should never happen at this stage
		WARNING_MSG( "SmartListCtrl::binSearch: bad index %d searching for %s (%s)\n",
			index, assetInfo.text().c_str(), assetInfo.longText().c_str() );
		return -1;
	}

	AssetInfo inf = getAssetInfo( index );
	int cmp = _stricmp( inf.text().c_str(), assetInfo.text().c_str() );
	if ( cmp == 0 )
	{
		// found. loop backwards to find the first in case of duplicates
		int i = index;
		while( --i > 0 )
		{
			AssetInfo inf = getAssetInfo( i );
			if ( _stricmp( inf.text().c_str(), assetInfo.text().c_str() ) == 0 )
				--index;
			else
				break;
		}
		return index;
	}
	else if ( begin < end )
	{
		if ( cmp < 0 )
		{
			return binSearch( size, index+1, end, assetInfo );
		}
		else
		{
			return binSearch( size, begin, index-1, assetInfo );
		}
	}
	return -1;
}

void SmartListCtrl::getData( int index, std::string& text, int& image, bool textOnly /*false*/ )
{
	if ( !provider_ )
		return;

	if ( generateDragListEndItem_ )
	{
		text = L("UAL/SMART_LIST_CTRL/MORE");
		image = -1;
		return;
	}

	XmlItem* item = getCustomItem( index );
	if ( item )
	{
		text = item->assetInfo().text();
		if ( !textOnly && thumbWidthCur_ && thumbHeightCur_ )
		{
			image = 0;
			const ListCache::ListCacheElem* elem =
				listCache_->cacheGet( text, item->assetInfo().longText() );
			if ( elem )
			{
				// cache hit
				image = elem->image;
				return;
			}

			// cache miss
			CImage img;
			thumbnailManager_->create( item->assetInfo().thumbnail(),
				img, thumbWidthCur_, thumbHeightCur_, this, true );
			if ( !img.IsNull() )
			{
				elem = listCache_->cachePut( text, item->assetInfo().longText(), img );
				if ( elem )
					image = elem->image;
			}
		}
		return;
	}

	const AssetInfo assetInfo = provider_->getAssetInfo( index );
	text = assetInfo.text();

	if ( !textOnly && thumbWidthCur_ && thumbHeightCur_ )
	{
		image = 0;
		const ListCache::ListCacheElem* elem = 
			listCache_->cacheGet( text, assetInfo.longText() );
		if ( elem )
		{
			// cache hit
			image = elem->image;
			return;
		}

		// cache miss
		CImage img;
		provider_->getThumbnail( *thumbnailManager_, index, img, thumbWidthCur_, thumbHeightCur_, this );

		if ( !img.IsNull() )
		{
			elem = listCache_->cachePut( text, assetInfo.longText(), img );
			if ( elem )
				image = elem->image;
		}
	}
	else
		image = -1;
}

void SmartListCtrl::changeItemCount( int numItems )
{
	// flag to avoid sending callback messages when manually selecting items
	bool oldIgnore = ignoreSelMessages_;
	ignoreSelMessages_ = true;
	
	// deselect all
	SetItemState( -1, 0, LVIS_SELECTED );

	// change item count
	int numCustomItems = 0;
	if ( customItems_ )
		numCustomItems = customItems_->size();
	SetItemCountEx( numItems + numCustomItems, LVSICF_NOSCROLL );

	// restore selected items
	if ( provider_ )
	{
		clock_t start = clock();
		for( SelectedItemItr s = selItems_.begin();
			s != selItems_.end() && ((clock()-start)*1000/CLOCKS_PER_SEC) < maxSelUpdateMsec_;
			++s )
		{
			for( int item = 0;
				item < numItems && ((clock()-start)*1000/CLOCKS_PER_SEC) < maxSelUpdateMsec_;
				++item )
			{
				if ( (*s).equalTo( provider_->getAssetInfo( item ) ) )
				{
					SetItemState( item, LVIS_SELECTED, LVIS_SELECTED );
					break;
				}
			}
		}
	}
	ignoreSelMessages_ = oldIgnore;
}

void SmartListCtrl::updateSelection()
{
	if ( !provider_ )
		return;

	// save selected items
	int numSel = GetSelectedCount();
	selItems_.clear();
	selItems_.reserve( numSel );
	int item = -1;
	clock_t start = clock();
	for( int i = 0; i < numSel && ((clock()-start)*1000/CLOCKS_PER_SEC) < maxSelUpdateMsec_; i++ )
	{
		item = GetNextItem( item, LVNI_SELECTED );
		selItems_.push_back( provider_->getAssetInfo( item ) );
	}
}

bool SmartListCtrl::isDragging()
{
	return dragging_;
}

void SmartListCtrl::showDrag( bool show )
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

void SmartListCtrl::updateDrag( int x, int y )
{
	POINT pt = { x, y };
	dragImgList_->DragMove( pt );
}

void SmartListCtrl::endDrag()
{
	dragImgList_->DragLeave( 0 );
	dragImgList_->EndDrag();
	delete dragImgList_;
	dragImgList_ = 0;
	dragging_ = false;
}


void SmartListCtrl::setDropTarget( int index )
{
	bool oldIgnore = ignoreSelMessages_;
	ignoreSelMessages_ = true;
	SetItemState( index, LVIS_DROPHILITED, LVIS_DROPHILITED );
	if ( index != lastListDropItem_ )
	{
		RedrawItems( index, index );
		if ( lastListDropItem_ != -1 ) 
		{
			SetItemState( lastListDropItem_, 0, LVIS_DROPHILITED );
			RedrawItems( lastListDropItem_, lastListDropItem_ );
		}
		UpdateWindow();
		lastListDropItem_ = index;
	}
	ignoreSelMessages_ = oldIgnore;
}

void SmartListCtrl::clearDropTarget()
{
	if ( lastListDropItem_ != -1 )
	{
		bool oldIgnore = ignoreSelMessages_;
		ignoreSelMessages_ = true;
		SetItemState( lastListDropItem_, 0, LVIS_DROPHILITED );
		RedrawItems( lastListDropItem_, lastListDropItem_ );
		UpdateWindow();
		lastListDropItem_ = -1;
		ignoreSelMessages_ = oldIgnore;
	}
}

void SmartListCtrl::allowMultiSelect( bool allow )
{
	DWORD wstyle = GetWindowLong( GetSafeHwnd(), GWL_STYLE );
	if ( allow )
		SetWindowLong( GetSafeHwnd(), GWL_STYLE, wstyle & ~LVS_SINGLESEL );
	else
		SetWindowLong( GetSafeHwnd(), GWL_STYLE, wstyle | LVS_SINGLESEL );
}

void SmartListCtrl::thumbManagerUpdate( const std::string& longText )
{
	if ( !GetSafeHwnd() || longText.empty() )
		return;

	std::string longTextTmp = longText;
	std::replace( longTextTmp.begin(), longTextTmp.end(), '/', '\\' );
	std::string textTmp = longTextTmp.c_str() + longTextTmp.find_last_of( '\\' ) + 1;

	updateItem(
		AssetInfo( "", textTmp, longTextTmp ),
		false /* dont try to remove from cache, it's not there */ );
}

// Messages
BEGIN_MESSAGE_MAP(SmartListCtrl, CListCtrl)
	ON_WM_SIZE()
	ON_WM_KEYDOWN()
	ON_NOTIFY_REFLECT( NM_RCLICK, OnRightClick )
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnGetDispInfo)
	ON_NOTIFY_REFLECT(LVN_ODFINDITEM, OnOdFindItem)
	ON_WM_TIMER()
	ON_NOTIFY_REFLECT(LVN_ODSTATECHANGED, OnOdStateChanged)
	ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, OnItemChanged)
	ON_NOTIFY_REFLECT(NM_CLICK, OnItemClick)
	ON_NOTIFY_REFLECT(LVN_BEGINDRAG, OnBeginDrag)
	ON_WM_LBUTTONDBLCLK()
	ON_NOTIFY_REFLECT(LVN_GETINFOTIP, OnToolTipText)
END_MESSAGE_MAP()


void SmartListCtrl::OnSize( UINT nType, int cx, int cy )
{
	CListCtrl::OnSize( nType, cx, cy );
	RedrawWindow( NULL, NULL, RDW_INVALIDATE );
}


void SmartListCtrl::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags )
{
	if ( nChar == 'A' && GetAsyncKeyState( VK_CONTROL ) < 0 )
	{
		// select all
		SetItemState( -1, LVIS_SELECTED, LVIS_SELECTED );
		updateSelection();
		return;
	}
	else if ( nChar == VK_DELETE && eventHandler_ )
	{
		eventHandler_->listItemDelete();
		return;
	}
	CListCtrl::OnKeyDown( nChar, nRepCnt, nFlags );
}

void SmartListCtrl::OnRightClick( NMHDR * pNotifyStruct, LRESULT* result )
{
	if ( eventHandler_ )
	{
		int item = GetNextItem( -1, LVNI_FOCUSED );
/*		ignoreSelMessages_ = true;
		SetItemState( -1, 0, LVIS_SELECTED );
		if ( item >= 0 )
			SetItemState( item, LVIS_SELECTED, LVIS_SELECTED );
		updateSelection();
		ignoreSelMessages_ = false;*/
		eventHandler_->listItemRightClick( item );
		return;
	}
}

void SmartListCtrl::OnGetDispInfo( NMHDR* pNMHDR, LRESULT* pResult )
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    LV_ITEM* pItem = &(pDispInfo)->item;

	std::string text;
	int iImage;
	getData( pItem->iItem, text, iImage, !(pItem->mask & LVIF_IMAGE) );

	if ( pItem->mask & LVIF_TEXT )
	{
		strncpy( pItem->pszText, text.c_str(), pItem->cchTextMax );
		pItem->pszText[ pItem->cchTextMax-1 ] = 0;
	}
	if ( pItem->mask & LVIF_IMAGE )
		pItem->iImage = iImage;
}

void SmartListCtrl::OnOdFindItem(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = -1;

	if ( !provider_ )
		return;

	NMLVFINDITEM* pFindInfo = (NMLVFINDITEM*)pNMHDR;

    //Is search NOT based on string?
    if( !(pFindInfo->lvfi.flags & LVFI_STRING) )
        return;

	std::string search( pFindInfo->lvfi.psz );
	StringUtils::toLowerCase( search );

	int numItems = provider_->getNumItems();
	for( int i = 0; i < numItems; ++i )
	{
		const AssetInfo assetInfo = provider_->getAssetInfo( i );
		std::string text = assetInfo.text();
		StringUtils::toLowerCase( text );
		if ( _strnicmp( search.c_str(), text.c_str(), search.length() ) == 0 )
		{
			*pResult = i;
			break;
		}

	}
}

void SmartListCtrl::delayedSelectionNotify()
{
	KillTimer( SMARTLIST_SELTIMER_ID );

	if ( eventHandler_ )
		eventHandler_->listItemSelect();
	
	delayedSelectionPending_ = false;
}

void SmartListCtrl::OnTimer( UINT id )
{
	if ( id == SMARTLIST_SELTIMER_ID )
	{
		delayedSelectionNotify();
	}
	else if ( id == SMARTLIST_LOADTIMER_ID )
	{
		KillTimer( SMARTLIST_LOADTIMER_ID );

		if ( !provider_ )
			return;

		bool finished = provider_->finished();
		int numItems = provider_->getNumItems();
		int numCustomItems = 0;
		if ( customItems_ )
			numCustomItems = customItems_->size();

		if ( numItems + numCustomItems != GetItemCount() )
			changeItemCount( numItems );

		if ( eventHandler_ )
			eventHandler_->listLoadingUpdate();

		if ( finished )
		{
			if ( eventHandler_ )
				eventHandler_->listLoadingFinished();
		}
		else
			SetTimer( SMARTLIST_LOADTIMER_ID, SMARTLIST_LOADTIMER_MSEC, 0 );
	}
	else if ( id == SMARTLIST_REDRAWTIMER_ID )
	{
		KillTimer( SMARTLIST_REDRAWTIMER_ID );
		redrawPending_ = false;
		RedrawWindow( NULL, NULL, RDW_INVALIDATE );
	}
}

void SmartListCtrl::OnOdStateChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	if ( !eventHandler_ || ignoreSelMessages_ )
		return;

	LPNMLVODSTATECHANGE state = (LPNMLVODSTATECHANGE)pNMHDR;

	updateSelection();

	if ( provider_ )
	{
		KillTimer( SMARTLIST_SELTIMER_ID );
		delayedSelectionPending_ = true;
		SetTimer( SMARTLIST_SELTIMER_ID, SMARTLIST_SELTIMER_MSEC, 0 );
	}
}

void SmartListCtrl::OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	if ( !eventHandler_ || ignoreSelMessages_ )
		return;

	LPNMLISTVIEW state = (LPNMLISTVIEW)pNMHDR;

	updateSelection();

	if ( provider_ )
	{
		KillTimer( SMARTLIST_SELTIMER_ID );
		delayedSelectionPending_ = true;
		SetTimer( SMARTLIST_SELTIMER_ID, SMARTLIST_SELTIMER_MSEC, 0 );
	}
	lastItemChanged_ = state->iItem;
}

void SmartListCtrl::OnItemClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	if ( !eventHandler_  )
		return;

	int item = GetNextItem( -1, LVNI_FOCUSED );
	if ( GetItemState( item, LVIS_SELECTED ) != LVIS_SELECTED )
		item = -1;
	if ( item != -1 && item != lastItemChanged_ )
		eventHandler_->listItemSelect();
	lastItemChanged_ = -1;
}

void SmartListCtrl::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	if ( !eventHandler_ )
		return;

	LPNMLISTVIEW info = (LPNMLISTVIEW)pNMHDR;
	std::string text;
	int image;
	getData( info->iItem, text, image );

	POINT pt;
	GetCursorPos( &pt );
	if ( dragImgList_ )
	{
		delete dragImgList_;
		dragImgList_ = 0;
	}

	int pos = GetNextItem( -1, LVNI_SELECTED );
	bool isFirst = true;
	int xoff = 0;
	int yoff = 0;
	int xstep = 0;
	int ystep = 0;
	IMAGEINFO imf;
	int maxDragWidth = 400;
	int maxDragHeight = 350;
	while ( pos != -1 ) {
		if ( isFirst ) {
			dragImgList_ = CreateDragImage( pos, &pt );
			dragImgList_->GetImageInfo( 0, &imf );
			xstep = imf.rcImage.right - imf.rcImage.left;
			ystep = imf.rcImage.bottom - imf.rcImage.top;
			yoff = imf.rcImage.bottom;
			isFirst = false;
		}
		else
		{
			if ( yoff + ystep > maxDragHeight && xoff + xstep > maxDragWidth )
				generateDragListEndItem_ = true; // reached the max, so generate a 'more...' item in GetData
			CImageList* oneImgList = CreateDragImage( pos, &pt );
			generateDragListEndItem_ = false;
			CImageList* tempImgList = new CImageList();
			tempImgList->Attach(
				ImageList_Merge( 
					dragImgList_->GetSafeHandle(),
					0, oneImgList->GetSafeHandle(), 0, xoff, yoff ) );
			delete dragImgList_;
			delete oneImgList;
			dragImgList_ = tempImgList;
			dragImgList_->GetImageInfo( 0, &imf );
			yoff += ystep;
			if ( yoff > maxDragHeight )
			{
				xoff += xstep;
				if ( xoff > maxDragWidth )
					break;
				yoff = 0;
			}
		}
		pos = GetNextItem( pos, LVNI_SELECTED );
	}

	if ( dragImgList_ ) 
	{
		CPoint offset( thumbWidthCur_ + 16 , max( 16, thumbHeightCur_ - 14 ) );
		dragImgList_->SetBkColor( GetBkColor() );
		dragImgList_->SetDragCursorImage( 0, offset );
		dragImgList_->BeginDrag( 0, offset );
		dragImgList_->DragEnter( 0, pt );
	}

	if ( delayedSelectionPending_ )
	{
		// if a selection timer is pending, force it
		delayedSelectionNotify();
	}

	dragging_ = true;
	eventHandler_->listStartDrag( info->iItem );

}

void SmartListCtrl::OnLButtonDblClk( UINT nFlags, CPoint point )
{
	if ( !eventHandler_ )
		return;

	int item = GetNextItem( -1, LVNI_FOCUSED );
	if ( GetItemState( item, LVIS_SELECTED ) != LVIS_SELECTED )
		item = -1;
	eventHandler_->listDoubleClick( item );
}

void SmartListCtrl::OnToolTipText(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	LPNMLVGETINFOTIP it = (LPNMLVGETINFOTIP)pNMHDR;
	int item = it->iItem;

	std::string text;
	if ( eventHandler_ )
		eventHandler_->listItemToolTip( item, text );
	else if ( provider_ )
		text = provider_->getAssetInfo( item ).text();
	
	text = text.substr( 0, it->cchTextMax - 1 );

	lstrcpyn( it->pszText, text.c_str(), it->cchTextMax );
}
