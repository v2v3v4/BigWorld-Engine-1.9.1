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

#pragma warning(disable:4786)	// turn off truncated identifier warnings

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "2DComponents", 0 )

#include "simple_gui.hpp"
#include "simple_gui_component.hpp"

#include "moo/render_context.hpp"
#include "moo/texture_manager.hpp"

#include "input/event_converters.hpp"
#include <map>
#include <limits>


#ifndef CODE_INLINE
#include "simple_gui.ipp"
#endif


/// SimpleGUI Singleton
BW_SINGLETON_STORAGE( SimpleGUI )


namespace { // anonymous

/*
typedef std::map< int, HCURSOR > CursorMap;
CursorMap s_cursorMap;
*/

// Named constants
const float c_defaultDrgaDistance = 0.002f;

// helper classes

/**
 *	Implements the EventForwarder concept. Used by the processMouseMove
 *	method to forward enter, leave and move events to the default enter, 
 *	leave and move event handlers of the target component.
 */
struct MouseMoveFuncs
{
	bool enterEvent( SimpleGUIComponent * comp, 
		const SimpleGUIMouseEvent & event )
	{
		return comp->handleMouseEnterEvent( event );
	}

	bool moveEvent( SimpleGUIComponent * comp, 
		const SimpleGUIMouseEvent & event )
	{
		return comp->handleMouseEvent( event );
	}

	bool leaveEvent( SimpleGUIComponent * comp, 
		const SimpleGUIMouseEvent & event )
	{
		return comp->handleMouseLeaveEvent( event );
	}
};

/**
 *	Implements the EventForwarder concept. Used by the processDragMove
 *	method to forward enter, leave and move events to the drag'n drop 
 *	enter, leave and move event handlers of the target component.
 */
struct DragMoveFuncs
{
	DragMoveFuncs( SimpleGUIComponent * draggedComp, bool & accepts ) :
		dragged( draggedComp ),
		acceptsDrop( accepts ) 
	{}

	bool enterEvent( SimpleGUIComponent * comp, 
		const SimpleGUIMouseEvent & event )
	{
		this->acceptsDrop = comp->handleDragEnterEvent( this->dragged, event );
		return true;
	}

	bool moveEvent( SimpleGUIComponent *, const SimpleGUIMouseEvent & )
	{
		return true;
	}

	bool leaveEvent( SimpleGUIComponent * comp,
		const SimpleGUIMouseEvent & event )
	{
		return comp->handleDragLeaveEvent( this->dragged, event );
	}

	SimpleGUIComponent * dragged;
	bool & acceptsDrop;
};




// helper functions
int depth_compare_gui_components( const void *arg1, const void *arg2 )
{
	SimpleGUIComponent** e1 = (SimpleGUIComponent**)arg1;
	SimpleGUIComponent** e2 = (SimpleGUIComponent**)arg2;

	//we don't need to return equals to ( ret = 0 ), because we don't care
	if ( (*e1)->position().z > (*e2)->position().z )
		return -1;
	return 1;
}

/**
 *	This method add a simple gui component onto the given 
 *	focus list, meaning the component now receives input events.
 *
 *	@param c			the component that will now receive the input focus.
 *	@param focusList	the focus list rom which to remove the component
 */
void addToFocusList( SimpleGUI::FocusList &focusList, SimpleGUIComponent* c )
{
	focusList.push_back(c);
}


/**
 *	This method removes a simple gui component from the given focus list.
 *
 *	@param c			the component that no longer wants focus.
 *	@param focusList	the focus list rom which to remove the component
 */
void delFromFocusList( SimpleGUI::FocusList &focusList, SimpleGUIComponent* c )
{
	SimpleGUI::FocusList::iterator it = std::find( focusList.begin(), focusList.end(), c );
	if ( it != focusList.end() )
	{
		focusList.erase( it );
	}
}


/**
 *	This function fills "result" with components from "input" that are in the
 *	GUI hierarchy, as roots or as children.
 *
 *	@param roots	list to components to look for the input components
 *	@param input	list to filter.
 *	@param result	filtered components are returned in this param
 */
void filterList( const SimpleGUI::Components& roots, const SimpleGUI::FocusList& input, SimpleGUI::FocusList& result )
{
	// TODO: change the way SimpleGUI works so it doesn't depend on focus lists
	// to send events, so we can get rid of this method.

	// First, gather all components that are root or children of root, the ones
	// that actually should be handling events, that are in the GUI hierarchy.
	std::set< SimpleGUIComponent * > rootComps;

	for ( SimpleGUI::Components::const_iterator c = roots.begin();
		c != roots.end(); ++c )
	{
		rootComps.insert( *c );
		(*c)->children( rootComps );
	}

	// Now filter the input list into the result list, adding only elements
	// from the input list that actually belong to the GUI hierarchy.
	result.reserve( input.size() );
	for ( SimpleGUI::FocusList::const_iterator i = input.begin();
		i != input.end(); ++i )
	{
		if (rootComps.find( (*i) ) != rootComps.end())
		{
			result.push_back( *i );
		}
	}
}


class EnterLeaveHandler
{
public:
	/**
	 *	This static function used in std::sort to sort components according to
	 *	their draw order.
	 *
	 *	@param a	First component
	 *	@param b	Second component
	 *	@return		true if the first component is greater than the second.
	 */
	static bool SimpleGUICloserThan ( SimpleGUIComponent* a, SimpleGUIComponent* b )
	{
		return a->drawOrder() > b->drawOrder();
	}

	/**
	 *	Template function that processes mouse events over two lists of components 
	 *	to detect enter, leave (cross) and move events. The first list is used to 
	 *	generate the cross events. The second one is used for move events. This 
	 *	functions It is used to generate events during normal and dragged mouse 
	 *	hovering. It relies on a EventForwarder concept to deliver the detected 
	 *	events.
	 *
	 *	@param	event				the mouse move event to be processed.
	 *	@param	mouseOverComponent	optional pointer to return the component which
	 *                              the mouse is currently over.
	 *	@param	roots				list of root components. The crossFocusList
	 *								param gets filtered to a list that includes
	 *								only components that are root or children of
	 *								a root.
	 *	@param	crossFocusList		list of components to check for cross events.
	 *	@param	sendMoveEvents		if true, move events are sent to the roots
	 *								events.
	 *	@param	eventForwarder		the event forwarder object.
	 *
	 *	@return	true if an event has been generated and processed.
	 */
	template< class EventForwarderT >
	static bool detectEvents( 
		const SimpleGUIMouseEvent & event,
		SimpleGUIComponent ** mouseOverComponent,
		const SimpleGUI::Components & roots,
		const SimpleGUI::FocusList & crossFocusList, 
		bool sendMoveEvents,
		EventForwarderT eventForwarder )
	{
		bool handled = false;
		// Note: the static lastMousePos relies on the fact that this method
		// is a template, thus a lastMousePos variable is used for each
		// templated version.
		static Vector2 lastMousePos = outOfBoundsPos_;

		// first filter out cross focus components that are not in any root
		// hierarchy, and sort it from top-most to bottom-most.
		SimpleGUI::FocusList filteredList;
		filterList( roots, crossFocusList, filteredList );
		std::sort<SimpleGUI::FocusList::iterator>(
			filteredList.begin(), filteredList.end(), SimpleGUICloserThan );

		if ( reset_ )
		{
			// a reset was requested, so reset the last mouse position for a
			// fresh start.
			lastMousePos = outOfBoundsPos_;
			reset_ = false;
		}

		// find the item currently below the cursor
		SimpleGUIComponent* oldOverComponent = NULL;
		SimpleGUIComponent* curOverComponent = NULL;
		for ( SimpleGUI::FocusList::iterator i = filteredList.begin();
			i != filteredList.end() && oldOverComponent == NULL; ++i )
		{
			if ( (*i)->hitTest( lastMousePos ) )
				oldOverComponent = (*i);
		}

		for ( SimpleGUI::FocusList::iterator i = filteredList.begin();
			i != filteredList.end() && curOverComponent == NULL; ++i )
		{
			if ( (*i)->hitTest( event.mousePos() ) )
				curOverComponent = (*i);
		}

		// generate leave event
		if ( oldOverComponent != NULL &&
			oldOverComponent != curOverComponent )
		{
			eventForwarder.leaveEvent( oldOverComponent, event );
		}

		// generate enter event
		if ( curOverComponent != NULL &&
			oldOverComponent != curOverComponent )
		{
			eventForwarder.enterEvent( curOverComponent, event );
		}

		// update the mouseOverComponent
		if ( mouseOverComponent != NULL )
			*mouseOverComponent = curOverComponent;

		lastMousePos = event.mousePos();

		if ( sendMoveEvents )
		{
			// send mouse move to the first component that handles it.
			for ( SimpleGUI::Components::const_reverse_iterator i = roots.rbegin();
				i != roots.rend(); ++i )
			{
				if ( (*i)->hitTest( event.mousePos() ) )
				{
					handled = eventForwarder.moveEvent( (*i), event );
					if (handled)
						break;
				}
			}
		}

		return handled;
	}


