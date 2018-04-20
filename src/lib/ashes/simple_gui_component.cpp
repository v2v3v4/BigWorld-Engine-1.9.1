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

#include "simple_gui_component.hpp"
#include "simple_gui.hpp"
#include "mouse_cursor.hpp"
#include "gui_shader.hpp"
#include "gui_vertex_format.hpp"
#include "math/colour.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/auto_config.hpp"
#include "pyscript/script.hpp"
#include "pyscript/script_math.hpp"
#include "pyscript/py_data_section.hpp"

#include "moo/texture_manager.hpp"
#include "moo/render_context.hpp"
#include "moo/effect_material.hpp"
#include "moo/visual_channels.hpp"
#include "romp/py_texture_provider.hpp"
#include "moo/dynamic_vertex_buffer.hpp"
#include "moo/dynamic_index_buffer.hpp"

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "2DComponents", 0 )

#ifndef CODE_INLINE
#include "simple_gui_component.ipp"
#endif

inline float parentSpaceToClipSpace( float v, float parentSizeInClip )
{
	float n = (v+1.0f) / 2.0f;
	return parentSizeInClip*n - parentSizeInClip/2.0f;
}

inline float clipSpaceToParentSpace( float v, float parentSizeInClip )
{
	float k = v + parentSizeInClip/2.0f;
	return 2.0f*(k/parentSizeInClip) - 1.0f;
}

// -----------------------------------------------------------------------------
// Section: PyGUIComponentColour
// -----------------------------------------------------------------------------

/**
 * TODO: to be documented.
 */
class PyGUIComponentColour : public PyColour
{
public:
	PyGUIComponentColour( SimpleGUIComponent * pComponent,
			bool isReadOnly = false,
			PyTypePlus * pType = &s_type_ ) :
		PyColour( isReadOnly, pType ),
		pComponent_( pComponent ) {}

	virtual Vector4 getVector() const
	{
		return Colour::getVector4( pComponent_->colour() );
	}

	virtual bool setVector( const Vector4 & v )
	{
		pComponent_->colour( Colour::getUint32( v ) );

		return true;
	}

private:
	SimpleGUIComponentPtr pComponent_;
};


// -----------------------------------------------------------------------------
// Section: PyGUIComponentPosition
// -----------------------------------------------------------------------------

/**
 * TODO: to be documented.
 */
class PyGUIComponentPosition : public PyVector< Vector3 >
{
public:
	PyGUIComponentPosition( SimpleGUIComponent * pComponent,
			bool isReadOnly = false,
			PyTypePlus * pType = &s_type_ ) :
		PyVector< Vector3 >( isReadOnly, pType ),
		pComponent_( pComponent ) {}

	virtual Vector3 getVector() const
	{
		return pComponent_->position();
	}

	virtual bool setVector( const Vector3 & v )
	{
		pComponent_->position( v );

		return true;
	}

private:
	SimpleGUIComponentPtr pComponent_;
};

PyObject * SimpleGUIComponent::pyGet_position()
{
	return new PyGUIComponentPosition( this );
}


// -----------------------------------------------------------------------------
// Section: PyGUIComponentSize
// -----------------------------------------------------------------------------

/**
 * TODO: to be documented.
 */
class PyGUIComponentSize : public PyVector< Vector2 >
{
public:
	PyGUIComponentSize( SimpleGUIComponent * pComponent,
			bool isReadOnly = false,
			PyTypePlus * pType = &s_type_ ) :
		PyVector< Vector2 >( isReadOnly, pType ),
		pComponent_( pComponent ) {}

	virtual Vector2 getVector() const
	{
		return pComponent_->size();
	}

	virtual bool setVector( const Vector2 & v )
	{
		pComponent_->size( v );

		return true;
	}

private:
	SimpleGUIComponentPtr pComponent_;
};

PyObject * SimpleGUIComponent::pyGet_size()
{
	return new PyGUIComponentSize( this );
}


// -----------------------------------------------------------------------------
// Section: Named constants
// -----------------------------------------------------------------------------

/**
 *	This global method specifies the resources required by this file
 */
static AutoConfigString s_mfmName( "system/ashesMaterial" );

// TODO: this code is a HACK used to change the intialisation order.
// After the new singleton manager and initialisation code
// is finished, we should revised code and rewrite it into
// an elegant way.
static DataSectionPtr& effectSection()
{
	static DataSectionPtr s_effectSection = NULL;
	return s_effectSection;
}

// To enable leak tracking, set TRACK_LEAKS to 1
#ifdef EDITOR_ENABLED
	#define TRACK_LEAKS 0
#else
	#define TRACK_LEAKS 1
#endif

#if ENABLE_DPRINTF && TRACK_LEAKS
	typedef std::map<int, std::pair<std::string, int> > IntStringIntMap;
	static IntStringIntMap s_instanceMap;
	static int  s_breakOnAllocId = 0;
	static bool s_breakOnLeak    = false;
#endif

/**
 *	Maps between materialFX enums and effect techniques.
 */
std::vector<D3DXHANDLE> SimpleGUIComponent::s_techniqueTable;
//have to keep a reference to the gui effect that sets up the
//technique table; otherwise if all gui items disappear then
//so will the effect, and that will break the link between
//the effect + technique table.
static Moo::ManagedEffectPtr s_guiEffect = NULL;


void SimpleGUIComponent::init( DataSectionPtr config )
{
	if (config.exists())
	{
#if ENABLE_DPRINTF && TRACK_LEAKS
		s_breakOnAllocId = config->readInt("simpleGui/breakOnAllocId", s_breakOnAllocId);
		s_breakOnLeak    = config->readBool("simpleGui/breakOnLeak", s_breakOnLeak);
#endif
	}

	effectSection() = BWResource::openSection( s_mfmName );
	MF_ASSERT_DEV( ("SimpleGUIComponent: Ashes material not found", effectSection().exists()) );
}

void SimpleGUIComponent::fini()
{
	effectSection() = NULL;
	s_techniqueTable.clear();
	s_guiEffect = NULL;

#if ENABLE_DPRINTF && TRACK_LEAKS
	IntStringIntMap::const_iterator instanceIt  = s_instanceMap.begin();
	IntStringIntMap::const_iterator instanceEnd = s_instanceMap.end();
	while (instanceIt != instanceEnd)
	{
		WARNING_MSG(
			"SimpleGUIComponent instance \"%s\" (AllocId=%d) not destroyed\n", 
			instanceIt->second.first.c_str(), instanceIt->second.second);

		++instanceIt;
	}
	if (!s_instanceMap.empty())
	{
		WARNING_MSG(
			"\n"
		    "---------------------------------------------------------\n"
			"Some SimpleGUIComponent instances haven't been destroyed.\n"
			"To debug, in <engine_config>.xml, set:                   \n"
			"   <simpleGui>                                           \n"
			"     <breakOnAllocId> AllocId </breakOnAllocId>          \n"
			"     <breakOnLeak>    true </breakOnLeak>                \n"
			"   </simpleGui>                                          \n"
		    "---------------------------------------------------------\n"
			"\n");

		if (s_breakOnLeak)
		{
			MF_ASSERT_DEV(("Breaking on GUI leak as requested", false));
		}
	}
#endif
}


/**
 *	This method sets up a table of techniques (s_techniqueTable) with techniques
 *	found in the given material.
 *
 *	@param		material the material to use
 *	@returns	true if successful
 */
bool SimpleGUIComponent::setupTechniqueTable( Moo::EffectMaterialPtr material )
{
	bool ret = false;
	// All GUI materials share the same underlying effect, which is why we can 
	// initialise this table by using any gui material instance.
	s_techniqueTable.clear();

	// Additionally, the GUI effect lists its techniques in order based on 
	// materialFX enum
	if ( material->pEffect() && material->pEffect()->pEffect() )
	{
		s_guiEffect = material->pEffect();
		ComObjectWrap<ID3DXEffect> pEffect = material->pEffect()->pEffect();
		if ( pEffect != NULL )
		{
			for ( int i=FX_ADD; i<=FX_ADD_SIGNED; i++ )
			{
				D3DXHANDLE handle = pEffect->GetTechnique( i );
				s_techniqueTable.push_back( handle );
			}
		}
		MF_ASSERT_DEV( s_techniqueTable.size() == (FX_ADD_SIGNED-FX_ADD+1) );
		ret = true;
	}
	else
	{
		ERROR_MSG("Material is invalid - not setting up technique table.\n");
	}

	return ret;
}

// -----------------------------------------------------------------------------
// Section: SimpleGUIComponent
// -----------------------------------------------------------------------------

#undef PY_ATTR_SCOPE
#define PY_ATTR_SCOPE SimpleGUIComponent::

PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( SimpleGUIComponent::eHAnchor, \
	horizontalAnchor, horizontalAnchor )

PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( SimpleGUIComponent::eVAnchor, \
	verticalAnchor, verticalAnchor )

PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( SimpleGUIComponent::ePositionMode, \
	horizontalPositionMode, horizontalPositionMode )

PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( SimpleGUIComponent::ePositionMode, \
	verticalPositionMode, verticalPositionMode )

PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( SimpleGUIComponent::eSizeMode, \
	widthMode, widthMode )

PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( SimpleGUIComponent::eSizeMode, \
	heightMode, heightMode )

PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( SimpleGUIComponent::eMaterialFX, \
	materialFX, materialFX )

PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( SimpleGUIComponent::eFilterType, \
	filterType, filterType )


//PY_TYPEOBJECT( SimpleGUIComponent )
PyTypeObject SimpleGUIComponent::s_type_ =								\
{																		\
	PyObject_HEAD_INIT(&PyType_Type)									\
	0,									/* ob_size */					\
	"SimpleGUIComponent",				/* tp_name */					\
	sizeof(SimpleGUIComponent),			/* tp_basicsize */				\
	0,									/* tp_itemsize */				\

																		\
	/* methods */														\
	SimpleGUIComponent::_tp_dealloc,	/* tp_dealloc */				\
	0,									/* tp_print */					\
	0,									/* tp_getattr */				\
	0,									/* tp_setattr */				\
	0,									/* tp_compare */				\
	SimpleGUIComponent::_tp_repr,		/* tp_repr */					\
	0,									/* tp_as_number */				\
	0,									/* tp_as_sequence */			\
	0,									/* tp_as_mapping */				\
	0,									/* tp_hash */					\
	0,									/* tp_call */					\
	0,									/* tp_str */					\
	SimpleGUIComponent::_tp_getattro,	/* tp_getattro */				\
	SimpleGUIComponent::_tp_setattro,	/* tp_setattro */				\
	0,									/* tp_as_buffer */				\
	Py_TPFLAGS_DEFAULT |
		Py_TPFLAGS_HAVE_WEAKREFS,		/* tp_flags */					\
	0,									/* tp_doc */					\
	0,									/* tp_traverse */				\
	0,									/* tp_clear */					\
	0,									/* tp_richcompare */			\
	offsetof( SimpleGUIComponent, weakreflist_)
		- (int((PyObject*)(SimpleGUIComponent*)100) - 100),	/* tp_weaklistoffset */			\
	0,									/* tp_iter */					\
	0,									/* tp_iternext */				\
	0,									/* tp_methods */				\
	0,									/* tp_members */				\
	0,									/* tp_getset */					\
	0,									/* tp_base */					\
	0,									/* tp_dict */					\
	0,									/* tp_descr_get */				\
	0,									/* tp_descr_set */				\
	0,									/* tp_dictoffset */				\
	0,									/* tp_init */					\
	0,									/* tp_alloc */					\
	(newfunc)SimpleGUIComponent::_pyNew,		/* tp_new */			\
	0,									/* tp_free */					\
	0,									/* tp_is_gc */					\
	0,									/* tp_bases */					\
	0,									/* tp_mro */					\
	0,									/* tp_cache */					\
	0,									/* tp_subclasses */				\
	0,									/* tp_weaklist */				\
	0									/* tp_del */					\
};

