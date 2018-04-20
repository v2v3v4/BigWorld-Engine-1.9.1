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
 *	FilterHolder: aclass that manages a series of filters and searchtext
 */



#include "pch.hpp"
#include "Shlwapi.h"
#include "filter_holder.hpp"
#include "common/string_utils.hpp"




// FilterHolder
FilterHolder::FilterHolder() :
	searchTextEnabled_( true )
{
}

bool FilterHolder::hasActiveFilters()
{
	for( FilterSpecItr f = filters_.begin(); f != filters_.end(); ++f )
		if ( (*f)->getActive() && !(*f)->getName().empty() )
			return true;
	return false;
}

bool FilterHolder::isFiltering()
{
	if ( filters_.empty() && searchText_.empty() )
		return false;

	if ( !searchText_.empty() )
		return true;

	if ( hasActiveFilters() )
		return true;

	return false;
}

void FilterHolder::addFilter( FilterSpecPtr filter )
{
	if ( !filter )
		return;

	filters_.push_back( filter );
}

FilterSpecPtr FilterHolder::getFilter( int index )
{
	if ( index < 0 || index >= (int)filters_.size() )
		return 0;

	return filters_[ index ];
}

void FilterHolder::setSearchText( const std::string& searchText )
{
	searchText_ = searchText;
	StringUtils::toLowerCase( searchText_ );
}

void FilterHolder::enableSearchText( bool enable )
{
	searchTextEnabled_ = enable;
}

bool FilterHolder::filter( const std::string& shortText, const std::string& text )
{
	bool searchOk = true;

	if ( searchTextEnabled_ && !searchText_.empty() && !shortText.empty() )
	{
		bool useWildcards = true;
		if ( searchText_.find( '*' ) == std::string::npos && searchText_.find( '?' ) == std::string::npos )
			useWildcards = false;

		if ( useWildcards )
		{
			if ( !PathMatchSpec( shortText.c_str(), searchText_.c_str() ) )
				searchOk = false;
		}
		else
		{
			std::string stxt = shortText;
			StringUtils::toLowerCase( stxt );
			if ( !strstr( stxt.c_str(), searchText_.c_str() ) )
				searchOk = false;
		}
	}

	if ( !searchOk )
		return false;

	if ( !filters_.empty() && !text.empty() )
	{
		std::map<std::string,bool> groups; // group name, group state
		for( FilterSpecItr f = filters_.begin(); f != filters_.end(); ++f )
		{
			if ( !(*f)->getActive() || (*f)->getName().empty() )
				continue;
			std::map<std::string,bool>::iterator g = groups.find( (*f)->getGroup() );
			if ( g != groups.end() )
			{
				// There's already an entry for this group, check if the filter test passes
				// If the group state is already true, don't change anything, even if the filter test fails
				if ( !(*g).second && (*f)->filter( text ) )
					(*g).second = true;
			}
			else
				// insert the new group with the filter test result as its state
				groups.insert( std::pair<std::string,bool>(
					(*f)->getGroup(), (*f)->filter( text ) )
					);
		}

		// find out if the item passed at least one test per group
		bool push = true; // default value is true, in the case that no filter is active.
		for( std::map<std::string,bool>::iterator g = groups.begin(); 
			g != groups.end();
			++g )
		{
			if ( !(*g).second )
			{
				push = false;
				break;
			}
		}

		return push;
	}
	return true;
}

void FilterHolder::enableAll( bool enable )
{
	for( FilterSpecItr f = filters_.begin(); f != filters_.end(); ++f )
		(*f)->enable( enable );
}

void FilterHolder::enable( const std::string& name, bool enable )
{
	for( FilterSpecItr f = filters_.begin(); f != filters_.end(); ++f )
		if ( (*f)->getName() == name )
		{
			(*f)->enable( enable );
			break;
		}
}
