/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef INPUT_HPP
#define INPUT_HPP

// identifier was truncated to '255' characters in the browser information
#pragma warning(disable: 4786)

#include "cstdmf/singleton.hpp"
#include "cstdmf/stdmf.hpp"
#include "cstdmf/stringmap.hpp"
#include "cstdmf/guard.hpp"

#include <string>
#include <vector>
#include <map>

struct IDirectInput8;
struct IDirectInputDevice8;

const uint32 MODIFIER_SHIFT		= 0x1;
const uint32 MODIFIER_CTRL		= 0x2;
const uint32 MODIFIER_ALT		= 0x4;


/**
 *	The base class for event types.
 */
class MFEvent
{
public:
	/**
	 *	This enumeration is used to indicate the type of an MFEvent.
	 */
	enum Type
	{
		NOT_SET,	///< The type is not yet set.

		KEY_DOWN,	///< The event corresponds to a key being pressed.
		KEY_UP,		///< The event corresponds to a key being released.

		MOUSE_MOVE,	///< The event corresponds to the mouse moving.

		AXIS_HOLD	///< The event corresponds to an axis held off zero.
	};

	/// Constructor.
	MFEvent( Type type ) : type_( type )	{};

	/// This method returns the type of this event.
	Type type() const			{ return type_; }

private:
	Type type_;
};


/**
 *	An instance of this class represents a key/button press. It may be from the
 *	keyboard, mouse, or a joystick.
 */
class KeyEvent : public MFEvent
{
public:
	/**
	 *	This enumeration is used to specify keys.
	 *
	 *	These enumeration values are chosen so that they match the DirectInput
	 *	values. See dinput.h
	 */
	enum Key
	{
		KEY_NOT_FOUND		= 0x00,
		KEY_NONE			= 0x00,

		// Keyboard Buttons.
		KEY_MINIMUM_KEY		= 0x01,

		KEY_ESCAPE          = 0x01,
		KEY_1               = 0x02,
		KEY_2               = 0x03,
		KEY_3               = 0x04,
		KEY_4               = 0x05,
		KEY_5               = 0x06,
		KEY_6               = 0x07,
		KEY_7               = 0x08,
		KEY_8               = 0x09,
		KEY_9               = 0x0A,
		KEY_0               = 0x0B,
		KEY_MINUS           = 0x0C,    /* - on main keyboard */
		KEY_EQUALS          = 0x0D,
		KEY_BACKSPACE       = 0x0E,    /* backspace */
		KEY_TAB             = 0x0F,
		KEY_Q               = 0x10,
		KEY_W               = 0x11,
		KEY_E               = 0x12,
		KEY_R               = 0x13,
		KEY_T               = 0x14,
		KEY_Y               = 0x15,
		KEY_U               = 0x16,
		KEY_I               = 0x17,
		KEY_O               = 0x18,
		KEY_P               = 0x19,
		KEY_LBRACKET        = 0x1A,
		KEY_RBRACKET        = 0x1B,
		KEY_RETURN          = 0x1C,    /* Enter on main keyboard */
		KEY_LCONTROL        = 0x1D,
		KEY_A               = 0x1E,
		KEY_S               = 0x1F,
		KEY_D               = 0x20,
		KEY_F               = 0x21,
		KEY_G               = 0x22,
		KEY_H               = 0x23,
		KEY_J               = 0x24,
		KEY_K               = 0x25,
		KEY_L               = 0x26,
		KEY_SEMICOLON       = 0x27,
		KEY_APOSTROPHE      = 0x28,
		KEY_GRAVE           = 0x29,    /* accent grave */
		KEY_LSHIFT          = 0x2A,
		KEY_BACKSLASH       = 0x2B,
		KEY_Z               = 0x2C,
		KEY_X               = 0x2D,
		KEY_C               = 0x2E,
		KEY_V               = 0x2F,
		KEY_B               = 0x30,
		KEY_N               = 0x31,
		KEY_M               = 0x32,
		KEY_COMMA           = 0x33,
		KEY_PERIOD          = 0x34,    /* . on main keyboard */
		KEY_SLASH           = 0x35,    /* / on main keyboard */
		KEY_RSHIFT          = 0x36,
		KEY_NUMPADSTAR      = 0x37,    /* * on numeric keypad */
		KEY_LALT            = 0x38,    /* left Alt */
		KEY_SPACE           = 0x39,
		KEY_CAPSLOCK        = 0x3A,
		KEY_F1              = 0x3B,
		KEY_F2              = 0x3C,
		KEY_F3              = 0x3D,
		KEY_F4              = 0x3E,
		KEY_F5              = 0x3F,
		KEY_F6              = 0x40,
		KEY_F7              = 0x41,
		KEY_F8              = 0x42,
		KEY_F9              = 0x43,
		KEY_F10             = 0x44,
		KEY_NUMLOCK         = 0x45,
		KEY_SCROLL          = 0x46,    /* Scroll Lock */
		KEY_NUMPAD7         = 0x47,
		KEY_NUMPAD8         = 0x48,
		KEY_NUMPAD9         = 0x49,
		KEY_NUMPADMINUS     = 0x4A,    /* - on numeric keypad */
		KEY_NUMPAD4         = 0x4B,
		KEY_NUMPAD5         = 0x4C,
		KEY_NUMPAD6         = 0x4D,
		KEY_ADD             = 0x4E,    /* + on numeric keypad */
		KEY_NUMPAD1         = 0x4F,
		KEY_NUMPAD2         = 0x50,
		KEY_NUMPAD3         = 0x51,
		KEY_NUMPAD0         = 0x52,
		KEY_NUMPADPERIOD    = 0x53,    /* . on numeric keypad */
		KEY_OEM_102         = 0x56,    /* < > | on UK/Germany keyboards */
		KEY_F11             = 0x57,
		KEY_F12             = 0x58,

