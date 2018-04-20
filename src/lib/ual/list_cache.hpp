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

#ifndef LIST_CACHE_HPP
#define LIST_CACHE_HPP


#include "atlimage.h"
#include <list>


class ListCache
{
public:
	struct ListCacheElem
	{
		ListCacheElem() :
			image( 0 )
		{};
		std::string key;
		int image;
	};

	ListCache();
	~ListCache();

	void init( CImageList* imgList, int imgFirstIndex );
	void clear();

	void setMaxItems( int maxItems );

	const ListCacheElem* cacheGet( const std::string& text, const std::string& longText );
	const ListCacheElem* cachePut( const std::string& text, const std::string& longText, const CImage& img );
	void cacheRemove( const std::string& text, const std::string& longText );
private:
	std::list<ListCacheElem> listCache_;
	std::vector<int> imgListFreeSpots_;
	CImageList* imgList_;
	int imgFirstIndex_;
	int maxItems_;
	typedef std::list<ListCacheElem>::iterator ListCacheElemItr;
};



#endif // LIST_CACHE_HPP