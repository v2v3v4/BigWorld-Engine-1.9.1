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
#include "gui_input_handler.hpp"
#include "gui_manager.hpp"
#include <algorithm>
#include <set>


BEGIN_GUI_NAMESPACE

std::map<char, int> BigworldInputDevice::keyMap_;
std::map<std::string, int> BigworldInputDevice::nameMap_;
bool BigworldInputDevice::lastKeyDown[ KeyEvent::NUM_KEYS ];

bool BigworldInputDevice::down( const bool* keyDownTable, char ch )
{
	return ( keyMap_.find( ch ) != keyMap_.end() && keyDownTable[ keyMap_[ ch ] ] );
}

bool BigworldInputDevice::down( const bool* keyDownTable, const std::string& key )
{
	std::string left, right;
	if( _strcmpi(key.c_str(), "WIN" ) == 0 )
		left = "LWIN", right = "RWIN";
	if( _strcmpi(key.c_str(), "CTRL" ) == 0 )
		left = "LCTRL", right = "RCTRL";
	if( _strcmpi(key.c_str(), "SHIFT" ) == 0 )
		left = "LSHIFT", right = "RSHIFT";
	if( _strcmpi(key.c_str(), "ALT" ) == 0 )
		left = "LALT", right = "RALT";
	return left.empty()																	?
		( nameMap_.find( key ) != nameMap_.end() && keyDownTable[ nameMap_[ key ] ] )	:
		( keyDownTable[ nameMap_[ left ] ] || keyDownTable[ nameMap_[ right ] ] );
}