	/**
	 *	Helper method to reset the last mouse position. It sets a flag so when
	 *	detectEvents() is called, it will actually reset the lastMousePos
	 *	static for the templated instance of the function. Useful for reseting
	 *	before starting a drag operation for instance.
	 */
	static void reset()
	{
		reset_ = true;
	}


	/**
	 *	Method that should be called whenever the mouse active state changes.
	 *	If the mouse is active, it'll send mouse enter events for all
	 *  components that return true for a hit test against the current mouse
	 *	cursor position.
	 *	If the mouse is inactive, it'll send mouse leave events for all
	 *	components that were previously returning true in their hit test
	 *  against the cursor. This is accomplished by sending an out-of-bounds
	 *	mouse position.
	 *
	 *	@param	roots				list of root components. The crossFocusList
	 *								param gets filtered to a list that includes
	 *								only components that are root or children of
	 *								a root.
	 *	@param	crossFocusList		list of components to check for cross events.
	 *	@param	eventForwarder		the event forwarder object.
	 */
	template< class EventForwarderT >
	static void cursorChanged(
		const SimpleGUI::Components & roots,
		const SimpleGUI::FocusList & crossFocusList, 
		EventForwarderT eventForwarder)
	{
		Vector2 pos;
		if (SimpleGUI::instance().mouseCursor().isActive())
		{
			pos = SimpleGUI::instance().mouseCursor().position();
		}
		else
		{
			pos = EnterLeaveHandler::outOfBoundsPos_;
		}

		SimpleGUIMouseEvent mouseEvent( MouseEvent(), pos );
		EnterLeaveHandler::detectEvents( mouseEvent, 
			NULL,
			roots,
			crossFocusList, 
			false,
			MouseMoveFuncs() );
	}

private:
	static bool reset_;
	static Vector2 outOfBoundsPos_;
};

bool EnterLeaveHandler::reset_ = true;
Vector2 EnterLeaveHandler::outOfBoundsPos_( std::numeric_limits<float>::max(),
									   std::numeric_limits<float>::max() );


} // anonymous namespace


/**
 *	Holds drag'n drop information.
 */
struct DragInfo
{
	SimpleGUIComponent *	component;
	SimpleGUIComponent *	target;
	SimpleGUIKeyEvent		keyEvent;
	bool					dragging;
	bool                    targetAccepts;
};


#include "moo/effect_constant_value.hpp"
/**
 * TODO: to be documented.
 */
class AshesProjSetter : public Moo::EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		Matrix wvp( Moo::rc().world() );
		wvp.postMultiply( Moo::rc().viewProjection() );
		pEffect->SetMatrix( constantHandle, &wvp );
		return true;
	}
};

static Moo::EffectConstantValuePtr *s_pProjSetter;

/**
 * TODO: to be documented.
 */
class AshesResolutionSetter : public Moo::EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		Vector4 res;
		res.x = SimpleGUI::instance().screenWidth();
		res.y = SimpleGUI::instance().screenHeight();
		res.z = SimpleGUI::instance().halfScreenWidth();
		res.w = SimpleGUI::instance().halfScreenHeight();
		pEffect->SetVector( constantHandle, &res );
		return true;
	}
};

static Moo::EffectConstantValuePtr *s_pResolutionSetter;

/**
 * TODO: to be documented.
 */
class AshesPixelSnapSetter : public Moo::EffectConstantValue
{
public:
	AshesPixelSnapSetter() : value_(false)
	{
	}

	bool value_;

	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetBool( constantHandle, this->value_ );
		return true;
	}
};

static Moo::EffectConstantValuePtr *s_pPixelSnapSetter;


#include "math/colour.hpp"
/**
 * TODO: to be documented.
 */
class TFactorSetter : public Moo::EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetVector( constantHandle, &v4Colour_ );
		return true;
	}

	void colour( DWORD colour )
	{
		v4Colour_ = Colour::getVector4Normalised(colour);
	}
private:
	Vector4	v4Colour_;
};

static Moo::EffectConstantValuePtr *s_pTFactorSetter;

/**
 *	Constructor.
 */
SimpleGUI::SimpleGUI() :
	pMouseCursor_( NULL ),
	resolutionHasChanged_( true ),
	lastResolution_( 0.f, 0.f ),
	lastRealResolution_( 0.f, 0.f ),
	realResolutionCounter_( 0 ),
	focusList_(),
	crossFocusList_(),
	dragFocusList_(),
	dropFocusList_(),
	clickComponent_( NULL ),
	dragInfo_( NULL ),
	dragDistanceSqr_( c_defaultDrgaDistance * c_defaultDrgaDistance ),
	resolutionOverride_( 0.f, 0.f ),
	pixelToClipX_( 0.f ),
	pixelToClipY_( 0.f ),
	hwnd_( NULL ),
	hInstance_( NULL ),
	inited_( false ),
	updateGUI_( true ),
	drawCallCount_(0)
{	
	clipFixer_.setIdentity();
	clipStack_.push( Vector4( -1.f, 1.f, 1.f, -1.f ) );

	MF_WATCH( "GUI/numDrawCalls", drawCallCount_, Watcher::WT_READ_ONLY,
			"Number of draw calls in the last frame." );
}


/**
 *	Destructor.
 */
SimpleGUI::~SimpleGUI()
{
	if (Moo::rc().device() != NULL)
	{
		Py_XDECREF( pMouseCursor_ );
		pMouseCursor_ = NULL;
	}

	Components::iterator it = components_.begin();
	Components::iterator end = components_.end();

	while (it != end)
	{
		Py_DECREF( *it );
		it++;
	}
	PyGC_Collect();

	components_.clear();
}


