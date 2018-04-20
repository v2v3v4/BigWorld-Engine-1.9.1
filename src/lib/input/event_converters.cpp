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

#include "pyscript/script.hpp"
#include "input.hpp"

#include "event_converters.hpp"

// -----------------------------------------------------------------------------
// Section: Event Converters
// -----------------------------------------------------------------------------

namespace Script
{

	/// PyScript converter for KeyEvents
	int setData( PyObject * pObject, KeyEvent & rEvent,
		const char * varName )
	{
		BW_GUARD;
		long	itype, ikey, imods;
		if (!PyTuple_Check( pObject ) || !PyArg_ParseTuple(
			pObject, "iii", &itype, &ikey, &imods ))
		{
			PyErr_Format( PyExc_TypeError,
				"%s must be set to three integer arguments", varName );
			return -1;
		}

		rEvent = KeyEvent( itype ? MFEvent::KEY_DOWN : MFEvent::KEY_UP,
			KeyEvent::Key( ikey ),
			uint32( imods ) );
		return 0;
	}

	/// PyScript converter for KeyEvents
	PyObject * getData( const KeyEvent & rEvent )
	{
		BW_GUARD;
		return Py_BuildValue( "(iii)",
			rEvent.type() == MFEvent::KEY_DOWN ? 1 : 0,
			rEvent.key(),
			rEvent.modifiers() );
	}

	/// PyScript converter for MouseEvents
	int setData( PyObject * pObject, MouseEvent & rEvent,
		const char * varName )
	{
		BW_GUARD;
		long	dx, dy, dz;
		if (!PyTuple_Check( pObject ) || !PyArg_ParseTuple(
			pObject, "iii", &dx, &dy, &dz ))
		{
			PyErr_Format( PyExc_TypeError,
				"%s must be set to three integer arguments", varName );
			return -1;
		}

		rEvent = MouseEvent( dx, dy, dz );
		return 0;
	}

	/// PyScript converter for MouseEvents
	PyObject * getData( const MouseEvent & rEvent )
	{
		BW_GUARD;
		return Py_BuildValue( "(iii)",
			rEvent.dx(),
			rEvent.dy(),
			rEvent.dz() );
	}

	/// PyScript converter for AxisEvents
	int setData( PyObject * pObject, AxisEvent & rEvent,
		const char * varName )
	{
		BW_GUARD;
		int axis;
		float val, dTime;
		if (!PyTuple_Check( pObject ) || !PyArg_ParseTuple(
			pObject, "iff", &axis, &val, &dTime ))
		{
			PyErr_Format( PyExc_TypeError,
				"%s must be set to an integer and two floats", varName );
			return -1;
		}

		axis = uint(axis) % AxisEvent::NUM_AXES;
		if (val < -1.f) val = -1.f;
		if (val >  1.f) val =  1.f;
		if (dTime < 0.f) dTime = 0.f;

		rEvent = AxisEvent( AxisEvent::Axis( axis ), val, dTime );
		return 0;
	}

	/// PyScript converter for AxisEvents
	PyObject * getData( const AxisEvent & rEvent )
	{
		BW_GUARD;
		return Py_BuildValue( "(iff)",
			rEvent.axis(),
			rEvent.value(),
			rEvent.dTime() );
	}

};	// namespace Script



// -----------------------------------------------------------------------------
// Section: Input BigWorld module functions
// -----------------------------------------------------------------------------


/*~ function BigWorld.isKeyDown
 *
 *	The 'isKeyDown' method allows the script to check if a particular key has
 *	been pressed and is currently still down. The term key is used here to refer
 *	to any control with an up/down status; it can refer to the keys of a
 *	keyboard, the buttons of a mouse or even that of a joystick. The complete
 *	list of keys recognised by the client can be found in the Keys module,
 *	defined in keys.py.
 *
 *	The return value is zero if the key is not being held down, and a non-zero
 *	value is it is being held down.
 *
 *	@param key	An integer value indexing the key of interest.
 *
 *	@return True (1) if the key is down, false (0) otherwise.
 *
 *	Code Example:
 *	@{
 *	if BigWorld.isKeyDown( Keys.KEY_ESCAPE ):
 *	@}
 */
/**
 *	Returns whether or not the given key is down.
 */
static PyObject * py_isKeyDown( PyObject * args )
{
	BW_GUARD;
	int	key;
	if (!PyArg_ParseTuple( args, "i", &key ))
	{
		PyErr_SetString( PyExc_TypeError,
			"BigWorld.isKeyDown: Argument parsing error." );
		return NULL;
	}

	return PyInt_FromLong( InputDevices::isKeyDown( (KeyEvent::Key)key ) );
}
PY_MODULE_FUNCTION( isKeyDown, BigWorld )