BigworldInputDevice::BigworldInputDevice( const bool* keyDownTable )
	:keyDownTable_( keyDownTable )
{
	if( keyMap_.empty() )
	{
		keyMap_[ '0' ] = KeyEvent::KEY_0, keyMap_[ '1' ] = KeyEvent::KEY_1, keyMap_[ '2' ] = KeyEvent::KEY_2,
		keyMap_[ '3' ] = KeyEvent::KEY_3, keyMap_[ '4' ] = KeyEvent::KEY_4, keyMap_[ '5' ] = KeyEvent::KEY_5,
		keyMap_[ '6' ] = KeyEvent::KEY_6, keyMap_[ '7' ] = KeyEvent::KEY_7, keyMap_[ '8' ] = KeyEvent::KEY_8,
		keyMap_[ '9' ] = KeyEvent::KEY_9;

		keyMap_[ 'A' ] = KeyEvent::KEY_A, keyMap_[ 'B' ] = KeyEvent::KEY_B, keyMap_[ 'C' ] = KeyEvent::KEY_C,
		keyMap_[ 'D' ] = KeyEvent::KEY_D, keyMap_[ 'E' ] = KeyEvent::KEY_E, keyMap_[ 'F' ] = KeyEvent::KEY_F,
		keyMap_[ 'G' ] = KeyEvent::KEY_G, keyMap_[ 'H' ] = KeyEvent::KEY_H, keyMap_[ 'I' ] = KeyEvent::KEY_I,
		keyMap_[ 'J' ] = KeyEvent::KEY_J, keyMap_[ 'K' ] = KeyEvent::KEY_K, keyMap_[ 'L' ] = KeyEvent::KEY_L,
		keyMap_[ 'M' ] = KeyEvent::KEY_M, keyMap_[ 'N' ] = KeyEvent::KEY_N, keyMap_[ 'O' ] = KeyEvent::KEY_O,
		keyMap_[ 'P' ] = KeyEvent::KEY_P, keyMap_[ 'Q' ] = KeyEvent::KEY_Q, keyMap_[ 'R' ] = KeyEvent::KEY_R,
		keyMap_[ 'S' ] = KeyEvent::KEY_S, keyMap_[ 'T' ] = KeyEvent::KEY_T, keyMap_[ 'U' ] = KeyEvent::KEY_U,
		keyMap_[ 'V' ] = KeyEvent::KEY_V, keyMap_[ 'W' ] = KeyEvent::KEY_W, keyMap_[ 'X' ] = KeyEvent::KEY_X,
		keyMap_[ 'Y' ] = KeyEvent::KEY_Y, keyMap_[ 'Z' ] = KeyEvent::KEY_Z;

		keyMap_[ ',' ] = KeyEvent::KEY_COMMA, keyMap_[ '.' ] = KeyEvent::KEY_PERIOD, keyMap_[ '/' ] = KeyEvent::KEY_SLASH,
		keyMap_[ ';' ] = KeyEvent::KEY_SEMICOLON, keyMap_[ '\'' ] = KeyEvent::KEY_APOSTROPHE, keyMap_[ '[' ] = KeyEvent::KEY_LBRACKET,
		keyMap_[ ']' ] = KeyEvent::KEY_RBRACKET, keyMap_[ '`' ] = KeyEvent::KEY_GRAVE, keyMap_[ '-' ] = KeyEvent::KEY_MINUS,
		keyMap_[ '=' ] = KeyEvent::KEY_EQUALS, keyMap_[ '\\' ] = KeyEvent::KEY_BACKSLASH;

		keyMap_[ ' ' ] = KeyEvent::KEY_SPACE;

		nameMap_[ "LSHIFT" ] = KeyEvent::KEY_LSHIFT, nameMap_[ "RSHIFT" ] = KeyEvent::KEY_RSHIFT;
		nameMap_[ "LCTRL" ] = KeyEvent::KEY_LCONTROL, nameMap_[ "RCTRL" ] = KeyEvent::KEY_RCONTROL;
		nameMap_[ "LALT" ] = KeyEvent::KEY_LALT, nameMap_[ "RALT" ] = KeyEvent::KEY_RALT;
		nameMap_[ "LWIN" ] = KeyEvent::KEY_LWIN, nameMap_[ "RWIN" ] = KeyEvent::KEY_RWIN;
		nameMap_[ "MENU" ] = KeyEvent::KEY_APPS;

		nameMap_[ "CAPSLOCK" ] = KeyEvent::KEY_CAPSLOCK;
		nameMap_[ "SCROLLLOCK" ] = KeyEvent::KEY_SCROLL;
		nameMap_[ "NUMLOCK" ] = KeyEvent::KEY_NUMLOCK;

		nameMap_[ "NUM0" ] = KeyEvent::KEY_NUMPAD0, nameMap_[ "NUM1" ] = KeyEvent::KEY_NUMPAD1,
		nameMap_[ "NUM2" ] = KeyEvent::KEY_NUMPAD2, nameMap_[ "NUM3" ] = KeyEvent::KEY_NUMPAD3,
		nameMap_[ "NUM4" ] = KeyEvent::KEY_NUMPAD4, nameMap_[ "NUM5" ] = KeyEvent::KEY_NUMPAD5,
		nameMap_[ "NUM6" ] = KeyEvent::KEY_NUMPAD6, nameMap_[ "NUM7" ] = KeyEvent::KEY_NUMPAD7,
		nameMap_[ "NUM8" ] = KeyEvent::KEY_NUMPAD8, nameMap_[ "NUM9" ] = KeyEvent::KEY_NUMPAD9;

		nameMap_[ "NUMMINUS" ] = KeyEvent::KEY_NUMPADMINUS, nameMap_[ "NUMPERIOD" ] = KeyEvent::KEY_NUMPADPERIOD,
		nameMap_[ "NUMADD" ] = KeyEvent::KEY_ADD, nameMap_[ "NUMSTAR" ] = KeyEvent::KEY_NUMPADSTAR,
		nameMap_[ "NUMENTER" ] = KeyEvent::KEY_NUMPADENTER, nameMap_[ "NUMSLASH" ] = KeyEvent::KEY_NUMPADSLASH,
		nameMap_[ "NUMRETURN" ] = KeyEvent::KEY_NUMPADENTER;

		nameMap_[ "RETURN" ] = KeyEvent::KEY_RETURN, nameMap_[ "ENTER" ] = KeyEvent::KEY_RETURN,
		nameMap_[ "TAB" ] = KeyEvent::KEY_TAB, nameMap_[ "ESCAPE" ] = KeyEvent::KEY_ESCAPE;

		nameMap_[ "F1" ] = KeyEvent::KEY_F1, nameMap_[ "F2" ] = KeyEvent::KEY_F2,
		nameMap_[ "F3" ] = KeyEvent::KEY_F3, nameMap_[ "F4" ] = KeyEvent::KEY_F4,
		nameMap_[ "F5" ] = KeyEvent::KEY_F5, nameMap_[ "F6" ] = KeyEvent::KEY_F6,
		nameMap_[ "F7" ] = KeyEvent::KEY_F7, nameMap_[ "F8" ] = KeyEvent::KEY_F8,
		nameMap_[ "F9" ] = KeyEvent::KEY_F9, nameMap_[ "F10" ] = KeyEvent::KEY_F10,
		nameMap_[ "F11" ] = KeyEvent::KEY_F11, nameMap_[ "F12" ] = KeyEvent::KEY_F12;

		nameMap_[ "UP" ] = KeyEvent::KEY_UPARROW, nameMap_[ "DOWN" ] = KeyEvent::KEY_DOWNARROW,
		nameMap_[ "LEFT" ] = KeyEvent::KEY_LEFTARROW, nameMap_[ "RIGHT" ] = KeyEvent::KEY_RIGHTARROW;

		nameMap_[ "INSERT" ] = KeyEvent::KEY_INSERT, nameMap_[ "HOME" ] = KeyEvent::KEY_HOME,
		nameMap_[ "PAGEUP" ] = KeyEvent::KEY_PGUP, nameMap_[ "PAGEDOWN" ] = KeyEvent::KEY_PGDN;
		nameMap_[ "DELETE" ] = KeyEvent::KEY_DELETE, nameMap_[ "END" ] = KeyEvent::KEY_END,
		nameMap_[ "BACKSPACE" ] = KeyEvent::KEY_BACKSPACE;
	}
}

