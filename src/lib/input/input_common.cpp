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

#include "input.hpp"

#include "math/mathdef.hpp"

/**
 *	@file	This file contains input manager code common to all
 *	supported platforms.
 */


// -----------------------------------------------------------------------------
// Section: KeyEvent
// -----------------------------------------------------------------------------

KeyEvent::KeyMap KeyEvent::keyMap_;

#define KEY_ADD( key )	map_[ #key ] = KEY_##key;

/**
 *	Constructor
 */
KeyEvent::KeyMap::KeyMap()
{
	map_[ "ESCAPE" ] = KEY_ESCAPE;

		KEY_ADD(ESCAPE          )
		KEY_ADD(1               )
		KEY_ADD(2               )
		KEY_ADD(3               )
		KEY_ADD(4               )
		KEY_ADD(5               )
		KEY_ADD(6               )
		KEY_ADD(7               )
		KEY_ADD(8               )
		KEY_ADD(9               )
		KEY_ADD(0               )
		KEY_ADD(MINUS           )
		KEY_ADD(EQUALS          )
		KEY_ADD(BACKSPACE       )
		KEY_ADD(TAB             )
		KEY_ADD(Q               )
		KEY_ADD(W               )
		KEY_ADD(E               )
		KEY_ADD(R               )
		KEY_ADD(T               )
		KEY_ADD(Y               )
		KEY_ADD(U               )
		KEY_ADD(I               )
		KEY_ADD(O               )
		KEY_ADD(P               )
		KEY_ADD(LBRACKET        )
		KEY_ADD(RBRACKET        )
		KEY_ADD(RETURN          )
		KEY_ADD(LCONTROL        )
		KEY_ADD(A               )
		KEY_ADD(S               )
		KEY_ADD(D               )
		KEY_ADD(F               )
		KEY_ADD(G               )
		KEY_ADD(H               )
		KEY_ADD(J               )
		KEY_ADD(K               )
		KEY_ADD(L               )
		KEY_ADD(SEMICOLON       )
		KEY_ADD(APOSTROPHE      )
		KEY_ADD(GRAVE           )
		KEY_ADD(LSHIFT          )
		KEY_ADD(BACKSLASH       )
		KEY_ADD(Z               )
		KEY_ADD(X               )
		KEY_ADD(C               )
		KEY_ADD(V               )
		KEY_ADD(B               )
		KEY_ADD(N               )
		KEY_ADD(M               )
		KEY_ADD(COMMA           )
		KEY_ADD(PERIOD          )
		KEY_ADD(SLASH           )
		KEY_ADD(RSHIFT          )
		KEY_ADD(NUMPADSTAR      )
		KEY_ADD(LALT            )
		KEY_ADD(SPACE           )
		KEY_ADD(CAPSLOCK        )
		KEY_ADD(F1              )
		KEY_ADD(F2              )
		KEY_ADD(F3              )
		KEY_ADD(F4              )
		KEY_ADD(F5              )
		KEY_ADD(F6              )
		KEY_ADD(F7              )
		KEY_ADD(F8              )
		KEY_ADD(F9              )
		KEY_ADD(F10             )
		KEY_ADD(NUMLOCK         )
		KEY_ADD(SCROLL          )
		KEY_ADD(NUMPAD7         )
		KEY_ADD(NUMPAD8         )
		KEY_ADD(NUMPAD9         )
		KEY_ADD(NUMPADMINUS     )
		KEY_ADD(NUMPAD4         )
		KEY_ADD(NUMPAD5         )
		KEY_ADD(NUMPAD6         )
		KEY_ADD(ADD             )
		KEY_ADD(NUMPAD1         )
		KEY_ADD(NUMPAD2         )
		KEY_ADD(NUMPAD3         )
		KEY_ADD(NUMPAD0         )
		KEY_ADD(NUMPADPERIOD    )
		KEY_ADD(OEM_102         )
		KEY_ADD(F11             )
		KEY_ADD(F12             )

		KEY_ADD(F13             )
		KEY_ADD(F14             )
		KEY_ADD(F15             )

		KEY_ADD(KANA            )
		KEY_ADD(ABNT_C1         )
		KEY_ADD(CONVERT         )
		KEY_ADD(NOCONVERT       )
		KEY_ADD(YEN             )
		KEY_ADD(ABNT_C2         )
		KEY_ADD(NUMPADEQUALS    )
		KEY_ADD(PREVTRACK   )
		KEY_ADD(AT              )
		KEY_ADD(COLON           )
		KEY_ADD(UNDERLINE       )
		KEY_ADD(KANJI           )
		KEY_ADD(STOP            )
		KEY_ADD(AX              )
		KEY_ADD(UNLABELED       )
		KEY_ADD(NEXTTRACK       )
		KEY_ADD(NUMPADENTER     )
		KEY_ADD(RCONTROL        )
		KEY_ADD(MUTE            )
		KEY_ADD(CALCULATOR      )
		KEY_ADD(PLAYPAUSE       )
		KEY_ADD(MEDIASTOP       )
		KEY_ADD(VOLUMEDOWN      )
		KEY_ADD(VOLUMEUP        )
		KEY_ADD(WEBHOME         )
		KEY_ADD(NUMPADCOMMA     )
		KEY_ADD(NUMPADSLASH     )
		KEY_ADD(SYSRQ           )
		KEY_ADD(RALT            )
		KEY_ADD(PAUSE           )
		KEY_ADD(HOME            )
		KEY_ADD(UPARROW         )
		KEY_ADD(PGUP            )
		KEY_ADD(LEFTARROW       )
		KEY_ADD(RIGHTARROW      )
		KEY_ADD(END             )
		KEY_ADD(DOWNARROW       )
		KEY_ADD(PGDN            )
		KEY_ADD(INSERT          )
		KEY_ADD(DELETE          )
		KEY_ADD(LWIN            )
		KEY_ADD(RWIN            )
		KEY_ADD(APPS            )
		KEY_ADD(POWER           )
		KEY_ADD(SLEEP           )
		KEY_ADD(WAKE            )
		KEY_ADD(WEBSEARCH       )
		KEY_ADD(WEBFAVORITES    )
		KEY_ADD(WEBREFRESH      )
		KEY_ADD(WEBSTOP         )
		KEY_ADD(WEBFORWARD      )
		KEY_ADD(WEBBACK         )
		KEY_ADD(MYCOMPUTER      )
		KEY_ADD(MAIL            )
		KEY_ADD(MEDIASELECT     )

		KEY_ADD(MOUSE0          )
		KEY_ADD(LEFTMOUSE       )
		KEY_ADD(MOUSE1          )
		KEY_ADD(RIGHTMOUSE      )
		KEY_ADD(MOUSE2          )
		KEY_ADD(MIDDLEMOUSE     )
		KEY_ADD(MOUSE3          )
		KEY_ADD(MOUSE4          )
		KEY_ADD(MOUSE5          )
		KEY_ADD(MOUSE6          )
		KEY_ADD(MOUSE7          )

		KEY_ADD(JOYDUP			)
		KEY_ADD(JOYDDOWN		)
		KEY_ADD(JOYDLEFT		)
		KEY_ADD(JOYDRIGHT		)
		KEY_ADD(JOYSTART		)
		KEY_ADD(JOYBACK			)
		KEY_ADD(JOYALPUSH		)
		KEY_ADD(JOYARPUSH		)

		KEY_ADD(JOYA			)
		KEY_ADD(JOYB			)
		KEY_ADD(JOYX			)
		KEY_ADD(JOYY			)

		KEY_ADD(JOYBLACK		)
		KEY_ADD(JOYWHITE		)

		KEY_ADD(JOYLTRIGGER		)
		KEY_ADD(JOYRTRIGGER		)

		KEY_ADD(JOYALUP			)
		KEY_ADD(JOYALDOWN		)
		KEY_ADD(JOYALLEFT		)
		KEY_ADD(JOYALRIGHT		)
		KEY_ADD(JOYALUP			)
		KEY_ADD(JOYALDOWN		)
		KEY_ADD(JOYALLEFT		)
		KEY_ADD(JOYALRIGHT		)
		KEY_ADD(DEBUG			)
}


