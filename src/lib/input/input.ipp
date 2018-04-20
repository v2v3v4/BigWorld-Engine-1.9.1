/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// input.ipp

#ifdef CODE_INLINE
#define INLINE inline
#else
#define INLINE
#endif


// -----------------------------------------------------------------------------
// Section: InputDevices
// -----------------------------------------------------------------------------

/**
 *	This method initialises the input devices.
 *
 *	@param hInst	The Windows HINSTANCE.
 *	@param hWnd		The Windows HWND
 *	@param flags	Windows flags to be used on initialisation.
 *
 *	@return		True if the initialisation succeeded, false otherwise.
 */
INLINE bool InputDevices::init( void * hInst, void * hWnd, int flags )
{
	BW_GUARD;
	return instance().privateInit( hInst, hWnd, flags );
}


/**
 *	This method processes the pending events in the input devices and sends them
 *	to the InputHandler argument. That is, it calls the handleKeyEvent and
 *	handleMouseEvent methods of the interface.
 *
 *	@param handler	The input handler that will process the events.
 *
 *	@return		Returns false if an error occurred.
 */
INLINE bool InputDevices::processEvents( InputHandler & handler )
{
	BW_GUARD;
	return instance().privateProcessEvents( handler );
}


/**
 *	This method returns whether or not the input key is down.
 *
 *	@param key	The key to test the state of.
 *
 *	@return		Returns true if the key is down and false otherwise.
 */
INLINE bool InputDevices::isKeyDown( KeyEvent::Key key )
{
	return instance().isKeyDown_[ key ];
}


/**
 *	This helper method returns whether or not either Alt key is down.
 *
 *	@return		Returns true if either Alt key is down and false otherwise.
 */
INLINE bool InputDevices::isAltDown()
{
	return isKeyDown( KeyEvent::KEY_LALT ) ||
				isKeyDown( KeyEvent::KEY_RALT );
}


/**
 *	This helper method returns whether or not either Ctrl key is down.
 *
 *	@return		Returns true if either Ctrl key is down and false otherwise.
 */
INLINE bool InputDevices::isCtrlDown()
{
	return isKeyDown( KeyEvent::KEY_LCONTROL ) ||
				isKeyDown( KeyEvent::KEY_RCONTROL );
}


/**
 *	This helper method returns whether or not either Shift keys is down.
 *
 *	@return		Returns true if either Shift key is down and false otherwise.
 */
INLINE bool InputDevices::isShiftDown()
{
	return isKeyDown( KeyEvent::KEY_LSHIFT ) ||
				isKeyDown( KeyEvent::KEY_RSHIFT );
}


/**
 *	This method returns the current state of the modifier keys.
 */
INLINE uint32 InputDevices::modifiers()
{
	return
		(isShiftDown()	? MODIFIER_SHIFT : 0) |
		(isCtrlDown()	? MODIFIER_CTRL  : 0) |
		(isAltDown()	? MODIFIER_ALT   : 0);
}


// -----------------------------------------------------------------------------
// Section: InputHandler
// -----------------------------------------------------------------------------

/**
 *	Base class key event handler, which never handles it
 */
INLINE
bool InputHandler::handleKeyEvent( const KeyEvent & )
{
	return false;
}

/**
 *	Base class mouse event handler, which never handles it
 */
INLINE
bool InputHandler::handleMouseEvent( const MouseEvent & )
{
	return false;
}

/**
 *	Base class axis event handler, which never handles it
 */
INLINE
bool InputHandler::handleAxisEvent( const AxisEvent & )
{
	return false;
}


// input.ipp