/**
 *	This static method creates the instance, and also initialises related
 *	static objects.
 *
 *	@param pConfig	Configuration data section for SimpleGUIComponent.
 */
/*static*/ void SimpleGUI::init( DataSectionPtr pConfig )
{
	// This local variable is not used because the base Singleton class will
	// store a pointer internally, so we don't need to store it.
	SimpleGUI* pNewInstance = new SimpleGUI();

	SimpleGUIComponent::init( pConfig );

	s_pProjSetter =	new Moo::EffectConstantValuePtr( new AshesProjSetter() );
	s_pResolutionSetter = new Moo::EffectConstantValuePtr( new AshesResolutionSetter() );
	s_pTFactorSetter = new Moo::EffectConstantValuePtr( new TFactorSetter() );
	s_pPixelSnapSetter = new Moo::EffectConstantValuePtr( new AshesPixelSnapSetter() );

	instance().inited_ = true;
}


/**
 *	This method sets the HWND for the main application window.
 *	SimpleGUI needs this only if the mouse cursor is to be used.
 *
 *	@param h	The HWND for the application
 */
void SimpleGUI::hwnd( void * h )
{
	hwnd_ = h;
}


/**
 *	Returns the HWND for the main application window.
 */
void * SimpleGUI::hwnd() const
{
	return hwnd_;
}


/**
 * Sets the application instance handle.
 *
 *	@param h	The application instance for the application
 */ 
void SimpleGUI::hInstance( void * h )
{
	hInstance_ = h;
}


/**
 *	This method adds a simple component to the GUI.
 *
 *	@param c	The component to add.
 */
void
SimpleGUI::addSimpleComponent( SimpleGUIComponent& c )
{
	Components::iterator it = std::find( components_.begin(), components_.end(), &c );

	if ( it == components_.end() )
	{
		Py_INCREF( &c );
		components_.push_back( &c );
		reSort();
	}
	else
	{
		WARNING_MSG( "SimpleGUI::addSimpleComponent - \
			attempted to add a component to the root twice\n" );
	}
}


/**
 *	This method returns a simple component from the GUI.
 *
 *	@param c	The component to remove.
 */
void
SimpleGUI::removeSimpleComponent( SimpleGUIComponent& c )
{
	Components::iterator it = std::find( components_.begin(), components_.end(), &c );

	if ( it != components_.end() )
	{
		Py_DECREF( *it );
		components_.erase( it );
		reSort();
	}
}


/**
 *	This method resorts the components in the GUI.
 */
void
SimpleGUI::reSort( void )
{
	if ( !components_.empty() )
	{
		qsort( &components_.front(), components_.size(), sizeof( SimpleGUIComponent* ), depth_compare_gui_components );
	}

	recalcDrawOrders();
}


/**
 *	Ticks the SimpleGUI system.
 *
 *	@param	dTime	Time elapsed since last update.
 */
void SimpleGUI::update( float dTime )
{
	dTime_ = dTime;
	mouseCursor().tick( dTime );
}


/**
 *	This static method cleans up the SimpleGUI singleton instance and related
 *	static objects. It should be called before Script::fini.
 */
/*static*/ void SimpleGUI::fini()
{
	if (!instance().inited_)
	{
		return;
	}

	instance().inited_ = false;

	delete pInstance();

	SimpleGUIComponent::fini();

	delete s_pTFactorSetter;
	s_pTFactorSetter = NULL;

	delete s_pProjSetter;
	s_pProjSetter = NULL;

	delete s_pResolutionSetter;
	s_pResolutionSetter = NULL;

	delete s_pPixelSnapSetter;
	s_pPixelSnapSetter = NULL;
}


/**
 *	This method returns the mouse cursor object, or creates it if it's NULL.
 *
 *  @return		The mouse cursor object.
 */
MouseCursor & SimpleGUI::internalMouseCursor() const
{
	MF_ASSERT_DEV( inited_ );

	if (!pMouseCursor_)
	{
		pMouseCursor_ = new MouseCursor();
	}
	return *pMouseCursor_;
} 

 
/**
 *	This method returns the const mouse cursor object.
 *
 *  @return		The mouse cursor object.
 */
const MouseCursor & SimpleGUI::mouseCursor() const
{
	return internalMouseCursor();
}


/**
 *	This method returns the non-const mouse cursor object.
 *
 *  @return		The mouse cursor object.
 */
MouseCursor & SimpleGUI::mouseCursor()
{
	return internalMouseCursor();
}


void SimpleGUI::setConstants( DWORD colour, bool pixelSnap )
{
	static Moo::EffectConstantValuePtr* pProjConstantValue_ = NULL;
	static Moo::EffectConstantValuePtr* pTFactorConstantValue_ = NULL;
	static Moo::EffectConstantValuePtr* pResolutionConstantValue_ = NULL;
	static Moo::EffectConstantValuePtr* pPixelSnapSetter_ = NULL;	

	if ( !pProjConstantValue_ )
	{
		pProjConstantValue_ = Moo::EffectConstantValue::get( "WorldViewProjection" );
		pTFactorConstantValue_ = Moo::EffectConstantValue::get( "GUIColour" );
		pResolutionConstantValue_ = Moo::EffectConstantValue::get( "GUIResolution" );
		pPixelSnapSetter_ = Moo::EffectConstantValue::get( "GUIPixelSnap" );		
	}

	((TFactorSetter*)(*s_pTFactorSetter).getObject())->colour( colour );
	((AshesPixelSnapSetter *)(*s_pPixelSnapSetter).getObject())->value_ = pixelSnap;
	*pProjConstantValue_ = *s_pProjSetter;
	*pTFactorConstantValue_ = *s_pTFactorSetter;
	*pResolutionConstantValue_ = *s_pResolutionSetter;
	*pPixelSnapSetter_ = *s_pPixelSnapSetter;
	Moo::rc().setFVF( GUIVertex::fvf() );
}


/**
 *	Determines if a given SimpleGUIComponent is in the current SimpleGUI tree
 */
bool SimpleGUI::isSimpleGUIComponentInTree( const SimpleGUIComponent * pComponent ) const
{
	Components::const_iterator it;
	Components::const_iterator end = components_.end();

	for ( it = components_.begin(); it != end; ++it )
	{
		if( (*it) == pComponent || (*it)->isParentOf( pComponent ) )
		{
			return true;
		}
	}
	return false;
}


/**
 *	Draws all GUI components.
 */
