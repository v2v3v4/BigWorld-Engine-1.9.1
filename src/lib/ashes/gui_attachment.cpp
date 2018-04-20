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

#include "gui_attachment.hpp"
#include "cstdmf/debug.hpp"
#include "moo/render_context.hpp"

DECLARE_DEBUG_COMPONENT2( "2DComponents", 0 )

#ifndef CODE_INLINE
#include "gui_attachment.ipp"
#endif


// -----------------------------------------------------------------------------
// Section: GuiAttachment
// -----------------------------------------------------------------------------

#undef PY_ATTR_SCOPE
#define PY_ATTR_SCOPE GuiAttachment::

PY_TYPEOBJECT( GuiAttachment )

PY_BEGIN_METHODS( GuiAttachment )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( GuiAttachment )

	/*~ attribute GuiAttachment.component
	 *
	 * @type SimpleGUIComponent	Stores the GUI component that is to be attached to a 
	 *							PyModel HardPoint through this GuiAttachment object.
	 */
	PY_ATTRIBUTE( component )
	/*~ attribute GuiAttachment.faceCamera
	 *
	 * @type Boolean			Turn on to make the GUIAttacment's component use the
	 *							camera direction.  Note this doesn't make the component
	 *							point at the camera position, instead it aligns the
	 *							with the camera plane.
	 */
	 PY_ATTRIBUTE( faceCamera )

PY_END_ATTRIBUTES()

/*~	function GUI.Attachment
 *
 *	Creates and returns a new GUIAttachment, which is 
 *	used to display gui elements in the 3D scene.
 *
 *	@return	The new GUIAttachment object
 */
PY_FACTORY_NAMED( GuiAttachment, "Attachment", GUI )


GuiAttachment::GuiAttachment( PyTypePlus * pType ):
	PyAttachment( pType ),
	gui_( NULL ),
	faceCamera_( false )
{
}


GuiAttachment::~GuiAttachment()
{
}


/**
 *	Section - PyAttachment methods
 */

/**
 *	This method implements the PyAttachment::tick interface.
 */
void GuiAttachment::tick( float dTime )
{
	if ( gui_ )
	{
		gui_->update( dTime, SimpleGUI::instance().screenWidth(), SimpleGUI::instance().screenHeight() );
		gui_->applyShaders( dTime );
	}
}


/**
 *	This method implements the PyAttachment::draw interface.  Since
 *	this gui component draws in the world, this is where we do our
 *	actual drawing.
 *
 *	The worldTransform passed in should already be on the Moo::rc().world()
 *	stack.
 */
void GuiAttachment::draw( const Matrix & worldTransform, float lod )
{
	if ( gui_ )
	{
		Moo::rc().push();
		if (faceCamera_)
		{
			Matrix m( Moo::rc().invView() );
			m.translation( worldTransform.applyToOrigin() );
			Moo::rc().world( m );
		}
		else
		{
			Moo::rc().world( worldTransform );
		}
		gui_->addAsSortedDrawItem();
		Moo::rc().pop();
	}
}


/**
 *	This accumulates our bounding box into the given matrix
 */
void GuiAttachment::boundingBoxAcc( BoundingBox & bb, bool skinny )
{
	if (!gui_)
		return;

	gui_->boundingBoxAcc( bb );	
}


void GuiAttachment::component( SmartPointer<SimpleGUIComponent> component )
{
	gui_ = component;
}


SmartPointer<SimpleGUIComponent> GuiAttachment::component() const
{
	return gui_;
}


/**
 *	Get an attribute for python
 */
PyObject * GuiAttachment::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();
	return PyAttachment::pyGetAttribute( attr );
}

/**
 *	Set an attribute for python
 */
int GuiAttachment::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();
	return PyAttachment::pySetAttribute( attr, value );
}


/**
 *	Factory method
 */
PyObject * GuiAttachment::pyNew( PyObject * args )
{
	return new GuiAttachment;
}