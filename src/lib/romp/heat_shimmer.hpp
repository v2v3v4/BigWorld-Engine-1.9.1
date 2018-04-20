/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef HEAT_SHIMMER_HPP
#define HEAT_SHIMMER_HPP

#include "cstdmf/singleton.hpp"

#include "effect_parameter_cache.hpp"
#include "full_screen_back_buffer.hpp"
#include "moo/visual.hpp"
#include "moo/graphics_settings.hpp"
#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"
#include "pyscript/script_math.hpp"

namespace Moo
{
	class EffectMaterial;
};


/**
 *	This class creates a shimmering heat effect.
 */
class HeatShimmer : public Moo::DeviceCallback, public FullScreenBackBuffer::User,
					public Singleton< HeatShimmer >
{
	typedef HeatShimmer This;
	
public:
	HeatShimmer();
	~HeatShimmer();

	///can ever use
	static bool isSupported();

#ifdef EDITOR_ENABLED
	void setEditorEnabled( bool state )			{ editorEnabled_ = state; }
#endif
	
	//FullScreenBackBuffer::User interface.
	bool isEnabled();
	void beginScene();
	void endScene();
	bool doTransfer( bool alreadyTransferred );
	void doPostTransferFilter()	{};

	bool init();
	void finz();	
		
	void draw( float alpha, float wobbliness );
	static void setShimmerStyle( int i );
	PY_AUTO_MODULE_STATIC_METHOD_DECLARE( RETVOID, setShimmerStyle, ARG( int, END ) )
	static void setShimmerAlpha( Vector4ProviderPtr v4 );
	PY_AUTO_MODULE_STATIC_METHOD_DECLARE( RETVOID, setShimmerAlpha, ARG( Vector4ProviderPtr, END ) );
	
	void drawShimmerChannel();
	void setShimmerMaterials(bool status);

	void deleteUnmanagedObjects();

private:
	void					setRenderState();
	bool					inited_;
#ifdef EDITOR_ENABLED
	bool					editorEnabled_;
#endif
	Moo::VisualPtr			visual_;
	EffectParameterCache	parameters_;
	Moo::EffectMaterialPtr	effectMaterial_;	
	bool					watcherEnabled_;
	static Vector4ProviderPtr s_alphaProvider;
	
	typedef Moo::GraphicsSetting::GraphicsSettingPtr GraphicsSettingPtr;
	GraphicsSettingPtr shimmerSettings_;
	void setShimmerOption(int) {}

};

#endif