void SimpleGUI::draw( void )
{
	if (!updateGUI_)
	{
		return;
	}

	static DogWatch	dwUpdateGUI("Update");
	dwUpdateGUI.start();

	//Update
	Moo::rc().getViewport( &originalView_ );

	Vector2 newResolution;
	if ( usingResolutionOverride() )
	{
		newResolution = resolutionOverride_;
	}
	else
	{
		newResolution.set( Moo::rc().screenWidth(), Moo::rc().screenHeight() );
	}

	if ( newResolution.x != lastResolution_.x ||
		newResolution.y != lastResolution_.y )
	{
		lastResolution_ = newResolution;

		//calculate pixelToClip scaling factors
		pixelToClipX_ = 2.f / newResolution.x;
		pixelToClipY_ = 2.f / newResolution.y;

		resolutionHasChanged_ = true;
	}
	else
	{
		resolutionHasChanged_ = false;
	}

	Vector2 realResolution( Moo::rc().screenWidth(), Moo::rc().screenHeight() );
	if ( realResolution.x != lastRealResolution_.x ||
		realResolution.y != lastRealResolution_.y )
	{
		realResolutionCounter_++;
		lastRealResolution_ = realResolution;
	}

	Components::iterator it = components_.begin();
	Components::iterator end = components_.end();

	while (it != end)
	{
		SimpleGUIComponent* c = (*it);

		c->update( dTime_, newResolution.x, newResolution.y );
		c->applyShaders( dTime_ );

		it++;
	}

	dwUpdateGUI.stop();

	static DogWatch	dwDrawGUI("Draw");
	dwDrawGUI.start();

	drawCallCount_ = 0;

	//Draw
	Moo::rc().setRenderState( D3DRS_LIGHTING, FALSE );

	Matrix origView = Moo::rc().view();
	Matrix origProj = Moo::rc().projection();
	Moo::rc().view( Matrix::identity );
	Moo::rc().projection( Matrix::identity );
	Moo::rc().updateViewTransforms();

	Moo::rc().device()->SetTransform( D3DTS_WORLD, &Matrix::identity );
	Moo::rc().device()->SetTransform( D3DTS_VIEW, &Matrix::identity );
	Moo::rc().device()->SetTransform( D3DTS_PROJECTION, &Matrix::identity );

	Moo::rc().setPixelShader( NULL );
	Moo::rc().setVertexShader( NULL );
	Moo::rc().setFVF( GUIVertex::fvf() );

	Moo::rc().push();
	Moo::rc().world(Matrix::identity);

	it = components_.begin();
	end = components_.end();

	while ( it != end )
	{
		SimpleGUIComponent* c = (*it);

		c->draw( true );

		it++;
	}

	Moo::rc().pop();

	Moo::rc().setRenderState( D3DRS_LIGHTING, TRUE );
	Moo::rc().setRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	Moo::rc().setViewport( &originalView_ );
	MF_ASSERT_DEV( clipStack_.size() == 1 );

	Moo::rc().view( origView );
	Moo::rc().projection( origProj );
	Moo::rc().updateViewTransforms();

	dwDrawGUI.stop();

	checkCursorChanged();
}

void SimpleGUI::setUpdateEnabled( bool enable )
{
	updateGUI_ = enable;
}

/**
 *	Converts coordinates from screen space (pixels) to clip scape.
 *
 *  @param w		horizontal position in screen space (input)
 *  @param h		vertical position in screen space (input)
 *  @param retW	horizontal position in clip space (output, optional)
 *  @param retH	vertical position in clip space (output, optional)
 */
void
SimpleGUI::pixelRangesToClip( float w, float h, float* retW, float* retH )
{
	if ( retW )
		*retW = w * pixelToClipX_;

	if ( retH)
		*retH = h * pixelToClipY_;
}


/**
 *	Converts coordinates from clip space to screen scape (pixels).
 *
 *  @param w		horizontal position in clip space (input)
 *  @param h		vertical position in clip space (input)
 *  @param retW	horizontal position (output, optional)
 *  @param retH	vertical position (output, optional)
 */
void
SimpleGUI::clipRangesToPixel( float w, float h, float* retW, float* retH )
{
	MF_ASSERT_DEV( pixelToClipX_ != 0.f );
	MF_ASSERT_DEV( pixelToClipY_ != 0.f );

	if ( retW )
		*retW = w / pixelToClipX_;
	if ( retH )
		*retH = h / pixelToClipY_;
}


/**
 *	This method finds out if a key event refers to mouse buttons.
 *
 *	@param event	The key event to handle.
 *
 *	@return True if the key event corresponds to a mouse button.
 */
bool SimpleGUI::isMouseKeyEvent( const KeyEvent& event ) const
{
	return
		event.key() == KeyEvent::KEY_LEFTMOUSE ||
		event.key() == KeyEvent::KEY_RIGHTMOUSE ||
		event.key() == KeyEvent::KEY_MIDDLEMOUSE;
}


/**
 *	This method handles key events for the gui system.
 *
 *	Key events are passed on to the current component that
 *	has the focus, if any.
 *
 *	@param event	The key event to handle.
 *
 *	@return True if the key event was handled.
 */
bool SimpleGUI::handleKeyEvent( const KeyEvent & event )
{
	bool handled = false;
	Vector2 mousePos = mouseCursor().position();
	SimpleGUIKeyEvent keyEvent( event, mousePos );

	if (mouseCursor().isActive() && isMouseKeyEvent( event ))
	{
		SimpleGUIComponent * pComponent =
									closestHitTest( focusList_, mousePos );
		if (pComponent != NULL)
		{
			handled = pComponent->handleKeyEvent( keyEvent );
		}

		handled |= this->processClickKey( keyEvent );
		handled |= this->processDragKey( keyEvent );
	}

	if (!handled)
	{
		for (int n = components_.size() - 1; n >= 0; --n)
		{
			SimpleGUIComponent * pComponent = components_[ n ];
			handled = pComponent->handleKeyEvent( keyEvent );
			if (handled)
			{
				break;
			}
		}
	}

    return handled;
}


/**
 *	This method handles mouse events for the gui system.
 *
 *	Mouse events are passed on to the whatever component
 *	is at the mouse location.
 *
 *	@param event	The mouse event to handle.
 *
 *	@return True if the mouse event was handled.
 */
bool SimpleGUI::handleMouseEvent( const MouseEvent & event )
{
	bool handled = false;

	if (mouseCursor().isActive())
	{
		Vector2 mousePos = mouseCursor().position();
		SimpleGUIMouseEvent mouseEvent( event, mousePos );
		handled = this->processDragMove( mouseEvent );

		if (!handled) 
		{
			handled = this->processMouseMove( mouseEvent ); 
		}
	}

	return handled;
}


/**
 *	This method handles axis events for the gui system.
 *
 *	Axis events are passed on to the current component that
 *	has the focus, if any.
 *
 *	@param event	The axis event to handle.
 *
 *	@return True if the axis event was handled.
 */
bool SimpleGUI::handleAxisEvent( const AxisEvent & event )
{
	bool handled = false;

	int n = components_.size()-1;
	for ( ;n>=0;n-- )
	{
		SimpleGUIComponent* c = components_[n];
        handled = c->handleAxisEvent( event );
        if (handled)
            break;
    }

    return handled;
}


/**
 *	Helper method to get the closest element that hitTests true for a position
 *	in a focus list.
 *
 *	@param list	focus list with the components to hit-test
 *	@param pos	position for the hit test, usually the mouse cursor position
 *
 *  @return the closest component that hit-tests to true for the position, or
 *          NULL if no component hit-tests
 */
SimpleGUIComponent* SimpleGUI::closestHitTest(
	FocusList& list, const Vector2 & pos )
{
	// first filter out cross focus components that are not in any root
	// hierarchy
	FocusList filteredList;
	filterList( components_, list, filteredList );

	SimpleGUIComponent* c = NULL;
	for ( FocusList::reverse_iterator i = filteredList.rbegin();
		i != filteredList.rend(); ++i )
	{
		if ( (*i)->hitTest( pos ) )
		{
			if ( c == NULL || c->drawOrder() < (*i)->drawOrder() )
			{
				c = (*i);
			}
		}
	}
	return c;
}


/**
 * Process a key event, looking for possible mouse click events.
 *
 *	@param event	the input key event.
 *
 *	@
 */