bool BigworldInputDevice::isKeyDown( const std::string& key )
{
	bool result = false;;
	std::string shortcut = key;
	shortcut.erase( std::remove( shortcut.begin(), shortcut.end(), ' ' ),
		shortcut.end() );
	_strupr( &shortcut[0] );// safe?

	/***********************************************************************
	recognised keys:
	SHIFT, LSHIFT, RSHIFT, CTRL, LCTRL, RCTRL, ALT, LALT, RALT,
	WIN, LWIN, RWIN, MENU,
	CAPSLOCK, SCROLLLOCK, NUMLOCK,
	NUM0, NUM1, ..., NUM9, NUMMINUS, NUMPERIOD, NUMADD, NUMSTAR, NUMENTER, NUMSLASH,
	RETURN, ENTER, TAB, ESCAPE,
	F1, F2, F3, F4, ..., F12,
	UP, DOWN, LEFT, RIGHT,
	INSERT, HOME, PAGEUP, PAGEDOWN, DELETE, END, BACKSPACE
	***********************************************************************/
	std::set<std::string> modifiers;
	std::string keyname;
	while( shortcut.find( '+' ) != shortcut.npos )
	{
		modifiers.insert( shortcut.substr( 0, shortcut.find( '+' ) ) );
		shortcut = shortcut.substr( shortcut.find( '+' ) + 1 );
	}
	keyname = shortcut;
	if( keyname.size() && shortcut.size() )
	{
		// 1. is key down?
		bool keydown = down( keyDownTable_, keyname );
		if( !keydown && keyname.size() == 1 && down( keyDownTable_, keyname[0] ) )
			keydown = true;
		// 2. was key up?
		bool lastKeydown = down( lastKeyDown, keyname );
		if( !lastKeydown && keyname.size() == 1 && down( lastKeyDown, keyname[0] ) )
			lastKeydown = true;
		if( keydown && !lastKeydown )
		{
			int totalKeyNum = std::count( keyDownTable_, keyDownTable_ + KeyEvent::KEY_MAXIMUM_KEY, true );

			if( totalKeyNum == modifiers.size() + 1 )
			{
				result = true;
				for( std::set<std::string>::iterator iter = modifiers.begin();
					iter != modifiers.end(); ++iter )
				{
					if( !down( keyDownTable_, *iter ) )
						return false;
				}
			}
		}
	}
	return result;
}

void BigworldInputDevice::refreshKeyDownState( const bool* keyDown )
{
	memcpy( lastKeyDown, keyDown, sizeof( lastKeyDown[ 0 ] ) * KeyEvent::NUM_KEYS );
}

std::map<char, int> Win32InputDevice::keyMap_;
std::map<std::string, int> Win32InputDevice::nameMap_;
char Win32InputDevice::ch_;

bool Win32InputDevice::down( char ch )
{
	return keyMap_.find( ch ) != keyMap_.end() && ch_ == keyMap_[ ch ];
}