PY_BEGIN_METHODS( SimpleGUIComponent )
	PY_METHOD( addChild )
	PY_METHOD( delChild )
	PY_METHOD( addShader )
	PY_METHOD( delShader )
	PY_METHOD( save )
	PY_METHOD( reSort )
	PY_METHOD( handleKeyEvent )
	PY_METHOD( handleMouseEvent )
	PY_METHOD( handleAxisEvent )
	PY_METHOD( screenToLocal )
	PY_METHOD( localToScreen )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( SimpleGUIComponent )
	/*~ attribute SimpleGUIComponent.parent
	 *
	 *  Stores a weak reference to this component's parent component.
	 *  It is None if there is no parent.
	 *	
	 *  @type weakref proxy to SimpleGUIComponent
	 */
	PY_ATTRIBUTE( parent )
	/*~ attribute SimpleGUIComponent.position
	 *
	 *	The position of the SimpleGUIComponent.  This is a Vector3.  The first
	 *	two numbers are the horizontal and vertical positions, the third is the
	 *	depth sort value.
	 *
	 *  The units used by the horizontal and vertical positions depends on the
	 *  current values of the horizontalPositionMode and verticalPositionMode 
	 *  attributes. 
	 *
	 *  If the position mode is "CLIP", then the position component is defined
	 *  in clip space. Note that if the component is a child of a 
	 *  WindowGUIComponent, then the clip coordinates are relative to the 
	 *  dimensions of that parent (e.g. x = -1 would cause the child of the
	 *  window to be aligned to the left edge of the window). (-1,-1) is the 
	 *  bottom left of the screen/window, (1,1) is the top right of the screen/window. 
	 *  (0,0) is the centre of the screen/window.
	 *
	 *  If the position mode is "PIXEL", then the coordinate is defined to be
	 *  in pixels, relative to the top left of the screen (or parent window, 
	 *  if the component is the child of a WindowGUIComponent).
	 *
	 *  For backwards compatability with old behaviour, a "LEGACY" 
	 *  position mode is provided. In this mode the position is taken to be in
	 *  clip space as in "CLIP", however if it is a child of a 
	 *  WindowGUIComponent it will will NOT be relative to that window. Newly
	 *  created components are created in "LEGACY" mode by default, however
	 *  this may change in a future release.
	 *
	 *	The horizontalAnchor and verticalAnchor settings impact on the final
	 *  screen position of the component. horizontalAnchor can either be "LEFT", 
	 *  "CENTER" or "RIGHT", verticalAnchor can be either "TOP", "CENTER" or 
	 *  "BOTTOM".  The anchor attributes determine which part of the gui is 
	 *  located by the position attribute. For example, if horizontalAnchor is 
	 *  "CENTER" and position is (0,0,0) in clip space, then the component is 
	 *  horizontally centred on the screen.	If horizontalAnchor were "LEFT" 
	 *  then the left of the component would be	centred on the screen.
	 *
	 *	The depth sort value is used to sort between various GUI components
	 *	with the same parent.  The lower numbered components appear on top of
	 *	higher numbered components.  Changing the depth of one component
	 *	doesn't automatically re-order all other children at the same level of
	 *	the tree however.  The reSort() method needs to be called on the
	 *	parent, or on GUI if you've changed the position of a root component,
	 *	to force this.
	 *
	 *	Note that depth values should be kept between 0 and 1, since some video
	 *	cards will automatically clip away triangles with z-values outside this
	 *	range, even if the z-buffer is disabled.
	 *
	 *	This defaults to (0,0,1)
	 *
	 *	@type	Vector3
	 */
	PY_ATTRIBUTE( position )
	/*~ attribute SimpleGUIComponent.horizontalPositionMode
	 *
	 *	Determines the units used by position.x. See the documentation
	 *  for SimpleGUIComponent.position for details on how it is interpreted.
	 *
	 *  Possible values are "CLIP", "PIXEL", "LEGACY". Defaults to "LEGACY".
	 *
	 *  @type String
	 */
	PY_ATTRIBUTE( horizontalPositionMode )

	/*~ attribute SimpleGUIComponent.verticalPositionMode
	 *
	 *	Determines the units used by position.y. See the documentation
	 *  for SimpleGUIComponent.position for details on how it is interpreted.
	 *
	 *  Possible values are "CLIP", "PIXEL", "LEGACY". Defaults to "LEGACY".
	 *
	 *  @type String
	 */
	PY_ATTRIBUTE( verticalPositionMode )

	/*~ attribute SimpleGUIComponent.width
	 *
	 *	The width of the SimpleGUIComponent. Its interpretation depends on the
	 *	widthMode attribute. If widthMode is "PIXELS", then width is
	 *	interpreted as pixels. If widthMode is "CLIP", then it is interpreted
	 *  as clip-space coordinates so that the width of the screen or the parent
	 *  WindowGUIComponent is exactly 2.0. If widthMode is "LEGACY" then it is
	 *  interpreted similarly to "CLIP", however 2.0 is ALWAYS the width of the
	 *  screen (even if it is a child of a WindowGUIComponent).
	 *
	 *	If the tiled attribute is False, then changing the width scales width of
	 *	the texture to match.  Otherwise it just changes the width of the
	 *	component, and the texture tiles to cover the entire surface.
	 *
	 *	@type	Float
	 */
	PY_ATTRIBUTE( width )
	/*~ attribute SimpleGUIComponent.height
	 *
	 *	The height of the SimpleGUIComponent. Its interpretation depends on the
	 *	heightMode attribute. If heightMode is "PIXELS", then height is
	 *	interpreted as pixels. If heightMode is "CLIP", then it is interpreted
	 *  as clip-space coordinates so that the height of the screen or the parent
	 *  WindowGUIComponent is exactly 2.0. If widthMode is "LEGACY" then it is
	 *  interpreted similarly to "CLIP", however 2.0 is ALWAYS the width of the
	 *  screen (even if it is a child of a WindowGUIComponent).
	 *
	 *	If the tiled attribute is False, then changing the height scales height
	 *	of the texture to match.  Otherwise it just changes the height of the
	 *	component, and the texture tiles to cover the entire surface.
	 *
	 *	@type	Float
	 */
	PY_ATTRIBUTE( height )
	/*~ attribute SimpleGUIComponent.size
	 *
	 *	This Vector2 is the width and height of the SimpleGUIComponent,
	 *	contained in one attribute. Changing either of those attributes will
	 *	change this, and vice versa.  The interpretation of its two attributes
	 *	depends on widthMode and heightMode. See the documentation for width
	 *  and height for details on how the size modes affect these values.
	 *
	 *  Setting the first (width) and/or second (height) component of the 
	 *	passed Vector2 to zero has special meaning. Setting only one of them 
	 *	to zero tells the component to fill-up that zeroed attribute with a 
	 *	value that will preserve the aspect ratio of the assigned texture. 
	 *	Setting both to zero, tells the component to fill-up both width and 
	 *	height attributes with the width and height of the texture, so that 
	 *	one texel on the texture gets mapped to one pixel on the screen.	
	 *	This special semantic only exists when setting the with and height 
	 *	of a component at once, by using the size attribute. Also note that
	 *	a valid texture must be assigned to the component for the width and 
	 *	height values to be automatically adjusted.
	 *
	 *	If the tiled attribute is False, then changing the size scales size of
	 *	the texture to match.  Otherwise it just changes the size of the 
	 *	component, and the texture tiles to cover the entire surface.
	 *
	 *  @see  width
	 *  @see  height
	 *	@type Vector2
	 */
	PY_ATTRIBUTE( size )
	/*~ attribute SimpleGUIComponent.visible
	 *
	 *	This attribute determines whether or not the SimpleGUIComponent is
	 *	rendered or not.  If it is False, then it will not be rendered,
	 *	otherwise it will.  It defaults to True.  Setting visible to false
	 *	will also implicitly hide all children of the component.  If you
	 *	want to disable rendering of a parent component but still have its
	 *	children drawn, then set the textureName of the parent to "", an
	 *	empty string.  Note that Simple GUI Components constructed with
	 *	empty strings are often used as dummy nodes in a GUI tree.
	 *
	 *	@type	Boolean
	 */
	PY_ATTRIBUTE( visible )
	/*~ attribute SimpleGUIComponent.textureName
	 *
	 *	The resource ID of the texture displayed in the component.  If this is
	 *	set to a new texture resource ID (string specifying the fileName of the
	 *	texture) then the new texture will be loaded.
	 *
	 *	If it is set to an empty string then the component will be invisible,
	 *	but its children will be drawn.  Note that this is intentionally
	 *	different to setting the visible flag to false; resetting the visible
	 *	flag will mean the entire tree underneath the component will also not
	 *	draw.
	 *
	 *	Using a gui component with an empty textureName string is useful for
	 *	creating dummy nodes in the GUI hierarchy, to aid organisation.
	 *
	 *	Setting this creates a new PyTextureProvider which is assigned to the
	 *	texture attribute.
	 *
	 *	@type	String
	 */
	PY_ATTRIBUTE( textureName )
	/*~ attribute SimpleGUIComponent.tiled
	 *
	 *	If this attribute is False, then the texture will be mapped across the
	 *	entire region of the component (see the mapping attribute).  If this
	 *	attribute is True, then the texture will be mapped to an area which
	 *	is tileWidth wide, and tileHeight high, and then tiled all over the
	 *	component.  Tiling starts with the top left corner of the texture in
	 *	the top left of the component.
	 *
	 *	@see mapping
	 *	@type	Boolean
	 */
	PY_ATTRIBUTE( tiled )
	/*~ attribute SimpleGUIComponent.tileWidth
	 *
	 *	The width the texture should be scaled to, in pixels, before it is
	 *  tiled.  This attribute is only meaningful if the tiled attribute is set
	 *	to True.
	 *
	 *	@type	Integer
	 */
	PY_ATTRIBUTE( tileWidth )
	/*~ attribute SimpleGUIComponent.tileHeight
	 *
	 *	The height the texture should be scaled to, in pixels, before it is
	 *	tiled.  This attribute is only meaningful if the tiled attribute is set
	 *	to True.
	 *
	 *	@type	Integer
	 */
	PY_ATTRIBUTE( tileHeight )
	/*~ attribute SimpleGUIComponent.widthMode
	 *
	 *	This attribute determines how the width attribute is interpreted. 
	 *  Possible values are "CLIP", "PIXEL", and "LEGACY". See 
	 *  SimpleGUIComponent.width for details on the meaning of these values.
	 *
	 *  @see    width
	 *	@type	String
	 */
	PY_ATTRIBUTE( widthMode )
	/*~ attribute SimpleGUIComponent.widthRelative
	 *
	 *	This attribute determines how the width attribute is interpreted. 
	 *  This attribute has been deprecated, use widthMode instead.
	 *
	 *  Setting this to True is equivalent to setting widthMode to "LEGACY",
	 *  and setting this to False is equivalent to setting widthMode to
	 *  "PIXEL".
	 *
	 *  @see    width
	 *	@type	Boolean
	 */
	PY_ATTRIBUTE( widthRelative )
	/*~ attribute SimpleGUIComponent.heightMode
	 *
	 *	This attribute determines how the height attribute is interpreted. 
	 *  Possible values are "CLIP", "PIXEL", and "LEGACY". See 
	 *  SimpleGUIComponent.height for details on the meaning of these values.
	 *
	 *  @see    height
	 *	@type	String
	 */
	PY_ATTRIBUTE( heightMode )
	/*~ attribute SimpleGUIComponent.heightRelative
	 *
	 *	This attribute determines how the height attribute is interpreted.
	 *  This attribute has been deprecated, use heightMode instead.
	 *
	 *  Setting this to True is equivalent to setting heightMode to "LEGACY",
	 *  and setting this to False is equivalent to setting heightMode to
	 *  "PIXEL".
	 *
	 *  @see    height
	 *	@type	Boolean
	 */
	PY_ATTRIBUTE( heightRelative )
	/*~ attribute SimpleGUIComponent.colour
	 *
	 *	This attribute is the colour of the SimpleGUIComponent.  This colour is
	 *	a Vector4, with components ranging between 0 and 255.  The components
	 *	are (Red, Green, Blue, Alpha).  The numbers are scaled to the 0-1
	 *	range, and then each pixel's colour (also in the 0-1 range) is
	 *	multiplied by the colour.
	 *
	 *	Note the Australian spelling of the word colour
	 *
	 *	@type	Colour
	 */
	PY_ATTRIBUTE( colour )
	/*~ attribute SimpleGUIComponent.horizontalAnchor
	 *
	 *	This attribute specifies which part of the SimpleGUIComponent is
	 *	located by the position attribute, in a horizontal direction.
	 *	Possible values are "LEFT", "CENTER" and "RIGHT".  The default is
	 *	"CENTER".
	 *
	 *	@type	String
	 */
	PY_ATTRIBUTE( horizontalAnchor )
	/*~ attribute SimpleGUIComponent.verticalAnchor
	 *
	 *	This attribute specifies which part of the SimpleGUIComponent is
	 *	located by the position attribute, in a vertical direction.
	 *	Possibe values are "TOP", "CENTER" and "BOTTOM".  The default is
	 *	"CENTER".
	 *
	 *	@type	String
	 */
	PY_ATTRIBUTE( verticalAnchor )
	/*~ attribute SimpleGUIComponent.materialFX
	 *
	 *	This attribute determines what operation is used to render the
	 *	SimpleGUIComponent onto the background.  Possible values are "BLEND",
	 *	which applies the alpha channel transparency, "SOLID" which ignores the
	 *	alpha channel and overwrites the background, and "ADD" which just adds
	 *	the colour to the existing background colour, making the component look
	 *	like it is transparent and glowing.	The default is "ADD".
	 *	Note that ADD_SIGNED is a fudge, it basically performs a MOD2 blend
	 *	with the frame buffer.  For most situations this should be fine.
	 *
	 *	Possible values are ADD, BLEND, BLEND_COLOUR, BLEND_INVERSE_COLOUR,
	 *	SOLID, MODULATE2X, ALPHA_TEST, BLEND_INVERSE_ALPHA, BLEND2X, ADD_SIGNED
	 *
	 *	@type	String
	 */
	PY_ATTRIBUTE( materialFX )
	/*~ attribute SimpleGUIComponent.filterType
	 *
	 *	This attribute determines what texture filtering is to be to render 
	 *	the SimpleGUIComponent.  Possible values are "POINT", when the texel 
	 *	with coordinates nearest to the desired pixel value is used, and "LINEAR",
	 *	when a weighted average of a 2 x 2 area of texels surrounding the desired 
	 *	pixel is used. Default value is "LINEAR".
	 *
	 *	Possible values are POINT and LINEAR
	 *
	 *	@type	String
	 */
	PY_ATTRIBUTE( filterType )
	/*~ attribute SimpleGUIComponent.children
	 *
	 *	This attribute is the list of all children of the current component.
	 *	All children are drawn after their parent.  However, note that the
	 *	position of the children is specified relative to the screen, not
	 *	relative to their parent.  If you want children to inherit the position
	 *	of the parent, then the parent should be a Window component.
	 *
	 *	Any shaders which are part of a parent are also applied to children.
	 *
	 *	In normal usage, this list is not manipulated directly, instead the
	 *	addChild and delChild functions are called on the SimpleGUIComponent to
	 *	add and remove children.
	 *
	 *	Note there is a special python syntax implemented that helps setting
	 *	and getting children - you can set children directly as attributes.
	 *	For example :
	 *	Example:
	 *	@{
	 *	#Set / Get / Del children using special syntax
	 *	s = GUI.Simple( "gui/maps/myGui.dds" )
	 *	parent = GUI.Simple( "" )
	 *	parent.child = s
	 *	child = parent.child
	 *	parent.child = None
	 *
	 *	#Set / Get / Del children using method calls.
	 *	#This is functionally equivalent to the example above.
	 *	s = GUI.Simple( "gui/maps/myGui.dds" )
	 *	parent = GUI.Simple( "" )
	 *	parent.addChild( s, "child" )
	 *	child = parent.getChild( "child" )
	 *	parent.delChild( child )
	 *	@}
	 *
	 *	@type Read-Only List of SimpleGUIComponents.
	 */
	PY_ATTRIBUTE( children )
	/*~ attribute SimpleGUIComponent.shaders
	 *
	 *	This attribute is the list of all shaders which are applied to this
	 *	SimpleGUIComponent and to its children.  A shader is used to modify the
	 *	way in which the component is rendered.  There are several different
	 *	shader types: ColourShaders, ClipShaders, MatrixShaders.
	 *
	 *	This list is not normally manipulated directly, instead the addShader
	 *	and delShader functions are called on the SimpleGUIComponent to add or
	 *	remove shaders.  However, it is possible to perform direct manipulation
	 *	on it if you like.
	 *
	 *	Note there is a special python syntax implemented that helps setting
	 *	and getting shaders - you can set shaders directly as attributes.
	 *	For example :
	 *	Example:
	 *	@{
	 *	#Set / Get / Del shaders using special syntax
	 *	s = GUI.ClipShader()
	 *	parent = GUI.Simple( "" )
	 *	parent.shader = s
	 *	shader = parent.shader
	 *	parent.shader = None
	 *
	 *	#Set / Get / Del shaders using method calls.
	 *	#This is functionally equivalent to the example above.
	 *	s = GUI.ClipShader()
	 *	parent = GUI.Simple( "" )
	 *	parent.addShader( s, "shader" )
	 *	shader = parent.getChild( "shader" )
	 *	parent.delShader( shader )
	 *	@}
	 *
	 *	@type List of Shaders.
	 */
	PY_ATTRIBUTE( shaders )
	/*~ attribute SimpleGUIComponent.angle
	 *
	 *	This attribute determines the rotation of the component.  It only
	 *	stores the following values: 0, 90, 180, 270.  The angle is measured
	 *	in degrees, and is measured in a clockwise direction, with 0 being the
	 *	default	the initial position.  Also note that if the component is
	 *	rectangular, rotating it doesn't automatically rescale it, so the image
	 *	is likely to be distorted in a sideways rotation.
	 *
	 *	This attribute is applied after the flip attribute.
	 *
	 *	@type	Float
	 */
	PY_ATTRIBUTE( angle )
	/*~ attribute SimpleGUIComponent.flip
	 *
	 *	This attribute determines whether or not to flip the component in the
	 *	horizontal direction.  If it is zero, then no flipping occurs,
	 *	otherwise the component is flipped.  This attribute is applied before
	 *	the angle attribute.
	 *
	 *	@type	Integer
	 */
	PY_ATTRIBUTE( flip )
	/*~ attribute SimpleGUIComponent.mapping
	 *
	 *	This attribute governs the mapping between  the corners of the
	 *	component, and the texture.  It consists of 4-tuple of Vector2s.  The
	 *	first component is the top left, second is the bottom left, third is
	 *	the bottom right and the fourth is the top right.
	 *
	 *	Each Vector2 specifies the texture address to use for that corner,
	 *	with (0,0) being topleft, and (1,1) being bottom right.  It is
	 *	acceptable to use numbers outside of 0-1.  In this case texture is not
	 *	tiled.  The component is interpolated between these 4 points.
	 *
	 *	Setting the flip or angle attributes sets the mapping attribute
	 *	appropriately.  However, setting mapping doesn't reset the flip and
	 *	angle attributes.
	 *
	 *	@type	4-tuple of Vector2s.
	 */
	PY_ATTRIBUTE( mapping )
	/*~ attribute SimpleGUIComponent.focus
	 *
	 *	This attribute determines whether or not this SimpleGUIComponent
	 *	responds to input events.  If focus is False, then no response
	 *	will be made to the input events, and the event handling functions
	 *	in the script object will not be called.  If it is True,
	 *	it will respond to events, and event handling functions in the script
	 *	object will be called.
	 *
	 *	When this attribute is set or cleared, the focus method on the attached
	 *	script object (see the script attribute) gets called, with
	 *	one argument, which is the new value of the focus attribute.
	 *
	 *	@see script
	 *	@type	Boolean
	 *
	 *	Related events:
	 *
	 *	@{
	 *		SimpleGUIComponent.handleKeyEvent
	 *		SimpleGUIComponent.handleMouseButtonEvent
	 *		SimpleGUIComponent.handleMouseClickEvent
	 *		SimpleGUIComponent.handleAxisEvent
	 *	@}
	 */
	PY_ATTRIBUTE( focus )
	/*~ attribute SimpleGUIComponent.crossFocus
	 *
	 *	This attribute determines whether or not this SimpleGUIComponent
	 *	will receive mouse cross (enter and leave) events. 
	 *
	 *	Enter events are generated when the mouse cursor enters the quad 
	 *	defined by the four corners of the component. Leave events are 
	 *	generated when the mouse cursor leaves this same quad.
	 *
	 *	If focus is False, the mouse cross event handling functions in the 
	 *	attached script object will not be called. If it is True, the 
	 *	mouse cross event handling functions in the attached script object 
	 *	will be called.
	 *
	 *	When this attribute is set or cleared, the crossFocus method on the 
	 *	attached script object (see the script attribute) gets called, with
	 *	one argument, which is the new value of the focus attribute.
	 *
	 *	@type	Boolean
	 *
	 *	Related events:
	 *
	 *	@{
	 *		SimpleGUIComponent.handleMouseEnterEvent
	 *		SimpleGUIComponent.handleMouseLeaveEvent
	 *	@}
	 */
	PY_ATTRIBUTE( crossFocus )
	/*~ attribute SimpleGUIComponent.moveFocus
	 *
	 *	This attribute determines whether or not this SimpleGUIComponent
	 *	will receive mouse move events. 
	 *
	 *	Mouse move events are generated when the mouse cursor moves inside
	 *	the quad defined by the four corners of the component. 
	 *
	 *	If focus is False, the mouse move event handling functions in the 
	 *	attached script object will not be called. If it is True, the 
	 *	mouse move event handling functions in the attached script object 
	 *	will be called.
	 *
	 *	When this attribute is set or cleared, the moveFocus method on the 
	 *	attached script object (see the script attribute) gets called, with
	 *	one argument, which is the new value of the focus attribute.
	 *
	 *	@see script
	 *	@type	Boolean
	 *
	 *	Related events:
	 *
	 *	@{
	 *		SimpleGUIComponent.handleMouseEvent
	 *	@}
	 */
	PY_ATTRIBUTE( moveFocus )
	/*~ attribute SimpleGUIComponent.dragFocus
	 *
	 *	This attribute determines whether or not this SimpleGUIComponent
	 *	is a candidate for dragging. When dragged, components will receive
	 *	drag start and grad stop events.
	 *
	 *	Drag start events are generated when the dragging operation is
	 *	first detected (see GUI.setDragDistance). Drag stop events are
	 *	generated when the mouse left button is release during the drag
	 *	operation. 
	 *
	 *	If focus is False, the component will not be available for dragging, 
	 *	and the drag event handling functions in the attached script object 
	 *	will not be called. If it is True, the component will be available 
	 *	for dragging, and the drag event handling functions in the attached 
	 *	script object will be called. 
	 *
	 *	When this attribute is set or cleared, the dragFocus method on the 
	 *	attached script object (see the script attribute) gets called, with
	 *	one argument, which is the new value of the focus attribute.
	 *
	 *	@see script
	 *	@type	Boolean
	 *
	 *	Related events:
	 *
	 *	@{
	 *		SimpleGUIComponent.handleDragStartEvent
	 *		SimpleGUIComponent.handleDragStopEvent
	 *	@}
	 */
	PY_ATTRIBUTE( dragFocus )
	/*~ attribute SimpleGUIComponent.dropFocus
	 *
	 *	This attribute determines whether or not this SimpleGUIComponent
	 *	can have a dragged component dropped over it. When receiving dragged 
	 *	component, drop target components will receive drag enter, drag 
	 *  leave and drop events.
	 *
	 *	Drag enter events are generated when a dragged component enters
	 *  the quad defined by the four corners of a component that has dropFocus
	 *	enabled. Drag leave events are generated when a dragged component 
	 *	leaves this same quad. Drop events are generated when a dragged 
	 *	component is dropped over a drop accepting component.
	 *
	 *	The value returned by the drag enter event handler method will be
	 *	used to determine if the drop target will accept the dragged 
	 *	component. That is, the drop event will not be generated when the
	 *	left mouse button is release if the earlier enter handler returned 
	 *	false (but the drag stop will always be. See dragFocus).
	 *
	 *	If focus is False, the component will not accept dropping, and the 
	 *	drag enter/leav and drop event handling functions in the attached 
	 *	script object will not be called. If it is True, the component 
	 *	will accept dropping and the drag enter/leave and drop event handling 
	 *	functions in the attached script object will be called. 
	 *
	 *	When this attribute is set or cleared, the dropFocus method on the 
	 *	attached script object (see the script attribute) gets called, with
	 *	one argument, which is the new value of the focus attribute.
	 *
	 *  @see dragFocus
	 *  @see script
	 *
	 *	@type	Boolean
	 *
	 *	Related events:
	 *
	 *	@{
	 *		SimpleGUIComponent.handleDragEnterEvent
	 *		SimpleGUIComponent.handleDragLeaveEvent
	 *		SimpleGUIComponent.handleDropEvent
	 *	@}
	 */
	PY_ATTRIBUTE( dropFocus )
	/*~ attribute SimpleGUIComponent.script
	 *
	 *	This attribute associates a class with the SimpleGUIComponent.  This can
	 *	contain functions for handling input events.  It should define the
	 *	following functions, even if simply to stub them out:
	 *
	 *	@{
	 *		def handleKeyEvent( self, isDown, key, modifiers ):
	 *		def handleAxisEvent( self, axis, value, dTime ):
	 *		def handleMouseButtonEvent( self, comp, key, down, modifiers, pos ):
	 *		def handleMouseClickEvent( self, comp, pos ):
	 *		def handleMouseEnterEvent( self, comp, pos ):
	 *		def handleMouseLeaveEvent( self, comp, pos ):
	 *		def handleMouseEvent( self, comp, pos ):
	 *		def handleDragStartEvent( self, comp, pos ):
	 *		def handleDragStopEvent( self, comp, pos ):
	 *		def handleDragEnterEvent( self, comp, pos ):
	 *		def handleDragLeaveEvent( self, comp, pos ):
	 *		def handleDropEvent( self, comp, pos, dropped ):
	 *		def focus( self, state ):
	 *		def crossFocus( self, state ):
	 *		def moveFocus( self, state ):
	 *		def dragFocus( self, state ):
	 *		def dropFocus( self, state ):
	 *		def onLoad( self, dataSection ):
	 *		def onSave( self, dataSection ):
	 *		def onBound( self ):
	 *	@}
	 *
	 *	The handle event functions are called whenever the
	 *	SimpleGUIComponent receives the corresponding event. The focus methods
	 *	are called whenever the SimpleGUIComponents focus attributes are
	 *	assigned to.
	 *
	 *	onLoad is called just after the basic attributes have been loaded, but
	 *	before any children are attached or shaders specified.
	 *
	 *	onSave is called just after the basic attributes have been saved.
	 *
	 *	onBound is called after loading and all children and shaders are bound.
	 *
	 *	If a script is associated with a gui component, then that class must
	 *	have a class member "factoryString" describing how to create it.
	 *
	 *	The factoryString which is stored in your .gui file is read in at load
	 *	time and executed using "Script::runString".
	 *	This means that the name of the class to construct must be imported
	 *	into the global namespace. As an example, in FantasyDemo we put all our
	 *	python gui scripts into the "PyGUI" module, and make sure the PyGUI
	 *	module is imported into the global namespace in the personality script
	 *	via the following code :
	 *	@{
	 *	from Helpers import PyGUI
	 *	__import__('__main__').PyGUI = PyGUI
	 *	@}
	 *
	 *	Please see the FantasyDemo personality script for an example of this.
	 *	Please see the script fantasydemo/res/client/Helpers/PyGUI.py for
	 *	examples of gui python scripts.
	 *	Please see the gui files in fantasdemo/res/gui for examples of gui
	 *	files containing these factory strings.
	 *	
	 *
	 *	@type	Object
	 */
	PY_ATTRIBUTE( script )
	/*~ attribute SimpleGUIComponent.texture
	 *
	 *	This attribute allows any PyTextureProvider object to be assigned to
	 *	the component.  This allows	an object which dynamically updates its
	 *	texture, for example a PyModelRenderer to supply a texture to the
	 *	component.
	 *
	 *	The other way of setting a PyTextureProvider is to set the textureName
	 *	attribute, which creates a new PyTextureProvider which exposes
	 *	the named texture.
	 *
	 *	This attribute may be None, if the component has an empty texture
	 *	name string.
	 *
	 *	@type	PyTextureProvider
	 */
	PY_ATTRIBUTE( texture )
	/*~ attribute SimpleGUIComponent.pixelSnap
	 *
	 *	Toggles on/off pixel snapping. Pixel snapping will, at the vertex shader
	 *	level, cause the component to the drawn on the nearest pixel boundary. This
	 *	is useful to avoid filtering issues when trying to achieve 1-1 mapping between
	 *	GUI artwork and the screen.
	 *
	 *	@type	Boolean
	 */
	PY_ATTRIBUTE( pixelSnap )