/**
 *	This method returns the character that is represented by this event. It
 *	considers the state of the modifiers.
 */
char KeyEvent::character() const
{
	char character = '\0';

	// If the Ctrl or Alt keys are down, return the NULL character.

	if ( modifiers_ & (MODIFIER_CTRL | MODIFIER_ALT) )
	{
		return '\0';
	}

	switch (this->key())
	{
	case KEY_A: character = (modifiers_ != MODIFIER_SHIFT) ? 'a' : 'A'; break;
	case KEY_B: character = (modifiers_ != MODIFIER_SHIFT) ? 'b' : 'B'; break;
	case KEY_C: character = (modifiers_ != MODIFIER_SHIFT) ? 'c' : 'C'; break;
	case KEY_D: character = (modifiers_ != MODIFIER_SHIFT) ? 'd' : 'D'; break;
	case KEY_E: character = (modifiers_ != MODIFIER_SHIFT) ? 'e' : 'E'; break;
	case KEY_F: character = (modifiers_ != MODIFIER_SHIFT) ? 'f' : 'F'; break;
	case KEY_G: character = (modifiers_ != MODIFIER_SHIFT) ? 'g' : 'G'; break;
	case KEY_H: character = (modifiers_ != MODIFIER_SHIFT) ? 'h' : 'H'; break;
	case KEY_I: character = (modifiers_ != MODIFIER_SHIFT) ? 'i' : 'I'; break;
	case KEY_J: character = (modifiers_ != MODIFIER_SHIFT) ? 'j' : 'J'; break;
	case KEY_K: character = (modifiers_ != MODIFIER_SHIFT) ? 'k' : 'K'; break;
	case KEY_L: character = (modifiers_ != MODIFIER_SHIFT) ? 'l' : 'L'; break;
	case KEY_M: character = (modifiers_ != MODIFIER_SHIFT) ? 'm' : 'M'; break;
	case KEY_N: character = (modifiers_ != MODIFIER_SHIFT) ? 'n' : 'N'; break;
	case KEY_O: character = (modifiers_ != MODIFIER_SHIFT) ? 'o' : 'O'; break;
	case KEY_P: character = (modifiers_ != MODIFIER_SHIFT) ? 'p' : 'P'; break;
	case KEY_Q: character = (modifiers_ != MODIFIER_SHIFT) ? 'q' : 'Q'; break;
	case KEY_R: character = (modifiers_ != MODIFIER_SHIFT) ? 'r' : 'R'; break;
	case KEY_S: character = (modifiers_ != MODIFIER_SHIFT) ? 's' : 'S'; break;
	case KEY_T: character = (modifiers_ != MODIFIER_SHIFT) ? 't' : 'T'; break;
	case KEY_U: character = (modifiers_ != MODIFIER_SHIFT) ? 'u' : 'U'; break;
	case KEY_V: character = (modifiers_ != MODIFIER_SHIFT) ? 'v' : 'V'; break;
	case KEY_W: character = (modifiers_ != MODIFIER_SHIFT) ? 'w' : 'W'; break;
	case KEY_X: character = (modifiers_ != MODIFIER_SHIFT) ? 'x' : 'X'; break;
	case KEY_Y: character = (modifiers_ != MODIFIER_SHIFT) ? 'y' : 'Y'; break;
	case KEY_Z: character = (modifiers_ != MODIFIER_SHIFT) ? 'z' : 'Z'; break;

	case KEY_0: character = (modifiers_ != MODIFIER_SHIFT) ? '0' : ')'; break;
	case KEY_1: character = (modifiers_ != MODIFIER_SHIFT) ? '1' : '!'; break;
	case KEY_2: character = (modifiers_ != MODIFIER_SHIFT) ? '2' : '@'; break;
	case KEY_3: character = (modifiers_ != MODIFIER_SHIFT) ? '3' : '#'; break;
	case KEY_4: character = (modifiers_ != MODIFIER_SHIFT) ? '4' : '$'; break;
	case KEY_5: character = (modifiers_ != MODIFIER_SHIFT) ? '5' : '%'; break;
	case KEY_6: character = (modifiers_ != MODIFIER_SHIFT) ? '6' : '^'; break;
	case KEY_7: character = (modifiers_ != MODIFIER_SHIFT) ? '7' : '&'; break;
	case KEY_8: character = (modifiers_ != MODIFIER_SHIFT) ? '8' : '*'; break;
	case KEY_9: character = (modifiers_ != MODIFIER_SHIFT) ? '9' : '('; break;

	case KEY_COMMA      : character = (modifiers_ != MODIFIER_SHIFT) ? ','  : '<'; break;
	case KEY_PERIOD     : character = (modifiers_ != MODIFIER_SHIFT) ? '.'  : '>'; break;
	case KEY_SLASH      : character = (modifiers_ != MODIFIER_SHIFT) ? '/'  : '?'; break;
	case KEY_SEMICOLON  : character = (modifiers_ != MODIFIER_SHIFT) ? ';'  : ':'; break;
	case KEY_APOSTROPHE : character = (modifiers_ != MODIFIER_SHIFT) ? '\'' : '"'; break;
	case KEY_LBRACKET	: character = (modifiers_ != MODIFIER_SHIFT) ? '['	: '{'; break;
	case KEY_RBRACKET	: character = (modifiers_ != MODIFIER_SHIFT) ? ']'	: '}'; break;
	case KEY_GRAVE      : character = (modifiers_ != MODIFIER_SHIFT) ? '`'  : '~'; break;
	case KEY_MINUS      : character = (modifiers_ != MODIFIER_SHIFT) ? '-'  : '_'; break;
	case KEY_EQUALS     : character = (modifiers_ != MODIFIER_SHIFT) ? '='  : '+'; break;
	case KEY_BACKSLASH  : character = (modifiers_ != MODIFIER_SHIFT) ? '\\' : '|'; break;

	case KEY_SPACE      : character = ' '; break;
	case KEY_RETURN     : character = '\r'; break;

	case KEY_BACKSPACE  :
	case KEY_DELETE     : character = '\b'; break;
	}

	return character;
}


