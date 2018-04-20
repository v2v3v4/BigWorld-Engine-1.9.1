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
 *	FILTER_SPEC: filters text according to its include/exclude rules
 */


#include <algorithm>
#include <vector>
#include <string>
#include "filter_spec.hpp"
#include "common/string_utils.hpp"


FilterSpec::FilterSpec( const std::string& name, bool active,
					   const std::string& include, const std::string& exclude,
					   const std::string& group ) :
	name_( name ),
	active_( active ),
	enabled_( true ),
	group_( group )
{
	std::string includeL = include;
	std::replace( includeL.begin(), includeL.end(), '/', '\\' );
	StringUtils::vectorFromString( includeL, includes_ );

	std::string excludeL = exclude;
	std::replace( excludeL.begin(), excludeL.end(), '/', '\\' );
	StringUtils::vectorFromString( excludeL, excludes_ );
}

FilterSpec::~FilterSpec()
{
}

bool FilterSpec::filter( const std::string& str )
{
	if ( !active_ || !enabled_ )
		return true;

	// pass filter test if it's in the includes and not in the excludes.
	return StringUtils::matchSpec( str, includes_ ) &&
		( excludes_.empty() || !StringUtils::matchSpec( str, excludes_ ) );
}

void FilterSpec::enable( bool enable )
{
	enabled_ = enable;
}