		KEY_F13             = 0x64,    /*                     (NEC PC98) */
		KEY_F14             = 0x65,    /*                     (NEC PC98) */
		KEY_F15             = 0x66,    /*                     (NEC PC98) */

		KEY_KANA            = 0x70,    /* (Japanese keyboard)            */
		KEY_ABNT_C1         = 0x73,    /* / ? on Portugese (Brazilian) keyboards */
		KEY_CONVERT         = 0x79,    /* (Japanese keyboard)            */
		KEY_NOCONVERT       = 0x7B,    /* (Japanese keyboard)            */
		KEY_YEN             = 0x7D,    /* (Japanese keyboard)            */
		KEY_ABNT_C2         = 0x7E,    /* Numpad . on Portugese (Brazilian) keyboards */
		KEY_NUMPADEQUALS    = 0x8D,    /* = on numeric keypad (NEC PC98) */
		KEY_PREVTRACK       = 0x90,    /* Previous Track (DIK_CIRCUMFLEX on Japanese keyboard) */
		KEY_AT              = 0x91,    /*                     (NEC PC98) */
		KEY_COLON           = 0x92,    /*                     (NEC PC98) */
		KEY_UNDERLINE       = 0x93,    /*                     (NEC PC98) */
		KEY_KANJI           = 0x94,    /* (Japanese keyboard)            */
		KEY_STOP            = 0x95,    /*                     (NEC PC98) */
		KEY_AX              = 0x96,    /*                     (Japan AX) */
		KEY_UNLABELED       = 0x97,    /*                        (J3100) */
		KEY_NEXTTRACK       = 0x99,    /* Next Track */
		KEY_NUMPADENTER     = 0x9C,    /* Enter on numeric keypad */
		KEY_RCONTROL        = 0x9D,
		KEY_MUTE            = 0xA0,    /* Mute */
		KEY_CALCULATOR      = 0xA1,    /* Calculator */
		KEY_PLAYPAUSE       = 0xA2,    /* Play / Pause */
		KEY_MEDIASTOP       = 0xA4,    /* Media Stop */
		KEY_VOLUMEDOWN      = 0xAE,    /* Volume - */
		KEY_VOLUMEUP        = 0xB0,    /* Volume + */
		KEY_WEBHOME         = 0xB2,    /* Web home */
		KEY_NUMPADCOMMA     = 0xB3,    /* , on numeric keypad (NEC PC98) */
		KEY_NUMPADSLASH     = 0xB5,    /* / on numeric keypad */
		KEY_SYSRQ           = 0xB7,
		KEY_RALT            = 0xB8,    /* right Alt */
		KEY_PAUSE           = 0xC5,    /* Pause */
		KEY_HOME            = 0xC7,    /* Home on arrow keypad */
		KEY_UPARROW         = 0xC8,    /* UpArrow on arrow keypad */
		KEY_PGUP            = 0xC9,    /* PgUp on arrow keypad */
		KEY_LEFTARROW       = 0xCB,    /* LeftArrow on arrow keypad */
		KEY_RIGHTARROW      = 0xCD,    /* RightArrow on arrow keypad */
		KEY_END             = 0xCF,    /* End on arrow keypad */
		KEY_DOWNARROW       = 0xD0,    /* DownArrow on arrow keypad */
		KEY_PGDN            = 0xD1,    /* PgDn on arrow keypad */
		KEY_INSERT          = 0xD2,    /* Insert on arrow keypad */
		KEY_DELETE          = 0xD3,    /* Delete on arrow keypad */
		KEY_LWIN            = 0xDB,    /* Left Windows key */
		KEY_RWIN            = 0xDC,    /* Right Windows key */
		KEY_APPS            = 0xDD,    /* AppMenu key */
		KEY_POWER           = 0xDE,    /* System Power */
		KEY_SLEEP           = 0xDF,    /* System Sleep */
		KEY_WAKE            = 0xE3,    /* System Wake */
		KEY_WEBSEARCH       = 0xE5,    /* Web Search */
		KEY_WEBFAVORITES    = 0xE6,    /* Web Favorites */
		KEY_WEBREFRESH      = 0xE7,    /* Web Refresh */
		KEY_WEBSTOP         = 0xE8,    /* Web Stop */
		KEY_WEBFORWARD      = 0xE9,    /* Web Forward */
		KEY_WEBBACK         = 0xEA,    /* Web Back */
		KEY_MYCOMPUTER      = 0xEB,    /* My Computer */
		KEY_MAIL            = 0xEC,    /* Mail */
		KEY_MEDIASELECT     = 0xED,    /* Media Select */

