/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


#ifdef CODE_INLINE
#define INLINE inline
#else
#define INLINE
#endif


/**
 *	This method returns whether or not the cursor is showing.
 */
INLINE bool XConsole::isCursorShowing() const
{
	return showCursor_;
}


/**
 *  This method returns the color of the console text.
 */
INLINE uint32 XConsole::consoleColour() const
{
	return consoleColour_;
}


/**
 *	This method sets the position of the cursor.
 */
INLINE void XConsole::setCursor( uint8 x, uint8 y )
{
	this->cursorX( x );
	this->cursorY( y );
}


/**
 *	This method returns the x position (or column) of the cursor.
 */
INLINE uint8 XConsole::cursorX() const
{
	return cursorX_;
}


/**
 *	This method returns the y position (or row) of the cursor.
 */
INLINE uint8 XConsole::cursorY() const
{
	return cursorY_;
}



/**
 *	This method sets the x position (or column) of the cursor.
 */
INLINE void XConsole::cursorX( uint8 x )
{
	cursorX_ = max( 0, min( (int)x, MAX_CONSOLE_WIDTH - 1 ) );
}


/**
 *	This method sets the y position (or row) of the cursor.
 */
INLINE void XConsole::cursorY( uint8 y )
{
//	cursorY_ = max( 0, min( (int)y, MAX_CONSOLE_HEIGHT - 1 ) );
	cursorY_ = y;
}


/**
 *	This method returns the scroll offset of this console.
 */
INLINE int XConsole::scrollOffset() const
{
	return scrollOffset_;
}


/**
 *	This method returns the scroll offset of this console.
 */
INLINE void XConsole::scrollOffset( int offset )
{
	scrollOffset_ = offset;
	this->onScroll();
}


/**
 *	This method returns the scrolls the console down.
 */
INLINE void XConsole::scrollDown()
{
	scrollOffset_++;
	this->onScroll();
}


/**
 *	This method returns the scrolls the console up.
 */
INLINE void XConsole::scrollUp()
{
	scrollOffset_--;

	if (scrollOffset_ < 0)
	{
		scrollOffset_ = 0;
	}
	this->onScroll();
}

/**
 *	Gets the colour for this line
 *
 *	@return true if this line had a colour override
 */
INLINE bool XConsole::lineColourRetrieve( int line, uint32 & rColour ) const
{
	if (lineColours_[line].inUse)
	{
		rColour = lineColours_[line].colour;
		return true;
	}
	else
	{
		rColour = consoleColour_;
		return false;
	}
}


// xconsole.ipp
