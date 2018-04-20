/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"
#include "show_cursor_helper.hpp"



bool ShowCursorHelper::visible()
{
	ShowCursor( TRUE );
	return ShowCursor( FALSE ) >= 0;
}


ShowCursorHelper::ShowCursorHelper( bool show ) :
	show_( show ),
	showCursorCount_( 0 )
{
	if ( show_ == visible() )
		return;

	// Show cursor will be called at least once from now, so pre-incrementing.
	showCursorCount_++;

	if ( show_ )
		while ( ShowCursor( TRUE ) < 0 ) showCursorCount_++;
	else
		while ( ShowCursor( FALSE ) >= 0 ) showCursorCount_++;
}

void ShowCursorHelper::dontRestore()
{
	showCursorCount_ = 0;
}

void ShowCursorHelper::restoreNow()
{
	// restore ShowCursor count
	if ( show_ )
		while ( showCursorCount_-- > 0 ) ShowCursor( FALSE );
	else
		while ( showCursorCount_-- > 0 ) ShowCursor( TRUE );
	showCursorCount_ = 0;
}


ShowCursorHelper::~ShowCursorHelper()
{
	restoreNow();
}
