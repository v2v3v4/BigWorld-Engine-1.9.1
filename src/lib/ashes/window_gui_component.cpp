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
#include "window_gui_component.hpp"
#include "cstdmf/debug.hpp"
#include "moo/render_context.hpp"
#include "moo/effect_material.hpp"

DECLARE_DEBUG_COMPONENT2( "2DComponents", 0 )

#ifndef CODE_INLINE
#include "window_gui_component.ipp"
#endif

PY_TYPEOBJECT( WindowGUIComponent )

PY_BEGIN_METHODS( WindowGUIComponent )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( WindowGUIComponent )
	/*~ attribute WindowGUIComponent.scroll
	 *
	 *	The offset that will be applied to the position attribute of all the 
	 *	children of this window.  It is a Vector2 which is specified in clip 
	 *	space coordinates( (-1,-1) in the bottom left of the screen, (1,1) in
	 *	the top right).  It is bounded between the minScroll and maxScroll 
	 *	attributes.
	 *
	 *	@type	Vector2
	 */
	PY_ATTRIBUTE( scroll )
	/*~ attribute WindowGUIComponent.minScroll
	 *
	 *	This attribute is the minimum bound for the scroll attribute, which 
	 *	specifies an offset for the position of all child components of this
	 *	window.  It is a Vector2 specified in clip space coordinates( (-1,-1)
	 *	in the bottom left of the screen, (1,1) in the top right).
	 *	
	 *	@type	Vector2
	 */
	PY_ATTRIBUTE( minScroll )
	/*~ attribute WindowGUIComponent.maxScroll
	 *
	 *	This attribute is the upper bound for the scroll attribute, which
	 *	specifies an offset for the position of all child components of this
	 *	window.  It is a Vector2 specified in clip space coordinates( (-1,-1)
	 *	in the bottom left of the screen, (1,1) in the top right).
	 *	
	 *	@type	Vector2
	 */
	PY_ATTRIBUTE( maxScroll )
PY_END_ATTRIBUTES()

/*~ function GUI.Window
 *
 *	This function creates a new WindowGUIComponent, which is used to apply 
 *	scroll functionality to its children.
 *
 *	@return		a new WindowGUIComponent.
 */
PY_FACTORY_NAMED( WindowGUIComponent, "Window", GUI )

COMPONENT_FACTORY( WindowGUIComponent )

/**
 *	Constructor.
 */
WindowGUIComponent::WindowGUIComponent( const std::string& name,
										PyTypePlus * pType ):
	SimpleGUIComponent( name, pType ),
	scroll_( 0,0 ),
	scrollMin_( 0,0 ),
	scrollMax_( 0,0 )
{
}


/**
 *	Destructor.
 */
WindowGUIComponent::~WindowGUIComponent()
{
}


/**
 *	This method overrides SimplGUIComponent's update method, and
 *	additionally calculates the scroll + offset transformation.
 */
void WindowGUIComponent::update( float dTime, float relativeParentWidth, 
								float relativeParentHeight )
{
	SimpleGUIComponent::update( dTime, relativeParentWidth, relativeParentHeight );

	scroll_.x = min( scroll_.x, scrollMax_.x );
	scroll_.y = min( scroll_.y, scrollMax_.y );
	scroll_.x = max( scroll_.x, scrollMin_.x );
	scroll_.y = max( scroll_.y, scrollMin_.y );

	// TODO: somehow try and avoid calculating this twice (as the base class calls it as well).
	//       perhaps we can just grab the position of the top left vertex in vertices_.
	float x,y,w,h;
	this->layout( relativeParentWidth, relativeParentHeight, x, y, w, h );
	
	//note - we null out any z-transforms, as this can cause confusion (and send
	//child components through the back of the far plane)
	scrollTransform_.setTranslate( Vector3(x,y,0) + Vector3(scroll_.x, scroll_.y, 0) );

	// scroll is relative to the top left, we want children to be relative to
	// the center of this window component (just like the screen).
	anchorTransform_.setTranslate( w/2, -h/2, 0 );

}