bool SimpleGUI::processClickKey( const SimpleGUIKeyEvent & event )
{
	bool handled = false;

	if (event.key() == KeyEvent::KEY_LEFTMOUSE)
	{
		if (event.isKeyDown()) 
		{
			this->clickComponent_ = closestHitTest( focusList_, event.mousePos() );
		}
		// not a click event if drag is happening
		else if (this->clickComponent_ != NULL && 
				(this->dragInfo_.get() == NULL || !this->dragInfo_->dragging) &&
					this->clickComponent_->hitTest( event.mousePos() ))
		{
			// This is a click event
			handled = this->clickComponent_->handleMouseClickEvent( event );
			this->clickComponent_ = NULL;
		}
	}

	return handled;
}


/**
 *	Process a key event, looking for possible drag'n drop events.
 *
 *	@param event	the input key event.
 */
bool SimpleGUI::processDragKey( const SimpleGUIKeyEvent & event )
{
	if (event.key() == KeyEvent::KEY_LEFTMOUSE)
	{
		if (event.isKeyDown()) 
		{
			SimpleGUIComponent* c = closestHitTest( dragFocusList_, event.mousePos() );
			if ( c != NULL )
			{
				// store button down position 
				// for possible drag operation
				this->dragInfo_.reset( new DragInfo );
				this->dragInfo_->keyEvent      = event;
				this->dragInfo_->component     = c;
				this->dragInfo_->target        = NULL;
				this->dragInfo_->dragging      = false;
				this->dragInfo_->targetAccepts = false;
			}
		}
		else if (this->dragInfo_.get() != NULL)
		{
			SimpleGUIComponent * component = this->dragInfo_->component;

			// Dragging is under way. 
			// This is a drop event
			if (this->dragInfo_->dragging)
			{
				SimpleGUIComponent * target  = this->dragInfo_->target;
				if (target != NULL && this->dragInfo_->targetAccepts)
				{
					target->handleDropEvent( component, event );
				}
				// component may have been deleted 
				// when processing drop event
				if (this->dragInfo_.get() != NULL)
				{
					component->handleDragStopEvent( event );
				}
			}
			this->dragInfo_.reset( NULL );

			// send mouse enter/leave events now, so the events don't have to 
			// wait until the next mouse move.
			SimpleGUIMouseEvent mouseEvent( MouseEvent(), event.mousePos() );

			EnterLeaveHandler::detectEvents( mouseEvent, 
				NULL,
				components_,
				crossFocusList_, 
				false,
				MouseMoveFuncs() );
		}
	}

	return this->dragInfo_.get() != NULL;
}


/**
 *	Process a mouse event, looking for possible mouse enter/leave events.
 *
 *	@param event	the input mouse event.
 */
bool SimpleGUI::processMouseMove( const SimpleGUIMouseEvent & event )
{
	return EnterLeaveHandler::detectEvents( event, 
		NULL,
		components_,
		crossFocusList_, 
		true,
		MouseMoveFuncs() );
}


/**
 *	Process a mouse event, looking for possible drag'n drop events.
 *
 *	@param event	the input mouse event.
 */
bool SimpleGUI::processDragMove( const SimpleGUIMouseEvent & event )
{
	if (this->dragInfo_.get() != NULL)
	{
		// Mouse button down has been detected but 
		// the drag operation itself not yet started
		if (!this->dragInfo_->dragging) 
		{
			const Vector2 &dragStartPos = this->dragInfo_->keyEvent.mousePos();		
			if ((dragStartPos - event.mousePos()).lengthSquared() > this->dragDistanceSqr_)
			{
				if (this->dragInfo_->component->handleDragStartEvent( 
						this->dragInfo_->keyEvent ))
				{
					this->dragInfo_->dragging = true;
				}
				else 
				{
					this->dragInfo_.reset( NULL );
				}				
			}
			// Ensure the EnterLeaveHandler::detectEvents has a fresh start,
			// required when dragging
			EnterLeaveHandler::reset();
		}

		// Dragging is under way. Look 
		// for dragEnter/dragLeave events
		if (this->dragInfo_.get() != NULL && this->dragInfo_->dragging) 
		{
			EnterLeaveHandler::detectEvents(
				event, &(this->dragInfo_->target), 
				components_,
				dropFocusList_,
				false, // not needed, DragMoveFuncs don't send move events
				DragMoveFuncs( 
					this->dragInfo_->component,
					this->dragInfo_->targetAccepts ) );
		}
	}

	return this->dragInfo_.get() != NULL;
}


/**
 *	This method add a simple gui component onto the
 *	focus list, meaning the component now receives button events.
 *
 *	@param c	the component that will now receive the input focus.
 */
void SimpleGUI::addInputFocus( SimpleGUIComponent* c )
{
	addToFocusList( this->focusList_, c );
}


/**
 *	This method removes a simple gui component from the focus list.
 *
 *	@param c	the component that no longer wants focus.
 */
void SimpleGUI::delInputFocus( SimpleGUIComponent* c )
{
	delFromFocusList( this->focusList_, c );
	if (c == this->clickComponent_)
	{
		this->clickComponent_ = NULL;
	}
}


/**
 *	Forces a synthetic enter or leave mouse event if the cursor position
 *	hit-tests with the component.
 *
 *	@param c		the component that will receive the event
 *	@param enter	if true, a mouseEnter is sent, otherwise a mouseLeave
 */
void SimpleGUI::generateEnterLeaveEvent( SimpleGUIComponent* c, bool enter )
{
	if (!inited_ || !mouseCursor().isActive())
	{
		return;
	}

	// Check to see if the GUI is below the mouse cursor. If so, synthesise a 
	// mouseEnter event.
	Vector2 mousePos = mouseCursor().position();
	if ( c->hitTest( mousePos ) )
	{
		SimpleGUIMouseEvent mouseEvent( MouseEvent(), mousePos );
		if ( enter == true )
			c->handleMouseEnterEvent( mouseEvent );
		else // enter == false
			c->handleMouseLeaveEvent( mouseEvent );
	}
}


/**
 *	This method recalculates the draw order of the components in the GUI.
 */
void SimpleGUI::recalcDrawOrders()
{
	// recalculate the draw orders
	uint32 currDrawOrder = 0;

	Components::iterator it = components_.begin();
	Components::iterator end = components_.end();

	while (it != end)
	{
		currDrawOrder = (*it++)->calcDrawOrderRecursively( currDrawOrder, 0 );
	}
}


/**
 *	This method checks if the cursor changed, and if so, send enter/leave
 *	events.
 */
void SimpleGUI::checkCursorChanged()
{
	static bool s_lastMouseActive = false;

	if (mouseCursor().isActive() != s_lastMouseActive)
	{
		// make sure the EnterLeaveHandler sends enter/leave events when
		// the mouse cursor is hidden/shown.
		
		EnterLeaveHandler::cursorChanged(
			components_,
			crossFocusList_, 
			MouseMoveFuncs() );

		s_lastMouseActive = mouseCursor().isActive();
	}
}


/**
 *	This method add a simple gui component onto the mouse cross focus list, 
 *	meaning the component now receives mouse enter and mouse leave events.
 *
 *	@param c	the component that will now receive the input focus.
 */
