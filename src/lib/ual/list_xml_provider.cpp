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
 *	ListXmlProvider: Inherits from SmartListProvider to implement an XML virtual list provider
 */

#include "pch.hpp"
#include <algorithm>
#include <vector>
#include <string>
#include "list_xml_provider.hpp"
#include "thumbnail_manager.hpp"

#include "common/string_utils.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/datasection.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT( 0 );


// ListXmlProvider
ListXmlProvider::ListXmlProvider() :
	errorLoading_( false )
{
	init( "" );
}

ListXmlProvider::~ListXmlProvider()
{
	clearItems();
}

void ListXmlProvider::init( const std::string& path )
{
	path_ = path;
	StringUtils::toLowerCase( path_ );
	std::replace( path_.begin(), path_.end(), '/', '\\' );

	refreshPurge( true );
}

void ListXmlProvider::refresh()
{
	refreshPurge( true );
}

bool ListXmlProvider::s_comparator( const ListXmlProvider::ListItemPtr& a, const ListXmlProvider::ListItemPtr& b )
{
	return _stricmp( a->text().c_str(), b->text().c_str() ) < 0;
}

void ListXmlProvider::refreshPurge( bool purge )
{
	errorLoading_ = false;

	clearItems();

	if ( !path_.length() )
		return;

	DataSectionPtr dataSection;

	if ( purge )
		BWResource::instance().purge( path_ );

	dataSection = BWResource::openSection( path_ );

	if ( !dataSection )
	{
		errorLoading_ = true;
		return;
	}

	std::vector<DataSectionPtr> sections;
	dataSection->openSections( "item", sections );
	for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
	{
		ListItemPtr item = new AssetInfo(
			(*s)->readString( "type" ),
			(*s)->asString(),
			(*s)->readString( "longText" ),
			(*s)->readString( "thumbnail" ),
			(*s)->readString( "description" )
			);
		items_.push_back( item );
	}

	if ( dataSection->readBool( "sort", false ) )
		std::sort< ItemsItr >( items_.begin(), items_.end(), s_comparator );

	filterItems();
}

void ListXmlProvider::clearItems()
{
	items_.clear();
	searchResults_.clear();
}

bool ListXmlProvider::finished()
{
	return true; // it's not asyncronous
}

int ListXmlProvider::getNumItems()
{
	return (int)searchResults_.size();
}

const AssetInfo ListXmlProvider::getAssetInfo( int index )
{
	if ( index < 0 || getNumItems() <= index )
		return AssetInfo();

	return *searchResults_[ index ];
}

void ListXmlProvider::getThumbnail( ThumbnailManager& manager,
								   int index, CImage& img, int w, int h,
								   ThumbnailUpdater* updater )
{
	if ( index < 0 || getNumItems() <= index )
		return;

	std::string thumb;
	thumb = searchResults_[ index ]->thumbnail();

	if ( !thumb.length() )
		thumb = searchResults_[ index ]->longText();

	std::string fname;
	fname = BWResource::findFile( thumb );

	manager.create( fname, img, w, h, updater );
}

void ListXmlProvider::filterItems()
{
	searchResults_.clear();

	for( ItemsItr i = items_.begin(); i != items_.end(); ++i )
	{
		// filters filtering
		if ( filterHolder_ && filterHolder_->filter( (*i)->text(), (*i)->longText() ) )
		{
			if ( (searchResults_.size() % VECTOR_BLOCK_SIZE) == 0 )
				searchResults_.reserve( searchResults_.size() + VECTOR_BLOCK_SIZE );
			searchResults_.push_back( (*i) );
		}
	}
}
