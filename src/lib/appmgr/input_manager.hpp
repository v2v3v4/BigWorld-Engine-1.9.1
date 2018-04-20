/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef INPUT_MANAGER_HPP
#define INPUT_MANAGER_HPP

#include <iostream>

#include "input/input.hpp"


/**
 * This class provides a high-level input routing service
 */
class InputManager : public InputHandler
{
public:
	InputManager();
	~InputManager();

	//input handler methods
	bool handleKeyEvent( const KeyEvent & /*event*/ );
	bool handleMouseEvent( const MouseEvent & /*event*/ );

private:
	InputManager(const InputManager&);
	InputManager& operator=(const InputManager&);

	friend std::ostream& operator<<(std::ostream&, const InputManager&);
};


/**
 * This class eats input events.  This is used in Borland UI mode,
 * when switching back to the 3D viewport.  When the Borland UI has
 * focus, it should be receiving events, but behind the scenes, DI
 * is storing up these events.  As soon as the 3D viewport is active,
 * direct input spams our input handler with events it shouldn't have
 * gotten.  This input handler handles the spam.
 */
class NullInputManager : public InputHandler
{
public:
	NullInputManager()
	{
	}

	~NullInputManager()
	{
	}

	bool handleKeyEvent( const KeyEvent & /*event*/ )
	{
		return true;
	}

	bool handleMouseEvent( const MouseEvent & /*event*/ )
	{
		return true;
	}
private:
	NullInputManager(const NullInputManager&);
	NullInputManager& operator=(const NullInputManager&);

	friend std::ostream& operator<<(std::ostream&, const NullInputManager&);
};

#ifdef CODE_INLINE
#include "input_manager.ipp"
#endif




#endif
/*input_manager.hpp*/