bool Win32InputDevice::down( const std::string& key )
{
	std::string left, right;
	if( _strcmpi(key.c_str(), "WIN" ) == 0 )
		return ( GetKeyState( VK_LWIN ) & 0x8000 ) != 0 || ( GetKeyState( VK_RWIN ) & 0x8000 ) != 0;
	else if( _strcmpi(key.c_str(), "LWIN" ) == 0 )
		return ( GetKeyState( VK_LWIN ) & 0x8000 ) != 0;
	else if( _strcmpi(key.c_str(), "RWIN" ) == 0 )
		return ( GetKeyState( VK_RWIN ) & 0x8000 ) != 0;
	else if( _strcmpi(key.c_str(), "CTRL" ) == 0 )
		return ( GetKeyState( VK_CONTROL ) & 0x8000 ) != 0;
	else if( _strcmpi(key.c_str(), "LCTRL" ) == 0 )
		return ( GetKeyState( VK_LCONTROL ) & 0x8000 ) != 0;
	else if( _strcmpi(key.c_str(), "RCTRL" ) == 0 )
		return ( GetKeyState( VK_RCONTROL ) & 0x8000 ) != 0;
	else if( _strcmpi(key.c_str(), "SHIFT" ) == 0 )
		return ( GetKeyState( VK_SHIFT ) & 0x8000 ) != 0;
	else if( _strcmpi(key.c_str(), "LSHIFT" ) == 0 )
		return ( GetKeyState( VK_LSHIFT ) & 0x8000 ) != 0;
	else if( _strcmpi(key.c_str(), "RSHIFT" ) == 0 )
		return ( GetKeyState( VK_RSHIFT ) & 0x8000 ) != 0;
	else if( _strcmpi(key.c_str(), "ALT" ) == 0 )
		return ( GetKeyState( VK_LMENU ) & 0x8000 ) != 0 || ( GetKeyState( VK_RMENU ) & 0x8000 ) != 0;
	else if( _strcmpi(key.c_str(), "LALT" ) == 0 )
		return ( GetKeyState( VK_LMENU ) & 0x8000 ) != 0;
	else if( _strcmpi(key.c_str(), "RALT" ) == 0 )
		return ( GetKeyState( VK_RMENU ) & 0x8000 ) != 0;
	else if( nameMap_.find( key ) != nameMap_.end() )
		return nameMap_[ key ] == ch_;
	return false;
}

