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
#include "base_camera.hpp"

#ifndef CODE_INLINE
#include "base_camera.ipp"
#endif

#include "moo/render_context.hpp"
#include "input/event_converters.hpp"

DECLARE_DEBUG_COMPONENT2( "Editor", 0 )


/**
 *	Constructor.
 */
BaseCamera::BaseCamera( PyTypePlus * pType ) :
	PyObjectPlus( pType ),
    invert_( false ),
	windowHandle_( 0 )
{
	view_.setIdentity();

	speed_[0] = 4.f;
	speed_[1] = 40.f;
}


/**
 *	Destructor.
 */
BaseCamera::~BaseCamera()
{
}


/**
 *	Render method. Simply sets the view transform to our one.
 */
void BaseCamera::render( float dTime )
{
	Moo::rc().view( this->view() );
}

void BaseCamera::windowHandle( const HWND handle )
{
    windowHandle_ = handle;
}


// -----------------------------------------------------------------------------
// Section: Python stuff
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( BaseCamera )

PY_BEGIN_METHODS( BaseCamera )
	PY_METHOD( update )
	PY_METHOD( render )
	PY_METHOD( handleKeyEvent )
	PY_METHOD( handleMouseEvent )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( BaseCamera )
	PY_ATTRIBUTE( speed )
	PY_ATTRIBUTE( turboSpeed )
	PY_ATTRIBUTE( invert )
	PY_ATTRIBUTE( view )
	PY_ATTRIBUTE( position )
PY_END_ATTRIBUTES()


/**
 *	Get an attribute for python
 */
PyObject * BaseCamera::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}

/**
 *	Set an attribute for python
 */
int BaseCamera::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return PyObjectPlus::pySetAttribute( attr, value );
}


/**
 *	This method does an update from python
 */
PyObject * BaseCamera::py_update( PyObject * args )
{
	float dtime;
	int active = true;	// probably shouldn't have this any more
	if (!PyArg_ParseTuple( args, "f|i", &dtime, &active ))
	{
		PyErr_SetString( PyExc_TypeError, "BaseCamera.update "
			"expects a float dtime and an optional bool active flag" );
		return NULL;
	}

	this->update( dtime, !!active );

	Py_Return;
}

/**
 *	This method does a render from python
 */
PyObject * BaseCamera::py_render( PyObject * args )
{
	float dtime;
	if (!PyArg_ParseTuple( args, "f", &dtime ))
	{
		PyErr_SetString( PyExc_TypeError, "BaseCamera.render "
			"expects a float dtime" );
		return NULL;
	}

	this->render( dtime );

	Py_Return;
}

/**
 *	This method does a handleKeyEvent from python
 */
PyObject * BaseCamera::py_handleKeyEvent( PyObject * args )
{
	KeyEvent	ke;
	if (Script::setData( args, ke, "handleKeyEvent arguments" ) != 0)
		return NULL;

	return Script::getData( this->handleKeyEvent( ke ) );
}

/**
 *	This method does a handleMouseEvent from python
 */
PyObject * BaseCamera::py_handleMouseEvent( PyObject * args )
{
	MouseEvent	me(0,0,0);
	if (Script::setData( args, me, "handleMouseEvent arguments" ) != 0)
		return NULL;

	return Script::getData( this->handleMouseEvent( me ) );
}

// base_camera.cpp
