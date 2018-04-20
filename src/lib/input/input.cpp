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

#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define INITGUID
#include <windows.h>

#include <objbase.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "input.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/dogwatch.hpp"

DECLARE_DEBUG_COMPONENT2( "UI", 0 )

#ifndef CODE_INLINE
#include "input.ipp"
#endif


// -----------------------------------------------------------------------------
// Section: Data declaration
// -----------------------------------------------------------------------------

/// device input library Singleton
BW_SINGLETON_STORAGE( InputDevices )


const int DIRECT_INPUT_AXIS_MAX = 1000;
const int DIRECT_INPUT_AXIS_DEAD_ZONE = 150;

bool InputDevices::focus_;

const int InputDevices::EXCLUSIVE_MODE = 0x01;



// -----------------------------------------------------------------------------
// Section: InputDevices
// -----------------------------------------------------------------------------

static const int KEYBOARD_BUFFER_SIZE = 32;
static const int MOUSE_BUFFER_SIZE = 64;
static const int JOYSTICK_BUFFER_SIZE = 32;

/**
 *	InputDevices::InputDevices:
 */
InputDevices::InputDevices() : 
	pDirectInput_( NULL ),
	pKeyboard_( NULL ),
	pMouse_( NULL ),
	keyboardAcquired_( false ),
	mouseAcquired_( false ),
	lostData_( NO_DATA_LOST )
{
	memset( isKeyDown_, 0, sizeof(isKeyDown_) );
	// TODO:PM We could initialise this with the correct state of the keyboard
	// and other buttons.
}


/**
 *	Destructor
 */
InputDevices::~InputDevices()
{
	BW_GUARD;
	// Unacquire and release our DirectInputDevice objects.
	if (pKeyboard_ != NULL)
	{
		pKeyboard_->Unacquire();
		pKeyboard_->Release();
		pKeyboard_ = NULL;
	}

	if (pMouse_ != NULL)
	{
		pMouse_->Unacquire();
		pMouse_->Release();
		pMouse_ = NULL;
	}

	// Release our DirectInput object.
	if (pDirectInput_ != NULL)
	{
		pDirectInput_->Release();
		pDirectInput_ = NULL;
	}
}


/**
 * InputDevices::privateInit:
 */