		KEY_MAXIMUM_KEY		= 0xED,

		// Mouse Buttons.
		KEY_MINIMUM_MOUSE	= 0x100,

		KEY_MOUSE0          = 0x100,
		KEY_LEFTMOUSE       = 0x100,

		KEY_MOUSE1          = 0x101,
		KEY_RIGHTMOUSE      = 0x101,

		KEY_MOUSE2          = 0x102,
		KEY_MIDDLEMOUSE     = 0x102,

		KEY_MOUSE3          = 0x103,
		KEY_MOUSE4          = 0x104,
		KEY_MOUSE5          = 0x105,
		KEY_MOUSE6          = 0x106,
		KEY_MOUSE7          = 0x107,

		KEY_MAXIMUM_MOUSE	= 0x107,

		// Joystick Buttons.
		KEY_MINIMUM_JOY		= 0x110,

		// Numbered
		KEY_JOY0			= 0x110,
		KEY_JOY1			= 0x111,
		KEY_JOY2			= 0x112,
		KEY_JOY3			= 0x113,
		KEY_JOY4			= 0x114,
		KEY_JOY5			= 0x115,
		KEY_JOY6			= 0x116,
		KEY_JOY7			= 0x117,
		KEY_JOY8			= 0x118,
		KEY_JOY9			= 0x119,
		KEY_JOY10			= 0x11A,
		KEY_JOY11			= 0x11B,
		KEY_JOY12			= 0x11C,
		KEY_JOY13			= 0x11D,
		KEY_JOY14			= 0x11E,
		KEY_JOY15			= 0x11F,
		KEY_JOY16			= 0x120,
		KEY_JOY17			= 0x121,
		KEY_JOY18			= 0x122,
		KEY_JOY19			= 0x123,
		KEY_JOY20			= 0x124,
		KEY_JOY21			= 0x125,
		KEY_JOY22			= 0x126,
		KEY_JOY23			= 0x127,
		KEY_JOY24			= 0x128,
		KEY_JOY25			= 0x129,
		KEY_JOY26			= 0x12A,
		KEY_JOY27			= 0x12B,
		KEY_JOY28			= 0x12C,
		KEY_JOY29			= 0x12D,
		KEY_JOY30			= 0x12E,
		KEY_JOY31			= 0x12F,

