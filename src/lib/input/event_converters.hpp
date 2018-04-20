/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef EVENT_CONVERTERS_HPP
#define EVENT_CONVERTERS_HPP

typedef struct _object PyObject;
class KeyEvent;
class MouseEvent;


namespace Script
{
	int setData( PyObject * pObject, KeyEvent & rEvent,
		const char * varName = "" );
	PyObject * getData( const KeyEvent & rEvent );

	int setData( PyObject * pObject, MouseEvent & rEvent,
		const char * varName = "" );
	PyObject * getData( const MouseEvent & rEvent );

	int setData( PyObject * pObject, AxisEvent & rEvent,
		const char * varName = "" );
	PyObject * getData( const AxisEvent & rEvent );
};


#endif // EVENT_CONVERTERS_HPP