PY_END_ATTRIBUTES()

/*~ function GUI.Simple
 *
 *	This function creates a SimpleGUIComponent, which renders the specified
 *	texture in the centre of the screen.  It can be customized once it has
 *	been created.
 *
 *	Example:
 *	@{
 *	myGui = GUI.Simple( "gui/maps/myGui.dds" )
 *	@}
 *
 *	@param	textureName	the string resourceId of the texture to display in the
 *	gui component.
 *
 *	@return				the SimpleGUIComponent that got created
 */
PY_FACTORY_NAMED( SimpleGUIComponent, "Simple", GUI )

PY_ENUM_MAP( SimpleGUIComponent::eHAnchor );
PY_ENUM_MAP( SimpleGUIComponent::eVAnchor );

PY_ENUM_CONVERTERS_CONTIGUOUS( SimpleGUIComponent::eHAnchor );
PY_ENUM_CONVERTERS_CONTIGUOUS( SimpleGUIComponent::eVAnchor );

PY_ENUM_MAP( SimpleGUIComponent::ePositionMode );
PY_ENUM_CONVERTERS_CONTIGUOUS( SimpleGUIComponent::ePositionMode );

PY_ENUM_MAP( SimpleGUIComponent::eSizeMode );
PY_ENUM_CONVERTERS_CONTIGUOUS( SimpleGUIComponent::eSizeMode );

PY_ENUM_MAP( SimpleGUIComponent::eMaterialFX )
PY_ENUM_CONVERTERS_CONTIGUOUS( SimpleGUIComponent::eMaterialFX )

PY_ENUM_MAP( SimpleGUIComponent::eFilterType )
PY_ENUM_CONVERTERS_CONTIGUOUS( SimpleGUIComponent::eFilterType )

PY_MODULE_STATIC_METHOD( SimpleGUIComponent, load, GUI )

PY_SCRIPT_CONVERTERS( SimpleGUIComponent )

/// declare our C++ factory method
COMPONENT_FACTORY( SimpleGUIComponent )


/// static factory map initialiser
template <> GUIComponentFactory::ObjectMap * GUIComponentFactory::pMap_ = NULL;

static int counter = 0;
static int id = 0;


/**
 *	Constructor
 */
SimpleGUIComponent::SimpleGUIComponent( const std::string& name,
									   PyTypePlus * pType )
:PyObjectPlus( pType ),
 texture_( NULL ),
 position_( 0.f, 0.f, 1.f ),
 horizontalPositionMode_( POSITION_MODE_LEGACY ),	// Default to old behaviour, at least for now.
 verticalPositionMode_( POSITION_MODE_LEGACY ),
 width_( 0.5f ),
 widthMode_( SIZE_MODE_LEGACY ),
 height_( 0.5f ),
 heightMode_( SIZE_MODE_LEGACY ),
 colour_( 0xffffffff ),
 runTimeColour_( 0xffffffff ),
 runTimeTransform_( Matrix::identity ),
 visible_( true ),
 momentarilyInvisible_( false ),
 horizontalAnchor_( ANCHOR_H_CENTER ),
 verticalAnchor_( ANCHOR_V_CENTER ),
 nVertices_( 0 ),
 nIndices_( 0 ),
 vertices_( 0 ),
 indices_( 0 ),
 blueprint_( 0 ),
 cachedAngle_( ROT_0 ),
 flip_( NO_FLIP ),
 materialFX_( FX_ADD ),
 filterType_( FT_POINT ),
 tiled_( false ),
 tileWidth_( 16 ),
 tileHeight_( 16 ),
 focus_( false ),
 moveFocus_( false ),
 crossFocus_( false ),
 dragFocus_( false ),
 dropFocus_( false ),
 drawOrder_( 0 ),
 nextDrawOrder_( 0 ),
 weakreflist_( NULL ),
 pScriptObject_( NULL ),
 pMouseOverChild_( NULL ),
 material_( NULL ),
 pixelSnap_( true )
{
#if ENABLE_DPRINTF && TRACK_LEAKS
	static int s_guiId = 1;
	if (s_guiId == s_breakOnAllocId)
	{
		MF_ASSERT_DEV(("Breaking on GUI Id as requested", false));
	}
	s_instanceMap[int(this)] = std::make_pair(name, s_guiId);
	++s_guiId;
#endif

	buildMesh();
	buildMaterial();

	textureName( name );
}


/*~ callback SimpleGUIComponent.onDelete
 *
 *	This method is called when the SimpleGUIComponent is deleted,
 *	if the component has an associated script object.  It has no
 *	parameters passed in, and expects no return value.
 */
/**
 *	Destructor
 */
SimpleGUIComponent::~SimpleGUIComponent()
{
#if ENABLE_DPRINTF && TRACK_LEAKS
	IntStringIntMap::iterator instance = s_instanceMap.find(int(this));
	MF_ASSERT_DEV(instance != s_instanceMap.end());
	if( instance != s_instanceMap.end() )
		s_instanceMap.erase(instance);
#endif

	if (SimpleGUI::pInstance() != NULL)
	{
		this->focus( false );
		this->crossFocus( false );
		this->moveFocus( false );
		this->dragFocus( false );
		this->dropFocus( false );
	}
	else
	{
		WARNING_MSG( "SimpleGUIComponent: Destroying GUI component before "
			"construction or after destruction of SimpleGUI, possible python "
			"leak.\n" );
	}

	if (weakreflist_ != NULL)
		PyObject_ClearWeakRefs(this);

	if( pScriptObject_ )
	{
		Script::call(
			PyObject_GetAttrString( pScriptObject_.getObject(), "onDelete" ),
			Py_BuildValue( "()" ),
			"SimpleGUIComponent::onDelete",
			true );

	}
	pScriptObject_ = NULL;

	cleanMesh();
}


/**
 *	This function builds the four vertices of a simple gui component mesh
 */
void SimpleGUIComponent::buildMesh( void )
{
	cleanMesh();

	nVertices_ = 4;
	nIndices_ = 6;

	blueprint_ = new GUIVertex[nVertices_];
	vertices_ = new GUIVertex[nVertices_];
	indices_ = new uint16[nIndices_];

	GUIVertex* v;

	v = &blueprint_[0];
	v->colour_ = 0xffffffff;
	v->pos_.x = -1.f;	v->pos_.y = -1.f;	v->pos_.z = 0.f;
	v->uv_[0] = 0.f;	v->uv_[1] = 0.f;

	v = &blueprint_[1];
	v->colour_ = 0xffffffff;
	v->pos_.x = -1.f;	v->pos_.y = 1.f;	v->pos_.z = 0.f;
	v->uv_[0] = 0.f;	v->uv_[1] = 1.f;

	v = &blueprint_[2];
	v->colour_ = 0xffffffff;
	v->pos_.x = 1.f;	v->pos_.y = 1.f;	v->pos_.z = 0.f;
	v->uv_[0] = 1.f;	v->uv_[1] = 1.f;

	v = &blueprint_[3];
	v->colour_ = 0xffffffff;
	v->pos_.x = 1.f;	v->pos_.y = -1.f;	v->pos_.z = 0.f;
	v->uv_[0] = 1.f;	v->uv_[1] = 0.f;

	indices_[0] = 0;
	indices_[1] = 2;
	indices_[2] = 1;
	indices_[3] = 0;
	indices_[4] = 3;
	indices_[5] = 2;
}


/**
 *	This method clears the mesh
 */
void SimpleGUIComponent::cleanMesh( void )
{
	if ( blueprint_ )
	{
		delete[] blueprint_;
		blueprint_ = 0;
	}

	if ( vertices_ )
	{
		delete[] vertices_;
		vertices_ = 0;
	}

	if ( indices_ )
	{
		delete[] indices_;
		indices_ = 0;
	}

	nVertices_ = 0;
	nIndices_ = 0;
}


/**
 *	This method builds the material based on the texture selected
 *	and any material effects
 */
bool SimpleGUIComponent::buildMaterial( void )
{
	bool ret = true;

	MF_ASSERT_DEV( ("SimpleGUIComponent: Ashes not initialised", 
					effectSection().exists()) );

	if ( !material_ )
	{
		material_ = new Moo::EffectMaterial();
		material_->load( effectSection() );
	}

	if ( !s_techniqueTable.size() )
	{
		ret = setupTechniqueTable(material_);
	}

	if ( ret )
	{
		ret = material_->hTechnique( s_techniqueTable[(materialFX_-FX_ADD)] );
	}

	return ret;
}


/**
 *	Add a child
 */
void SimpleGUIComponent::addChild( const std::string & name,
	SimpleGUIComponent * child )
{
	if (child->parent_.exists())
	{
		// already assigned to a parent.		
		WARNING_MSG( "SimpleGUIComponent::addChild - \
			attempted to add a component that already has a parent.\n" );
		return;
	}

	children_[ name ] = child;
	child->parent_ = this;

	reSort();
}

/**
 *	Remove a child by component pointer
 */
void SimpleGUIComponent::removeChild( SimpleGUIComponent * child )
{
	ChildRecVector::iterator it;
	for (it = children_.begin(); it != children_.end(); it++)
	{
		if (it->second.getObject() == child) break;
	}

	if (it != children_.end())
	{
		it->second->parent_ = NULL;
		children_.erase( it );
		reSort();
	}
}

/**
 *	Remove a child by name
 */
void SimpleGUIComponent::removeChild( const std::string & name )
{
	bool erased = children_.erase( name );

	if (erased)
	{
		reSort();
	}
}


/**
 *	Retrieve a child by name
 */
SmartPointer<SimpleGUIComponent> SimpleGUIComponent::child( const std::string& name )
{
	ChildRecVector::iterator pChild = children_.find( name );
	if ( pChild != children_.end() )
		return pChild->second;

	return NULL;
}


/**
 *	This method is a helper for reSort
 */
int SimpleGUIComponent::DepthCompare::operator()(
	const int & arg1, const int & arg2 )
{
	SimpleGUIComponentPtr e1 = (crv_.begin() + arg1)->second;
	SimpleGUIComponentPtr e2 = (crv_.begin() + arg2)->second;

	return (e1->position().z > e2->position().z);
}


/**
 *	This method re-sorts the child components of this component and
 *  recalculates the drawOrder as neccessary.
 */
void SimpleGUIComponent::reSort()
{
	reSortChildren();
	calcDrawOrder();
}


/**
 *	This method re-sorts the child components of this component and
 *  its descendants. It does not recalculate the drawOrder.
 */
void SimpleGUIComponent::reSortRecursively()
{
	reSortChildren();
	for( ChildRecVector::const_iterator i = children_.begin();
		i != children_.end(); ++i )
	{
		(*i).second->reSortRecursively();
	}
}


/**
 *	This method re-sorts the child components of this component.
 */
void SimpleGUIComponent::reSortChildren()
{
	if (childOrder_.size() != children_.size())
	{
		childOrder_.resize( children_.size() );
		for (uint i=0; i < children_.size(); i++) childOrder_[i] = i;
	}

	DepthCompare dc( children_ );
	std::sort( childOrder_.begin(), childOrder_.end(), dc );
}


/**
 *	This method finds out if a component is in the children hierarchy.
 *
 *	@param child	component to search for in the hierarchy
 *
 *	@return true if "child" is in the hierarchy, false otherwise.
 */
bool SimpleGUIComponent::isParentOf( const SimpleGUIComponent* child  ) const
{
	for( ChildRecVector::const_iterator i = children_.begin();
		i != children_.end(); ++i )
	{
		if ( (*i).second == child || (*i).second->isParentOf( child ) )
			return true;
	}
	return false;
}


/**
 *	Obtain a list recursive list of all children of the current GuiComponent.
 *
 *	@param returnList	The list to populate with the child components.
 */
void SimpleGUIComponent::children( std::set< SimpleGUIComponent * >& returnList ) const
{
	for( ChildRecVector::const_iterator i = children_.begin();
		i != children_.end(); ++i )
	{
		returnList.insert( (*i).second.getObject() );
		(*i).second->children( returnList );
	}
}


/**
 *	This method associates a script with this component.
 *
 *	@param classObject	A Python class.
 *
 *	@return true if the script is now associated with the component.
 */
/*
bool SimpleGUIComponent::scriptObject( PyObject * classObject )
{
	if (classObject != NULL && PyClass_Check( classObject ))
	{
		PyObject * noArgs = PyTuple_New(0);
		pScriptObject_ = PyObject_CallObject( classObject, noArgs );
		Py_DECREF( noArgs );

		if (pScriptObject_ == NULL)
		{
			WARNING_MSG( "SimpleGUIComponent::scriptObject - No class object\n" );
			// exception already set
			return false;
		}
	}
	else
	{
		WARNING_MSG( "SimpleGUIComponent::init - Invalid class object\n" );
		PyErr_SetString( PyExc_TypeError,
			"SimpleGUIComponent::scriptObject expects a class object" );
		return false;
	}

	return true;
}
*/


/**
 *	Add this shader to the shader list
 */
void SimpleGUIComponent::addShader( const std::string & name, GUIShader * shader )
{
	shaders_[ name ] = shader;
}


/**
 *	Retrieve a shader by name
 */
GUIShaderPtr SimpleGUIComponent::shader( const std::string& name )
{
	return shaders_[name];
}


/**
 *	Remove a shader by shader pointer
 */
void SimpleGUIComponent::removeShader( GUIShader* shader )
{
	GUIShaderPtrVector::iterator it;
	for (it = shaders_.begin(); it != shaders_.end(); it++)
	{
		if (it->second.getObject() == shader) break;
	}

	if (it != shaders_.end())
	{
		shaders_.erase( it );
	}
}

/**
 *	Remove a shader by name
 */
void SimpleGUIComponent::removeShader( const std::string & name )
{
	shaders_.erase( name );
}



/**
 *	Get a pointer to the vertices for this component (plus their count)
 */
GUIVertex* SimpleGUIComponent::vertices( int* numVertices )
{
	if ( numVertices )
		*numVertices = nVertices_;

	return vertices_;
}


/**
 *	Get an attribute for python
 */