		// Aliae
		KEY_JOYDUP			= 0x110,
		KEY_JOYDDOWN		= 0x111,
		KEY_JOYDLEFT		= 0x112,
		KEY_JOYDRIGHT		= 0x113,
		KEY_JOYSTART		= 0x114,
		KEY_JOYSELECT		= 0x115,
		KEY_JOYBACK			= 0x115,
		KEY_JOYALPUSH		= 0x116,
		KEY_JOYARPUSH		= 0x117,

		KEY_JOYCROSS		= 0x118,
		KEY_JOYA			= 0x118,
		KEY_JOYCIRCLE		= 0x119,
		KEY_JOYB			= 0x119,
		KEY_JOYSQUARE		= 0x11A,
		KEY_JOYX			= 0x11A,
		KEY_JOYTRIANGLE		= 0x11B,
		KEY_JOYY			= 0x11B,

		KEY_JOYL1			= 0x11C,
		KEY_JOYBLACK		= 0x11C,
		KEY_JOYR1			= 0x11D,
		KEY_JOYWHITE		= 0x11D,

		KEY_JOYL2			= 0x11E,
		KEY_JOYLTRIGGER		= 0x11E,
		KEY_JOYR2			= 0x11F,
		KEY_JOYRTRIGGER		= 0x11F,

		KEY_JOYAHARD		= 0x120,
		KEY_JOYBHARD		= 0x121,
		KEY_JOYXHARD		= 0x122,
		KEY_JOYYHARD		= 0x123,
		KEY_JOYBLACKHARD	= 0x124,
		KEY_JOYWHITEHARD	= 0x125,
		KEY_JOYLTRIGGERHARD	= 0x126,
		KEY_JOYRTRIGGERHARD	= 0x127,

		KEY_JOYALUP			= 0x130,
		KEY_JOYALDOWN		= 0x131,
		KEY_JOYALLEFT		= 0x132,
		KEY_JOYALRIGHT		= 0x133,
		KEY_JOYARUP			= 0x134,
		KEY_JOYARDOWN		= 0x135,
		KEY_JOYARLEFT		= 0x136,
		KEY_JOYARRIGHT		= 0x137,

		KEY_MAXIMUM_JOY		= 0x137,
		KEY_DEBUG			= 0x138,

		NUM_KEYS
	};

	/// Constructor.
	KeyEvent( Type type,
			Key key,
			uint32 modifiers ) :
		MFEvent( type ),
		key_( key ),
		modifiers_( modifiers ) {}

	/// Default constructor
	KeyEvent() :
		MFEvent( NOT_SET ),
		key_( KEY_NONE ),
		modifiers_( 0 ) {}

	/// Returns the actual key that was pressed. e.g. 'e' and 'E' are the same.
	Key key() const { return key_; }

	/// This method returns whether or not this event was caused by a key being
	///	pressed down.
	bool isKeyDown() const	{ return this->type() == KEY_DOWN; }
	/// This method returns whether or not this event was caused by a key being
	///	released.
	bool isKeyUp() const	{ return this->type() == KEY_UP; }

	/// Returns the actual character pressed. e.g. 'e' and 'E' are different.
	char character() const;

	/// This method returns the key associated with the input string.
	static Key stringToKey( const std::string & str )
		{ return keyMap_.stringToKey( str ); }

	/// This method returns the string associated with the input key.
	static const char * keyToString( const Key key )
		{ return keyMap_.keyToString( key ); }