bool InputDevices::privateInit( void * _hInst, void * _hWnd, int flags )
{
	BW_GUARD;
	HINSTANCE hInst = static_cast< HINSTANCE >( _hInst );
	HWND hWnd = static_cast< HWND >( _hWnd );

	HRESULT hr;

	// hrm, _hInst is being passed in as null from borland, and dinput doesn't
	// like that
#ifdef EDITOR_ENABLED
	hInst = (HINSTANCE) GetModuleHandle( NULL );
#endif

	// Register with the DirectInput subsystem and get a pointer to a
	// IDirectInput interface we can use.
    hr = DirectInput8Create( hInst,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		(LPVOID*)&pDirectInput_,
		NULL );
	if (FAILED(hr)) return false;

	// ****** Keyboard initialisation. ******

	// Obtain an interface to the system keyboard device.
	hr = pDirectInput_->CreateDevice( GUID_SysKeyboard, &pKeyboard_, NULL );
	if (FAILED(hr)) return false;

	// Set the data format to "keyboard format" - a predefined data format
	//
	// A data format specifies which controls on a device we
	// are interested in, and how they should be reported.
	//
	// This tells DirectInput that we will be passing an array
	// of 256 bytes to IDirectInputDevice::GetDeviceState.
	hr = pKeyboard_->SetDataFormat( &c_dfDIKeyboard );
	if (FAILED(hr)) return false;

	// Set the cooperativity level to let DirectInput know how
	// this device should interact with the system and with other
	// DirectInput applications.
	hr = pKeyboard_->SetCooperativeLevel( hWnd, DISCL_FOREGROUND |
		((flags & EXCLUSIVE_MODE) ? DISCL_EXCLUSIVE : DISCL_NONEXCLUSIVE) );
	if (FAILED(hr)) return false;

	// IMPORTANT STEP TO USE BUFFERED DEVICE DATA!
	//
	// DirectInput uses unbuffered I/O (buffer size = 0) by default.
	// If you want to read buffered data, you need to set a nonzero
	// buffer size.
	//
	// Set the buffer size to KEYBOARD_BUFFER_SIZE elements.
	//
	// The buffer size is a DWORD property associated with the device.
	DIPROPDWORD dipdw;

	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = KEYBOARD_BUFFER_SIZE;

	hr = pKeyboard_->SetProperty( DIPROP_BUFFERSIZE, &dipdw.diph );
	if (FAILED(hr)) return false;

	// Remember that it's not yet been acquired
	keyboardAcquired_ = false;

	// ****** Mouse initialisation. ******

	// Obtain an interface to the system mouse device.
	hr = pDirectInput_->CreateDevice( GUID_SysMouse, &pMouse_, NULL );
	if (FAILED(hr)) return false;

	// Set the data format to "mouse format" - a predefined data format
	//
	// A data format specifies which controls on a device we
	// are interested in, and how they should be reported.
	//
	// This tells DirectInput that we will be passing an array
	// of 256 bytes to IDirectInputDevice::GetDeviceState.
	hr = pMouse_->SetDataFormat( &c_dfDIMouse2 );
	if (FAILED(hr)) return false;

	// Set the cooperativity level to let DirectInput know how
	// this device should interact with the system and with other
	// DirectInput applications.
	hr = pMouse_->SetCooperativeLevel( hWnd, DISCL_FOREGROUND |
		((flags & EXCLUSIVE_MODE) ? DISCL_EXCLUSIVE : DISCL_NONEXCLUSIVE) );
	if (FAILED(hr)) return false;

	// IMPORTANT STEP TO USE BUFFERED DEVICE DATA!
	//
	// DirectInput uses unbuffered I/O (buffer size = 0) by default.
	// If you want to read buffered data, you need to set a nonzero
	// buffer size.
	//
	// Set the buffer size to MOUSE_BUFFER_SIZE elements.
	//
	// The buffer size is a DWORD property associated with the device.
	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = MOUSE_BUFFER_SIZE;

	hr = pMouse_->SetProperty( DIPROP_BUFFERSIZE, &dipdw.diph );
	if (FAILED(hr)) return false;

	// Remember that it's not yet been acquired
	mouseAcquired_ = false;

	// ****** Joystick initialisation. ******
	if (this->joystick_.init( pDirectInput_, _hWnd ))
	{
		INFO_MSG( "InputDevices::InputDevices: Joystick initialised\n" );
	}
	else
	{
		INFO_MSG( "InputDevices::InputDevices: Joystick failed to initialise\n" );
	}

	return true;
}


/**
 *	This method processes all device events since it was last called. It asks
 *	the input handler to handle each of these events.
 */