void SimpleGUI::addMouseCrossFocus( SimpleGUIComponent* c )
{
	addToFocusList( this->crossFocusList_, c );
	generateEnterLeaveEvent( c, true /*enter*/ );
}


/**
 *	This method removes a simple gui component from the 
 *	mouse cross focus list.
 *
 *	@param c	the component that no longer wants focus.
 */
void SimpleGUI::delMouseCrossFocus( SimpleGUIComponent* c )
{
	if ( c != NULL && c->ob_refcnt > 0 )
		generateEnterLeaveEvent( c, false /*leave*/ );
	delFromFocusList( this->crossFocusList_, c );
}


/**
 *	This method add a simple gui component onto the mouse move focus 
 *	list, meaning the component now receives button move events.
 *
 *	@param c	the component that will now receive the input focus.
 */
void SimpleGUI::addMouseMoveFocus( SimpleGUIComponent* c )
{
	// now handled through the hierarchy
}


/**
 *	This method removes a simple gui component from the 
 *	mouse move focus list.
 *
 *	@param c	the component that no longer wants focus.
 */
void SimpleGUI::delMouseMoveFocus( SimpleGUIComponent* c )
{
	// now handled through the hierarchy
}


/**
 *	This method add a simple gui component onto the drag focus list, 
 *	meaning the component now receives drag start events.
 *
 *	@param c	the component that will now receive the drag focus.
 */
void SimpleGUI::addMouseDragFocus( SimpleGUIComponent* c )
{
	addToFocusList( this->dragFocusList_, c );
}


/**
 *	This method removes a simple gui component from the drag focus list.
 *
 *	@param c	the component that no longer wants focus.
 */
void SimpleGUI::delMouseDragFocus( SimpleGUIComponent* c )
{
	delFromFocusList( this->dragFocusList_, c );
	if (this->dragInfo_.get() != NULL && this->dragInfo_->component == c)
	{
		this->dragInfo_.reset( NULL );
	}
}


/**
 *	This method add a simple gui component onto the drop focus list, 
 *	meaning the component now receives drop events.
 *
 *	@param c	the component that will now receive the drop focus.
 */
void SimpleGUI::addMouseDropFocus( SimpleGUIComponent* c )
{
	addToFocusList( this->dropFocusList_, c );
}


/**
 *	This method removes a simple gui component from the drop focus list.
 *
 *	@param c	the component that no longer wants focus.
 */
void SimpleGUI::delMouseDropFocus( SimpleGUIComponent* c )
{
	delFromFocusList( this->dropFocusList_, c );
	if (this->dragInfo_.get() != NULL && this->dragInfo_->target == c)
	{
		this->dragInfo_->target = NULL;		
		this->dragInfo_->dragging = false;
		this->dragInfo_->targetAccepts = false;
	}
}

// -----------------------------------------------------------------------------
// Section: Clip region methods
// -----------------------------------------------------------------------------

/**
 *	This method pushes a gui component's area as the current clipping region.
 *
 *	@param c		The simple gui component whose area defines the new clip region.
 *	@return false	if the region is NULL and therefore drawing shouldn't continue.
 */
bool SimpleGUI::pushClipRegion( SimpleGUIComponent& c )
{
	int nVerts;
	GUIVertex* verts = c.vertices( &nVerts );

	//Current implementation only allows setting a clip region from
	//a very simple gui component.
	MF_ASSERT_DEV( nVerts == 4);

	//note we need to transform the clipping region by the current transform.
	//this allows for nested ( and scrolling ) clip regions.
	const Matrix& w = Moo::rc().world();

	Vector3 v1 = w.applyPoint(verts[1].pos_);
	Vector3 v3 = w.applyPoint(verts[3].pos_);

	// The bottom-right of the rect is shifted down one pixel, since D3D RECT's are right hand exclusive.
	Vector4 a( v1.x, v3.y, v3.x+pixelToClipX_, v1.y-pixelToClipY_ );
	
	return this->pushClipRegion(a);
}


/**
 *	This method pushes a clipping region.
 *
 *	@param cr		The clip region, (x,y,x,y)
 *	@return false	if the region is NULL and therefore drawing shouldn't continue.
 */
bool SimpleGUI::pushClipRegion( const Vector4& cr )
{
	Vector4 a(cr);

	//if there is an existing clip region, then clip the current one to it.
	if ( !clipStack_.empty() )
	{
		Vector4 b = clipStack_.top();
		a.x = max( a.x, b.x );
		a.y = min( a.y, b.y );
		a.z = min( a.z, b.z );
		a.w = max( a.w, b.w );
	}

	clipStack_.push( a );

	bool success = commitClipRegion();

	if ( !success )
		clipStack_.pop();

	return success;
}


/**
 *	This private method commits the current clip region, either as a
 *	scissors rectangle ( xbox ) or by using an elaborate fudge with
 *	viewports.
 */
bool SimpleGUI::commitClipRegion()
{
	if ( clipStack_.empty() )
	{
		Moo::rc().setRenderState( D3DRS_SCISSORTESTENABLE, FALSE );
		return true;
	}

	Vector4 top = clipStack_.top();
	D3DRECT region;
	region.x1 = (LONG)(top.x * Moo::rc().halfScreenWidth() + Moo::rc().halfScreenWidth());
	region.y1 = (LONG)(top.y * -Moo::rc().halfScreenHeight() + Moo::rc().halfScreenHeight());
	region.x2 = (LONG)(top.z * Moo::rc().halfScreenWidth() + Moo::rc().halfScreenWidth());
	region.y2 = (LONG)(top.w * -Moo::rc().halfScreenHeight() + Moo::rc().halfScreenHeight());

	//right now, windows are always anchored in the middle.
	Vector2 center( (top.x + top.z)/2.f, (top.y + top.w)/2.f );

	if ( ( region.x2 - region.x1 > 0.f ) && ( region.y2 - region.y1 > 0.f ) )
	{
		RECT rect;
		rect.left = region.x1;
		rect.right = region.x2;
		rect.top = region.y1;
		rect.bottom = region.y2;

		Moo::rc().device()->SetScissorRect( &rect );
		return ( Moo::rc().setRenderState( D3DRS_SCISSORTESTENABLE, TRUE ) == S_OK );
	}
	else
		return false;
}


/**
 *	This method pops the current clipping region.
 */
void SimpleGUI::popClipRegion()
{
	MF_ASSERT_DEV( !clipStack_.empty() );

	if( !clipStack_.empty() )
		clipStack_.pop();

	commitClipRegion();
}


/**
 *	This method returns the current clip region, as a vector4
 *	of ( left, top, right, bottom ) in clip coords
 */
const Vector4& SimpleGUI::clipRegion()
{
	static Vector4 fullscreen( -1.f, 1.f, 1.f, -1.f );

	if ( clipStack_.empty() )
		return fullscreen;
	else
		return clipStack_.top();
}


/**
 *	This method returns whether or not the given point is
 *	within the current clip region or not.
 */
bool SimpleGUI::isPointInClipRegion( const Vector2& pt )
{
	if ( clipStack_.empty() )
		return true;
	else
	{
		const Vector4& clipRgn = clipStack_.top();
		//clipRgn is (x,y,x,y)
		return ( pt.x >= clipRgn.x && pt.y <= clipRgn.y && pt.x <= clipRgn.z && pt.y >= clipRgn.w );
	}
}


/**
 * Sets the current resolution override. This overrides what the GUI system thinks
 * the current resolution is (good for scaling UI's that use PIXEL layout modes).
 * Setting this to a zero vector disables the override
 */
