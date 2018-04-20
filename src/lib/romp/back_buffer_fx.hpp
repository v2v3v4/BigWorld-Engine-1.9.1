/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BACK_BUFFER_FX_HPP
#define BACK_BUFFER_FX_HPP

#include "cstdmf/aligned.hpp"
#include "moo/device_callback.hpp"
#include "romp/custom_mesh.hpp"
#include "moo/moo_dx.hpp"
#include "moo/render_target.hpp"

#include "back_buffer_copy.hpp"
#include "transfer_mesh.hpp"

namespace Moo
{
	class Material;
	class RenderTarget;

}


/**
 *	This class renders a back buffer special effect.
 *	A BackBufferCopy, DistortionMesh and PixelShader are used.
 */
class BackBufferEffect : public Moo::DeviceCallback
{
public:
	BackBufferEffect( int w = 64, int h = 32 );
	~BackBufferEffect();

	virtual bool init();
	virtual void finz();	

	virtual void finalInit(){};

	void createUnmanagedObjects()		{ init(); }
	void deleteUnmanagedObjects()		{ finz(); }

	Moo::RenderTarget* renderTarget()	{ return rt0_; }

	virtual void draw();
	void areaOfEffect( const Vector2& tl, const Vector2& dimensions );

protected:

	//multiplatform helper
	static bool loadVertexShader( const std::string& resourceStub, DX::VertexShader** result );
	static bool loadPixelShader( const std::string& resourceStub, DX::PixelShader** result );

	virtual void grabBackBuffer();
	virtual void setRenderState();
	virtual void applyEffect();
	virtual void endDraw();
	
	Moo::Material* material_;
	Moo::RenderTarget* rt0_;
	
	DX::Viewport vp_;

	uint32 renderTargetWidth_;
	uint32 renderTargetHeight_;

	BackBufferCopy* bbc_;
	TransferMesh* transferMesh_;
	DX::PixelShader* pixelShader_;
	DX::VertexShader* vertexShader_;

	Vector2 tl_;
	Vector2 dimensions_;

	bool inited_;
	
	BackBufferEffect( const BackBufferEffect& );
	BackBufferEffect& operator=( const BackBufferEffect& );
};



#ifdef CODE_INLINE
#include "back_buffer_fx.ipp"
#endif

#endif // BACK_BUFFER_FX_HPP