	/// This method returns whether or not either Shift key was down when this
	///	event occurred.
	bool isShiftDown() const
					{ return !!(modifiers_ & MODIFIER_SHIFT); }
	/// This method returns whether or not either Ctrl key was down when this
	///	event occurred.
	bool isCtrlDown() const
					{ return !!(modifiers_ & MODIFIER_CTRL); }
	/// This method returns whether or not either Alt key was down when this
	///	event occurred.
	bool isAltDown() const
					{ return !!(modifiers_ & MODIFIER_ALT); }

	/// This method returns the modifiers that are currently down.
	uint32 modifiers() const		{ return modifiers_; }

private:
	Key key_;
	uint32 modifiers_;

	class KeyMap
	{
		public:
			KeyMap();

			Key stringToKey( const std::string & str ) const;
			const char * keyToString( const Key & key ) const;

		private:
			StringHashMap<Key> map_;
	};

	static KeyMap keyMap_;
};


/**
 *	An instance of this class represents a mouse movement event.
 */
class MouseEvent : public MFEvent
{
public:
	/// Constructor
	explicit MouseEvent( long dx = 0, long dy = 0, long dz = 0 ) :
		MFEvent( MOUSE_MOVE ),
		dx_( dx ),
		dy_( dy ),
		dz_( dz )
	{}

	/**
	 *	This method returns the amount the mouse has moved along the x-axis
	 *	during this event.
	 */
	long dx() const		{ return dx_; }

	/**
	 *	This method returns the amount the mouse has moved along the y-axis
	 *	during this event.
	 */
	long dy() const		{ return dy_; }

	/**
	 *	This method returns the amount the mouse has moved along the z-axis
	 *	during this event. The z-axis corresponds to the mouse wheel.
	 */
	long dz() const		{ return dz_; }

private:
	long dx_;
	long dy_;
	long dz_;
};


/**
 *	An instance of this class represents a spring-loaded axis offset
 *	for a certain amount of time.
 */
class AxisEvent : public MFEvent
{
public:
	/**
	 *	This enumeration is used to identify different parts of a joypad
	 */
	enum Axis
	{
		AXIS_LX,
		AXIS_LY,
		AXIS_RX,
		AXIS_RY,
		NUM_AXES
	};

	/// Constructor
	explicit AxisEvent( Axis a = NUM_AXES, float v = 0.f, float dT = 0.f ) :
		MFEvent( AXIS_HOLD ),
		axis_( a ),
		value_( v ),
		dTime_( dT )
	{}

	Axis axis() const		{ return axis_; }
	float value() const		{ return value_; }
	float dTime() const		{ return dTime_; }

private:
	Axis	axis_;
	float	value_;
	float	dTime_;
};



/**
 *	This abstract base class is an interface for all input handlers.
 */
class InputHandler
{
public:
	virtual bool handleKeyEvent( const KeyEvent & event );
	virtual bool handleMouseEvent( const MouseEvent & event );
	virtual bool handleAxisEvent( const AxisEvent & event );
};


/**
 *	This class represents a joystick.
 */
class Joystick
{
public:
	/**
	 *	This class is used to represent a part (or axis) of the joystick. An
	 *	axis, in general, has a value between -1 and 1.
	 */
	class Axis
	{
		public:
			/// Constructor.
			Axis() :
				value_( 0 ),
				enabled_( false ),
				sentZero_( true )		{}

			/// This method returns the value associated with this axis.
			operator float() const			{ return this->value(); }

			/// This method returns the value associated with this axis.
			float value() const				{ return value_; }
			/// This method sets the value associated with this axis.
			void value( float value )		{ value_ = value; }

			/// This method returns whether or not this axis is enabled.
			bool enabled() const			{ return enabled_; }
			/// This method sets whether or not this axis is enabled.
			void enabled( bool enabled )	{ enabled_ = enabled; }

			/// This method returns whether or not a zero position event has been sent.
			bool sentZero() const			{ return sentZero_; }
			/// This method sets whether or not a zero position event has been sent.
			void sentZero( bool sentZero )	{ sentZero_ = sentZero; }