void SimpleGUI::resolutionOverride( const Vector2& res)
{
	if ( almostZero( res.lengthSquared(), 0.0002f ) && usingResolutionOverride() )
	{
		// Switching off override
		Vector2 realRes( Moo::rc().screenWidth(), Moo::rc().screenHeight() );
		pixelToClipX_ = 2.f / realRes.x;
		pixelToClipY_ = 2.f / realRes.y;

	}
	else if ( !almostZero( res.length(), 0.0002f ) && !usingResolutionOverride() )
	{
		// Turning on override
		pixelToClipX_ = 2.f / res.x;
		pixelToClipY_ = 2.f / res.y;
	}

	resolutionOverride_ = res;

	
}

// -----------------------------------------------------------------------------
// Section: Script methods
// -----------------------------------------------------------------------------

PY_MODULE_STATIC_METHOD( SimpleGUI, addRoot, GUI )
PY_MODULE_STATIC_METHOD( SimpleGUI, delRoot, GUI )
PY_MODULE_STATIC_METHOD( SimpleGUI, reSort, GUI )
PY_MODULE_STATIC_METHOD( SimpleGUI, roots, GUI )


/*~ function GUI.addRoot
 *
 *	This function adds a GUI component to the gui root.  GUI elements exist in
 *	a treelike structure, with every element having a parent.  The top level
 *	parent is the root of the whole tree.  This root exists internal to the GUI
 *	module.	In order for a gui component to be processed, for both rendering
 *	and event handling, it, or one of its parents, must be added to the root.
 *
 *	@param component	the gui component to add to the root
 */
/**
 *	This method is a script wrapper to the C++ method.
 */
PyObject * SimpleGUI::py_addRoot( PyObject * args )
{
	PyObject	* pComponent;
	if (!PyArg_ParseTuple( args, "O", &pComponent ))
	{
		PyErr_SetString( PyExc_TypeError, "GUI.addRoot: "
			"Argument parsing error: Expected a GUI component" );
		return NULL;
	}

	if (PyWeakref_CheckProxy( pComponent))
	{
		pComponent = (PyObject*) PyWeakref_GET_OBJECT( pComponent );
	}
	if (!SimpleGUIComponent::Check( pComponent ))
	{
		PyErr_SetString( PyExc_TypeError, "GUI.addRoot: "
			"Argument parsing error: Expected a GUI component(weak ref may be gone)" );
		return NULL;
	}

	SimpleGUI::instance().addSimpleComponent(
		*(SimpleGUIComponent*)pComponent );

	Py_Return;
}


/*~ function GUI.delRoot
 *
 *	This function removes a Gui component from the root gui's list of children.
 *
 *	@param	component	the component to remove
 */
/**
 *	This method is a script wrapper to the C++ method.
 */
PyObject * SimpleGUI::py_delRoot( PyObject * args )
{
	PyObject	* pComponent;
	if (!PyArg_ParseTuple( args, "O", &pComponent ))
	{
		PyErr_SetString( PyExc_TypeError, "GUI.delRoot: "
			"Argument parsing error: Expected a GUI component" );
		return NULL;
	}

	if (PyWeakref_CheckProxy( pComponent))
	{
		pComponent = (PyObject*) PyWeakref_GET_OBJECT( pComponent );
	}
	if (!SimpleGUIComponent::Check( pComponent ))
	{
		PyErr_SetString( PyExc_TypeError, "GUI.delRoot: "
			"Argument parsing error: Expected a GUI component(weak ref may be gone)" );
		return NULL;
	}

	SimpleGUI::instance().removeSimpleComponent(
		*(SimpleGUIComponent*)pComponent );

	Py_Return;
}


/*~ function GUI.reSort
 *
 *	This function resorts the gui components at the root of the GUI tree.
 *	This function sorts according to the third (depth) component of the
 *	children's position attributes.  Changing this depth value in a child
 *	doesn't automatically reorder the children, so an explicit call to
 *	this function is required.
 *
 *	@return Read-Only list of GUI components.
 *
 */
/**
 *	This method is a script wrapper to the C++ method.
 */
PyObject * SimpleGUI::py_reSort( PyObject * args )
{
	SimpleGUI::instance().reSort();

	Py_Return;
}


/*~ function GUI.roots
 *
 *	This function returns a list of the current root components.  It is not
 *	editable.  Instead, use the GUI.addRoot and GUI.delRoot to add and remove
 *	components from the list.
 *
 *	@return Read-Only list of GUI components.
 *
 */
/**
 *	This method returns a list of the current roots. It is
 *	currently not editable.
 *	TODO: Use stl_to_py and make it editable. Then turn it
 *	into a module property instead of this function.
 */
PyObject * SimpleGUI::py_roots( PyObject * args )
{
	Components & cs = SimpleGUI::instance().components_;

	PyObject * pList = PyList_New( cs.size() );
	for (uint i = 0; i < cs.size(); i++)
	{
		PyList_SetItem( pList, i, cs[i] );
		Py_INCREF( cs[i] );
	}
	return pList;
}


/*~ function GUI.update
 *
 *	This function causes the GUI to be updated as if the specified amount of
 *	time had passed.  This updates all of the components and shaders which
 *	have been added to the GUI root.
 *
 *	The system automatically updates the gui each tick with dTime on each
 *	update being the actual time since the last update.  This call would not
 *	normally need to be used.
 *
 *	@param	dTime	The amount of time assumed to have passed since the last
 *					update.
 */
/**
 *	This method gets the GUI to update
 */
PyObject * SimpleGUI::py_update( PyObject * args )
{
	float dtime;
	if (!PyArg_ParseTuple( args, "f", &dtime ))
	{
		PyErr_SetString( PyExc_TypeError, "GUI.update: "
			"Argument parsing error: Expected float dtime" );
		return NULL;
	}

	SimpleGUI::instance().update( dtime );

	Py_Return;
}
PY_MODULE_STATIC_METHOD( SimpleGUI, update, GUI )


/*~ function GUI.draw
 *
 *	This method causes the GUI and all components which have been added as
 *	roots to be redrawn.
 *
 *	It is called automatically by the system each tick.  It would not normally
 *	be necessary to call this function.
 */
/**
 *	This method gets the GUI to draw
 */
PyObject * SimpleGUI::py_draw( PyObject * args )
{
	if (PyTuple_Size( args ) != 0)
	{
		PyErr_SetString( PyExc_TypeError, "GUI.draw: "
			"Argument parsing error: Expected no arguments" );
		return NULL;
	}

	SimpleGUI::instance().draw();

	Py_Return;
}
PY_MODULE_STATIC_METHOD( SimpleGUI, draw, GUI )


/*~ function GUI.handleKeyEvent
 *
 *	This functions gets the GUI to handle the given key event.  It hands the 
 *	event to each of its child components which have the focus attribute set, 
 *	until one of them returns non zero, at which point it returns non-zero. 
 *	Otherwise, it returns zero.
 *
 *	This needs to be called from the personality script, otherwise the GUI will
 *	not process key events.
 *
 *	@param	event	A keyEvent is a 3-tuple of integers, as follows:
 *					(down, key, modifiers) down is 0 if the key transitioned
 *					from down to up, non-zero if it transitioned from up to
 *					down key is the keycode for which key was pressed.
 *					modifiers indicates which modifier keys were pressed when
 *					the event occurred, and can include MODIFIER_SHIFT,
 *					MODIFIER_CTRL, MODIFIER_ALT.
 *
 *	@return			non-zero means the event was handled, zero means it wasn't.
 */