/*~ function BigWorld.stringToKey
 *
 *	The 'stringToKey' method converts from the name of a key to its
 *	corresponding key index as used by the 'isKeyDown' method. The string names
 *	for a key can be found in the keys.py file. If the name supplied is not on
 *	the list defined, the value returned is zero, indicating an error. This
 *	method has a inverse method, 'keyToString' which does the exact opposite.
 *
 *	@param string	A string argument containing the name of the key.
 *
 *	@return An integer value for the key with the supplied name.
 *
 *	Code Example:
 *	@{
 *	if BigWorld.isKeyDown( BigWorld.stringToKey( "KEY_ESCAPE" ) ):
 *	@}
 */
/**
 *	Returns the key given its name
 */
static PyObject * py_stringToKey( PyObject * args )
{
	BW_GUARD;
	char * str;
	if (!PyArg_ParseTuple( args, "s", &str ))
	{
		PyErr_SetString( PyExc_TypeError,
			"BigWorld.stringToKey: Argument parsing error." );
		return NULL;
	}

	return PyInt_FromLong( KeyEvent::stringToKey( str ) );
}
PY_MODULE_FUNCTION( stringToKey, BigWorld )


/*~ function BigWorld.keyToString
 *
 *	The 'keyToString' method converts from a key index to its corresponding
 *	string name. The string names returned by the integer index can be found in
 *	the keys.py file. If the index supplied is out of bounds, an empty string
 *	will be returned.
 *
 *	@param key	An integer representing a key index value.
 *
 *	@return A string containing the name of the key supplied.
 *
 *	Code Example:
 *	@{
 *	print BigWorld.keyToString( key ), "pressed."
 *	@}
 */
/**
 *	Returns the name of the given key
 */
static PyObject * py_keyToString( PyObject * args )
{
	BW_GUARD;
	int	key;
	if (!PyArg_ParseTuple( args, "i", &key ))
	{
		PyErr_SetString( PyExc_TypeError,
			"BigWorld.keyToString: Argument parsing error." );
		return NULL;
	}

	return PyString_FromString( KeyEvent::keyToString(
		(KeyEvent::Key) key ) );
}
PY_MODULE_FUNCTION( keyToString, BigWorld )


/**
 *	Returns the value of the given joypad axis
 */
static PyObject * py_axisDirection( PyObject * args )
{
	BW_GUARD;
	int	stick;
	if (!PyArg_ParseTuple( args, "i", &stick ))
	{
		PyErr_SetString( PyExc_TypeError,
			"BigWorld.axisDirection: Argument parsing error." );
		return NULL;
	}

	return PyInt_FromLong( InputDevices::joystick().stickDirection(stick) );
}
/*~ function BigWorld.axisDirection
 *
 *	This function returns the direction the specified joystick is pointing in.
 *
 *	The return value indicates which direction the joystick is facing, as
 *	follows:
 *
 *	@{
 *	- 0 down and left
 *	- 1 down
 *	- 2 down and right
 *	- 3 left
 *	- 4 centred
 *	- 5 right
 *	- 6 up and left
 *	- 7 up
 *	- 8 up and right
 *	@}
 *
 *	@param	axis	This is one of AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, with the
 *					first letter being L or R meaning left thumbstick or right
 *					thumbstick, the second, X or Y being the direction.
 *
 *	@return			An integer representing the direction of the specified
 *					thumbstick, as listed above.
 */
PY_MODULE_FUNCTION( axisDirection, BigWorld )


/*~ function BigWorld rumble
 *  This function provides access to the left and right rumble motors of a
 *  console controller. As this is used to set the rate of spin for each rumble
 *  motor, a standard "jolt" effect would require that it be called a second
 *  time to stop them. This function has no effect when called on the PC
 *  client.
 *
 *  Code Example:
 *  @{
 *  # Note that Functor in this example is a class of object which,
 *  # when called, calls the function at its first constructor argument, with
 *  # arguments equal to its following constructor arguments. It is quite
 *  # possible to implement a class such as this entirely in Python.
 *
 *  # start jolt
 *  BigWorld.rumble( 0.5, 0.5 )
 *
 *  # end jolt
 *  # this calls BigWorld.rumble( 0.0, 0.0 ) in 0.1 seconds' time
 *  BigWorld.callback( 0.1, Functor( BigWorld.rumble, 0.0, 0.0 ) )
 *  @}
 *  @param rightMotor The speed at which the right rumble motor should spin.
 *  This value is clamped between 0.0 and 1.0.
 *  @param leftMotor The speed at which the left rumble motor should spin.
 *  This value is clamped between 0.0 and 1.0.
 *  @return None.
 */
/**
 *	Stub rumble function.
 */
static PyObject * py_rumble( PyObject * )
{
	BW_GUARD;
	Py_RETURN_NONE;
}
PY_MODULE_FUNCTION( rumble, BigWorld )

// event_converters.cpp