bool InputDevices::privateProcessEvents( InputHandler & handler )
{
	BW_GUARD;
	HRESULT hr;

	if (!focus_) return true;

	// Update the Joystick state when this is called.
	this->joystick_.update();

	bool jbLostData = false;
	this->joystick().processEvents( handler, isKeyDown_, &jbLostData );
	if ( jbLostData )
		lostData_ |= JOY_DATA_LOST;

	static DogWatch watchHandleKey( "Keyboard" );

	{ // DogWatch scope
	ScopedDogWatch watcher( watchHandleKey );

	if (pKeyboard_ != NULL)
	{
		DIDEVICEOBJECTDATA didod[ KEYBOARD_BUFFER_SIZE ];
		DWORD dwElements = 0;

		dwElements = KEYBOARD_BUFFER_SIZE;
		if (keyboardAcquired_)
		{
			hr = pKeyboard_->GetDeviceData(
				sizeof(DIDEVICEOBJECTDATA), didod, &dwElements, 0 );
		}
		else
		{
			hr = DIERR_NOTACQUIRED;
			dwElements = 0;
		}

		switch (hr)
		{
		case DI_OK:
			break;

		case DI_BUFFEROVERFLOW:
			// We got an error or we got DI_BUFFEROVERFLOW.
			//
			// Either way, it means that continuous contact with the device has been
			// lost, either due to an external interruption, or because the buffer
			// overflowed and some events were lost.
			//
			DEBUG_MSG( "InputDevices::privateProcessEvents: "
				"keyboard buffer overflow\n" );
			lostData_ |= KEY_DATA_LOST;
			break;

		case DIERR_INPUTLOST:
		case DIERR_NOTACQUIRED:
			// We got an error or we got DI_BUFFEROVERFLOW.
			//
			// Either way, it means that continuous contact with the device has been
			// lost, either due to an external interruption, or because the buffer
			// overflowed and some events were lost.
			//
			keyboardAcquired_ = false;
			/*
			DEBUG_MSG( "InputDevices::privateProcessEvents: "
				"keyboard input not acquired, acquiring\n" );
			*/
			hr = pKeyboard_->Acquire();
			if (FAILED(hr)) {
				/*
				DEBUG_MSG( "InputDevices::privateProcessEvents: "
					"keyboard acquire failed\n" );
				*/
				return false;
			}
			keyboardAcquired_ = true;
			dwElements = KEYBOARD_BUFFER_SIZE;
			hr = pKeyboard_->GetDeviceData( sizeof(DIDEVICEOBJECTDATA),
											didod, &dwElements, 0 );
			lostData_ |= KEY_DATA_LOST;
			break;

		default:
			DEBUG_MSG( "InputDevices::privateProcessEvents: "
				"unhandled keyboard error\n" );
			return false;
		}

		// Handle all those key events then
		for (DWORD i = 0; i < dwElements; i++)
		{
			isKeyDown_[ static_cast<KeyEvent::Key>(didod[ i ].dwOfs) ] = (didod[ i ].dwData & 0x80) ?
					true : false;

			KeyEvent event(
				(didod[ i ].dwData & 0x80) ?
					MFEvent::KEY_DOWN :
					MFEvent::KEY_UP,
				static_cast<KeyEvent::Key>(didod[ i ].dwOfs),
				this->modifiers() );

			handler.handleKeyEvent( event );
		}
	}

	} // DogWatch scope

	// Now handle the mouse events.
	// TODO:PM We should probably do this differently. We should really handle
	// the event in the order that they were generated. That is, get both
	// buffers and then continually handle the earliest until all handled.

	if (pMouse_)
	{
		DIDEVICEOBJECTDATA didod[ MOUSE_BUFFER_SIZE ];
		DWORD dwElements = 0;

		dwElements = MOUSE_BUFFER_SIZE;
		if (mouseAcquired_)
		{
			hr = pMouse_->GetDeviceData(
				sizeof(DIDEVICEOBJECTDATA), didod, &dwElements, 0 );
		}
		else
		{
			hr = DIERR_NOTACQUIRED;
			dwElements = 0;
		}

		switch (hr)
		{
		case DI_OK:
			break;

		case DI_BUFFEROVERFLOW:
			/*
			DEBUG_MSG( "InputDevices::privateProcessEvents: "
				"mouse buffer overflow\n" );
			*/
			lostData_ |= MOUSE_DATA_LOST;
			break;

		case DIERR_INPUTLOST:
		case DIERR_NOTACQUIRED:
			mouseAcquired_ = false;
			/*
			DEBUG_MSG( "InputDevices::privateProcessEvents: "
				"mouse input not acquired, acquiring\n" );
			*/
			hr = pMouse_->Acquire();
			if (FAILED(hr))
			{
				/*
				DEBUG_MSG( "InputDevices::privateProcessEvents: "
					"mouse acquire failed\n" );
				*/
				return false;
			}
			mouseAcquired_ = true;
			dwElements = MOUSE_BUFFER_SIZE;
			hr = pMouse_->GetDeviceData( sizeof(DIDEVICEOBJECTDATA),
											didod, &dwElements, 0 );
			lostData_ |= MOUSE_DATA_LOST;
			break;

		default:
			DEBUG_MSG( "InputDevices::privateProcessEvents: "
				"unhandled mouse error\n" );
			return false;
		}

		// With the keyboard event, we group the mouse movements together and
		// only send at the end or when a button is pressed.

		// Handle all the mouse events
		long dx = 0;
		long dy = 0;
		long dz = 0;

		static DogWatch watchMouse( "Mouse" );

		watchMouse.start();
		for (DWORD i = 0; i < dwElements; i++)
		{
			switch ( didod[i].dwOfs )
			{
			case DIMOFS_BUTTON0:
			case DIMOFS_BUTTON1:
			case DIMOFS_BUTTON2:
			case DIMOFS_BUTTON3:
			case DIMOFS_BUTTON4:
			case DIMOFS_BUTTON5:
			case DIMOFS_BUTTON6:
			case DIMOFS_BUTTON7:
				{
					if ( dx != 0 || dy != 0 || dz != 0 )
					{
						MouseEvent mouseEvent( dx, dy, dz );
						handler.handleMouseEvent( mouseEvent );
						dx = dy = dz = 0;
					}

					KeyEvent keyEvent(
						(didod[ i ].dwData & 0x80) ?
							MFEvent::KEY_DOWN :
							MFEvent::KEY_UP,
						static_cast< KeyEvent::Key >(
							KeyEvent::KEY_MOUSE0 +
								(didod[ i ].dwOfs - DIMOFS_BUTTON0)),
						this->modifiers() );

					isKeyDown_[ keyEvent.key() ] = keyEvent.isKeyDown();

					handler.handleKeyEvent( keyEvent );
				}
				break;

			case DIMOFS_X:
				dx += didod[ i ].dwData;
				break;

			case DIMOFS_Y:
				dy += didod[ i ].dwData;
				break;

			case DIMOFS_Z:
				dz += didod[ i ].dwData;
				break;
			}
		}
		watchMouse.stop();


		if ( dx != 0 || dy != 0 || dz != 0 )
		{
			MouseEvent mouseEvent( dx, dy, dz );
			handler.handleMouseEvent( mouseEvent );
			dx = dy = dz = 0;
		}
	}

	//handle lost data
	if (lostData_ != NO_DATA_LOST)
		handleLostData( handler, lostData_ );

	for (uint i = 0; i < gVirtualKeyboards.size(); i++)
	{
		KeyboardDevice * pKB = gVirtualKeyboards[i];
		pKB->update();

		KeyEvent event;
		while (pKB->next( event ))
		{
			isKeyDown_[ event.key() ] = event.isKeyDown();
			handler.handleKeyEvent( event );
		}
	}

	return true;
}