/**
 *	This method gets the GUI to handle the given key event
 */
PyObject * SimpleGUI::py_handleKeyEvent( PyObject * args )
{
	KeyEvent ke;
	if (Script::setData( args, ke, "handleKeyEvent arguments" ) != 0)
		return NULL;

	return Script::getData(
		SimpleGUI::instance().handleKeyEvent( ke ) );
}
PY_MODULE_STATIC_METHOD( SimpleGUI, handleKeyEvent, GUI )

/*~ function GUI.handleMouseEvent
 *
 *	This function gets the GUI to handle the given mouse event.  It hands the 
 *	event to each of its child components which have the focus attribute set, 
 *	until one of them returns non zero, at which point it returns non-zero. 
 *	Otherwise, it returns zero.
 *
 *	This needs to be called from the personality script, otherwise the GUI will
 *	not process mouse events.
 *
 *	@param	event	A mouseEvent is a 3-tuple of integers, as follows:
 *					(dx, dy, dz), where dx, dy an dz are the distance the mouse
 *					has moved in the x, y and z dimensions, respectively.
 *
 *	@return			non-zero means the event was handled, zero means it wasn't.
 */
/**
 *	This method gets the GUI to handle the given mouse event
 */
PyObject * SimpleGUI::py_handleMouseEvent( PyObject * args )
{
	MouseEvent mouseEvent(0,0,0);
	if (Script::setData( args, mouseEvent, "handleMouseEvent arguments" ) != 0)
		return NULL;

	return Script::getData(
		SimpleGUI::instance().handleMouseEvent( mouseEvent ) );
}
PY_MODULE_STATIC_METHOD( SimpleGUI, handleMouseEvent, GUI )


/*~ function GUI.handleAxisEvent
 *
 *	This function gets the GUI to handle the given axis event.  It hands the 
 *	event to each of its child components which have the focus attribute set, 
 *	until one of them returns non zero, at which point it returns non-zero.
 *	Otherwise, it returns zero.
 *
 *	This needs to be called from the personality script, otherwise the GUI will
 *	not process axis events.
 *
 *	@param	event	An axisEvent is a 3-tuple of one integer and two floats, as
 *					follows: (axis, value, dTime) where axis is one of AXIS_LX,
 *					AXIS_LY, AXIS_RX, AXIS_RY, with the first letter being L or
 *					R meaning left thumbstick or right thumbstick, the second,
 *					X or Y being the direction. value is the position of that
 *					axis, between -1 and 1. 	dTime is the time in seconds
 *					since that axis was last processed.
 *
 *	@return			non-zero means the event was handled, zero means it wasn't.
 */
/**
 *	This method gets the GUI to handle the given axis event
 */
PyObject * SimpleGUI::py_handleAxisEvent( PyObject * args )
{
	AxisEvent ae;
	if (Script::setData( args, ae, "handleAxisEvent arguments" ) != 0)
		return NULL;

	return Script::getData(
		SimpleGUI::instance().handleAxisEvent( ae ) );
}
PY_MODULE_STATIC_METHOD( SimpleGUI, handleAxisEvent, GUI )


/*~	function GUI.screenResolution
 *
 *	This function simply returns the current (width,height) in pixels of the
 *	screen.
 *
 *	@return	A 2-tuple representing the width and height of the screen, in
 *	pixels.
 */
/**
 *	This method gets the current screen resolution, in pixels.
 */
PyObject * SimpleGUI::py_screenResolution( PyObject * args )
{
	PyObject * pTuple = PyTuple_New( 2 );

	if ( SimpleGUI::instance().usingResolutionOverride() )
	{
		Vector2 resolutionOverride = SimpleGUI::instance().resolutionOverride();
		PyTuple_SET_ITEM( pTuple, 0, Script::getData( resolutionOverride.x ) );
		PyTuple_SET_ITEM( pTuple, 1, Script::getData( resolutionOverride.y ) );
	}
	else
	{
		PyTuple_SET_ITEM( pTuple, 0, Script::getData( Moo::rc().screenWidth() ) );
		PyTuple_SET_ITEM( pTuple, 1, Script::getData( Moo::rc().screenHeight() ) );
	}

	return pTuple;
}
PY_MODULE_STATIC_METHOD( SimpleGUI, screenResolution, GUI )

/*~	function GUI.setResolutionOverride
 *
 *	This method overrides the resolution that the GUI system uses for
 *	calculations. This allows components setup in pixel space to be 
 *	automatically scaled. For example, if the resolution override is set
 *	to 1024x768, then the GUI will take up the same area of the screen 
 *	no matter what the actual screen resolution is.
 *
 *	@param	resolution	A Vector2 of the new override. If it is a zero vector
 *						then the override is disabled.
 */
static void setResolutionOverride( const Vector2& res )
{
	SimpleGUI::instance().resolutionOverride( res );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, setResolutionOverride, ARG( Vector2, END), GUI )


/*~	function GUI.setDragDistance
 *
 *	Sets the minimum distance the mouse pointer has to travel after the left
 *	mouse button has been pressed before the movement is considered a drag.
 *
 *	Once the movement is classified as dragging, a click event will no longer 
 *	be generated if the mouse is released over the dragged component.
 *
 *	@param	distance	the minimum drag distance (float, in clip space).
 *						The default value is 0.002.
 */
/**
 *	Sets the Sets the minimum drag distance.
 */
PyObject * SimpleGUI::py_setDragDistance( PyObject * args )
{
	float dragDistange = 0;
	if (!PyArg_ParseTuple( args, "f", &dragDistange ))
	{
		PyErr_SetString( PyExc_TypeError, "GUI.setDragDistance: "
			"Argument parsing error: Expected a float" );
		return NULL;
	}

	SimpleGUI::instance().dragDistanceSqr_ = dragDistange * dragDistange;

	Py_Return;
}
PY_MODULE_STATIC_METHOD( SimpleGUI, setDragDistance, GUI )


// Class linking definitions
extern int SimpleGUIComponent_token;
extern int TextGUIComponent_token;
extern int FrameGUIComponent_token;
extern int FrameGUIComponent2_token;
extern int ConsoleGUIComponent_token;
extern int BoundingBoxGUIComponent_token;
extern int AlphaGUIShader_token;
extern int ClipGUIShader_token;
extern int ColourGUIShader_token;
extern int MatrixGUIShader_token;
extern int WindowGUIComponent_token;
extern int GuiAttachment_token;
extern int GraphGUIComponent_token;
extern int MeshGUIAdaptor_token;
extern int GoboComponent_token;
int GUI_token =
	SimpleGUIComponent_token &&
	TextGUIComponent_token &&
	FrameGUIComponent_token &&
	FrameGUIComponent2_token &&
	ConsoleGUIComponent_token &&
	BoundingBoxGUIComponent_token &&
	AlphaGUIShader_token &&
	ClipGUIShader_token &&
	ColourGUIShader_token &&
	MatrixGUIShader_token &&
	WindowGUIComponent_token &&
	GuiAttachment_token &&
	GraphGUIComponent_token &&
	MeshGUIAdaptor_token &&
	GoboComponent_token;