PyObject * SimpleGUIComponent::pyGetAttribute( const char * attr )
{
	// try our normal attributes
	PY_GETATTR_STD();

	// try one of the child names
	ChildRecVector::iterator cit = children_.find( attr );
	if (cit != children_.end())
	{
		PyObject * ret = cit->second.getObject();
		Py_INCREF( ret );
		return ret;
	}

	// try one of the shader names
	GUIShaderPtrVector::iterator sit = shaders_.find( attr );
	if (sit != shaders_.end())
	{
		PyObject * ret = sit->second.getObject();
		Py_INCREF( ret );
		return ret;
	}

	// ask our base class
	return PyObjectPlus::pyGetAttribute( attr );
}


/**
 *	Set an attribute for python
 */
int SimpleGUIComponent::pySetAttribute( const char * attr, PyObject * value )
{
	// try our normal attributes
	PY_SETATTR_STD();

	// see if it's a component
	if (PyWeakref_CheckProxy( value ))
	{
		value = (PyObject*) PyWeakref_GET_OBJECT( value );
	}
	if (SimpleGUIComponent::Check( value ) && attr[0])
	{
		// make sure there isn't a shader by this name
		GUIShaderPtrVector::iterator sit = shaders_.find( attr );
		if (sit != shaders_.end())
		{
			PyErr_Format( PyExc_NameError,
				"SimpleGUIComponent cannot add child named '%s' "
				"because it already has a shader by that name", attr );
			return -1;
		}

		// ok, add it then
		this->addChild( attr, static_cast<SimpleGUIComponent*>( value ) );
		return 0;
	}

	// see if it's a shader
	if (GUIShader::Check( value ) && attr[0])
	{
		// make sure there isn't a child by this name
		ChildRecVector::iterator cit = children_.find( attr );
		if (cit != children_.end())
		{
			PyErr_Format( PyExc_NameError,
				"SimpleGUIComponent cannot add shader named '%s' "
				"because it already has a child by that name", attr );
			return -1;
		}

		// ok, add it then
		this->addShader( attr, static_cast<GUIShader*>( value ) );
		return 0;
	}

	// see if it's None and an existing child or shader
	if (value == Py_None)
	{
		ChildRecVector::iterator cit = children_.find( attr );
		if (cit != children_.end())
		{
			this->removeChild( attr );
			return 0;
		}

		GUIShaderPtrVector::iterator sit = shaders_.find( attr );
		if (sit != shaders_.end())
		{
			this->removeShader( attr );
			return 0;
		}
	}

	// ask our base class
	return PyObjectPlus::pySetAttribute( attr, value );
}


/**
 *	Sets component size.
 */
void SimpleGUIComponent::size( const Vector2 & size )
{
	eSizeMode widthMode = this->widthMode();
	eSizeMode heightMode = this->heightMode();

	float width = size.x;
	float height = size.y;

	if (texture_ && texture_->pTexture())
	{
		float ratio = float(texture_->width()) / texture_->height();
		if (size.x == 0 && size.y == 0)
		{
			this->widthMode( SIZE_MODE_PIXEL );
			this->heightMode( SIZE_MODE_PIXEL );
			width = float(texture_->width());
			height = float(texture_->height());
		}
		else if (size.x == 0)
		{
			width = size.y * ratio;
		}
		else if (size.y == 0)
		{
			height = size.x / ratio;
		}
	}

	this->width( width );
	this->height( height );

	this->widthMode( widthMode );
	this->heightMode( heightMode );
}


/**
 *	Special get method for the colour attribute
 */
PyObject * SimpleGUIComponent::pyGet_colour()
{
	return new PyGUIComponentColour( this );
}


/**
 *	Special set method for the colour attribute
 */
int SimpleGUIComponent::pySet_colour( PyObject * value )
{
	Vector4		vColour;
	int ret = Script::setData( value, vColour, "colour" );
	if (!ret)
	{
		this->colour( Colour::getUint32( vColour ) );
	}
	return ret;
}


/**
 *	Special get method for the children attribute
 */
PyObject * SimpleGUIComponent::pyGet_children()
{
	PyObject * pList = PyList_New( children_.size() );

	ChildRecVector::iterator it;
	for (it = children_.begin(); it != children_.end(); it++)
	{
		PyObject * pTuple = PyTuple_New( 2 );

		PyTuple_SetItem( pTuple, 0, PyString_FromString( it->first ) );
		PyObject * pChild = it->second.getObject();
		Py_INCREF( pChild );
		PyTuple_SetItem( pTuple, 1, pChild );

		PyList_SetItem( pList, it - children_.begin(), pTuple );
	}

	return pList;
}


/**
 *	Special get method for the shaders attribute
 */
PyObject * SimpleGUIComponent::pyGet_shaders()
{
	PyObject * pList = PyList_New( shaders_.size() );

	GUIShaderPtrVector::iterator it;
	for (it = shaders_.begin(); it != shaders_.end(); it++)
	{
		PyObject * pTuple = PyTuple_New( 2 );

		PyTuple_SetItem( pTuple, 0, PyString_FromString( it->first ) );
		PyObject * pChild = it->second.getObject();
		Py_INCREF( pChild );
		PyTuple_SetItem( pTuple, 1, pChild );

		PyList_SetItem( pList, it - shaders_.begin(), pTuple );
	}

	return pList;
}


/**
 *	Special get method for the angle attribute
 */
PyObject * SimpleGUIComponent::pyGet_angle()
{
	if ( this->angle() <= ROT_270 )
		return Script::getData( this->angle() * 90.f );
	else
		return Script::getData( 0.f );
}


/**
 *	Special set method for the angle attribute
 */
int SimpleGUIComponent::pySet_angle( PyObject * value )
{
	float rot;

	int ret = Script::setData( value, rot, "angle" );
	if (!ret)
	{		// 45 degress added so we can truncate
		Angle	radRot( (rot+45.f) * float(MATH_PI / 180.f) );
		float	posRot = radRot;
		if (posRot < 0) posRot += float(MATH_PI*2.0);

		int	enumRot = int( posRot / double(MATH_PI/2.0) );
		if (enumRot < 0) enumRot = 0;
		if (enumRot > 3) enumRot = 3;

		// I get the feeling it would have been easier to
		// actually rotate it properly!

		this->angle( (eRotation)enumRot );
	}
	return ret;
}

/**
 *	Special get method for the mapping attribute
 */
PyObject * SimpleGUIComponent::pyGet_mapping()
{
	PyObject * pTuple = PyTuple_New( 4 );

	for (uint i=0; i < 4; i++)
	{
		PyTuple_SetItem( pTuple, i, Script::getData( blueprint_[i].uv_ ) );
	}

	return pTuple;
}


/**
 *	Special set method for the mapping attribute
 */
int SimpleGUIComponent::pySet_mapping( PyObject * value )
{
	// make sure it's a tuple of 4 elements
	if (!PyTuple_Check( value ) || PyTuple_Size( value ) != 4)
	{
		PyErr_SetString( PyExc_TypeError,
			"mapping must be set to a tuple of four pairs" );
		return -1;
	}

	// pull them out, aborting if there's an error
	Vector2	vex[4];
	for (uint i=0; i < 4; i++)
	{
		if (Script::setData( PyTuple_GetItem( value, i ),
			vex[i], "mapping.coord" ) != 0)
		{
			return -1;
		}
	}

	// ok, set away then
	this->mapping( vex );
	return 0;
}


/**
 *	Static python factory method
 */
PyObject * SimpleGUIComponent::pyNew( PyObject * args )
{
	char * textureName;
	if (!PyArg_ParseTuple( args, "s", &textureName ))
	{
		PyErr_SetString( PyExc_TypeError, "GUI.Simple: "
			"Argument parsing error: Expected a texture name" );
		return NULL;
	}

	return new SimpleGUIComponent( textureName );
}

/*~ function SimpleGUIComponent.addChild
 *
 *	This function adds a GUI Component as a child of the current SimpleGUIComponent.  This child will
 *	depth sort above its new parent, and sort with its siblings according to the depth component of
 *	its position attribute.  The other components of its position are relative to the screen, rather than its
 *	parent SimpleGUIComponent.  Additionally, any shaders applied to the parent also get applied to the child.
 *
 *	If a name is not supplied when adding it, then a random name is generated.  The child can be addressed
 *	by the name as a property of parent.  For example
 *
 *	@{
 *	>>>	import GUI
 *	>>>	parent = GUI.Simple( "parent.dds" )
 *	>>>	child = GUI.Simple( "child.dds" )
 *	>>>	parent.addChild( child, "myChild" )
 *	>>>	sameChild = parent.myChild
 *	@}
 *
 *	At this point, sameChild and child refer to the same object

 *	@param	child	the GUI component to be added as a child
 *	@param	name	(optional) the name to insert the child under
 */
/**
 *	Add a child for python
 */
PyObject * SimpleGUIComponent::py_addChild( PyObject * args )
{
	char noName[32];

	PyObject	* pComponent;
	char * name = noName;

	// parse args
	if (!PyArg_ParseTuple( args, "O|s", &pComponent, &name ))
	{
		PyErr_SetString( PyExc_TypeError, "SimpleGUIComponent.addChild() "
			"expects a GUI component and optionally a name" );
		return NULL;
	}

	if (PyWeakref_CheckProxy( pComponent))
	{
		pComponent = (PyObject*) PyWeakref_GET_OBJECT( pComponent );
	}
	if (!SimpleGUIComponent::Check( pComponent ))
	{
		PyErr_SetString( PyExc_TypeError, "SimpleGUIComponent.addChild() "
			"expects a GUI component and optionally a name" );
		return NULL;
	}

	// make up a name if none was set
	if (name == noName || !name[0])
	{
		name = noName;
		bw_snprintf( noName, sizeof( noName ), "C%08X", (int)pComponent );
	}

	this->addChild( name, static_cast<SimpleGUIComponent*>( pComponent ) );

	Py_Return;
}

/*~ function SimpleGUIComponent.delChild
 *
 *	This function removes a child from the SimpleGUIComponent.
 *
 *	@param	child	The GUI Component to remove
 */
/**
 *	Del a child for python
 */
PyObject * SimpleGUIComponent::py_delChild( PyObject * args )
{
	if (PyTuple_Size( args ) == 1)
	{
		PyObject * pItem = PyTuple_GetItem( args, 0 );
		if (PyWeakref_CheckProxy( pItem))
		{
			pItem = (PyObject*) PyWeakref_GET_OBJECT( pItem );
		}
		if (SimpleGUIComponent::Check( pItem ))
		{
			this->removeChild( static_cast<SimpleGUIComponent*>( pItem ) );
			Py_Return;
		}
		if (PyString_Check( pItem ))
		{
			this->removeChild( PyString_AsString( pItem ) );
			Py_Return;
		}
	}

	PyErr_SetString( PyExc_TypeError, "SimpleGUIComponent.py_delChild "
		"expects a GUI component or a string" );
	return NULL;
}

/*~ function SimpleGUIComponent.addShader
 *
 *	This function adds a GUIShader to the current SimpleGUIComponent.  Shaders 
 *	can be used to do things such as change the color and alpha of a component, 
 *	or clip it, or transform it.
 *
 *	If the optional name is not supplied when adding it, then a random name is 
 *	generated.  In addition to appearing in the shaders attribute, this shader 
 *	will also be available using the name as an attribute of the component. 
 *
 *	For example:
 *
 *	@{
 *	>>>	import GUI
 *	>>>	component = GUI.Simple( "component.dds" )
 *	>>>	shader = GUI.ColourShader()
 *	>>>	component.addShader( shader, "myShader" )
 *	>>>	sameShader = component.myShader
 *	@}
 *
 *	In this code, shader and myShader will refer to the same object
 *
 *	@param	shader	The shader to add
 *	@param	name	(optional) The string giving the name to add it under.
 */
/**
 *	Add a shader for python
 */
PyObject * SimpleGUIComponent::py_addShader( PyObject * args )
{
	char noName[32];

	PyObject	* pShader;
	char * name = noName;

	// parse args
	if (!PyArg_ParseTuple( args, "O|s", &pShader, &name ) ||
		!GUIShader::Check( pShader ))
	{
		PyErr_SetString( PyExc_TypeError, "SimpleGUIComponent.addShader() "
			"expects a GUI shader and optionally a name" );
		return NULL;
	}

	// make up a name if none was set
	if (name == noName || !name[0])
	{
		name = noName;
		bw_snprintf( noName, sizeof( noName ), "S%08X", (int)pShader );
	}

	this->addShader( name, static_cast<GUIShader*>( pShader ) );

	Py_Return;
}

/*~ function SimpleGUIComponent.delShader
 *
 *	This function removes a shader from the SimpleGUIComponent.
 *
 *	@param	shader	A shader object to remove
 */
/**
 *	Del a shader for python
 */
PyObject * SimpleGUIComponent::py_delShader( PyObject * args )
{
	if (PyTuple_Size( args ) == 1)
	{
		PyObject * pItem = PyTuple_GetItem( args, 0 );
		if (GUIShader::Check( pItem ))
		{
			this->removeShader( static_cast<GUIShader*>( pItem ) );
			Py_Return;
		}
		if (PyString_Check( pItem ))
		{
			this->removeShader( PyString_AsString( pItem ) );
			Py_Return;
		}
	}

	PyErr_SetString( PyExc_TypeError, "SimpleGUIComponent.py_delShader "
		"expects a GUI shader or a string" );
	return NULL;
}


typedef SimpleGUIComponent * (*CCreator)();
typedef GUIShader * (*SCreator)();

/*~ function SimpleGUIComponent.load
 *
 *	This function loads the SimpleGUIComponent from a data section.  The data
 *	section can either be a resourceId, or an actual PyDataSection.  In
 *	general, this will have been created using the save
 *	method on a previous incarnation of the SimpleGUIComponent.
 *
 *	For example:
 *	@{
 *	# first create a new component and configure it.  This can be done
 *	# interactively at the console.
 *	c = GUI.Simple( "maps/myPicture.bmp" )
 *	c.colour = (255,0,0,0)  # tint the component red
 *	c.save( "gui/myGui.xml" )
 *
 *	# now create a new component, and load it
 *	cnew = GUI.Simple( "maps/someplaceholderimage.bmp" )
 *	cnew.load( "gui/myGui.xml" )
 *	@}
 *	This example creates and saves a gui, and then loads that gui into a new
 *	component.
 *
 *	@param	dataSection		either a resourceId that names a data section, or
 *							a PyDataSection
 */
/**
 *	The static python load method (in the GUI module)
 */
PyObject * SimpleGUIComponent::py_load( PyObject * args )
{
	DataSectionPtr pTop;

	if (PyTuple_Size( args ) == 1)
	{
		PyObject * pArg = PyTuple_GET_ITEM( args, 0 );

		if (PyString_Check( pArg ))
		{
			char * res = PyString_AsString( pArg );
			pTop = BWResource::openSection( res );

			if (!pTop || pTop->countChildren() == 0)
			{
				PyErr_Format( PyExc_ValueError, "GUI.load() "
					"could not open resource '%s' (or it is empty)", res );
				return NULL;
			}
		}
		else if (PyDataSection::Check( pArg ) )
		{
			pTop = ((PyDataSection *)pArg)->pSection();
		}
	}

	if (!pTop)
	{
		PyErr_SetString( PyExc_TypeError, "GUI.load() "
			"expects a resource name string or data section" );
		return NULL;
	}

/*
	// parse arguments
	char * res;
	if (!PyArg_ParseTuple( args, "s", &res ))
	{
		PyErr_SetString( PyExc_TypeError, "GUI.load() "
			"expects a resource name string" );
		return NULL;
	}

	// open the XML file
	DataSectionPtr pTop = BWResource::openSection( res );
	if (!pTop || pTop->countChildren() == 0)
	{
		PyErr_Format( PyExc_ValueError, "GUI.load() "
			"could not open resource '%s' (or it is empty)", res );
		return NULL;
	}
*/

	// ok, load all the childless components and shaders then
	std::vector< std::pair<SimpleGUIComponentPtr,int> > loaded;
	std::vector< GUIShaderPtr > loadedShaders;
	std::map<int,int> loadedIds;
	std::map<int,int> loadedShaderIds;
	LoadBindings bindings;

	for (DataSectionIterator it = pTop->begin(); it != pTop->end(); it++)
	{
		std::string sname = (*it)->sectionName();

		// see if it's a component
		CCreator c = GUIComponentFactory::get( sname );
		if (c)
		{
			SimpleGUIComponentPtr pNew( (*c)(), true );
			if (!pNew || !pNew->load( *it, bindings ))
			{
				PyErr_Format( PyExc_ValueError, "GUI.load() "
					"error loading component index %d", loaded.size() );
				return NULL;
			}

			loadedIds[ (*it)->asInt() ] = loaded.size();
			loaded.push_back( std::make_pair( pNew, bindings.size() ) );
			continue;
		}

		// try for a shader then
		SCreator s = GUIShaderFactory::get( sname );
		if (s)
		{
			GUIShaderPtr pNew( (*s)(), true );
			if (!pNew || !pNew->load( *it ))
			{
				PyErr_Format( PyExc_ValueError, "GUI.load() "
					"error loading shader index %d", loaded.size() );
				return NULL;
			}

			loadedShaderIds[ (*it)->asInt() ] = loadedShaders.size();
			loadedShaders.push_back( pNew );
			continue;
		}

		PyErr_Format( PyExc_KeyError, "GUI.load() "
			"unknown GUI component type '%s'", sname.c_str() );
		return NULL;
	}


	// now bind them to their children
	int bindex = 0;
	for (uint i = 0; i < loaded.size(); i++)
	{
		SimpleGUIComponentPtr pParent = loaded[i].first;
		int nBindings = (loaded[i].second) & 0x7FFFFFFF;
		for (; bindex < nBindings; bindex++)
		{
			LoadBinding & bi = bindings[bindex];

			// try it as a component
			std::map<int,int>::iterator found = loadedIds.find( bi.id_ );
			if (found != loadedIds.end())
			{
				pParent->addChild( bi.name_,
					loaded[ found->second ].first.getObject() );
				loaded[ found->second ].second |= 0x80000000; // not root
				continue;
			}

			// try it as a shader
			found = loadedShaderIds.find( bi.id_ );
			if (found != loadedShaderIds.end())
			{
				pParent->addShader( bi.name_,
					loadedShaders[ found->second ].getObject() );
				continue;
			}

			PyErr_Format( PyExc_ValueError, "GUI.load() "
				"could not find member id %d name %s of component index %d",
				bi.id_, bi.name_.c_str(), i );
			return NULL;
		}
	}

	// let them know their children are there
	//  (we wait for all to be bound)
	for (uint i = 0; i < loaded.size(); i++)
	{
		loaded[i].first->bound();
	}

	// find the root of the tree, and make sure there's only one
	int rootIndex = -1;
	for (uint i = 0; i < loaded.size(); i++)
	{
		if (loaded[i].second & 0x80000000) continue;
		if (rootIndex != -1)
		{
			PyErr_Format( PyExc_ValueError, "GUI.load() "
				"component tree has two or more roots (indicies %d and %d)",
				rootIndex, i );
			return NULL;
		}
		rootIndex = i;
	}

	// complain if there's no root
	if (rootIndex == -1)
	{
		PyErr_Format( PyExc_ValueError, "GUI.load() "
			"could not find the root component (from %d)", loaded.size() );
		return NULL;
	}

	PyObject * pRet = loaded[ rootIndex ].first.getObject();
	Py_INCREF( pRet );	// current reference owned by smart pointer
	return pRet;
}