/**
 *	This method is called if DirectInput encountered buffer
 *	overflow or lost data, and button events were lost.
 *
 *	We get the current state of all buttons, and compare them to
 *	our presumed state.  If there is any difference, then create
 *	imaginary events.
 *
 *	Note that while these events will be delivered out of order,
 *	vital key up events that were missed will be delivered, saving
 *	the game from untenable positions
 */
void InputDevices::handleLostData( InputHandler & handler, int mask )
{
	BW_GUARD;
	HRESULT hr;


	//process any lost joystick button state
	if ( (mask & JOY_DATA_LOST) && joystick_.pDIJoystick() )
	{
		DIJOYSTATE joyState;
		ZeroMemory( &joyState, sizeof( joyState ) );

		hr = joystick_.pDIJoystick()->GetDeviceState( sizeof( joyState ), &joyState );

		if ( SUCCEEDED( hr ) )
		{
			//success.  iterate through valid joystick codes and check the state
			for ( int k = KeyEvent::KEY_MINIMUM_JOY;
					k != KeyEvent::KEY_MAXIMUM_JOY;
					k++ )
			{
				//success.  iterate through valid key codes and check the state
				for ( int k = KeyEvent::KEY_MINIMUM_JOY;
						k != KeyEvent::KEY_MAXIMUM_JOY;
						k++ )
				{
					KeyEvent event(
						(joyState.rgbButtons[ k - KeyEvent::KEY_MINIMUM_JOY ] & 0x80) ?
							MFEvent::KEY_DOWN :
							MFEvent::KEY_UP,
						static_cast<KeyEvent::Key>(k),
						this->modifiers() );

					//pass event to handler if there is a mismatch between
					//immediate device state and our recorded state
					if ( event.isKeyDown() != isKeyDown_[ event.key() ] )
					{
						isKeyDown_[ event.key() ] = event.isKeyDown();
						handler.handleKeyEvent( event );
					}
				}
			}
			lostData_ &= ~JOY_DATA_LOST;
		}
		else
		{
			DEBUG_MSG( "InputDevices::handleLostData::GetDeviceState[joystick] failed  %lx\n", hr );
		}
	}


	//find lost keyboard states
	if ( (mask & KEY_DATA_LOST) && pKeyboard_ )
	{
		char keyState[256];
		ZeroMemory( &keyState[0], sizeof( keyState ) );

		hr = pKeyboard_->GetDeviceState( sizeof( keyState ), &keyState );

		if ( SUCCEEDED( hr ) )
		{
			//success.  iterate through valid key codes and check the state
			for ( int k = KeyEvent::KEY_MINIMUM_KEY;
					k != KeyEvent::KEY_MAXIMUM_KEY;
					k++ )
			{
				KeyEvent event(
					(keyState[ k ] & 0x80) ?
						MFEvent::KEY_DOWN :
						MFEvent::KEY_UP,
					static_cast<KeyEvent::Key>(k),
					this->modifiers() );

				//pass event to handler if there is a mismatch between
				//immediate device state and our recorded state
				if ( event.isKeyDown() != isKeyDown_[ event.key() ] )
				{
					isKeyDown_[ event.key() ] = event.isKeyDown();
					handler.handleKeyEvent( event );
				}
			}
			lostData_ &= ~KEY_DATA_LOST;
		}
		else
		{
			DEBUG_MSG( "InputDevices::handleLostData::GetDeviceState[keyboard] failed  %lx\n", hr );
		}
	}


	//find lost mouse states
	if ( (mask & MOUSE_DATA_LOST) && pMouse_ )
	{
		// BC: using DIMOUSESTATE2 instead of DIMOUSESTATE, which
		// has only 4 buttons, causing reading overrun error.
		DIMOUSESTATE2 mouseState;
		ZeroMemory( &mouseState, sizeof( mouseState ) );

		hr = pMouse_->GetDeviceState( sizeof( mouseState ), &mouseState );

		if ( SUCCEEDED( hr ) )
		{
			//success.  iterate through valid mouse codes and check the state
			for ( int k = KeyEvent::KEY_MINIMUM_MOUSE;
					k != KeyEvent::KEY_MAXIMUM_MOUSE;
					k++ )
			{
				KeyEvent event(
					(mouseState.rgbButtons[ k - KeyEvent::KEY_MINIMUM_MOUSE ] & 0x80) ?
						MFEvent::KEY_DOWN :
						MFEvent::KEY_UP,
					static_cast<KeyEvent::Key>(k),
					this->modifiers() );

				//pass event to handler if there is a mismatch between
				//immediate device state and our recorded state
				if ( event.isKeyDown() != isKeyDown_[ event.key() ] )
				{
					isKeyDown_[ event.key() ] = event.isKeyDown();
					handler.handleKeyEvent( event );
				}
			}
			lostData_ &= ~MOUSE_DATA_LOST;
		}
		else
		{
			DEBUG_MSG( "InputDevices::handleLostData::GetDeviceState[mouse] failed  %lx\n", hr );
		}
	}
}