Win32InputDevice::Win32InputDevice( char ch )
{
	ch_ = ch;
	if( keyMap_.empty() )
	{
		keyMap_[ '0' ] = KeyEvent::KEY_0, keyMap_[ '1' ] = KeyEvent::KEY_1, keyMap_[ '2' ] = KeyEvent::KEY_2,
		keyMap_[ '3' ] = KeyEvent::KEY_3, keyMap_[ '4' ] = KeyEvent::KEY_4, keyMap_[ '5' ] = KeyEvent::KEY_5,
		keyMap_[ '6' ] = KeyEvent::KEY_6, keyMap_[ '7' ] = KeyEvent::KEY_7, keyMap_[ '8' ] = KeyEvent::KEY_8,
		keyMap_[ '9' ] = KeyEvent::KEY_9;

		keyMap_[ 'A' ] = KeyEvent::KEY_A, keyMap_[ 'B' ] = KeyEvent::KEY_B, keyMap_[ 'C' ] = KeyEvent::KEY_C,
		keyMap_[ 'D' ] = KeyEvent::KEY_D, keyMap_[ 'E' ] = KeyEvent::KEY_E, keyMap_[ 'F' ] = KeyEvent::KEY_F,
		keyMap_[ 'G' ] = KeyEvent::KEY_G, keyMap_[ 'H' ] = KeyEvent::KEY_H, keyMap_[ 'I' ] = KeyEvent::KEY_I,
		keyMap_[ 'J' ] = KeyEvent::KEY_J, keyMap_[ 'K' ] = KeyEvent::KEY_K, keyMap_[ 'L' ] = KeyEvent::KEY_L,
		keyMap_[ 'M' ] = KeyEvent::KEY_M, keyMap_[ 'N' ] = KeyEvent::KEY_N, keyMap_[ 'O' ] = KeyEvent::KEY_O,
		keyMap_[ 'P' ] = KeyEvent::KEY_P, keyMap_[ 'Q' ] = KeyEvent::KEY_Q, keyMap_[ 'R' ] = KeyEvent::KEY_R,
		keyMap_[ 'S' ] = KeyEvent::KEY_S, keyMap_[ 'T' ] = KeyEvent::KEY_T, keyMap_[ 'U' ] = KeyEvent::KEY_U,
		keyMap_[ 'V' ] = KeyEvent::KEY_V, keyMap_[ 'W' ] = KeyEvent::KEY_W, keyMap_[ 'X' ] = KeyEvent::KEY_X,
		keyMap_[ 'Y' ] = KeyEvent::KEY_Y, keyMap_[ 'Z' ] = KeyEvent::KEY_Z;

		keyMap_[ ',' ] = KeyEvent::KEY_COMMA, keyMap_[ '.' ] = KeyEvent::KEY_PERIOD, keyMap_[ '/' ] = KeyEvent::KEY_SLASH,
		keyMap_[ ';' ] = KeyEvent::KEY_SEMICOLON, keyMap_[ '\'' ] = KeyEvent::KEY_APOSTROPHE, keyMap_[ '[' ] = KeyEvent::KEY_LBRACKET,
		keyMap_[ ']' ] = KeyEvent::KEY_RBRACKET, keyMap_[ '`' ] = KeyEvent::KEY_GRAVE, keyMap_[ '-' ] = KeyEvent::KEY_MINUS,
		keyMap_[ '=' ] = KeyEvent::KEY_EQUALS, keyMap_[ '\\' ] = KeyEvent::KEY_BACKSLASH;

		keyMap_[ ' ' ] = KeyEvent::KEY_SPACE;

		nameMap_[ "LSHIFT" ] = KeyEvent::KEY_LSHIFT, nameMap_[ "RSHIFT" ] = KeyEvent::KEY_RSHIFT;
		nameMap_[ "LCTRL" ] = KeyEvent::KEY_LCONTROL, nameMap_[ "RCTRL" ] = KeyEvent::KEY_RCONTROL;
		nameMap_[ "LALT" ] = KeyEvent::KEY_LALT, nameMap_[ "RALT" ] = KeyEvent::KEY_RALT;
		nameMap_[ "LWIN" ] = KeyEvent::KEY_LWIN, nameMap_[ "RWIN" ] = KeyEvent::KEY_RWIN;
		nameMap_[ "MENU" ] = KeyEvent::KEY_APPS;

		nameMap_[ "CAPSLOCK" ] = KeyEvent::KEY_CAPSLOCK;
		nameMap_[ "SCROLLLOCK" ] = KeyEvent::KEY_SCROLL;
		nameMap_[ "NUMLOCK" ] = KeyEvent::KEY_NUMLOCK;

		nameMap_[ "NUM0" ] = KeyEvent::KEY_NUMPAD0, nameMap_[ "NUM1" ] = KeyEvent::KEY_NUMPAD1,
		nameMap_[ "NUM2" ] = KeyEvent::KEY_NUMPAD2, nameMap_[ "NUM3" ] = KeyEvent::KEY_NUMPAD3,
		nameMap_[ "NUM4" ] = KeyEvent::KEY_NUMPAD4, nameMap_[ "NUM5" ] = KeyEvent::KEY_NUMPAD5,
		nameMap_[ "NUM6" ] = KeyEvent::KEY_NUMPAD6, nameMap_[ "NUM7" ] = KeyEvent::KEY_NUMPAD7,
		nameMap_[ "NUM8" ] = KeyEvent::KEY_NUMPAD8, nameMap_[ "NUM9" ] = KeyEvent::KEY_NUMPAD9;

		nameMap_[ "NUMMINUS" ] = KeyEvent::KEY_NUMPADMINUS, nameMap_[ "NUMPERIOD" ] = KeyEvent::KEY_NUMPADPERIOD,
		nameMap_[ "NUMADD" ] = KeyEvent::KEY_ADD, nameMap_[ "NUMSTAR" ] = KeyEvent::KEY_NUMPADSTAR,
		nameMap_[ "NUMENTER" ] = KeyEvent::KEY_NUMPADENTER, nameMap_[ "NUMSLASH" ] = KeyEvent::KEY_NUMPADSLASH,
		nameMap_[ "NUMRETURN" ] = KeyEvent::KEY_NUMPADENTER;

		nameMap_[ "RETURN" ] = KeyEvent::KEY_RETURN, nameMap_[ "ENTER" ] = KeyEvent::KEY_RETURN,
		nameMap_[ "TAB" ] = KeyEvent::KEY_TAB, nameMap_[ "ESCAPE" ] = KeyEvent::KEY_ESCAPE;

		nameMap_[ "F1" ] = KeyEvent::KEY_F1, nameMap_[ "F2" ] = KeyEvent::KEY_F2,
		nameMap_[ "F3" ] = KeyEvent::KEY_F3, nameMap_[ "F4" ] = KeyEvent::KEY_F4,
		nameMap_[ "F5" ] = KeyEvent::KEY_F5, nameMap_[ "F6" ] = KeyEvent::KEY_F6,
		nameMap_[ "F7" ] = KeyEvent::KEY_F7, nameMap_[ "F8" ] = KeyEvent::KEY_F8,
		nameMap_[ "F9" ] = KeyEvent::KEY_F9, nameMap_[ "F10" ] = KeyEvent::KEY_F10,
		nameMap_[ "F11" ] = KeyEvent::KEY_F11, nameMap_[ "F12" ] = KeyEvent::KEY_F12;

		nameMap_[ "UP" ] = KeyEvent::KEY_UPARROW, nameMap_[ "DOWN" ] = KeyEvent::KEY_DOWNARROW,
		nameMap_[ "LEFT" ] = KeyEvent::KEY_LEFTARROW, nameMap_[ "RIGHT" ] = KeyEvent::KEY_RIGHTARROW;

		nameMap_[ "INSERT" ] = KeyEvent::KEY_INSERT, nameMap_[ "HOME" ] = KeyEvent::KEY_HOME,
		nameMap_[ "PAGEUP" ] = KeyEvent::KEY_PGUP, nameMap_[ "PAGEDOWN" ] = KeyEvent::KEY_PGDN;
		nameMap_[ "DELETE" ] = KeyEvent::KEY_DELETE, nameMap_[ "END" ] = KeyEvent::KEY_END,
		nameMap_[ "BACKSPACE" ] = KeyEvent::KEY_BACKSPACE;
	}
}

