/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SIMPLE_GUI_HPP
#define SIMPLE_GUI_HPP

#include <vector>
#include <stack>

#include "mouse_cursor.hpp"
#include "moo/moo_math.hpp"
#include "moo/moo_dx.hpp"
#include "input/input.hpp"
#include "pyscript/script.hpp"
#include "cstdmf/singleton.hpp"


class SimpleGUIComponent;
class SimpleGUIKeyEvent;
class SimpleGUIMouseEvent;


extern int GUI_token;


/*~ module GUI
 *
 *	The GUI module contains all the functions which are used to create GUI
 *  components and shaders,
 *	to display them on the screen, and to handle input events.
 */

/**
 * This is a singleton class that manages SimpleGUIComponents.
 * It maintains the roots of the GUI component tree.
 *
 * Any components added to / created by the SimpleGUI will
 * be automatically freed upon destruction.
 */
class SimpleGUI : public InputHandler, public Singleton< SimpleGUI >
{
public:
	typedef std::vector< SimpleGUIComponent* > FocusList;
	typedef std::vector< SimpleGUIComponent* > Components;

	~SimpleGUI();

	void				hInstance( void *h );
	void				hwnd( void *h );
	void *				hwnd() const;
	
	static void			init( DataSectionPtr pConfig );
	static void			fini();

	const MouseCursor &	mouseCursor() const;
	MouseCursor &		mouseCursor();

	/// This method returns whether or not the resolution has changed.
	bool				hasResolutionChanged( void )
											{ return resolutionHasChanged_; }

	//SimpleGUIComponent&	createSimpleComponent( const std::string& textureName );
	void				addSimpleComponent( SimpleGUIComponent& c );
	void				removeSimpleComponent( SimpleGUIComponent& c );
	void				update( float dTime );
	void				draw( void );
	void				setUpdateEnabled( bool enable = true );

	void				reSort( void );

	void				pixelRangesToClip( float w, float h, float* retW, float* retH );
	void				clipRangesToPixel( float w, float h, float* retW, float* retH );

	void				resolutionOverride( const Vector2& res );
	const Vector2&		resolutionOverride() const;
	bool				usingResolutionOverride() const;

	float				screenWidth() const;
	float				screenHeight() const;
	float				halfScreenWidth() const;
	float				halfScreenHeight() const;
	Vector2				screenResolution() const;
	uint32				realScreenResolutionCounter() const;


	void				countDrawCall();

	/// Input method
	void				addInputFocus( SimpleGUIComponent* c );
	void				delInputFocus( SimpleGUIComponent* c );

	void				addMouseCrossFocus( SimpleGUIComponent* c );
	void				delMouseCrossFocus( SimpleGUIComponent* c );

	void				addMouseMoveFocus( SimpleGUIComponent* c );
	void				delMouseMoveFocus( SimpleGUIComponent* c );

	void				addMouseDragFocus( SimpleGUIComponent* c );
	void				delMouseDragFocus( SimpleGUIComponent* c );
	void				addMouseDropFocus( SimpleGUIComponent* c );
	void				delMouseDropFocus( SimpleGUIComponent* c );

	bool				handleKeyEvent( const KeyEvent & /*event*/ );
	bool				handleMouseEvent( const MouseEvent & /*event*/ );
	bool				handleAxisEvent( const AxisEvent & /*event*/ );

	bool				processClickKey( const SimpleGUIKeyEvent & event );
	bool				processDragKey( const SimpleGUIKeyEvent & event );
	bool				processMouseMove( const SimpleGUIMouseEvent & event );
	bool				processDragMove( const SimpleGUIMouseEvent & event );

	/// Hierarchy / clipping methods
	bool				pushClipRegion( SimpleGUIComponent& c );	
	bool				pushClipRegion( const Vector4& );
	void				popClipRegion();	
	const Vector4&		clipRegion();
	bool				isPointInClipRegion( const Vector2& pt );
	void				setConstants(DWORD colour, bool pixelSnap);
	bool				isSimpleGUIComponentInTree(
							const SimpleGUIComponent * pComponent ) const;

	void				recalcDrawOrders();


	PY_MODULE_STATIC_METHOD_DECLARE( py_addRoot )
	PY_MODULE_STATIC_METHOD_DECLARE( py_delRoot )
	PY_MODULE_STATIC_METHOD_DECLARE( py_reSort )
	PY_MODULE_STATIC_METHOD_DECLARE( py_roots )

	PY_MODULE_STATIC_METHOD_DECLARE( py_update )
	PY_MODULE_STATIC_METHOD_DECLARE( py_draw )
	PY_MODULE_STATIC_METHOD_DECLARE( py_handleKeyEvent )
	PY_MODULE_STATIC_METHOD_DECLARE( py_handleMouseEvent )
	PY_MODULE_STATIC_METHOD_DECLARE( py_handleAxisEvent )
	PY_MODULE_STATIC_METHOD_DECLARE( py_screenResolution )
	PY_MODULE_STATIC_METHOD_DECLARE( py_setDragDistance )

private:
	SimpleGUI();
	SimpleGUI(const SimpleGUI&);
	SimpleGUI& operator=(const SimpleGUI&);

	MouseCursor &		internalMouseCursor() const;
	bool				commitClipRegion();
	bool				isMouseKeyEvent( const KeyEvent& event ) const;
	SimpleGUIComponent* closestHitTest( FocusList& list, const Vector2 & pos );
	void				generateEnterLeaveEvent( SimpleGUIComponent* c, bool enter );
	void				checkCursorChanged();

	mutable MouseCursor * pMouseCursor_;

	//SmartPointer< SimpleGUIComponent >	pRoot_;
	typedef std::auto_ptr< struct DragInfo >	DragInfoPtr;
	Components			components_;
	
	Vector2				resolutionOverride_;

	float				pixelToClipX_;
	float				pixelToClipY_;

	FocusList			focusList_;
	FocusList			crossFocusList_;
	FocusList			dragFocusList_;
	FocusList			dropFocusList_;
	SimpleGUIComponent* clickComponent_;
	DragInfoPtr         dragInfo_;

	float				dragDistanceSqr_;
	
	bool				resolutionHasChanged_;
	Vector2				lastResolution_;

	Vector2				lastRealResolution_;
	uint32				realResolutionCounter_;
	bool				updateGUI_;

	void*				hwnd_;
	void*				hInstance_;
	bool				inited_;
	float				dTime_;

	typedef std::stack< Vector4, std::vector< Vector4 > > ClipStack;
	ClipStack			clipStack_;
	//PC client needs this because the clip stack is implemented using viewports
	//and view matrices
	DX::Viewport		originalView_;
	Matrix				clipFixer_;

	size_t				drawCallCount_;

	typedef SimpleGUI This;
};


#ifdef CODE_INLINE
#include "simple_gui.ipp"
#endif


#endif // SIMPLE_GUI_HPP