// -----------------------------------------------------------------------------
// Section: Joystick
// -----------------------------------------------------------------------------

/**
 *	Structure to hold the Direct Input callback objects.
 */
struct EnumJoysticksCallbackData
{
	IDirectInputDevice8 ** ppDIJoystick;
	IDirectInput8 * pDirectInput;
};


/*
 * Called once for each enumerated joystick. If we find one, create a device
 * interface on it so we can play with it.
 */
BOOL CALLBACK EnumJoysticksCallback( const DIDEVICEINSTANCE * pdidInstance,
								void * pData )
{
	BW_GUARD;
	EnumJoysticksCallbackData * pCallbackData =
		reinterpret_cast<EnumJoysticksCallbackData *>( pData );

	// Obtain an interface to the enumerated joystick.
	HRESULT hr = pCallbackData->pDirectInput->CreateDevice(
			GUID_Joystick,
			pCallbackData->ppDIJoystick,
			NULL );

	// If it failed, then we can't use this joystick. (Maybe the user unplugged
	// it while we were in the middle of enumerating it.)

	if( FAILED(hr) )
		return DIENUM_CONTINUE;


	// Stop enumeration. Note: we're just taking the first joystick we get. You
	// could store all the enumerated joysticks and let the user pick.
	return DIENUM_STOP;
}




/*
 *	Callback function for enumerating the axes on a joystick.
 */
