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
 *	ListMultiProvider: Inherits from SmartListProvider to implement a list
 *	provider that manages one or more sub-providers, allowing multiple asset
 *	sources to be shown under one UAL folder
 */

#include "pch.hpp"
#include <algorithm>
#include <vector>
#include <string>
#include "list_multi_provider.hpp"
#include "thumbnail_manager.hpp"

#include "common/string_utils.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/datasection.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT( 0 );


// ListMultiProvider::ListItem
ListMultiProvider::ListItem::ListItem( ListProviderPtr provider, int index ) :
	provider_( provider ),
	index_( index ),
	inited_( false )
{
}

const char* ListMultiProvider::ListItem::text() const
{
	if ( !inited_ )
	{
		// The asset info is not yet cached, so cache it
		text_ = provider_->getAssetInfo( index_ ).text();
		inited_ = true;
	}
	return text_.c_str();
}


// ListMultiProvider
ListMultiProvider::ListMultiProvider() :
	lastNumItems_( 0 )
{
}

ListMultiProvider::~ListMultiProvider()
{
}

void ListMultiProvider::refresh()
{
	// refresh all providers
	for( ProvVec::iterator i = providers_.begin();
		i != providers_.end(); ++i )
	{
		(*i)->refresh();
	}
	// and refresh the items_ vector with the new data
	fillItems();
}

bool ListMultiProvider::finished()
{
	for( ProvVec::iterator i = providers_.begin();
		i != providers_.end(); ++i )
	{
		if ( !(*i)->finished() )
		{
			// at least one provider hasen't finished, so return false
			return false;
		}
	}
	return true;
}

int ListMultiProvider::getNumItems()
{
	int total = 0;
	for( ProvVec::iterator i = providers_.begin();
		i != providers_.end(); ++i )
	{
		total += (*i)->getNumItems();
	}
	// return the sum of the num of items of each provider
	return total;
}

const AssetInfo ListMultiProvider::getAssetInfo( int index )
{
	updateItems();
	if ( index < 0 || index >= (int)items_.size() )
		return AssetInfo();

	// gets the item directly from the provider, using the provider_ and 
	// index_ members of the element 'index' of the items_ vector.
	return items_[ index ].provider()->getAssetInfo( items_[ index ].index() );
}

void ListMultiProvider::getThumbnail( ThumbnailManager& manager,
										int index, CImage& img, int w, int h,
										ThumbnailUpdater* updater )
{
	updateItems();
	if ( index < 0 || index >= (int)items_.size() )
		return;

	// gets the thumb directly from the provider, using the provider_ and 
	// index_ members of the element 'index' of the items_ vector.
	items_[ index ].provider()->getThumbnail( manager,
		items_[ index ].index(),
		img, w, h, updater );
}

void ListMultiProvider::filterItems()
{
	// filter all providers
	for( ProvVec::iterator i = providers_.begin();
		i != providers_.end(); ++i )
	{
		(*i)->filterItems();
	}
	// and fill the items_ vector
	fillItems();
}


/**
 *	adds a list provider to the providers_ vector.
 */
void ListMultiProvider::addProvider( ListProviderPtr provider )
{
	if ( !provider )
		return;

	providers_.push_back( provider );
}


/**
 *	Method that updates the items_ vector if the number of items has changed.
 */
void ListMultiProvider::updateItems()
{
	if ( getNumItems() != lastNumItems_ )
	{
		fillItems();
		lastNumItems_ = getNumItems();
	}
}


/**
 *	Sorting callback for std::sort.
 */
bool ListMultiProvider::s_comparator( const ListItem& a, const ListItem& b )
{
	// If both items are in the same provider, compare by index because items
	// are sorted in each provider.
	if ( a.provider() == b.provider() )
		return a.index() < b.index();

	// different providers, so compare the filenames
	return _stricmp( a.text(), b.text() ) <= 0;
}

/**
 *	Fills the items_ vector with the items of all providers and sorts them.
 */
void ListMultiProvider::fillItems()
{
	items_.clear();
	lastNumItems_ = getNumItems();
	if ( !lastNumItems_ || !providers_.size() )
		return;

	// reserve to optimise performance.
	items_.reserve( lastNumItems_ );

	for( ProvVec::iterator p = providers_.begin();
		p != providers_.end(); ++p )
	{
		// loop through the providers
		int numItems = (*p)->getNumItems();
		ListProviderPtr listProv = *p;
		for( int i = 0; i < numItems; ++i )
		{
			// push back all the items from the provider
			items_.push_back( ListItem( listProv, i ) );
		}
	}
	// finally, sort the vector
	std::sort< std::vector<ListItem>::iterator >(
		items_.begin(), items_.end(), s_comparator );
}