/**
 *	Returns a key associated with the input string.
 *
 *	@param str	The string to search for.
 *
 *	@return	The associated Key identifier.
 */
KeyEvent::Key KeyEvent::KeyMap::stringToKey( const std::string & str ) const
{
	StringHashMap<Key>::const_iterator foundIter = map_.find( str );

	if (foundIter != map_.end())
	{
		return foundIter->second;
	}
	else
	{
		// We do not have the string in the map.
		return KEY_NOT_FOUND;
	}
}

/**
 *	Returns the name associated with the input key
 *
 *	@param str	The string to search for.
 *
 *	@return	The associated Key identifier.
 */
const char * KeyEvent::KeyMap::keyToString( const Key & key ) const
{
	StringHashMap<Key>::const_iterator iter = map_.begin();
	while (iter != map_.end())
	{
		if (iter->second == key) return iter->first.c_str();
		iter++;
	}

	static const char * nullString = "";
	return nullString;
}



// -----------------------------------------------------------------------------
// Section: Joystick
// -----------------------------------------------------------------------------


/**
 *	The constructor for Joystick.
 */
Joystick::Joystick() :
	pDIJoystick_( NULL ),
	xMinKey_( KeyEvent::KEY_LEFTARROW  ),
	xMaxKey_( KeyEvent::KEY_RIGHTARROW ),
	yMinKey_( KeyEvent::KEY_DOWNARROW ),
	yMaxKey_( KeyEvent::KEY_UPARROW ),
	isUsingKeyboard_( true ),
	axis_( AxisEvent::NUM_AXES ),
	lastProcessedTime_( 0 )
{
	// start quantized direction at the centre
	quantJoyDir_[0] = 4;
	quantJoyDir_[1] = 4;
}



