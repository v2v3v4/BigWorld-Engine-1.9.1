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
 *	UalFavourites: favourites manager for the Ual Dialog
 */



#include "pch.hpp"
#include <string>
#include "cstdmf/debug.hpp"

#include "ual_favourites.hpp"


DECLARE_DEBUG_COMPONENT( 0 )



// UalFavourites

UalFavourites::UalFavourites()
{
}

UalFavourites::~UalFavourites()
{
}

DataSectionPtr UalFavourites::add( const XmlItem& item )
{
	if ( !path_.length() || item.empty() )
		return 0;

	// See if it's already in the favourites
	DataSectionPtr dsitem = getItem( item );
	if ( !!dsitem )
		return dsitem;

	// Add it to the favourites
	dsitem = XmlItemList::add( item );
	if ( changedCallback_ )
		(*changedCallback_)();
	return dsitem;
}

DataSectionPtr UalFavourites::addAt(
	const XmlItem& item,
	const XmlItem& atItem )
{
	DataSectionPtr dsitem = XmlItemList::addAt( item, atItem );
	if ( changedCallback_ )
		(*changedCallback_)();
	return dsitem;
}

void UalFavourites::remove( const XmlItem& item, bool callCallback )
{
	XmlItemList::remove( item );
	if ( callCallback && changedCallback_ )
		(*changedCallback_)();
}

void UalFavourites::clear()
{
	XmlItemList::clear();
	if ( changedCallback_ )
		(*changedCallback_)();
}