/*~ function SimpleGUIComponent.save
 *
 *	This method persists the SimpleGUIComponent as an xml data section. The method takes a resourceID
 *	as an argument, which must end in ".gui".  It creates or overwrites the named file.  This can then be
 *	loaded later using the load method, allowing a quick recreation of the current object.
 *
 *	For example:
 *	@{
 *	# first create a new component and configure it.  This can be done
 *	# interactively at the console.
 *	c = GUI.Simple( "maps/myPicture.bmp" )
 *	c.colour = (255,0,0,0)  # tint the component red
 *	c.save( "gui/myGui.xml" )
 *
 *	# now create a new component, and load it
 *	cnew = GUI.Simple( "maps/someplaceholderimage.bmp" )
 *	cnew.load( "gui/myGui.xml" )
 *	@}
 *	This example creates and saves a gui, and then loads that gui into a new
 *	component.
 *
 *	@param	resourceID	the filename to save to.
 */
/**
 *	The (non-static) python save method
 */
PyObject * SimpleGUIComponent::py_save( PyObject * args )
{
	// parse arguments
	char * res;
	if (!PyArg_ParseTuple( args, "s", &res ))
	{
		PyErr_SetString( PyExc_TypeError, "GUI.load() "
			"expects a resource name string" );
		return NULL;
	}

	// make sure it's OK
	int resLen = strlen(res);
	if (resLen < 4 || strcmp(res+resLen-4,".gui") != 0)
	{
		PyErr_Format( PyExc_ValueError, "GUI.save() "
			"resource name '%s' does not end in '.gui'", res );
		return NULL;
	}

	// ok, open the file then
	DataSectionPtr pFile = BWResource::instance().rootSection()->
		openSection( res, true );
	if (!pFile)
	{
		PyErr_Format( PyExc_ValueError, "GUI.save() "
			"could not open or create resource '%s'", res );
		return NULL;
	}

	// clear it out
	pFile->delChildren();

	// set up the bindings vector
	SaveBindings sb;
	sb.components_.push_back( this );

	// write all the components out
	for (uint i = 0; i < sb.components_.size(); i++)
	{
		SimpleGUIComponent * pComponent = sb.components_[i];

		DataSectionPtr pNew = pFile->newSection( pComponent->factory().name() );
		pNew->setInt( int(pComponent) );

		pComponent->save( pNew, sb );
	}

	// write all the shaders out
	for (uint i = 0; i < sb.shaders_.size(); i++)
	{
		GUIShader * pShader = sb.shaders_[i];

		DataSectionPtr pNew = pFile->newSection( pShader->factory().name() );
		pNew->setInt( int(pShader) );

		pShader->save( pNew );
	}

	// and then save it
	pFile->save();

	Py_Return;
}


/*~ function SimpleGUIComponent.reSort
 *
 *	This function reSorts the children of this component according to the third (depth) component of their
 *	position attributes.  Changing this depth value in a child doesn't automatically reorder the children.
 *	An explicit call to this function is required.  If all gui components on the screen require reordering
 *	then GUI.reSort() can be called instead.
 */
/**
 *	This method is a script wrapper to the C++ method.
 */
PyObject * SimpleGUIComponent::py_reSort( PyObject * args )
{
	this->reSort();

	Py_Return;
}

/**
 *   Calculates the final clip space position and size to be used for rendering.
 */
void SimpleGUIComponent::layout( float relativeParentWidth, float relativeParentHeight, 
								float& x, float& y, float& w, float& h) const
{
	w = this->widthInClip( relativeParentWidth );
	h = this->heightInClip( relativeParentHeight );
	
	float clipX, clipY;
	this->positionInClip( relativeParentWidth, relativeParentHeight, clipX, clipY );

	float anchorOffsetX, anchorOffsetY;
	this->anchorOffset( w, h, anchorOffsetX, anchorOffsetY );

	x = clipX + anchorOffsetX;
	y = clipY + anchorOffsetY;
}

/**
 *	This method finds the anchor offset this component, based on the supplied
 *  width of this component and the width of the relative parent. All dimensions
 *  must be in clip space.
 */
void SimpleGUIComponent::anchorOffset( float w, float h, float &xOffset, float &yOffset ) const
{
	switch( horizontalAnchor_ )
	{
	case ANCHOR_H_LEFT:
		xOffset = 0;
		break;
	case ANCHOR_H_CENTER:
		xOffset = -w/2;
		break;
	case ANCHOR_H_RIGHT:
		xOffset = -w;
		break;
	}

	switch( verticalAnchor_ )
	{
	case ANCHOR_V_TOP:
		yOffset = 0;
		break;
	case ANCHOR_V_CENTER:
		yOffset = h/2;
		break;
	case ANCHOR_V_BOTTOM:
		yOffset = h;
		break;
	}
}

/**
 *	Calculate the vertices of this component for a tile effect
 */
void SimpleGUIComponent::tile()
{
	float widthInClipCoords = blueprint_[2].pos_.x - blueprint_[0].pos_.x;
	float heightInClipCoords = blueprint_[0].pos_.y - blueprint_[2].pos_.y;

	float sw, sh;	//width, height in screen coordinates( pixels )
	SimpleGUI::instance().clipRangesToPixel( widthInClipCoords, heightInClipCoords, &sw, &sh );

	float tuMin, tuMax;
	float tvMin, tvMax;

	tuMin = 0.f;
	tuMax = sw / tileWidth_;
	tvMin = 0.f;
	tvMax = sh / tileHeight_;

	GUIVertex* v;

	v = &blueprint_[0];
	v->uv_[0] = tuMin;
	v->uv_[1] = tvMin;

	v = &blueprint_[1];
	v->uv_[0] = tuMin;
	v->uv_[1] = tvMax;

	v = &blueprint_[2];
	v->uv_[0] = tuMax;
	v->uv_[1] = tvMax;

	v = &blueprint_[3];
	v->uv_[0] = tuMax;
	v->uv_[1] = tvMin;
}

/**
 *  Gets the width of the component in screen clip space, no matter what the curent width mode is.
 */
float SimpleGUIComponent::widthInClip( float relativeParentWidth ) const
{
	float ret;

	switch( widthMode() )
	{
	case SIZE_MODE_CLIP:
		SimpleGUI::instance().pixelRangesToClip( (width_/2.0f) * relativeParentWidth, 0, &ret, NULL );
		break;

	case SIZE_MODE_PIXEL:
		SimpleGUI::instance().pixelRangesToClip( width_, 0, &ret, NULL );
		break;

	case SIZE_MODE_LEGACY:
	default:
		ret = width_;
		break;
	}

	return ret;
}

/**
 *  Gets the width of this component in screen pixels, no matter what the current width mode is.
 */
float SimpleGUIComponent::widthInPixels( float relativeParentWidth ) const
{
	switch( widthMode() )
	{
	case SIZE_MODE_LEGACY:
		return width_ * SimpleGUI::instance().halfScreenWidth();

	case SIZE_MODE_CLIP:
		return (width_/2.0f) * relativeParentWidth;

	case SIZE_MODE_PIXEL:
	default:
		return width_;
	}
}

/**
 *  Gets the height of the component in screen clip space, no matter what the current height mode is.
 */
float SimpleGUIComponent::heightInClip( float relativeParentHeight ) const
{
	float ret;

	switch( heightMode() )
	{
	case SIZE_MODE_CLIP:
		SimpleGUI::instance().pixelRangesToClip( 0, (height_/2.0f) * relativeParentHeight, NULL, &ret );
		break;

	case SIZE_MODE_PIXEL:
		SimpleGUI::instance().pixelRangesToClip( 0, height_, NULL, &ret );
		break;

	case SIZE_MODE_LEGACY:
	default:
		ret = height_;
		break;
	}

	return ret;
}

/**
 *  Gets the height of the component in screen pixels, no matter what the curent height mode is.
 */
float SimpleGUIComponent::heightInPixels( float relativeParentHeight ) const
{
	switch( heightMode() )
	{
	case SIZE_MODE_LEGACY:
		return height_ * SimpleGUI::instance().halfScreenHeight();

	case SIZE_MODE_CLIP:
		return (height_/2.0f) * relativeParentHeight;

	case SIZE_MODE_PIXEL:
	default:
		return height_;
	}

}

/*~ function SimpleGUIComponent.screenToLocal
 *
 *	This method converts the given position in clip space into the local space
 *	relative to the bounds of this component. The units that it is returned in
 *	depends on the current width and height modes. If the width or height mode
 *	is CLIP, then the corresponding component is returned in clip space of the
 *	component (i.e. 0,0 is the center of the component, -1,1 is the top left and
 *	1,-1 is the bottom right).
 *
 *	If the width or height mode is PIXEL, then the coresponding component is 
 *	returned in pixels relative to the top left of the component.
 *
 *	If the width or height mode is LEGACY, then the corresponding component is
 *	returned such that the position is relative to the center of the component,
 *	however the left side of the component is the negative distance from the
 *	center of the component in the clip space of the screen. Similarly, the 
 *	right hand side is the positive distance, the bottom is the negative distance
 *	and the top is the positive distance.
 *
 *	@param	event	a Vector2 containing the position in the clip space of 
 *					the screen.
 *	
 *	@return			A Vector2 containing the local space position.
 */
/**
 *	Converts the given screen space (i.e. clip space) coordinate to local coordinates
 *  relative to this component. The units returned depend on what the current
 *	modes for width and height.
 */
Vector2	SimpleGUIComponent::screenToLocal( const Vector2 & screen ) const
{
	float relativeParentWidth, relativeParentHeight;

	Vector2 topLeft, topRight, botLeft, botRight;
	this->clipBounds( topLeft, topRight, botLeft, botRight,
						&relativeParentWidth,
						&relativeParentHeight);

	Vector2 centre ( topLeft.x + (topRight.x - topLeft.x)/2.0f,
					 botLeft.y + (topLeft.y - botLeft.y)/2.0f );

	// Convert the clip bounds to be in absolute screen clip
	SimpleGUIComponentPtr nrp = this->nearestRelativeParent();
	while( nrp )
	{
		Vector2 nrp_topLeft, nrp_topRight, nrp_botLeft, nrp_botRight;
		nrp->clipBounds( nrp_topLeft, nrp_topRight, nrp_botLeft, nrp_botRight );

		Vector2 nrp_centre ( nrp_topLeft.x + (nrp_topRight.x - nrp_topLeft.x)/2.0f,
							 nrp_botLeft.y + (nrp_topLeft.y  - nrp_botLeft.y)/2.0f );

		centre += nrp_centre;

		nrp = nrp->nearestRelativeParent();
	}


	// Relative to the top left of the box
	Vector2 loc;
	loc.x = screen.x - (centre.x - ((topRight.x - topLeft.x)/2.0f));
	loc.y = screen.y - (centre.y + ((topLeft.y - botLeft.y)/2.0f));

	// 0-1 normalized position starting at the top left.
	float normX = loc.x / (topRight.x - topLeft.x);
	float normY = loc.y / (botLeft.y - topLeft.y);
	
	// Transform this into something useful based on our size modes
	switch( widthMode() )
	{
	case SIZE_MODE_CLIP:
		loc.x = -1.0f + normX * 2.0f;
		break;

	case SIZE_MODE_PIXEL:
		loc.x = widthInPixels( relativeParentWidth ) * normX;
		break;

	case SIZE_MODE_LEGACY:
	default:
		loc.x = screen.x - centre.x;
		break;
	}

	switch( heightMode() )
	{
	case SIZE_MODE_CLIP:
		loc.y = -1.0f + (1.0f-normY) * 2.0f;
		break;

	case SIZE_MODE_PIXEL:
		{
			loc.y = normY * heightInPixels( relativeParentHeight );
			break;
		}

	case SIZE_MODE_LEGACY:
	default:
		loc.y = screen.y - centre.y;
		break;
	}

	return loc;
}

/*~ function SimpleGUIComponent.localToScreen
 *
 *	This method converts the given local coordinates (i.e. relative to the bounds)
 *	of this component into clip space of the screen. The units for the input
 *	depends on the current width and height modes.
 *
 *	@param	event	a Vector2 containing the position relative this
 *					component.
 *	
 *	@return			A Vector2 containing the screen space clip position.
 */
/**
 *	Converts the given local space (i.e. relative to this GUI component) coordinate
 *	into screen space (i.e. clip space). The input units depend on what the width
 *	and height modes are currently set to.
 */
Vector2	SimpleGUIComponent::localToScreen( const Vector2 & local ) const
{
	float relativeParentWidth, relativeParentHeight;

	Vector2 topLeft, topRight, botLeft, botRight;
	this->clipBounds( topLeft, topRight, botLeft, botRight,
						&relativeParentWidth,
						&relativeParentHeight);

	float widthInClip = topRight.x-topLeft.x;
	float heightInClip = botLeft.y - topLeft.y;

	Vector2 centre ( topLeft.x + (topRight.x - topLeft.x)/2.0f,
					 botLeft.y + (topLeft.y - botLeft.y)/2.0f );

	float normX, normY;
	
	// Based on size modes, transform this local position to be relative to
	// the top left of the component.
	switch( widthMode() )
	{
	case SIZE_MODE_CLIP:
		normX = ((local.x + 1.0f) / 2.0f);
		break;

	case SIZE_MODE_PIXEL:
		normX = local.x / widthInPixels( relativeParentWidth );
		break;

	case SIZE_MODE_LEGACY:
	default:
		normX = ((local.x + centre.x) - topLeft.x) / widthInClip;
		break;
	}

	switch( heightMode() )
	{
	case SIZE_MODE_CLIP:
		normY = ((-local.y + 1.0f) / 2.0f);
		break;

	case SIZE_MODE_PIXEL:
		normY = local.y / heightInPixels( relativeParentHeight );
		break;

	case SIZE_MODE_LEGACY:
	default:
		normY = ((local.y + centre.y) - topLeft.y) / heightInClip;
		break;
	}

	Vector2 screen( topLeft.x+widthInClip*normX, topLeft.y+heightInClip*normY );


	// Convert the position to the in screen clip.
	SimpleGUIComponentPtr nrp = this->nearestRelativeParent();
	while( nrp )
	{
		Vector2 nrp_topLeft, nrp_topRight, nrp_botLeft, nrp_botRight;
		nrp->clipBounds( nrp_topLeft, nrp_topRight, nrp_botLeft, nrp_botRight );

		Vector2 centre ( nrp_topLeft.x + (nrp_topRight.x - nrp_topLeft.x)/2.0f,
						 nrp_botLeft.y + (nrp_topLeft.y  - nrp_botLeft.y)/2.0f );

		screen += centre;
		nrp = nrp->nearestRelativeParent();
	}

	return screen;
}

inline float convertSizeMode( SimpleGUIComponent::eSizeMode oldMode, 
							  SimpleGUIComponent::eSizeMode newMode, 
							  float v, float nrpSize, float screenSize )
{
	float halfScreenSize = screenSize/2.0f;

	switch( oldMode )
	{
		case SimpleGUIComponent::SIZE_MODE_PIXEL:
			if ( newMode == SimpleGUIComponent::SIZE_MODE_LEGACY )
			{
				// Convert from pixel to clip screen
				return v / halfScreenSize;
			}
			else if ( newMode == SimpleGUIComponent::SIZE_MODE_CLIP )
			{
				// Convert from pixel to clip relative
				return (v/nrpSize)*2.0f;
			}
			break;

		case SimpleGUIComponent::SIZE_MODE_LEGACY:
			if( newMode == SimpleGUIComponent::SIZE_MODE_PIXEL )
			{
				// Convert from clip screen to pixel
				return v * halfScreenSize;
			}
			else if( newMode == SimpleGUIComponent::SIZE_MODE_CLIP )
			{
				// Convert from clip screen to clip relative
				return ((v * halfScreenSize) / nrpSize) * 2.0f;
			}
			break;

		case SimpleGUIComponent::SIZE_MODE_CLIP:
			if( newMode == SimpleGUIComponent::SIZE_MODE_PIXEL )
			{
				// Convert from clip relative to pixel.
				return (v/2.0f) * nrpSize;
			}
			else if( newMode == SimpleGUIComponent::SIZE_MODE_LEGACY )
			{
				// Convert from clip relative to clip screen
				return ((v/2.0f) * nrpSize) / halfScreenSize;
			}
			break;
		default:
			break;
	}

	MF_ASSERT(!"convertSizeMode - encountered unknown size mode.");			
	return v;
}

/**
 *  Changes the current width mode, converting the current width to the target mode's coord system.
 */
void SimpleGUIComponent::widthMode( eSizeMode newMode )
{
	if ( widthMode_ == newMode )
		return;

	float nrpWidth, nrpHeight;
	nearestRelativeDimensions( nrpWidth, nrpHeight );

	width_ = convertSizeMode( widthMode_, newMode, width_, nrpWidth, SimpleGUI::instance().screenWidth() );
	widthMode_ = newMode;
}

/**
 *  Changes the current height mode, converting the current height to the target mode's coord system.
 */
void SimpleGUIComponent::heightMode( eSizeMode newMode )
{
	if ( heightMode_ == newMode )
		return;

	float nrpWidth, nrpHeight;
	nearestRelativeDimensions( nrpWidth, nrpHeight );

	height_ = convertSizeMode( heightMode_, newMode, height_, nrpHeight, SimpleGUI::instance().screenHeight() );
	heightMode_ = newMode;
}