bool Win32InputDevice::isKeyDown( const std::string& key )
{
	bool result = false;;
	std::string shortcut = key;
	shortcut.erase( std::remove( shortcut.begin(), shortcut.end(), ' ' ),
		shortcut.end() );
	_strupr( &shortcut[0] );// safe?

	/***********************************************************************
	recognised keys:
	SHIFT, LSHIFT, RSHIFT, CTRL, LCTRL, RCTRL, ALT, LALT, RALT,
	WIN, LWIN, RWIN, MENU,
	CAPSLOCK, SCROLLLOCK, NUMLOCK,
	NUM0, NUM1, ..., NUM9, NUMMINUS, NUMPERIOD, NUMADD, NUMSTAR, NUMENTER, NUMSLASH,
	RETURN, ENTER, TAB, ESCAPE,
	F1, F2, F3, F4, ..., F12,
	UP, DOWN, LEFT, RIGHT,
	INSERT, HOME, PAGEUP, PAGEDOWN, DELETE, END, BACKSPACE
	***********************************************************************/
	std::set<std::string> modifiers;
	std::string keyname;
	while( shortcut.find( '+' ) != shortcut.npos )
	{
		modifiers.insert( shortcut.substr( 0, shortcut.find( '+' ) ) );
		shortcut = shortcut.substr( shortcut.find( '+' ) + 1 );
	}
	keyname = shortcut;
	if( !keyname.empty() )
	{
		bool keydown = down( keyname );
		if( !keydown && keyname.size() == 1 && down( keyname[0] ) )
			keydown = true;

		if( keydown )
		{
			result = true;
			for( std::set<std::string>::iterator iter = modifiers.begin();
				iter != modifiers.end(); ++iter )
			{
				if( !down( *iter ) )
					return false;
			}
		}
	}
	return result;
}

static HHOOK s_hook = NULL;

static LRESULT CALLBACK GUIKeyboadProc( int nCode, WPARAM wParam, LPARAM lParam )
{
	// NOTE: no input will be processed if the main app window is disabled,
	// i.e. when a modal popup window is running. This prevents keystrokes
	// such as Ctrl+Z from working when a modal window is on top.
	if( nCode == HC_ACTION && ( lParam & 0xc0000000 ) == 0 && AfxGetMainWnd()->IsWindowEnabled() )
		GUI::Manager::instance().processInput( &GUI::Win32InputDevice( char( lParam / 65536 % 256 ) ) );
	return CallNextHookEx( s_hook, nCode, wParam, lParam );
}

void Win32InputDevice::install()
{
	s_hook = SetWindowsHookEx( WH_KEYBOARD, GUIKeyboadProc, NULL, GetCurrentThreadId() );
}

void Win32InputDevice::fini()
{
	if (s_hook != NULL)
	{
		UnhookWindowsHookEx( s_hook );
		s_hook = NULL;
	}
}

END_GUI_NAMESPACE
