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
 *	ListCache: keeps a cache of list elements to improve performance in virtual
 *  lists. It manages the CImageList of the virtual to store the thumbnails.
 */


#include "pch.hpp"
#include <string>
#include <vector>
#include "list_cache.hpp"
#include "common/string_utils.hpp"



// List Cache
#define BUILD_LISTCACHE_KEY( A, B ) ((A) + "|" + (B))

ListCache::ListCache() :
	imgList_( 0 ),
	imgFirstIndex_( 0 )
{
	setMaxItems( 200 );
}

ListCache::~ListCache()
{
}

void ListCache::init( CImageList* imgList, int imgFirstIndex )
{
	imgList_ = imgList;
	imgFirstIndex_ = imgFirstIndex;

	clear();
}

void ListCache::clear()
{
	if ( !imgList_ || !imgList_->GetSafeHandle() )
		return;

	listCache_.clear();
	imgListFreeSpots_.clear();
	if ( maxItems_ > 0 )
		imgListFreeSpots_.reserve( maxItems_ );

	int index = imgFirstIndex_;
	while ( index < imgList_->GetImageCount() )
		imgListFreeSpots_.push_back( index++ );
}

void ListCache::setMaxItems( int maxItems )
{
	maxItems_ = maxItems;
	if ( maxItems_ > 0 )
		imgListFreeSpots_.reserve( maxItems_ );
}

const ListCache::ListCacheElem* ListCache::cacheGet( const std::string& text, const std::string& longText )
{
	if ( !imgList_ || !imgList_->GetSafeHandle() )
		return 0;

	if ( maxItems_ <= 0 )
		return 0;

	std::string key = BUILD_LISTCACHE_KEY( text, longText );
	StringUtils::toLowerCase( key );

	for( ListCacheElemItr i = listCache_.begin(); i != listCache_.end(); ++i )
	{
		if ( (*i).key == key )
		{
			// cache hit. Put first on the list
			ListCacheElem tempElem = (*i);
			listCache_.erase( i );
			listCache_.push_front( tempElem );
			return &(*listCache_.begin());
		}
	}

	// cache miss
	return 0;
}

const ListCache::ListCacheElem* ListCache::cachePut( const std::string& text, const std::string& longText, const CImage& img )
{
	if ( !imgList_ || !imgList_->GetSafeHandle() )
		return 0;

	ListCacheElem newElem;

	newElem.key = BUILD_LISTCACHE_KEY( text, longText );

	StringUtils::toLowerCase( newElem.key );

	int image = 0;

	if ( maxItems_ <= 0 )
	{
		// cache only one item
		listCache_.clear();
		imgList_->Remove( imgFirstIndex_ );
		if ( !img.IsNull() )
			image = imgList_->Add( CBitmap::FromHandle( (HBITMAP)img ), (CBitmap*)0 );

		newElem.image = image;
		listCache_.push_back( newElem );
		return &(*listCache_.rbegin());
	}

	if ( (int)listCache_.size() >= maxItems_ )
	{
		// cache full, replace oldest (last in the list)
		int oldestImg = listCache_.back().image;

		if ( !img.IsNull() )
		{
			// find out if an unused spot is available in the image list
			int replaceImg = -1;
			if ( oldestImg >= imgFirstIndex_ )
			{
				// replace the oldest image
				replaceImg = oldestImg;
			}
			else if ( !imgListFreeSpots_.empty() )
			{
				// replace an available free spot
				replaceImg = imgListFreeSpots_.back();
				imgListFreeSpots_.pop_back();
			}

			// add image
			if ( replaceImg >= imgFirstIndex_ )
			{
				// oldest used image, so replace it
				image = oldestImg;
				imgList_->Replace( image, CBitmap::FromHandle( (HBITMAP)img ), (CBitmap*)0 );
			}
			else
			{
				// oldest didn't use image, so add one
				image = imgList_->Add( CBitmap::FromHandle( (HBITMAP)img ), (CBitmap*)0 );
			}
		}
		else if ( oldestImg >= imgFirstIndex_ )
		{
			// oldest used image, and current doesn't, so add it as a free spot
			imgListFreeSpots_.push_back( oldestImg );
		}

		newElem.image = image;

		// remove the oldest (last in the list)
		listCache_.pop_back();
		// and insert new one
		listCache_.push_front( newElem );
		return &(*listCache_.begin());
	}
	else
	{
		if ( !img.IsNull() )
		{
			if ( !imgListFreeSpots_.empty() )
			{
				// replace an available free spot
				image = imgListFreeSpots_.back();
				imgListFreeSpots_.pop_back();

				imgList_->Replace( image,
					CBitmap::FromHandle( (HBITMAP)img ), (CBitmap*)0 );
			}
			else
			{
				// no free space, so add it.
				image = imgList_->Add( CBitmap::FromHandle( (HBITMAP)img ), (CBitmap*)0 );
			}
		}

		newElem.image = image;

		// cache has free space, add to cache
		listCache_.push_front( newElem );
		return &(*listCache_.begin());
	}
}

void ListCache::cacheRemove( const std::string& text, const std::string& longText )
{
	std::string key = BUILD_LISTCACHE_KEY( text, longText );
	StringUtils::toLowerCase( key );

	for( ListCacheElemItr i = listCache_.begin(); i != listCache_.end(); )
		if ( (*i).key == key )
		{
			if ( (*i).image >= imgFirstIndex_ )
			{
				// add it as a free spot
				imgListFreeSpots_.push_back( (*i).image );
			}
			// erase the actual cache entry
			i = listCache_.erase( i );
		}
		else
			 ++i;
}