/**
 *  Determines the position of the component in screen clip space, without regarding 
 *  the current anchors (so it's a direct transform of position_, nothing more taken
 *  into account).
 */
void SimpleGUIComponent::positionInClip( float relativeParentWidth, float relativeParentHeight,
											  float &x, float &y) const
{
	// Calculate the parent width in screen clip space.
	float rpWidthClip, rpHeightClip;
	SimpleGUI::instance().pixelRangesToClip( relativeParentWidth, relativeParentHeight, 
										&rpWidthClip, &rpHeightClip );

	switch( horizontalPositionMode() )
	{
	case POSITION_MODE_CLIP:
		{
			x = parentSpaceToClipSpace( position_.x, rpWidthClip);
			break;
		}

	case POSITION_MODE_PIXEL:
		{
			SimpleGUI::instance().pixelRangesToClip( position_.x, 0, &x, NULL );
			x = x - rpWidthClip/2.0f;
			break;
		}

	case POSITION_MODE_LEGACY:
	default:
		{
			x = position_.x;
			break;
		}
	};

	switch( verticalPositionMode() )
	{
	case POSITION_MODE_CLIP:
		{
			y = parentSpaceToClipSpace( position_.y, rpHeightClip );
			break;
		}

	case POSITION_MODE_PIXEL:
		{
			SimpleGUI::instance().pixelRangesToClip( 0, position_.y, NULL, &y );
			y = -y + rpHeightClip/2.0f;
			break;
		}

	case POSITION_MODE_LEGACY:
	default:
		{
			y = position_.y;
			break;
		}
	};
}

/**
 *  Sets the horizontal position mode, converting the current x coord to the target mode's
 *  coordinate system.
 */
void SimpleGUIComponent::horizontalPositionMode( ePositionMode newMode )
{
	if( horizontalPositionMode_ == newMode )
		return;

	float nrpWidth, nrpHeight;
	nearestRelativeDimensions( nrpWidth, nrpHeight );

	float nrpWidthClip, nrpHeightClip;
	SimpleGUI::instance().pixelRangesToClip( nrpWidth, nrpHeight,&nrpWidthClip, &nrpHeightClip );


	// Based on the current mode and the new mode, convert existing position to new coordinate system.
	switch( horizontalPositionMode_ )
	{
	case POSITION_MODE_CLIP:
		if( newMode == POSITION_MODE_LEGACY )
		{
			// Convert from clip relative to clip screen
			position_.x = parentSpaceToClipSpace( position_.x, nrpWidthClip);
		}
		else if( newMode == POSITION_MODE_PIXEL )
		{
			// Convert from clip relative to pixel
			position_.x = (position_.x+1.0f) * (nrpWidth/2.0f);
		}
		break;

	case POSITION_MODE_LEGACY:
		// Convert from clip screen to clip relative
		position_.x = clipSpaceToParentSpace( position_.x, nrpWidthClip );

		if( newMode == POSITION_MODE_PIXEL )
		{
			// Convert from clip relative to pixel
			position_.x = (position_.x+1.0f) * (nrpWidth/2.0f);
		}
		break;

	case POSITION_MODE_PIXEL:
		// Convert from pixel to clip relative
		position_.x = (position_.x/nrpWidth) * 2.0f - 1.0f;

		if( newMode == POSITION_MODE_LEGACY )
		{
			// Convert from clip relative to clip screen
			position_.x = parentSpaceToClipSpace( position_.x, nrpWidthClip );
		}
		break;
	}

	horizontalPositionMode_ = newMode;
}

/**
 *  Sets the vertical position mode, converting the current y coord to the target mode's
 *  coordinate system.
 */
void SimpleGUIComponent::verticalPositionMode( ePositionMode newMode )
{
	if( verticalPositionMode_ == newMode )
		return;

	float nrpWidth, nrpHeight;
	nearestRelativeDimensions( nrpWidth, nrpHeight );

	float nrpWidthClip, nrpHeightClip;
	SimpleGUI::instance().pixelRangesToClip( nrpWidth, nrpHeight,&nrpWidthClip, &nrpHeightClip );


	// Based on the current mode and the new mode, convert existing position to new coordinate system.
	switch( verticalPositionMode_ )
	{
	case POSITION_MODE_CLIP:
		if( newMode == POSITION_MODE_LEGACY )
		{
			// Convert from clip relative to clip screen
			position_.y = parentSpaceToClipSpace( position_.y, nrpHeightClip);
		}
		else if( newMode == POSITION_MODE_PIXEL )
		{
			// Convert from clip relative to pixel
			position_.y = (-position_.y+1.0f) * (nrpHeight/2.0f);
		}
		break;

	case POSITION_MODE_LEGACY:
		// Convert from clip screen to clip relative
		position_.y = clipSpaceToParentSpace( position_.y, nrpHeightClip );

		if( newMode == POSITION_MODE_PIXEL )
		{
			// Convert from clip relative to pixel
			position_.y = (-position_.y+1.0f) * (nrpHeight/2.0f);
		}
		break;

	case POSITION_MODE_PIXEL:
		// Convert from pixel to clip relative
		position_.y = (-position_.y/nrpHeight) * 2.0f + 1.0f;

		if( newMode == POSITION_MODE_LEGACY )
		{
			// Convert from clip relative to clip screen
			position_.y = parentSpaceToClipSpace( position_.y, nrpHeightClip );
		}
		break;
	}

	verticalPositionMode_ = newMode;
}


/**
 *	Update this component
 */
void SimpleGUIComponent::update( float dTime, float relativeParentWidth, 
								float relativeParentHeight )
{
	float x,y,w,h;
	this->layout( relativeParentWidth, relativeParentHeight, x, y, w, h );
	
	GUIVertex* v;

	v = &blueprint_[0];
	v->pos_.x = x;		v->pos_.y = y;		v->pos_.z = position_.z;

	v = &blueprint_[1];
	v->pos_.x = x;		v->pos_.y = y - h;	v->pos_.z = position_.z;

	v = &blueprint_[2];
	v->pos_.x = x + w;	v->pos_.y = y - h;	v->pos_.z = position_.z;

	v = &blueprint_[3];
	v->pos_.x = x + w;	v->pos_.y = y;		v->pos_.z = position_.z;

	if ( tiled_ )
	{
		tile();
	}

	memcpy( vertices_, blueprint_, 4 * sizeof( GUIVertex ) );
	runTimeColour_ = colour_;
	runTimeTransform_.setIdentity();

	//reset run-time clip region
	static Vector4 fullscreen( -1.f, 1.f, 1.f, -1.f );
	runTimeClipRegion_ = fullscreen;

	//now, we have a drawable set of vertices.

	updateChildren( dTime, relativeParentWidth, relativeParentHeight );
}


/**
 *	Update this component's children
 */
void SimpleGUIComponent::updateChildren( float dTime, float relParentWidth, float relParentHeight )
{
	//update children
	ChildRecVector::iterator it = children_.begin();
	ChildRecVector::iterator end = children_.end();

	while( it != end )
	{
		it->second->update( dTime, relParentWidth, relParentHeight );
		it++;
	}
}


/**
 *	This method is the root of the recursive shader descent for
 *	this component and its children
 *
 *	@param dTime	Delta time for this frame of the app
 */
void SimpleGUIComponent::applyShaders( float dTime )
{
	if ( visible() )
	{
		ChildRecVector::iterator cit = children_.begin();
		ChildRecVector::iterator cend = children_.end();

		while( cit != cend )
		{
			cit->second->applyShaders( dTime );

			cit++;
		}

		GUIShaderPtrVector::iterator it = shaders_.begin();
		GUIShaderPtrVector::iterator end = shaders_.end();

		while( it != end )
		{
			this->applyShader( *it->second, dTime );
			it++;
		}
	}
}


/**
 *	This method applies a shader to our corner gui components and our children
 *
 *	@param shader	The shader to apply
 *	@param dTime	Delta time for this frame of the app
 */
void SimpleGUIComponent::applyShader( GUIShader& shader, float dTime )
{
	//shader returns true if it wants traversal of children as well
	//TODO : allow the shader to perform its own traversals.
	if ( shader.processComponent( *this, dTime ) )
	{
		ChildRecVector::iterator cit = children_.begin();
		ChildRecVector::iterator cend = children_.end();

		while( cit != cend )
		{
			cit->second->applyShader( shader, 0.f );
			cit++;
		}
	}
}


// -----------------------------------------------------------------------------
// Section: SimpleGuiSortedDrawItem
// -----------------------------------------------------------------------------
/**
 * TODO: to be documented.
 */
class SimpleGuiSortedDrawItem : public Moo::ChannelDrawItem, public Aligned
{
public:
	SimpleGuiSortedDrawItem( SimpleGUIComponent* gui, const Matrix& worldTransform, float distance )
	:	gui_( gui ),
		worldTransform_( worldTransform )
	{
		distance_ = distance;
	}
	~SimpleGuiSortedDrawItem()
	{

	}
	void draw()
	{
		//unfortunately we have to do this here, because gui sorted draw items
		//may well be interspersed with any other kind of sorted draw item.
		Moo::rc().setVertexShader( NULL );
		Moo::rc().setFVF( GUIVertex::fvf() );
		Moo::rc().device()->SetTransform( D3DTS_VIEW, &Moo::rc().view() );
		Moo::rc().device()->SetTransform( D3DTS_PROJECTION, &Moo::rc().projection() );
		Moo::rc().setRenderState( D3DRS_LIGHTING, FALSE );
		Moo::rc().push();
		Moo::rc().world( worldTransform_ );
		gui_->drawSelf( false );
		Moo::rc().pop();
	}
	void fini()
	{
		delete this;
	}
private:
	SimpleGUIComponent* gui_;
	Matrix	worldTransform_;
};
// -----------------------------------------------------------------------------
// End Section: SimpleGuiSortedDrawItem
// -----------------------------------------------------------------------------

/**
 *	This method is called when this component is part of a gui tree existing
 *	under a GUIAttachment object, and therefore drawn in the world instead
 *	of overlaid on the screen like your usual GUI.
 */
void SimpleGUIComponent::addAsSortedDrawItem()
{
	if ( visible() )
	{
		Moo::rc().push();
		Moo::rc().preMultiply( runTimeTransform_ );
		Matrix world( Moo::rc().world() );		

		float distance = (world.applyToOrigin() - Moo::rc().invView().applyToOrigin()).length();	
		Moo::SortedChannel::addDrawItem( new SimpleGuiSortedDrawItem( this, world, distance ) );

		//TODO : maybe factor in a fudge value for the world transform for children, so they are
		//drawn in the correct order.  We'll see if there's a problem first.		
		std::vector<int>::iterator it = childOrder_.begin();
		std::vector<int>::iterator end = childOrder_.end();

		while( it != end )
		{
			(children_.begin() + *it)->second->addAsSortedDrawItem();
			it++;
		}

		Moo::rc().pop();
	}
}


/**
 *	This method draws the simple gui component.
 */
void SimpleGUIComponent::draw( bool overlay )
{
	// store current world tranform in run time transform so
	// that hit tests can reflect the correct frame of reference
	Matrix tempRunTimeTrans = Moo::rc().viewProjection();
	tempRunTimeTrans.preMultiply( Moo::rc().world() );
	tempRunTimeTrans.preMultiply( runTimeTransform_ );

	if ( visible() )
	{
		Moo::rc().push();
		Moo::rc().preMultiply( runTimeTransform_ );
		Moo::rc().device()->SetTransform( D3DTS_WORLD, &Moo::rc().world() );

		this->drawSelf(overlay);
		this->drawChildren(overlay);

		Moo::rc().pop();
		Moo::rc().device()->SetTransform( D3DTS_WORLD, &Moo::rc().world() );
	}

	runTimeClipRegion_ = SimpleGUI::instance().clipRegion();
	runTimeTransform_ = tempRunTimeTrans;
	momentarilyInvisible_ = false;
}


/**
 *	This method implements a standard draw of self and
 *	is used internally and by derived classes.
 */
void SimpleGUIComponent::drawSelf( bool overlay )
{
	if( momentarilyInvisible() )
		return;

	if( !(vertices_ && nVertices_) )
		return;

	SimpleGUI::instance().setConstants(runTimeColour(), pixelSnap_);
	material_->pEffect()->pEffect()->SetInt( "filterType", filterType_ );
	if ( material_->begin() ) 
	{
		bool valid = true;
		for ( uint32 i=0; i<material_->nPasses() && valid; i++ )
		{
			material_->beginPass(i);				
			Moo::rc().setTexture( 0, texture_ ? texture_->pTexture() : NULL );
			if (tiled_)
			{
				Moo::rc().setSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
				Moo::rc().setSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
			}

			if ( !overlay )
			{
				Moo::rc().setRenderState( D3DRS_ZENABLE, TRUE );
				Moo::rc().setRenderState( D3DRS_ZWRITEENABLE, FALSE );
				Moo::rc().setRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
				Moo::rc().setRenderState( D3DRS_ZFUNC, D3DCMP_LESS );
			}


			uint32 vertexBase = 0, lockIndex = 0;
			bool indexed = nIndices_ && indices_;

			//DynamicVertexBuffer
			Moo::DynamicVertexBufferBase2<GUIVertex>& vb = Moo::DynamicVertexBufferBase2< GUIVertex >::instance();
			if ( vb.lockAndLoad( vertices_, nVertices_, vertexBase ) &&
				 SUCCEEDED(vb.set( 0 )) )
			{
				if ( indexed )
				{
					//DynamicIndexBuffer
					Moo::DynamicIndexBufferBase& dynamicIndexBuffer = Moo::rc().dynamicIndexBufferInterface().get( D3DFMT_INDEX16 );
					Moo::IndicesReference ind = dynamicIndexBuffer.lock2( nIndices_ );
					if ( ind.valid() )
					{
						ind.fill( indices_, nIndices_ );
						dynamicIndexBuffer.unlock();

						valid = SUCCEEDED(dynamicIndexBuffer.indexBuffer().set());
						lockIndex = dynamicIndexBuffer.lockIndex();
					}
				}

				if (valid)
				{
					SimpleGUI::instance().countDrawCall();
					if ( indexed )
					{
						Moo::rc().drawIndexedPrimitive(D3DPT_TRIANGLELIST, vertexBase, 0, nVertices_, lockIndex, nIndices_/3); 
					}
					else
					{
						Moo::rc().drawPrimitive(D3DPT_TRIANGLELIST, vertexBase, nVertices_/3);
					}
				}
			}
			material_->endPass();
		}

		material_->end();
	}
}


/**
 *	This method implements a standard draw children traversal and
 *	is used internally and by derived classes.
 */
void SimpleGUIComponent::drawChildren( bool overlay )
{
	std::vector<int>::iterator it = childOrder_.begin();
	std::vector<int>::iterator end = childOrder_.end();

	while( it != end )
	{
		(children_.begin() + *it)->second->draw( overlay );
		++it;
	}
}


/**
 *	Set the texture name for the component
 */
void SimpleGUIComponent::textureName( const std::string& name )
{
	if ( textureName() != name )
	{
		/*
		camelo: this looks wrong. What's the rationale behind 
		setting the texture to NULL if a forward slash is not 
		found (the comparisson should be against std::string::npos, 
		by the way)? What if the texture is in the root of res? 
		What if the path uses back slashes, instead? 

		John's original commit comments indicates this was added 
		toghether with TextureProviders so it may be a quick (and 
		ugly) hack. I am commeting it for now. We'll need to keep 
		our eyes pilled for any side effect this may have on 
		TextureProviders.

		This is another textbook example of why you should always 
		document the intention of your code.

		if (uint32(name.find_first_of( '/' )) >= name.length())
		{
			texture_ = NULL;
		}
		else
		*/

		texture_ = Moo::TextureManager::instance()->get( name );
		buildMaterial();
	}
}


/**
 *	Get the texture of this component as a texture provider
 */
PyObject * SimpleGUIComponent::pyGet_texture()
{
	if (texture_)
	{
		return new PyTextureProvider( this, texture_ );
	}
	
	//ok to return None as the texture provider
	Py_Return;
}


/**
 *	Set the texture of this component from a texture provider
 */
int SimpleGUIComponent::pySet_texture( PyObject * value )
{
	SmartPointer<PyTextureProvider> pyTP;
	if (Script::setData( value, pyTP, "SimpleGUIComponent.texture" ) != 0)
		return -1;
	if ( pyTP.hasObject() )
		texture_ = pyTP->texture();
	buildMaterial();
	return 0;
}


