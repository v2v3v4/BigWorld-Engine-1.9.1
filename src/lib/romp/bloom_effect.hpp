/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BLOOM_EFFECT_HPP
#define BLOOM_EFFECT_HPP

#include "cstdmf/singleton.hpp"

#include "effect_parameter_cache.hpp"
#include "full_screen_back_buffer.hpp"
#include "moo/graphics_settings.hpp"
#include "pyscript/script_math.hpp"

class BackBufferCopy;
class TransferMesh;

namespace Moo
{
	class RenderTarget;
	class EffectMaterial;
};

namespace DX
{
	typedef IDirect3DBaseTexture9	BaseTexture;
}

/**
 * TODO: to be documented.
 */
struct FilterSample {
	float fCoefficient;
	float fOffset;
};

enum filterModes
{
	GAUSS_4X4 = 1,
	GAUSS_24X24 = 3
};

/**
 *	This isolates the bright areas of the screen,
 *	and then smudges them back over themselves,
 *	creating a blur that encroaches on neighbouring
 *	pixels.
 */
class Bloom : public Moo::DeviceCallback, public FullScreenBackBuffer::User, public Moo::EffectManager::IListener,
					public Singleton< Bloom >
{
	typedef Bloom This;
public:	
	~Bloom();
	Bloom();
	
	static bool isSupported();
	bool init();
	void fini();

	void onSelectPSVersionCap(int psVerCap);

#ifdef EDITOR_ENABLED
	void setEditorEnabled( bool state )			{ editorEnabled_ = state; }
#endif

	bool isEnabled();
	void beginScene()	{};
	void endScene()		{};
	bool doTransfer( bool alreadyTransferred )	{ return false; }
	void doPostTransferFilter();
	void justBlur( bool state );
	void applyPreset( bool blurOnly, int filterMode , float colourAtten, int nPasses );
	static void bloomController( Vector4ProviderPtr p );
	PY_AUTO_MODULE_STATIC_METHOD_DECLARE( RETVOID, bloomController, ARG( Vector4ProviderPtr, END ) )
	static void bloomColourAttenuation( Vector4ProviderPtr p );
	PY_AUTO_MODULE_STATIC_METHOD_DECLARE( RETVOID, bloomColourAttenuation, ARG( Vector4ProviderPtr, END ) )

	void draw();
	void deleteUnmanagedObjects();

private:
	
	bool initInternal();
	void finzInternal();
	bool safeCreateRenderTarget( Moo::RenderTargetPtr& rt,
								int width,
								int height,
								bool reuseZ,
								const std::string& name );
	bool safeCreateEffect( Moo::EffectMaterialPtr& mat,
							const std::string& effectName );

	void	captureBackBuffer();
	void	downSample( DX::BaseTexture* pSrc,
						Moo::EffectMaterial& mat,
						EffectParameterCache& matCache );
	void	filterCopy( DX::BaseTexture* pSrc,
						DWORD dwSamples,
						FilterSample rSample[],
						bool filterX );

	Moo::EffectMaterialPtr downSample_;
	Moo::EffectMaterialPtr downSampleColourScale_;
	Moo::EffectMaterialPtr gaussianBlur_;
	Moo::EffectMaterialPtr colourScale_;
	Moo::EffectMaterialPtr transfer_;
	EffectParameterCache downSampleParameters_;
	EffectParameterCache downSampleColourScaleParameters_;
	EffectParameterCache gaussianParameters_;
	EffectParameterCache colourScaleParameters_;
	EffectParameterCache transferParameters_;
	bool inited_;
#ifdef EDITOR_ENABLED
	bool editorEnabled_;
#endif
	bool watcherEnabled_;	
	BackBufferCopy* bbc_;
	TransferMesh* transferMesh_;
	Moo::RenderTargetPtr	rt0_;
	Moo::RenderTargetPtr	rt1_;
	Moo::RenderTargetPtr	wasteOfMemory_;
	uint32 renderTargetWidth_;
	uint32 renderTargetHeight_;

	Vector2	sourceDimensions_;
	int		filterMode_;
	int		bbWidth_;
	int		bbHeight_;
	int		srcWidth_;
	int		srcHeight_;
	Vector4	colourAttenuation_;
	float	scalePower_;
	float	cutoff_;
	float	width_;
	bool	bloomBlur_;
	int		nPasses_;	
	Vector4ProviderPtr controller_;
	Vector4ProviderPtr colourAttenuationController_;

	typedef Moo::GraphicsSetting::GraphicsSettingPtr GraphicsSettingPtr;
	GraphicsSettingPtr bloomSettings_;
	void setBloomOption(int) {}
};

#endif