void WindowGUIComponent::updateChildren( float dTime, float relParentWidth, float relParentHeight )
{
	float widthPixel = this->widthInPixels( relParentWidth );
	float heightPixel = this->heightInPixels( relParentHeight );

	// We're a relative parent, override width/height to us.
	SimpleGUIComponent::updateChildren( dTime, widthPixel, heightPixel );
}


/**
 *	This method overrides SimpleGUIComponent's draw method, in
 *	order to push clip regions and transforms.
 */
void WindowGUIComponent::draw( bool overlay )
{
	// store current world tranform in run time transform so
	// that hit tests can reflect the correct frame of reference
	Matrix tempRunTimeTrans = Moo::rc().viewProjection();
	tempRunTimeTrans.preMultiply( Moo::rc().world() );
	tempRunTimeTrans.preMultiply( runTimeTransform() );

	if ( visible() )
	{
		Moo::rc().push();
		Moo::rc().preMultiply( runTimeTransform() );
		Moo::rc().device()->SetTransform( D3DTS_WORLD, &Moo::rc().world() );

		bool clipped = false;

		if ( overlay )
			clipped = SimpleGUI::instance().pushClipRegion( *this );
		
		SimpleGUIComponent::drawSelf(overlay);

		//TODO : implement 3D clipping
		if ( !overlay || clipped )
		{
			//push our childrens' transform ( due to scrolling, and to anchor offsets )
			Moo::rc().push();
			Moo::rc().preMultiply( scrollTransform_ );
			Moo::rc().preMultiply( anchorTransform_ );
			Moo::rc().device()->SetTransform( D3DTS_WORLD, &Moo::rc().world() );

			SimpleGUIComponent::drawChildren(overlay);			

			Moo::rc().pop();
			Moo::rc().device()->SetTransform( D3DTS_WORLD, &Moo::rc().world() );
		}

		if ( clipped )
			SimpleGUI::instance().popClipRegion();

		//pop the overall transform
		Moo::rc().pop();
		Moo::rc().device()->SetTransform( D3DTS_WORLD, &Moo::rc().world() );
	}

	runTimeClipRegion_ = SimpleGUI::instance().clipRegion();
	runTimeTransform( tempRunTimeTrans );
	momentarilyInvisible( false );
}


/**
 *	This method adds Window specific attributes to the standard
 *	load method.
 */
bool WindowGUIComponent::load( DataSectionPtr pSect, LoadBindings & bindings )
{
	if (!this->SimpleGUIComponent::load( pSect, bindings )) return false;

	this->scroll_ = pSect->readVector2( "scroll", this->scroll_ );
	this->scrollMin_ = pSect->readVector2( "minScroll", this->scrollMin_ );
	this->scrollMax_ = pSect->readVector2( "maxScroll", this->scrollMax_ );

	return true;
}


/**
 *	This method adds Window specific attributes to the standard
 *	save method.
 */
void WindowGUIComponent::save( DataSectionPtr pSect, SaveBindings & bindings )
{
	this->SimpleGUIComponent::save( pSect, bindings );

	pSect->writeVector2( "scroll", this->scroll_ );
	pSect->writeVector2( "minScroll", this->scrollMin_ );
	pSect->writeVector2( "maxScroll", this->scrollMax_ );
}


/// Get an attribute for python
PyObject * WindowGUIComponent::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return SimpleGUIComponent::pyGetAttribute( attr );
}


/// Set an attribute for python
int WindowGUIComponent::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return SimpleGUIComponent::pySetAttribute( attr, value );
}


/**
 *	Static python factory method
 */
PyObject * WindowGUIComponent::pyNew( PyObject * args )
{
	char * textureName = "";
	if (!PyArg_ParseTuple( args, "|s", &textureName ))
	{
		PyErr_SetString( PyExc_TypeError, "GUI.Simple: "
			"Argument parsing error: Expected an optional texture name" );
		return NULL;
	}

	return new WindowGUIComponent( textureName );
}

// window_gui_component.cpp
