/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef DISTORTION_HPP
#define DISTORTION_HPP

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
 *
 */
class Distortion : public Moo::DeviceCallback, public FullScreenBackBuffer::User,
					public Singleton< Distortion >
{
	//TODO: class layout is similar to the HeatShimmer... perhaps a common base class is in order?
	typedef Distortion This;
	
public:
	Distortion();
	~Distortion();

	///can ever use
	static bool isSupported();

#ifdef EDITOR_ENABLED
	void setEditorEnabled( bool state )			{ editorEnabled_ = state; }
#endif
	
	//FullScreenBackBuffer::User interface.
	bool isEnabled();
	void beginScene();
	void endScene();
	void drawScene();
	bool doTransfer( bool alreadyTransferred );
	void doPostTransferFilter();

	bool init();
	void finz();

	void drawMasks();
	void copyBackBuffer();
	void tick( float dTime );
	void deleteUnmanagedObjects();
	void drawDistortionChannel( bool clear = true );
	bool pushRT()
	{
		if (s_pRenderTexture_ && s_pRenderTexture_->push())
		{
			Moo::rc().beginScene();
			Moo::rc().setViewport( FullScreenBackBuffer::instance().getViewport() );
			return true;
		}
		return false;
	}

	uint drawCount() const;

	void popRT()
	{
		if (s_pRenderTexture_)
		{
			drawMasks();
			Moo::rc().endScene();
			s_pRenderTexture_->pop();
		}
	}
private:
#ifdef EDITOR_ENABLED
	bool							editorEnabled_;
#endif
	bool							inited_;
	bool							watcherEnabled_;
	float							dTime_;
	Moo::VisualPtr					visual_;
	EffectParameterCache			parameters_;
	Moo::EffectMaterialPtr			effectMaterial_;
	static Moo::RenderTargetPtr		s_pRenderTexture_;
	
	typedef Moo::GraphicsSetting::GraphicsSettingPtr GraphicsSettingPtr;
	GraphicsSettingPtr distortionSettings_;
	void setDistortionOption(int) {}
};

#endif //DISTORTION_HPP