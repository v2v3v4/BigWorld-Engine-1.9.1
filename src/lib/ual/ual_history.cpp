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
 *	UalHistory: history manager for the Ual Dialog
 */



#include "pch.hpp"
#include <string>
#include "cstdmf/debug.hpp"

#include "ual_history.hpp"


DECLARE_DEBUG_COMPONENT( 0 )



// UalHistory

UalHistory::UalHistory() :
	maxItems_( 50 ),
	changedCallback_( 0 ),
	preparedItemValid_( false )
{
}

UalHistory::~UalHistory()
{
}

void UalHistory::prepareItem( const XmlItem& item )
{
	preparedItem_ = item;
	preparedItemValid_ = true;
}

bool UalHistory::addPreparedItem()
{
	if ( !preparedItemValid_ )
		return false;
	preparedItemValid_ = false;
	return !!add( preparedItem_ );
}

void UalHistory::discardPreparedItem()
{
	preparedItemValid_ = false;
}

const XmlItem UalHistory::getPreparedItem()
{
	if ( preparedItemValid_ )
		return preparedItem_;
	else
		return XmlItem();
}

void UalHistory::saveTimestamp( DataSectionPtr ds )
{
	time_t secs;
	time( &secs );

	if ( sizeof( time_t ) == 8 )
	{
		ds->writeLong( "timestamp1", (long(secs>>32)) );
		ds->writeLong( "timestamp2", (long(secs)) );
	}
	else
	{
		ds->writeLong( "timestamp", (long)secs );
	}
}

time_t UalHistory::loadTimestamp( DataSectionPtr ds )
{
	time_t ret;
	if ( sizeof( time_t ) == 8 )
	{
		ret =
			((time_t)ds->readLong( "timestamp1" )) << 32 |
			((time_t)ds->readLong( "timestamp2" ));
	}
	else
	{
		ret = (time_t)ds->readLong( "timestamp" );
	}
	return ret;
}

DataSectionPtr UalHistory::add( const XmlItem& item )
{
	if ( !path_.length() || item.empty() )
		return 0;

	DataSectionPtr section = lockSection();
	if ( !section )
		return 0;

	// See if it's already in the history
	DataSectionPtr dsitem = getItem( item );
	if ( !!dsitem )
	{
		saveTimestamp( dsitem );
		section->save();
		unlockSection();
		return dsitem;
	}

	// Remove old items if the history is full
	std::vector<DataSectionPtr> sections;
	section->openSections( "item", sections );
	while ( (int)sections.size() >= maxItems_ )
	{
		time_t oldestTime = 0;
		std::vector<DataSectionPtr>::iterator oldestSection = sections.begin();
		for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
		{
			time_t ts = loadTimestamp( *s );
			if ( oldestTime == 0 || ts < oldestTime )
			{
				oldestTime = ts;
				oldestSection = s;
			}
		}

		section->delChild( *oldestSection );
		sections.erase( oldestSection );
	}
	section->save();

	// Add it to the history and save
	dsitem = XmlItemList::add( item );
	if ( !dsitem )
	{
		unlockSection();
		return 0;
	}
	saveTimestamp( dsitem );
	section->save();
	unlockSection();
	if ( changedCallback_ )
		(*changedCallback_)();
	return dsitem;
}

void UalHistory::remove( const XmlItem& item, bool callCallback )
{
	XmlItemList::remove( item );
	if ( callCallback && changedCallback_ )
		(*changedCallback_)();
}

void UalHistory::clear()
{
	XmlItemList::clear();
	if ( changedCallback_ )
		(*changedCallback_)();
}