#define IMPLEMENT_FOCUS_FUNCTION( ATTRIB, PY_NAME, ADD_FUNC, DEL_FUNC )		\
	if ( ATTRIB == state )													\
		return;																\
																			\
	ATTRIB = state;															\
																			\
	if (pScriptObject_)														\
	{																		\
		int intState = ( state ? 1 : 0 );									\
																			\
		Script::call(														\
			PyObject_GetAttrString( pScriptObject_.getObject(), #PY_NAME ),	\
			Py_BuildValue( "(i)", intState ),								\
			"SimpleGUIComponent::" #PY_NAME,								\
			true );															\
	}																		\
																			\
	if ( ATTRIB )															\
		SimpleGUI::instance().ADD_FUNC( this );								\
	else																	\
		SimpleGUI::instance().DEL_FUNC( this );								\

void SimpleGUIComponent::focus( bool state )
{
	IMPLEMENT_FOCUS_FUNCTION( focus_, focus, addInputFocus, delInputFocus )
}

/**
 *	This method sets whether this component has the mouve move focus.
 *
 *	@param state	The new move focus state for this component.
 */
void SimpleGUIComponent::moveFocus( bool state )
{
	IMPLEMENT_FOCUS_FUNCTION( moveFocus_, moveFocus, 
			addMouseMoveFocus, delMouseMoveFocus )
}


/**
 *	This method sets whether this component has the cross focus.
 *
 *	@param state	The new focus state for this component.
 */
void SimpleGUIComponent::crossFocus( bool state )
{
	IMPLEMENT_FOCUS_FUNCTION( crossFocus_, crossFocus, 
			addMouseCrossFocus, delMouseCrossFocus )
}


/**
 *	This method sets whether this component has the drag focus.
 *
 *	@param state	The new focus state for this component.
 */
void SimpleGUIComponent::dragFocus( bool state )
{
	IMPLEMENT_FOCUS_FUNCTION( dragFocus_, dragFocus, 
			addMouseDragFocus, delMouseDragFocus )
}


/**
 *	This method sets whether this component has the drop focus.
 *
 *	@param state	The new focus state for this component.
 */
void SimpleGUIComponent::dropFocus( bool state )
{
	IMPLEMENT_FOCUS_FUNCTION( dropFocus_, dropFocus, 
			addMouseDropFocus, delMouseDropFocus )
}

#undef IMPLEMENT_FOCUS_FUNCTION

/*~ function SimpleGUIComponent.handleKeyEvent
 *
 *	This method handles key events from the gui system. If this component 
 *	doesn't have focus, it exits immediatly, ignoring the event and returning
 *	false. Otherwise, it gives to each of its children a chance to handle the 
 *	event. If they don't, then it checks if it has an attached script object, 
 *	and gives that a chance to handle the event. Last, if the key event is 
 *	still unhandled, if it is a mouse button event, and if the mouse cursor
 *	is active, it tries to forward the event to handleMouseButtonEvent method
 *	of the script object, if one is attached.
 *
 *	@param	event	a keyEvent is a 3-tuple of integers, as follows: (down, 
 *					key, modifiers) down is 0 if the key transitioned from 
 *					down to up, non-zero if it transitioned from up to down 
 *					key is the keycode for which key was pressed. Modifiers 
 *					indicates which modifier keys were pressed when the event 
 *					occurred, and can include MODIFIER_SHIFT, MODIFIER_CTRL, 
 *					MODIFIER_ALT.
 *
 *	@return			True if the event was handled, False if it wasn't
 */
/**
 *	This method handles key events for the gui system.
 *
 *	Key events are passed on to the current component(s) that
 *	has(have) the focus, if any.
 *
 *	@param event	The key event to handle.
 *
 *	@return True if the key event was handled.
 */
bool SimpleGUIComponent::handleKeyEvent( const SimpleGUIKeyEvent & event )
{
	bool handled = false;

	ChildRecVector::iterator it = children_.begin();
	ChildRecVector::iterator end = children_.end();

	while (it != end)
	{
		SimpleGUIComponentPtr c = it->second;
		it++;

		handled = c->handleKeyEvent( event );
		if (handled) 
		{
			break;
		}
	}

	// OK, the event is ours. Pass it on
	// to a script object if we have one.
	if (this->focus() && pScriptObject_ && !handled)
	{
		PyObject * ret = Script::ask(
			PyObject_GetAttrString( pScriptObject_.getObject(), "handleKeyEvent" ),
			Script::getData( event ), "SimpleGUIComponent::handleKeyEvent: ", true );

		Script::setAnswer( ret, handled,
			"SimpleGUIComponent handleKeyEvent retval" );

		// last but not least, try the 
		// mouse button event handler
		if (!handled && SimpleGUI::instance().mouseCursor().isActive() &&
			event.key() >= KeyEvent::KEY_MINIMUM_MOUSE && 
			event.key() <= KeyEvent::KEY_MAXIMUM_MOUSE && 
			this->hitTest( event.mousePos() ))
		{
			handled = this->invokeKeyEventHandler( 
				this->pScriptObject_.getObject(), 
				"handleMouseButtonEvent", event,
				NULL, "SimpleGUIComponent::handleMouseButtonEvent: ",
				"EventsSimpleGUIComponent handleMouseButtonEvent retval" );
		}
	}

	return handled;
}


/*~ function SimpleGUIComponent.handleMouseEvent
 *
 *	This method handles mouse events for the gui system. If this component 
 *	doesn't have focus, then it returns 0, meaning it hasnt handled the event.  
 *	Otherwise, it gives its children a chance to handle the event. If none of
 *	it's children consumes the event, it checks if it has an attached script 
 *	object, and gives that a chance to handle the event.
 *
 *	@param	event	a mouseEvent is a 3-tuple of integers, as follows: 
 *					(dx, dy, dz), where dx, dy an dz are the distance 
 *					the mouse has moved in the x, y and z dimensions, 
 *					respectively.
 *
 *	@return			True if the event was handled, False if it wasn't.
 */
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
bool SimpleGUIComponent::handleMouseEvent( const SimpleGUIMouseEvent & event )
{
	bool handled = false;

	ChildRecVector::iterator it = children_.begin();
	ChildRecVector::iterator end = children_.end();

	while (it != end)
	{
		SimpleGUIComponentPtr c = it->second;
		it++;

		if ( c->hitTest( event.mousePos() ) )
		{
			handled = c->handleMouseEvent( event );
			if (handled)
				break;
		}
	}

	// OK, the event is ours. Pass it on 
	// to a script object if we have one.
	if (this->moveFocus() && !handled && pScriptObject_ &&
		this->hitTest( event.mousePos() ))
	{
		handled = this->invokeMouseEventHandler( 
			this->pScriptObject_.getObject(), 
			"handleMouseEvent", event.mousePos(), 
			NULL, "SimpleGUIComponent::handleMouseEvent: ",
			"EventsSimpleGUIComponent handleMouseEvent retval" );
	}

	return handled;
}


/*~ function SimpleGUIComponent.handleAxisEvent
 *
 *	This method handles axis events for the gui system. If this component
 *	doesn't have focus, then it returns 0, meaning it hasnt handled the event.
 *	Otherwise, it gives its children a chance to handle the event.  If they
 *	don't, then it checks if it has an attached script object, and gives that a
 *	chance to handle the event.
 *
 *	@param	event	a axisEvent is a 3-tuple of one integer and two floats, as
 *					follows: (axis, value, dTime) - axis is one of AXIS_LX,
 *					AXIS_LY, AXIS_RX, AXIS_RY, with the first letter being L or
 *					R meaning left thumbstick or right thumbstick, the second,
 *					X or Y being the direction.  value is the position of that
 *					axis, between -1 and 1. 	dTime is the time since that
 *					axis was last processed.
 *
 *	@return			True if the event was handled, False if it wasn't.
 */
/**
 *	This method handles mouse events for the gui system.
 *
 *	Axis events are passed on to the whatever component(s)
 *	has(have) the focus.
 *
 *	@param event	The mouse event to handle.
 *
 *	@return True if the mouse event was handled.
 */
bool SimpleGUIComponent::handleAxisEvent( const AxisEvent & event )
{
	bool handled = false;

	//do this before or after we ask the script?
	ChildRecVector::iterator it = children_.begin();
	ChildRecVector::iterator end = children_.end();

	while (it != end)
	{
		SimpleGUIComponentPtr c = it->second;
		it++;

		handled = c->handleAxisEvent( event );
		if (handled)
			break;
	}

	// OK, the event is ours. Pass it on 
	// to a script object if we have one.
	if (this->focus() && !handled && pScriptObject_)
	{
		PyObject * pResult = Script::ask(
			PyObject_GetAttrString( pScriptObject_.getObject(),
				"handleAxisEvent" ),
			Script::getData(event),
			"SimpleGUIComponent::handleAxisEvent: ",
			true );

		Script::setAnswer( pResult, handled,
			"SimpleGUIComponent::handleAxisEvent" );
	}

	return handled;
}


/*~ function SimpleGUIComponent.handleMouseEnterEvent
 *
 *	This event handler is triggered when the mouse cursor enters the 
 *	quad defined by the four corners of the component. To have this
 *	handler triggered, the component must have crossFocus enabled.
 *
 *	@param	component	The component which the mouse point is entering.
 *	@param	position	mouse position, 2-tuple (x, y).
 *
 *	@return	True if the event was handled, False if it wasn't.
 */
bool SimpleGUIComponent::handleMouseEnterEvent( 
		const SimpleGUIMouseEvent & event ) 
{
	return this->invokeMouseEventHandler( 
		this->pScriptObject_.getObject(), 
		"handleMouseEnterEvent", event.mousePos(), 
		NULL, "SimpleGUIComponent::handleMouseEnterEvent: ",
		"EventsSimpleGUIComponent handleMouseEnterEvent retval" );
}


/*~ function SimpleGUIComponent.handleMouseLeaveEvent
 *
 *	This event handler is triggered when the mouse cursor leaves the 
 *	quad defined by the four corners of the component. To have this
 *	handler triggered, the component must have crossFocus enabled.
 *
 *	@param	component	The component which the mouse point is leaving.
 *	@param	position	mouse position, 2-tuple (x, y).
 *
 *	@return	True if the event was handled, False if it wasn't.
 */
bool SimpleGUIComponent::handleMouseLeaveEvent( 
		const SimpleGUIMouseEvent & event ) 
{
	return this->invokeMouseEventHandler( 
		this->pScriptObject_.getObject(), 
		"handleMouseLeaveEvent", event.mousePos(), 
		NULL, "SimpleGUIComponent::handleMouseLeaveEvent: ",
		"EventsSimpleGUIComponent handleMouseLeaveEvent retval" );
}


/*~ function SimpleGUIComponent.handleMouseClickEvent
 *
 *	This event handler is triggered when the a left mouse click occurs.
 *	A click is defined by a mouse button down, followed by a button up
 *	over the same component. To have this handler triggered, a component 
 *	must have focus enabled. 
 *	
 *	A button down followed by a button up may not be detected as a click 
 *	if the component has dragFocus enabled and the distance from the 
 *	position of press to that of the release is more than the minimum 
 *	drag distance (see GUI.setDragDistance). 
 *
 *	Because a click is always preceeded by a mouse button press, a mouse 
 *	button down event will always be triggered on the clicked component. 
 *	Sometimes, it may be necessary to implement the handleMouseButtonEvent 
 *	and have it to return True in order to prevent the mouse down event to 
 *	be propagated to the game scripts.
 *
 *	@param	component	The component being clicked.
 *	@param	position	mouse position of button up, 2-tuple (x, y).
 *
 *	@return	True if the event was handled, False if it wasn't.
 */
bool SimpleGUIComponent::handleMouseClickEvent( 
		const SimpleGUIKeyEvent & event )
{
	return this->invokeMouseEventHandler( 
		this->pScriptObject_.getObject(), 
		"handleMouseClickEvent", event.mousePos(), 
		NULL, "SimpleGUIComponent::handleMouseClickEvent: ",
		"EventsSimpleGUIComponent handleMouseClickEvent retval" );
}


/*~ function SimpleGUIComponent.handleDragStartEvent
 *
 *	This event handler is triggered when a draggable component is first 
 *	detected as being dragged. A component is detected as being dragged 
 *	when a left mouse button down event is generated on it and the mouse 
 *	cursor travels more and the minimun drag distance before the left 
 *	button is released (see GUI.setDragDistance). To have this handler 
 *	triggered, a component must have dragFocus enabled. 
 *
 *	Because a drag is always preceeded by a mouse button press, a mouse 
 *	button down event will always be triggered on the clicked component. 
 *	Some times, it may be necessary to implement the handleMouseButtonEvent 
 *	and have it to return True in order to prevent the mouse down event to 
 *	be propagated further to the game scripts.
 *
 *	@param	component	The component being dragged.
 *	@param	position	mouse position of first button down, 2-tuple (x, y).
 *
 *	@return	True if the component is willing to be dragged.
 */
bool SimpleGUIComponent::handleDragStartEvent( const SimpleGUIKeyEvent & event )
{
	return this->invokeMouseEventHandler( 
		this->pScriptObject_.getObject(), 
		"handleDragStartEvent", event.mousePos(), 
		NULL, "SimpleGUIComponent::handleDragStartEvent: ",
		"EventsSimpleGUIComponent handleDragStartEvent retval" );
}


/*~ function SimpleGUIComponent.handleDragStopEvent
 *
 *	This event handler is triggered when a drag operation has finished.
 *	The drag operation finishes when the left mouse button is released.
 *	To have this handler triggered, a component must have dragFocus enabled.
 *
 *	@param	component	The component being dragged.
 *	@param	position	mouse position, 2-tuple (x, y).
 *
 *	@return	the return value is always ignored. 
 */
bool SimpleGUIComponent::handleDragStopEvent( const SimpleGUIKeyEvent & event )
{
	return this->invokeMouseEventHandler( 
		this->pScriptObject_.getObject(), 
		"handleDragStopEvent", event.mousePos(), 
		NULL, "SimpleGUIComponent::handleDragStopEvent: ",
		"EventsSimpleGUIComponent handleDragStopEvent retval" );
}


/*~ function SimpleGUIComponent.handleDropEvent
 *
 *	This event handler is triggered a dragged component is dropped overa drop 
 *	accepting component. To have this handler triggered, a component must have 
 *	dropFocus enabled.
 *
 *	@param	component	The drop target component.
 *	@param	position	mouse position, 2-tuple (x, y).
 *	@param	dropped		The dragged component being dropped.
 *
 *	@return	the return value is always ignored. 
 */
bool SimpleGUIComponent::handleDropEvent( SimpleGUIComponent * dragged, 
		const SimpleGUIKeyEvent & event )
{
	return this->invokeMouseEventHandler( 
		this->pScriptObject_.getObject(), 
		"handleDropEvent", event.mousePos(), dragged, 
		"SimpleGUIComponent::handleDropEvent: ",
		"EventsSimpleGUIComponent handleDropEvent retval" );
}


/*~ function SimpleGUIComponent.handleDragEnterEvent
 *
 *	This event handler is triggered when a dragged component enters the 
 *	quad defined by the four corners of a drop accepting component. To have 
 *	this handler triggered, a component must have dropFocus enabled.
 *
 *	The value returned by the handleDragEnterEvent method will be used to
 *	determine if the drop target is willing to accept the component being 
 *	dragged. That is, the handleDropEvent will not be generated if the
 *	handleDragEnterEvent call that preceeded it returned false.
 *
 *	@param	component	The drop target component.
 *	@param	position	mouse position, 2-tuple (x, y).
 *	@param	dragged		The component being dragged.
 *
 *	@return	True if the component is willing to accept the dropped item.
 */
bool SimpleGUIComponent::handleDragEnterEvent( SimpleGUIComponent * dragged, 
	const SimpleGUIMouseEvent & event )
{
	return this->invokeMouseEventHandler( 
		this->pScriptObject_.getObject(), 
		"handleDragEnterEvent", event.mousePos(), dragged, 
		"SimpleGUIComponent::handleDragEnterEvent: ",
		"EventsSimpleGUIComponent handleDragEnterEvent retval" );
}


/*~ function SimpleGUIComponent.handleDragLeaveEvent
 *
 *	This event handler is triggered when a dragged component leaves the 
 *	quad defined by the four corners of a drop accepting component. To have 
 *	this handler triggered, a component must have dropFocus enabled.
 *
 *	@param	component	The drop target component.
 *	@param	position	mouse position, 2-tuple (x, y).
 *	@param	dragged		The component being dragged.
 *
 *	@return	True if the event was handled, False if it wasn't.
 */
bool SimpleGUIComponent::handleDragLeaveEvent( SimpleGUIComponent * dragged, 
	const SimpleGUIMouseEvent & event )
{
	return this->invokeMouseEventHandler( 
		this->pScriptObject_.getObject(), 
		"handleDragLeaveEvent", event.mousePos(), dragged, 
		"SimpleGUIComponent::handleDragLeaveEvent: ",
		"EventsSimpleGUIComponent handleDragLeaveEvent retval" );
}


/**
 *	Invokes an event handler method in the specified PyObject using
 *	the the MouseEvent signature, with a optional dragged component 
 *	argument (target component, mouse position [, dragged component]).
 *
 *	@param	pEventHandler		the script object to handle the event.
 *	@param	methodName			the name of the method to handle the event.
 *	@param	mousePos			mouse position to be passed as argument.
 *	@param	dragged				dragged component to be passed (NULL for none).
 *	@param	callErrorPrefix		error prefix to be used if call fails.
 *	@param	returnErrorPrefix	error prefix to be used if return is invalid.
 *
 *	@return	the value returned by the event handler method.
 */
bool SimpleGUIComponent::invokeMouseEventHandler( 
		PyObject * pEventHandler, const char * methodName, 
		const Vector2 & mousePos, SimpleGUIComponent * dragged, 
		const char * callErrorPrefix, const char * returnErrorPrefix )
{
	bool handled = false;

	if (pEventHandler != NULL)
	{
		// tuple will steal a reference
		Py_INCREF( this ); 

		PyObject* args = PyTuple_New( dragged != NULL ? 3 : 2 );
		PyTuple_SetItem( args, 0, this );
		PyTuple_SetItem( args, 1, Script::getData( mousePos ) );
		if (dragged != NULL)
		{
			Py_INCREF( dragged ); 
			PyTuple_SetItem( args, 2, dragged );
		}

		PyObject * ret = Script::ask( 
			PyObject_GetAttrString( pEventHandler, const_cast< char* >( methodName ) ), 
			args, callErrorPrefix, true );

		Script::setAnswer( ret, handled, returnErrorPrefix );
	}

	return handled;
}


/**
 *	Invokes an event handler method in the specified PyObject using
 *	the the KeyEvent signature, with a optional dragged component 
 *	argument (target component, key, key state, key modifiers, mouse 
 *	position [, dragged component]).
 *
 *	@param	pEventHandler		the script object to handle the event.
 *	@param	methodName			the name of the method to handle the event.
 *	@param	event				key event data.
 *	@param	dragged				dragged component to be passed (NULL for none).
 *	@param	callErrorPrefix		error prefix to be used if call fails.
 *	@param	returnErrorPrefix	error prefix to be used if return is invalid.
 *
 *	@return	the value returned by the event handler method.
 */
bool SimpleGUIComponent::invokeKeyEventHandler( 
		PyObject * pEventHandler, const char * methodName, 
		const SimpleGUIKeyEvent & event, SimpleGUIComponent * dragged, 
		const char * callErrorPrefix, const char * returnErrorPrefix )
{
	bool handled = false;

	if (pEventHandler != NULL)
	{
		// tuple will steal a reference
		Py_INCREF( this ); 

		PyObject* args = PyTuple_New( dragged != NULL ? 6 : 5 );
		PyTuple_SetItem( args, 0, this );
		PyTuple_SetItem( args, 1, Script::getData( event.key() ) );
		PyTuple_SetItem( args, 2, Script::getData( event.isKeyDown() ) );
		PyTuple_SetItem( args, 3, Script::getData( event.modifiers() ) );
		PyTuple_SetItem( args, 4, Script::getData( event.mousePos() ) );
		if (dragged != NULL)
		{
			Py_INCREF( dragged ); 
			PyTuple_SetItem( args, 5, dragged );
		}


		PyObject * ret = Script::ask( 
			PyObject_GetAttrString( pEventHandler, const_cast< char* >( methodName ) ), 
			args, callErrorPrefix, true );

		Script::setAnswer( ret, handled, returnErrorPrefix );
	}

	return handled;
}


/**
 *	This method returns the bounds of the component, in clip coordinates,
 *	adjusted by the anchor points.  It does not apply the run time transform
 *	to the bounds, so you can treat this method as returning local space bounds.
 */
void SimpleGUIComponent::clipBounds( Vector2& topLeft,
									 Vector2& topRight,
									 Vector2& botLeft,
									 Vector2& botRight,
									 float* out_relativeParentWidth, 
									 float* out_relativeParentHeight ) const
{
	float relativeParentWidth, relativeParentHeight;
	this->nearestRelativeDimensions( relativeParentWidth, relativeParentHeight );

	float clipWidth, clipHeight;
	this->layout( relativeParentWidth, relativeParentHeight, topLeft.x, 
				topLeft.y, clipWidth, clipHeight );

	//get four corners of component
	topRight = topLeft;
	topRight.x += clipWidth;
	botRight = topRight;
	botRight.y -= clipHeight;
	botLeft = topLeft;
	botLeft.y -= clipHeight;

	if (out_relativeParentWidth)
	{
		*out_relativeParentWidth = relativeParentWidth;
	}

	if (out_relativeParentHeight)
	{
		*out_relativeParentHeight = relativeParentHeight;
	}
}


/**
 *	This method checks if the given position is over the component.
 *	Because the test takes the component runTimeTransform to compute the
 *	projected screen coordinates of the component, it works even if the 
 *	component is in 3D space (attacked to a model via PyGUIAttachment).
 *
 *	@param	testPos	the position to test
 *
 *	@return	True if the position is over the component. False otherwise.
 *
 *	@todo : hierarchy support
 */
bool SimpleGUIComponent::hitTest( const Vector2 & testPos ) const
{	
	Vector2 topLeft, topRight, botLeft, botRight;
	this->clipBounds( topLeft, topRight, botLeft, botRight );

	//project using last world-view-projection transform
	const float &z = position_.z;
	Vector3 p[] = {
		runTimeTransform_.applyPoint( Vector3( topLeft.x, topLeft.y, z ) ),
		runTimeTransform_.applyPoint( Vector3( topRight.x, topRight.y, z ) ),
		runTimeTransform_.applyPoint( Vector3( botRight.x, botRight.y, z ) ),
		runTimeTransform_.applyPoint( Vector3( botLeft.x, botLeft.y, z ) ) };

	//do hit test of mouse coords against projected component quad
	const Vector3 vk( 0, 0, 1 );
	const Vector3 mp( testPos.x, testPos.y, 0 );
	float sig1 = ( mp-p[3] ).crossProduct( p[0]-p[3] ).dotProduct( vk );
	for (int i=0; i<3; ++i) 
	{
		float sig2 = ( mp-p[i] ).crossProduct( p[i+1]-p[i] ).dotProduct( vk );
		if (sig1 * sig2 <= 0)
		{
			return false;
		}
	}

	//make sure point is within the clipping region
	if (SimpleGUI::instance().pushClipRegion( runTimeClipRegion_ ))
	{
		bool inside = SimpleGUI::instance().isPointInClipRegion( testPos );
		SimpleGUI::instance().popClipRegion();
		return inside;
	}

	return false;
}


/**
 *	This method calculates the draw order of the component and its children
 *	recursively.
 */
void SimpleGUIComponent::calcDrawOrder()
{
	if (!SimpleGUI::instance().isSimpleGUIComponentInTree( this ))
	{
		return;
	}
	if (calcDrawOrderRecursively( drawOrder_, nextDrawOrder_ ) == 0)
	{
		SimpleGUI::instance().recalcDrawOrders();
	}
}


/**
 *	This method calculates the draw order of the component and its children
 *	recursively.
 *
 *	@param	drawOrder	the drawOrder of this component
 *	@param	nextDrawOrder	the drawOrder of the next component after this or
 *							0 if we aren't limited.
 *
 *	@return		next drawOrder available after this component, or 0 if we
 *					ran out of drawOrders (ie hit nextDrawOrder)
 */
uint32 SimpleGUIComponent::calcDrawOrderRecursively( uint32 drawOrder,
												  uint32 nextDrawOrder )
{
	drawOrder_ = drawOrder;

	uint32 currDrawOrder = drawOrder + 1;

	// Each child needs at least one drawOrder value for itself
	if( nextDrawOrder > 0 && nextDrawOrder - currDrawOrder < children_.size() )
	{
		return 0;
	}

	std::vector<int>::const_iterator it;
	std::vector<int>::const_iterator end = childOrder_.end();

	for ( it = childOrder_.begin(); it != end; ++it )
	{
		if( nextDrawOrder > 0 && currDrawOrder >= nextDrawOrder )
		{
			return 0;
		}

		SimpleGUIComponentPtr child = (children_.begin() + *it)->second;
		currDrawOrder = child->
			calcDrawOrderRecursively( currDrawOrder, nextDrawOrder );

		if( currDrawOrder == 0 )
		{
			return 0;
		}
	}

	nextDrawOrder_ = currDrawOrder;
	return nextDrawOrder_;
}


/*~ callback SimpleGUIComponent.onLoad
 *
 *	This method is called when the SimpleGUIComponent is loaded,
 *	before any of its children are loaded.
 *
 *	The data section that contains custom script properties is passed
 *	to this callback method.  This is the same data section that is passed
 *	to the save callback method.
 *
 *  To process script upon completion of the load process, use
 *	the onBind callback method instead.
 */
/**
 *	The load method
 */
bool SimpleGUIComponent::load( DataSectionPtr pSect, LoadBindings & bindings )
{
	// load our standard variables
	this->position( pSect->readVector3( "position", this->position() ) );

	// position mode defaults to old behaviour if not specified, for backwards compatability.
	horizontalPositionMode_ = ePositionMode( 
		pSect->readInt( "horizontalPositionMode", int(POSITION_MODE_LEGACY) ) );
	verticalPositionMode_ = ePositionMode( 
		pSect->readInt( "verticalPositionMode", int(POSITION_MODE_LEGACY) ) );

	this->width( pSect->readFloat( "width", this->width() ) );
	this->height( pSect->readFloat( "height", this->height() ) );

	widthMode_  = eSizeMode( pSect->readInt( "widthMode",  int(SIZE_MODE_LEGACY) ) );
	heightMode_ = eSizeMode( pSect->readInt( "heightMode", int(SIZE_MODE_LEGACY) ) );

	this->colour( Colour::getUint32(
		pSect->readVector4( "colour", Colour::getVector4( this->colour() ) ) ));
	this->angle( eRotation( pSect->readInt( "angle", int( this->angle() ) ) ) );
	this->flip( pSect->readInt( "flip", this->flip() ) );
	this->visible( pSect->readBool( "visible", this->visible() ) );

	this->horizontalAnchor( eHAnchor(
		pSect->readInt( "horizontalAnchor", int(this->horizontalAnchor()) ) ) );
	this->verticalAnchor( eVAnchor(
		pSect->readInt( "verticalAnchor", int(this->verticalAnchor()) ) ) );

	this->textureName( pSect->readString( "textureName", this->textureName() ));
	this->materialFX( eMaterialFX(
		pSect->readInt( "materialFX", int(this->materialFX()) ) ) );
	this->filterType( eFilterType(
		pSect->readInt( "filterType", int(this->filterType()) ) ) );
	this->tiled( pSect->readBool( "tiled", this->tiled() ) );
	this->tileWidth( pSect->readInt( "tileWidth", this->tileWidth() ) );
	this->tileHeight( pSect->readInt( "tileHeight", this->tileHeight() ) );

	// these override anything specified by widthMode/heightMode respectively.
	// kept for backwards compatability, it isn't saved to new gui files.
	if (pSect->openSection( "widthInClip" ))
	{
		WARNING_MSG("SimpleGUIComponent::load - widthInClip has been deprecated, use widthMode instead.\n");
		widthMode_ = pSect->readBool( "widthInClip") ? SIZE_MODE_LEGACY : SIZE_MODE_PIXEL;
	}

	if (pSect->openSection( "heightInClip" ))
	{
		WARNING_MSG("SimpleGUIComponent::load - heightInClip has been deprecated, use heightMode instead.\n");
		heightMode_ = pSect->readBool( "heightInClip") ? SIZE_MODE_LEGACY : SIZE_MODE_PIXEL;
	}

	this->pixelSnap( pSect->readBool( "pixelSnap", this->pixelSnap() ) );
	
	
	// focus
	this->focus( pSect->readBool( "focus" ) );
	this->moveFocus( pSect->readBool( "moveFocus" ) );
	this->crossFocus( pSect->readBool( "crossFocus" ) );
	this->dragFocus( pSect->readBool( "dragFocus" ) );
	this->dropFocus( pSect->readBool( "dropFocus" ) );

	// mapping
	if ( nVertices_ == 4 )
	{
		DataSectionPtr mapDS = pSect->openSection( "mapping" );
		if ( mapDS )
		{
			for ( int i = 0; i < 4; i++ )
			{
				char sectName[41];
				sectName[40] = 0;
				bw_snprintf( sectName, 40, "coords%d", i );
				blueprint_[i].uv_ = mapDS->readVector2( sectName );
			}
		}
	}


	// load our script object if we have one
	DataSectionPtr pScSect = pSect->openSection( "script" );
	if (pScSect)
	{
		std::string quotedFactoryStr = pScSect->asString();
		quotedFactoryStr.erase( 0, 1 );
		quotedFactoryStr.erase( quotedFactoryStr.length()-1 );

		PyObject* weakref = PyWeakref_NewProxy(static_cast<PyObject*>(this), NULL);

		PyObject * pNewObj = Script::ask(
			Script::runString( quotedFactoryStr.c_str(), false ),
			Py_BuildValue("(O)", weakref ),
			"SimpleGUIComponent::load (factory) " );

		if (!pNewObj)
		{
			ERROR_MSG( "SimpleGUIComponent::load: "
				"Error occurred running factory string '%s'\n",
				quotedFactoryStr.c_str() );
			// error already printed
		}
		else if (pNewObj == Py_None)
		{
			ERROR_MSG( "SimpleGUIComponent::load: "
				"'None' returned from factory string '%s'\n",
				quotedFactoryStr.c_str() );
			Py_DECREF( pNewObj );
		}
		else
		{
			pScriptObject_ = pNewObj;

			PyObject * pLoader = PyObject_GetAttrString( pNewObj, "onLoad" );
			PyErr_Clear();

			if (pLoader != NULL)
			{
				PyDataSectionPtr pPySect( new PyDataSection( pScSect ), true );
				Script::call(
					pLoader,
					Py_BuildValue( "(O)", (PyObject*)pPySect.getObject() ),
					"SimpleGUIComponent::load (onLoad) " );
			}

			Py_DECREF( pNewObj );
		}
	}

	// load our children
	DataSectionPtr pKids = pSect->openSection( "children" );
	if (pKids)
	{
		for (DataSectionIterator it = pKids->begin(); it != pKids->end(); it++)
		{
			LoadBinding lb;
			lb.name_ = (*it)->sectionName();
			lb.id_ = (*it)->asInt();
			bindings.push_back( lb );
		}
	}

	// load our shaders
	DataSectionPtr pShas = pSect->openSection( "shaders" );
	if (pShas)
	{
		for (DataSectionIterator it = pShas->begin(); it != pShas->end(); it++)
		{
			LoadBinding lb;
			lb.name_ = (*it)->sectionName();
			lb.id_ = (*it)->asInt();
			bindings.push_back( lb );
		}
	}

	return true;
};


/*~ callback SimpleGUIComponent.save
 *
 *	This method is called when the SimpleGUIComponent is saving.
 *	The data section used for saving is passed into the method.
 *	GUI Scripts should save their persistent data into this data section.
 *
 *  To process script upon completion of the load process, use
 *	the onBind callback method instead.
 */
/**
 *	The save method
 */
void SimpleGUIComponent::save( DataSectionPtr pSect, SaveBindings & bindn )
{
	// save our standard variables
	pSect->writeVector3( "position", this->position() );
	pSect->writeInt( "horizontalPositionMode", int(this->horizontalPositionMode()) );
	pSect->writeInt( "verticalPositionMode", int(this->verticalPositionMode()) );
	pSect->writeInt( "widthMode", int(this->widthMode()) );
	pSect->writeFloat( "width", this->width() );
	pSect->writeInt( "heightMode", int(this->heightMode()) );
	pSect->writeFloat( "height", this->height() );
	pSect->writeVector4( "colour", Colour::getVector4( this->colour() ) );
	pSect->writeInt( "angle", int( this->angle() ) );
	pSect->writeInt( "flip", int( this->flip() ) );
	pSect->writeBool( "visible", this->visible() );
	pSect->writeInt( "horizontalAnchor", int(this->horizontalAnchor()) );
	pSect->writeInt( "verticalAnchor", int(this->verticalAnchor()) );
	pSect->writeString( "textureName", this->textureName() );
	pSect->writeInt( "materialFX", int(this->materialFX()) );
	pSect->writeInt( "filterType", int(this->filterType()) );
	pSect->writeBool( "tiled", this->tiled() );
	pSect->writeInt( "tileWidth", this->tileWidth() );
	pSect->writeInt( "tileHeight", this->tileHeight() );
	pSect->writeBool( "pixelSnap", this->pixelSnap() );	

	// focus
	pSect->writeBool( "focus", this->focus() );
	pSect->writeBool( "moveFocus", this->moveFocus() );
	pSect->writeBool( "crossFocus", this->crossFocus() );
	pSect->writeBool( "dragFocus", this->dragFocus() );
	pSect->writeBool( "dropFocus", this->dropFocus() );

	// mapping
	if ( nVertices_ == 4 )
	{
		DataSectionPtr mapDS = pSect->openSection( "mapping", true );
		for ( int i = 0; i < 4; i++ )
		{
			char sectName[41];
			sectName[40] = 0;
			bw_snprintf( sectName, 40, "coords%d", i );
			mapDS->writeVector2( sectName, blueprint_[i].uv_ );
		}
	}

	// save our script object if we have one
	if (pScriptObject_)
	{
		PyObject * pFactoryStr = PyObject_GetAttrString(
			pScriptObject_.getObject(), "factoryString" );
		PyErr_Clear();

		if (pFactoryStr != NULL && PyString_Check( pFactoryStr ))
		{
			std::string quotedFactoryStr = "\"";
			quotedFactoryStr += PyString_AsString( pFactoryStr );
			quotedFactoryStr += "\"";

			DataSectionPtr pScSect = pSect->newSection( "script" );
			pScSect->setString( quotedFactoryStr );

			PyObject * pSaver = PyObject_GetAttrString(
				pScriptObject_.getObject(), "onSave" );
			PyErr_Clear();

			if (pSaver != NULL)
			{
				PyDataSectionPtr pPySect( new PyDataSection( pScSect ), true );
				Script::call(
					pSaver,
					Py_BuildValue( "(O)", (PyObject*)pPySect.getObject() ),
					"SimpleGUIComponent::save " );
			}
		}
	}

	// save our children (!)
	if (!children_.empty())
	{
		DataSectionPtr pKids = pSect->openSection( "children", true );

		ChildRecVector::iterator it;
		for (it = children_.begin(); it != children_.end(); it++)
		{
			SimpleGUIComponent * pChild = it->second.getObject();

			pKids->writeInt( it->first, int( pChild ) );

			std::vector<SimpleGUIComponent *>::iterator bi = std::find(
				bindn.components_.begin(), bindn.components_.end(), pChild );
			if (bi == bindn.components_.end())
				bindn.components_.push_back( pChild );
		}
	}

	// save our shaders
	if (!shaders_.empty())
	{
		DataSectionPtr pShas = pSect->openSection( "shaders", true );

		GUIShaderPtrVector::iterator it;
		for (it = shaders_.begin(); it != shaders_.end(); it++)
		{
			GUIShader * pShader = it->second.getObject();

			pShas->writeInt( it->first, int( pShader ) );

			std::vector<GUIShader *>::iterator bi = std::find(
				bindn.shaders_.begin(), bindn.shaders_.end(), pShader );
			if (bi == bindn.shaders_.end())
				bindn.shaders_.push_back( pShader );
		}
	}
};


/*~ callback SimpleGUIComponent.onBound
 *
 *	This method is called when the SimpleGUIComponent has finished
 *	loading itself and its children.
 *	No parameters are passed into this method.
 *
 *	To process script before the children have loaded, use the
 *	onLoad callback method instead.
 */
/**
 *	The bound method. This is called by SimpleGUI when all our
 *	children and shaders have been added to us.
 */
void SimpleGUIComponent::bound()
{
	if (pScriptObject_)
	{
		Script::call(
			PyObject_GetAttrString( pScriptObject_.getObject(), "onBound" ),
			PyTuple_New(0),
			"SimpleGUIComponent::bound",
			true );
	}
}


void SimpleGUIComponent::boundingBoxAcc( BoundingBox& bb, bool skinny )
{
	//we cheat creating the bounding box, so we don't have
	//to worry about anchor points ( bounding box includes
	//all possible anchor points for the component )
	Vector3 minPt( position() );
	Vector3 maxPt( position() );
	minPt -= Vector3( width(), height(), 0.f );
	maxPt += Vector3( width(), height(), 0.f );

	minPt = runTimeTransform().applyPoint( minPt );
	maxPt = runTimeTransform().applyPoint( maxPt );

	bb.addBounds( minPt );
	bb.addBounds( maxPt );
}


/**
 *	Sets the draw order. This should be called from this class and SimpleGUI
 *	only. Used for handling mouse events.
 *
 *	@param order	Value incremented each time a component is rendered.
 */
void SimpleGUIComponent::drawOrder( uint32 order )
{
	drawOrder_ = order;
}


/**
 *	Gets the draw order. This should be called from this class and SimpleGUI
 *	only. Used for handling mouse events.
 *
 *	@return		draw order of the component according to the last draw.
 */
uint32 SimpleGUIComponent::drawOrder()
{
	return drawOrder_;
}


// simple_gui_component.cpp