/**
 *	This method updates the state of the Joystick.
 */
bool Joystick::update()
{
	BW_GUARD;
	bool updated = false;

	if (isUsingKeyboard_ || !hasJoystick())
	{
		updated = this->updateFromKeyboardDevice();
	}
	else
	{
		updated = this->updateFromJoystickDevice();

		if (!updated)
		{
			updated = this->updateFromKeyboardDevice();
		}
	}

	return updated;
}


/**
 *	This method generates a key event on behalf of the joystick
 */
void Joystick::generateKeyEvent( bool isDown, int key, InputHandler & handler,
	bool * pIsKeyDown )
{
	BW_GUARD;
	KeyEvent event(
		isDown ? MFEvent::KEY_DOWN : MFEvent::KEY_UP,
		KeyEvent::Key( key ),
		InputDevices::modifiers() );

	if (pIsKeyDown != NULL)
	{
		pIsKeyDown[ event.key() ] = event.isKeyDown();
	}

	handler.handleKeyEvent( event );
}



/**
 *	Helper function to get a direction from a joystick position
 */
static int joystickDirection( float joy_x, float joy_y, float box )
{
	BW_GUARD;
	if (joy_x*joy_x + joy_y*joy_y < box*box) return 4;
	float a = atan2f( joy_y, joy_x );

	const int dirMap[] = { 5, 8, 7, 6, 3, 0, 1, 2 };
	return dirMap[ uint(a * 4.f / MATH_PI + 8.5f) & 7 ];
	//int xd = (joy.x <= -box) ? -1 : (joy.x < box) ? 0 : 1;
	//int yd = (joy.y <= -box) ? -1 : (joy.y < box) ? 0 : 1;
	//return (yd+1)*3 + (xd+1);
}

