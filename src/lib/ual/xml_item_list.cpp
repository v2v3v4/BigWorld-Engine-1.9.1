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
 *	XmlItemList: xml item list manager for the Ual Dialog
 */



#include "pch.hpp"
#include <string>
#include "resmgr/bwresource.hpp"
#include "common/string_utils.hpp"
#include "cstdmf/debug.hpp"

#include "xml_item_list.hpp"


DECLARE_DEBUG_COMPONENT( 0 )


// XmlItemList

XmlItemList::XmlItemList() :
	sectionLock_( 0 ),
	section_( 0 ),
	rootSection_( 0 )
{
}

XmlItemList::~XmlItemList()
{
}

void XmlItemList::setPath( const std::string& path )
{
	path_ = path;
}

void XmlItemList::setDataSection( const DataSectionPtr section )
{
	rootSection_ = section;
	path_ = "";
}

DataSectionPtr XmlItemList::lockSection()
{
	ASSERT( sectionLock_ < 8 ); // too much lock nesting = not unlocking somewhere
	if ( !rootSection_ && path_.empty() )
		return 0;

	if ( !section_ )
	{
		if ( !rootSection_ )
		{
			BWResource::instance().purge( path_ );
			section_ = BWResource::openSection( path_, true );
			if ( !section_ )
				return 0;
		}
		else
			section_ = rootSection_;
	}
	sectionLock_++;
	return section_;
}

void XmlItemList::unlockSection()
{
	ASSERT( sectionLock_ );
	sectionLock_--;
	if ( sectionLock_ == 0 )
		section_ = 0;
}

void XmlItemList::getItems( XmlItemVec& items )
{
	DataSectionPtr section = lockSection();
	if ( !section )
		return;

	std::vector<DataSectionPtr> sections;
	section->openSections( "item", sections );
	for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
	{
		XmlItem::Position pos = XmlItem::TOP;
		if ( _stricmp( (*s)->readString( "position" ).c_str(), "top" ) == 0 )
			pos = XmlItem::TOP;
		else if ( ( _stricmp( (*s)->readString( "position" ).c_str(), "bottom" ) == 0 ) )
			pos = XmlItem::BOTTOM;
		items.push_back( 
			XmlItem(
				AssetInfo( 
					(*s)->readString( "type" ),
					(*s)->asString(),
					(*s)->readString( "longText" ),
					(*s)->readString( "thumbnail" ),
					(*s)->readString( "description" ) ),
				pos )
			);
	}

	unlockSection();
}

DataSectionPtr XmlItemList::getItem( const XmlItem& item )
{
	if ( item.assetInfo().text().empty() )
		return 0;

	DataSectionPtr section = lockSection();
	if ( !section )
		return 0;

	std::string text = StringUtils::lowerCase( item.assetInfo().text() );
	std::string longText = StringUtils::lowerCase( item.assetInfo().longText() );

	std::vector<DataSectionPtr> sections;
	section->openSections( "item", sections );
	for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
	{
		if ( (*s)->readString( "type" ) == item.assetInfo().type() &&
			StringUtils::lowerCase( (*s)->asString() ) == text &&
			StringUtils::lowerCase( (*s)->readString( "longText" ) ) == longText )
		{
			unlockSection();
			return (*s);
		}
	}

	unlockSection();
	return 0;
}

void XmlItemList::dumpItem( DataSectionPtr section,  const XmlItem& item )
{
	if ( !section )
		return;

	section->setString( item.assetInfo().text() );
	section->writeString( "type", item.assetInfo().type() );
	section->writeString( "longText", item.assetInfo().longText() );
	if ( !item.assetInfo().thumbnail().empty() )
		section->writeString( "thumbnail", item.assetInfo().thumbnail() );
	if ( !item.assetInfo().description().empty() )
		section->writeString( "description", item.assetInfo().description() );
}

DataSectionPtr XmlItemList::add( const XmlItem& item )
{
	if ( item.assetInfo().text().empty() )
		return 0;

	DataSectionPtr section = lockSection();
	if ( !section )
		return 0;

	// Add it to the list and save
	DataSectionPtr newItem = section->newSection( "item" );
	if ( !newItem )
	{
		unlockSection();
		return 0;
	}
	dumpItem( newItem, item );
	section->save();
	unlockSection();
	return newItem;
}

DataSectionPtr XmlItemList::addAt( const XmlItem& item, const XmlItem& atItem )
{
	if ( item.assetInfo().text().empty() )
		return 0;

	DataSectionPtr section = lockSection();
	if ( !section )
		return 0;

	DataSectionPtr newItem = 0;

	std::vector<DataSectionPtr> sections;
	section->openSections( "item", sections );
	for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
	{
		DataSectionPtr dsitem;
		if ( (*s)->readString( "type" ) == atItem.assetInfo().type() &&
			(*s)->asString() == atItem.assetInfo().text() &&
			(*s)->readString( "longText" ) == atItem.assetInfo().longText() )
		{
			// add the new item in place
			newItem = section->newSection( "item" );
			dumpItem( newItem, item );
		}
		// add old item
		dsitem = section->newSection( "item" );
		dsitem->copy( *s );

		// delete old item
		section->delChild( *s );
	}
	if ( !newItem )
	{
		newItem = section->newSection( "item" );
		dumpItem( newItem, item );
	}

	section->save();
	unlockSection();
	return newItem;
}

void XmlItemList::remove( const XmlItem& item )
{
	DataSectionPtr section = lockSection();
	if ( !section )
		return;

	std::string text = StringUtils::lowerCase( item.assetInfo().text() );
	std::string longText = StringUtils::lowerCase( item.assetInfo().longText() );

	std::vector<DataSectionPtr> sections;
	section->openSections( "item", sections );
	for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
	{
		if ( (*s)->readString( "type" ) == item.assetInfo().type() &&
			StringUtils::lowerCase( (*s)->asString() ) == text &&
			StringUtils::lowerCase( (*s)->readString( "longText" ) ) == longText )
		{
			section->delChild( *s );
			section->save();
			break;
		}
	}
	unlockSection();
}

void XmlItemList::clear()
{
	DataSectionPtr section = lockSection();
	if ( !section )
		return;

	std::vector<DataSectionPtr> sections;
	section->openSections( "item", sections );
	std::vector<DataSectionPtr>::iterator oldestSection = sections.begin();
	for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
		section->delChild( *s );

	section->save();
	unlockSection();
}