BOOL CALLBACK EnumAxesCallback( const DIDEVICEOBJECTINSTANCE * pdidoi,
							void * pJoystickAsVoid )
{
	BW_GUARD;
	Joystick * pJoystick = reinterpret_cast<Joystick *>( pJoystickAsVoid );

	DIPROPRANGE diprg;
	diprg.diph.dwSize       = sizeof(DIPROPRANGE);
	diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	diprg.diph.dwHow        = DIPH_BYOFFSET;
	diprg.diph.dwObj        = pdidoi->dwOfs; // Specify the enumerated axis
	diprg.lMin              = -DIRECT_INPUT_AXIS_MAX;
	diprg.lMax              = +DIRECT_INPUT_AXIS_MAX;

	// Set the range for the axis
	if( FAILED( pJoystick->pDIJoystick()->
		SetProperty( DIPROP_RANGE, &diprg.diph ) ) )
	{
		return DIENUM_STOP;
	}

	// Set the UI to reflect what axes the joystick supports
	AxisEvent::Axis amap = AxisEvent::NUM_AXES;
	switch( pdidoi->dwOfs )
	{
		// these are PlayStation mappings
		case DIJOFS_X:			amap = AxisEvent::AXIS_LX;		break;
		case DIJOFS_Y:			amap = AxisEvent::AXIS_LY;		break;
		case DIJOFS_Z:			amap = AxisEvent::AXIS_RX;		break;
//		case DIJOFS_RX:			amap = AxisEvent::AXIS_;		break;
//		case DIJOFS_RY:			amap = AxisEvent::AXIS_;		break;
		case DIJOFS_RZ:			amap = AxisEvent::AXIS_RY;		break;
//		case DIJOFS_SLIDER(0):	amap = AxisEvent::AXIS_;		break;
//		case DIJOFS_SLIDER(1):	amap = AxisEvent::AXIS_;		break;
	}

	if (amap != AxisEvent::NUM_AXES)
		pJoystick->getAxis( amap ).enabled( true );

	return DIENUM_CONTINUE;
}



/**
 *	This method initialises the Joystick.
 */
bool Joystick::init( IDirectInput8 * pDirectInput,
					void * hWnd )
{
	BW_GUARD;
	EnumJoysticksCallbackData callbackData =
	{
		&pDIJoystick_,
		pDirectInput
	};

	// Look for a simple joystick we can use for this sample program.
	if ( FAILED( pDirectInput->EnumDevices( DI8DEVCLASS_GAMECTRL,
		EnumJoysticksCallback,
		&callbackData,
		DIEDFL_ATTACHEDONLY ) ) )
	{
		return false;
	}

	// Make sure we got a joystick
	if( NULL == pDIJoystick_ )
	{
		DEBUG_MSG( "Joystick::init: Joystick not found\n" );

		return false;
	}

	// Set the data format to "simple joystick" - a predefined data format
	//
	// A data format specifies which controls on a device we are interested in,
	// and how they should be reported. This tells DInput that we will be
	// passing a DIJOYSTATE structure to IDirectInputDevice::GetDeviceState().

	if ( FAILED( pDIJoystick_->SetDataFormat( &c_dfDIJoystick ) ) )
	{
		return false;
	}

	// Set the cooperative level to let DInput know how this device should
	// interact with the system and with other DInput applications.

	if ( FAILED( pDIJoystick_->SetCooperativeLevel( (HWND)hWnd,
						DISCL_EXCLUSIVE|DISCL_FOREGROUND ) ) )
	{
		return false;
	}

	// IMPORTANT STEP TO USE BUFFERED DEVICE DATA!
	//
	// DirectInput uses unbuffered I/O (buffer size = 0) by default.
	// If you want to read buffered data, you need to set a nonzero
	// buffer size.
	//
	// Set the buffer size to KEYBOARD_BUFFER_SIZE elements.
	//
	// The buffer size is a DWORD property associated with the device.
	DIPROPDWORD dipdw;

	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = JOYSTICK_BUFFER_SIZE;

	if (FAILED( pDIJoystick_->SetProperty( DIPROP_BUFFERSIZE, &dipdw.diph ) ) )
	{
		return false;
	}

	// Determine the capabilities of the device.

	DIDEVCAPS diDevCaps;
	diDevCaps.dwSize = sizeof( DIDEVCAPS );
	if ( SUCCEEDED( pDIJoystick_->GetCapabilities( &diDevCaps ) ) )
	{
		if ( diDevCaps.dwFlags & DIDC_POLLEDDATAFORMAT )
		{
			DEBUG_MSG( "Joystick::init: Polled data format\n" );
		}
		else
		{
			DEBUG_MSG( "Joystick::init: Not Polled data format\n" );
		}

		if ( diDevCaps.dwFlags & DIDC_POLLEDDEVICE )
		{
			DEBUG_MSG( "Joystick::init: Polled device\n" );
		}
		else
		{
			DEBUG_MSG( "Joystick::init: Not Polled device\n" );
		}
	}
	else
	{
		DEBUG_MSG( "Joystick::init: Did not get capabilities\n" );
	}

	// Enumerate the axes of the joyctick and set the range of each axis. Note:
	// we could just use the defaults, but we're just trying to show an example
	// of enumerating device objects (axes, buttons, etc.).

	pDIJoystick_->EnumObjects( EnumAxesCallback, (VOID*)this, DIDFT_AXIS );

	isUsingKeyboard_ = false;

	return true;
}