static const float JOY_SEL_AMT = 0.5f;


/**
 *	This function gets the joystick to generate its events from its
 *	internal data.
 */
void Joystick::generateCommonEvents( InputHandler & handler, bool * pIsKeyDown )
{
	BW_GUARD;
	// Figure out how times have changed
	uint64 curProcessedTime = timestamp();
	float dTime = 0.f;
	if (lastProcessedTime_)
	{
		dTime = float( int64(curProcessedTime - lastProcessedTime_) /
			stampsPerSecondD() );
		if (dTime > 1.f) dTime = 1.f;
	}
	lastProcessedTime_ = curProcessedTime;

	// Now send out all the axis events (our update method was just called)
	if (!isUsingKeyboard_)
	{
		// first update our quantized directions
		int oldJoyDir[2];
		for (int i = 0; i < 2; i++)
		{
			oldJoyDir[i] = quantJoyDir_[i];

			AxisEvent::Axis horA = i ? AxisEvent::AXIS_RX : AxisEvent::AXIS_LX;
			AxisEvent::Axis verA = i ? AxisEvent::AXIS_RY : AxisEvent::AXIS_LY;

			quantJoyDir_[i] = joystickDirection(
				axis_[horA].value(), axis_[verA].value(),
				JOY_SEL_AMT );
		}

		// now send the axis events
		for (int a = AxisEvent::AXIS_LX; a < AxisEvent::NUM_AXES; a++)
		{
			if (axis_[a].value() != 0.f || !axis_[a].sentZero())
			{
				AxisEvent event( (AxisEvent::Axis)a, axis_[a].value(), dTime );
				handler.handleAxisEvent( event );
				axis_[a].sentZero( axis_[a].value() == 0.f );
			}
		}

		// and then send the direction key events
		for (int i = 0; i < 2; i++)
		{
			int nDir, oDir;

			int keyEvBase = i ? KeyEvent::KEY_JOYARUP : KeyEvent::KEY_JOYALUP;

			// do they differ in x?
			if ((nDir = quantJoyDir_[i] % 3) != (oDir = oldJoyDir[i] % 3))
			{
				if (oDir != 1) this->generateKeyEvent(
					false, keyEvBase + (oDir ? 3 : 2), handler, pIsKeyDown );
				if (nDir != 1) this->generateKeyEvent(
					true , keyEvBase + (nDir ? 3 : 2), handler, pIsKeyDown );
			}

			// do they differ in y?
			if ((nDir = quantJoyDir_[i] / 3) != (oDir = oldJoyDir[i] / 3))
			{
				if (oDir != 1) this->generateKeyEvent(
					false, keyEvBase + (oDir ? 1 : 0), handler, pIsKeyDown );
				if (nDir != 1) this->generateKeyEvent(
					true , keyEvBase + (nDir ? 1 : 0), handler, pIsKeyDown );
			}
		}
	}
}

std::vector<KeyboardDevice*> gVirtualKeyboards;

// input_common.cpp
