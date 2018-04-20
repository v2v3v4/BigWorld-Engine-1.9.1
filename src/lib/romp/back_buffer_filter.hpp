/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BACK_BUFFER_FILTER_HPP
#define BACK_BUFFER_FILTER_HPP

#include "moo/device_callback.hpp"
#include "moo/vertex_formats.hpp"
#include "romp/custom_mesh.hpp"
#include "moo/moo_dx.hpp"
#include "moo/com_object_wrap.hpp"

namespace Moo
{
	class Material;
	class RenderTarget;

}

/**
 *	@todo Document this class.
 *	This class implements a very specific full-screen
 *	filtering effect involving greyscale swirls and
 *	motion blur.  This class is no longer supported.
 */
class BackBufferFilter : public Moo::DeviceCallback
{
public:
	~BackBufferFilter();

	void deleteUnmanagedObjects();
	void createUnmanagedObjects();

	void beginScene();
	void endScene();

	static void initInstance();
	static void deleteInstance();
	static BackBufferFilter& instance() { MF_ASSERT( instance_ ); return *instance_; }

private:
	BackBufferFilter();
	
	Moo::Material* material1_;
	Moo::Material* material2_;
	Moo::RenderTarget* rt_[2];
	uint32 currentRenderTarget_;
	DX::PixelShader* pixelShader_;

	CustomMesh<Moo::VertexTUV> copyToScreenMesh_;
	ComObjectWrap<DX::IndexBuffer > indexBuffer_;
	
	DX::Viewport vp_;

	float uSinOffset_;
	float uCosOffset_;
	float vSinOffset_;
	float vCosOffset_;

	uint32 renderTargetWidth_;
	uint32 renderTargetHeight_;


	static BackBufferFilter* instance_;
	
	BackBufferFilter( const BackBufferFilter& );
	BackBufferFilter& operator=( const BackBufferFilter& );
};

#ifdef CODE_INLINE
#include "back_buffer_filter.ipp"
#endif

#endif // BACK_BUFFER_FILTER_HPP