/*
 *	Simple helper method to convert from the joystick axis coordinates that
 *	DirectInput returns to a float in the range [-1, 1].
 */
static inline float scaleFromDIToUnit( int value )
{
	BW_GUARD;
	// We want to do the following, where the range mappings are linear.
	//
	// [-DIRECT_INPUT_AXIS_MAX,			DIRECT_INPUT_AXIS_DEAD_ZONE	] -> [-1, 0]
	// [-DIRECT_INPUT_AXIS_DEAD_ZONE,	DIRECT_INPUT_AXIS_DEAD_ZONE	] -> [ 0, 0]
	// [DIRECT_INPUT_AXIS_DEAD_ZONE,	DIRECT_INPUT_AXIS_MAX		] -> [ 0, 1]

	bool isNegative = false;

	if (value < 0)
	{
		value = -value;
		isNegative = true;
	}

	value -= DIRECT_INPUT_AXIS_DEAD_ZONE;


	if (value < 0)
	{
		value = 0;
	}

	float floatValue = (float)value/
		(float)(DIRECT_INPUT_AXIS_MAX - DIRECT_INPUT_AXIS_DEAD_ZONE);

	return isNegative ? -floatValue : floatValue;
}



/**
 *	This method updates this object from a keyboard device.
 */
bool Joystick::updateFromKeyboardDevice()
{
	BW_GUARD;
	float xValue =
		InputDevices::isKeyDown( xMaxKey_ ) ?  1.f :
		InputDevices::isKeyDown( xMinKey_ ) ? -1.f : 0.f;

	float yValue =
		InputDevices::isKeyDown( yMaxKey_ ) ?  1.f :
		InputDevices::isKeyDown( yMinKey_ ) ? -1.f : 0.f;


	this->getAxis( AxisEvent::AXIS_LX ).value( xValue );
	this->getAxis( AxisEvent::AXIS_LY ).value( yValue );
	this->getAxis( AxisEvent::AXIS_RX ).value( 0.f );
	this->getAxis( AxisEvent::AXIS_RY ).value( 0.f );

	return true;
}


/**
 *	This method updates this object from a joystick device.
 */
bool Joystick::updateFromJoystickDevice()
{
	BW_GUARD;
	HRESULT     hr;
	DIJOYSTATE  js;           // DInput joystick state

	if ( pDIJoystick_ )
	{
		const int MAX_ATTEMPTS = 10;
		int attempts = 0;

		do
		{
			// Poll the device to read the current state
			hr = pDIJoystick_->Poll();

			if ( SUCCEEDED( hr ) )
			{
				// Get the input's device state
				hr = pDIJoystick_->GetDeviceState( sizeof(DIJOYSTATE), &js );
			}

			if (hr == DIERR_NOTACQUIRED || hr == DIERR_INPUTLOST)
			{
				// DInput is telling us that the input stream has been
				// interrupted. We aren't tracking any state between polls, so
				// we don't have any special reset that needs to be done. We
				// just re-acquire and try again.

				HRESULT localHR = pDIJoystick_->Acquire();

				if (FAILED( localHR ))
				{
					// DEBUG_MSG( "Joystick::updateFromJoystickDevice: Acquire failed\n" );
					return false;
				}
			}
		}
		while ((hr != DI_OK) && (++attempts < MAX_ATTEMPTS));

		if( FAILED(hr) )
			return false;

		// PlayStation Pelican adapter settings
		// We use a math-like not screen-like coordinate system here
		this->getAxis( AxisEvent::AXIS_LX ).value( scaleFromDIToUnit( js.lX ) );
		this->getAxis( AxisEvent::AXIS_LY ).value(-scaleFromDIToUnit( js.lY ) );

		this->getAxis( AxisEvent::AXIS_RX ).value( scaleFromDIToUnit( js.lZ ) );
		this->getAxis( AxisEvent::AXIS_RY ).value(-scaleFromDIToUnit( js.lRz) );

/*
		// Point of view
		if( g_diDevCaps.dwPOVs >= 1 )
		{
			bw_snwprintf( strText, sizeof(strText)/sizeof(wchar_t), "%ld", js.rgdwPOV[0] );
			SetWindowText( GetDlgItem( hDlg, IDC_POV ), strText );
		}

		// Fill up text with which buttons are pressed
		str = strText;
		for( int i = 0; i < 32; i++ )
		{
			if ( js.rgbButtons[i] & 0x80 )
				str += bw_snprintf( str, sizeof(str), "%02d ", i );
		}
		*str = 0;   // Terminate the string

		SetWindowText( GetDlgItem( hDlg, IDC_BUTTONS ), strText );
*/
	}

	return true;
}