		private:
			float value_;
			bool enabled_;
			bool sentZero_;
	};

	/// @name Constructors and Initialisation
	//@{
	Joystick();

	bool init( IDirectInput8 * pDirectInput,
		void * hWnd );

	bool update();
	//@}

	bool processEvents( InputHandler & handler,
		bool * pIsKeyDown = NULL,
		bool * pLostDataFlag = NULL );

	/// @name Accessors
	//@{
	IDirectInputDevice8 * pDIJoystick() const	{ return pDIJoystick_; }

	void useKeyboard()							{ isUsingKeyboard_ = true; }
	void useJoystick()							{ isUsingKeyboard_ = false; }
	bool isUsingKeyboard( void )				{ return isUsingKeyboard_; }

	Axis & getAxis( AxisEvent::Axis t )				{ return axis_[ t ]; }
	const Axis & getAxis( AxisEvent::Axis t ) const	{ return axis_[ t ]; }

	int stickDirection( int stick ) const		{ return quantJoyDir_[stick]; }
	//@}

private:
	bool updateFromKeyboardDevice();
	bool updateFromJoystickDevice();

	bool hasJoystick() const					{ return pDIJoystick_ != NULL; }

	void generateKeyEvent( bool isDown, int key, InputHandler & handler,
		bool * pIsKeyDown );

	void generateCommonEvents( InputHandler & handler, bool * pIsKeyDown );

private:
	IDirectInputDevice8 * pDIJoystick_;

	KeyEvent::Key xMinKey_;
	KeyEvent::Key xMaxKey_;
	KeyEvent::Key yMinKey_;
	KeyEvent::Key yMaxKey_;

	bool isUsingKeyboard_;
	std::vector<Axis> axis_;

	uint64		lastProcessedTime_;

	int			quantJoyDir_[2];
};



/**
 *	This class manages the input devices.
 */
class InputDevices : public Singleton< InputDevices >
{
public:
	// Flags
	static const int	EXCLUSIVE_MODE;
	
	InputDevices();
	~InputDevices();

	bool init( void * hInst, void * hWnd, int flags );

	static bool processEvents( InputHandler & handler );

	static uint32 modifiers();

	static bool isKeyDown( KeyEvent::Key key );
	static bool isAltDown();
	static bool isCtrlDown();
	static bool isShiftDown();

	/// This method returns an object representing the joystick.
	static Joystick & joystick()	{ return instance().joystick_; }

	/// This method sets whether or not the application currently has focus.
	static void setFocus( bool state )	{ focus_ = state; }
	static bool hasFocus()				{ return focus_; }

	static bool * keyDownTable()	{ return instance().isKeyDown_; }

private:
	// Making these void * to avoid including windows.h
	bool privateInit( void * hInst, void * hWnd, int flags );

	bool privateProcessEvents( InputHandler & handler );

	enum lostDataFlags
	{
		NO_DATA_LOST	= 0,
		KEY_DATA_LOST	= 1 << 0,
		MOUSE_DATA_LOST	= 1 << 1,
		JOY_DATA_LOST	= 1 << 2
	};
	void handleLostData( InputHandler & handler, int mask );


	IDirectInput8 * pDirectInput_;
	IDirectInputDevice8 *	pKeyboard_;
	IDirectInputDevice8 *	pMouse_;
	bool					keyboardAcquired_;
	bool					mouseAcquired_;

	Joystick joystick_;

	bool isKeyDown_[ KeyEvent::NUM_KEYS ];

	static bool focus_;
	int lostData_;
};


/**
 *	This class is an interface allowing external files to provide
 *	kyeboard devices.
 */
class KeyboardDevice
{
public:
	virtual void update() = 0;
	virtual bool next( KeyEvent & event ) = 0;
};

extern std::vector<KeyboardDevice*> gVirtualKeyboards;


#ifdef CODE_INLINE
#include "input.ipp"
#endif




#endif
/*input.hpp*/