/**
 *	Mapping between direct input joystick button number and
 *	our joystick key events.
 */
static const KeyEvent::Key s_joyKeys_PlayStation[32] =
{
	KeyEvent::KEY_JOYTRIANGLE,
	KeyEvent::KEY_JOYCIRCLE,
	KeyEvent::KEY_JOYCROSS,
	KeyEvent::KEY_JOYSQUARE,
	KeyEvent::KEY_JOYL2,
	KeyEvent::KEY_JOYR2,
	KeyEvent::KEY_JOYL1,
	KeyEvent::KEY_JOYR1,
	KeyEvent::KEY_JOYSELECT,
	KeyEvent::KEY_JOYSTART,
	KeyEvent::KEY_JOYARPUSH,
	KeyEvent::KEY_JOYALPUSH,
	KeyEvent::KEY_JOYDUP,
	KeyEvent::KEY_JOYDRIGHT,
	KeyEvent::KEY_JOYDDOWN,
	KeyEvent::KEY_JOYDLEFT,

	KeyEvent::KEY_JOY16,
	KeyEvent::KEY_JOY17,
	KeyEvent::KEY_JOY18,
	KeyEvent::KEY_JOY19,
	KeyEvent::KEY_JOY20,
	KeyEvent::KEY_JOY21,
	KeyEvent::KEY_JOY22,
	KeyEvent::KEY_JOY23,
	KeyEvent::KEY_JOY24,
	KeyEvent::KEY_JOY25,
	KeyEvent::KEY_JOY26,
	KeyEvent::KEY_JOY27,
	KeyEvent::KEY_JOY28,
	KeyEvent::KEY_JOY29,
	KeyEvent::KEY_JOY30,
	KeyEvent::KEY_JOY31
};


/**
 *	This methods processes the pending joystick events.
 */
bool Joystick::processEvents( InputHandler & handler,
							 bool * pIsKeyDown,
							 bool * pLostDataFlag )
{
	BW_GUARD;
	if (!pDIJoystick_)
		return true;


	DIDEVICEOBJECTDATA didod[ JOYSTICK_BUFFER_SIZE ];
	DWORD dwElements = 0;
	HRESULT hr;

	dwElements = JOYSTICK_BUFFER_SIZE;
	hr = pDIJoystick_->GetDeviceData( sizeof(DIDEVICEOBJECTDATA),
										didod, &dwElements, 0 );
	switch (hr) {

	case DI_OK:
		break;

	case DI_BUFFEROVERFLOW:
//		DEBUG_MSG( "Joystick::processEvents: joystick buffer overflow\n" );

		if ( pLostDataFlag )
			*pLostDataFlag = true;

		break;

	case DIERR_INPUTLOST:
	case DIERR_NOTACQUIRED:
//		DEBUG_MSG( "Joystick::processEvents: input not acquired, acquiring\n" );
		hr = pDIJoystick_->Acquire();
		if (FAILED(hr)) {
			DEBUG_MSG( "Joystick::processEvents: acquire failed\n" );
			return false;
		}
		dwElements = JOYSTICK_BUFFER_SIZE;
		hr = pDIJoystick_->GetDeviceData( sizeof(DIDEVICEOBJECTDATA),
										didod, &dwElements, 0 );
		if ( pLostDataFlag )
			*pLostDataFlag = true;

		break;

	default:
		DEBUG_MSG( "Joystick::processEvents: unhandled joystick error\n" );
		return false;
	}


	for (DWORD i = 0; i < dwElements; i++)
	{
		DWORD offset = didod[ i ].dwOfs;

		if (DIJOFS_BUTTON0 <= offset && offset <= DIJOFS_BUTTON31)
		{
			this->generateKeyEvent(
				!!(didod[ i ].dwData & 0x80),
				s_joyKeys_PlayStation[ offset - DIJOFS_BUTTON0 ],
				handler,
				pIsKeyDown );
		}
		else
		{
			// TODO:PM Not handling joystick events currently.
			// It is all state based.
		}
	}

	this->generateCommonEvents( handler, pIsKeyDown );

	return true;
}

//input.cpp
