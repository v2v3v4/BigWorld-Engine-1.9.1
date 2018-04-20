/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef RENDER_TARGET_HPP
#define RENDER_TARGET_HPP

#include <iostream>
#include "base_texture.hpp"
#include "camera.hpp"
#include "device_callback.hpp"
#include "moo_math.hpp"

namespace Moo
{

typedef SmartPointer< class RenderTarget > RenderTargetPtr;


/**
 *	This class creates and handles a render target that can be used as a texture.
 */
class RenderTarget : public BaseTexture, public DeviceCallback
{
public:
	RenderTarget( const std::string & identitifer );
	~RenderTarget();

	//native methods
	virtual bool create( int width, int height, bool reuseMainZBuffer = false, 
		D3DFORMAT pixelFormat = D3DFMT_A8R8G8B8, 
		RenderTarget* pDepthStencilParent = NULL, D3DFORMAT depthFormatOverride = D3DFMT_UNKNOWN );
	virtual HRESULT release();
	virtual bool push( void );
	virtual void pop( void );
	void	clearOnRecreate( bool enable, const Colour& col = (DWORD)0x00000000 );
	virtual bool valid( );

	//methods inherited from BaseTexture
	virtual DX::BaseTexture*	pTexture( );
	virtual DX::Surface*	depthBuffer( );
	virtual uint32			width( ) const;
	virtual uint32			height( ) const;
	virtual D3DFORMAT		format( ) const;
	virtual uint32			textureMemoryUsed( );
	virtual const std::string& resourceID( ) const;

	//methods inherited from DeviceCallback
	virtual void			deleteUnmanagedObjects( );
	virtual void			createUnmanagedObjects( );

	void setRT2( RenderTarget* rt2 ) { pRT2_ = rt2; }
private:
	uint32				width_;
	uint32				height_;
	
	//temporary implementation of MRT
	RenderTarget* 		pRT2_;

	std::string			resourceID_;

	ComObjectWrap<DX::Texture>		pRenderTarget_;
	ComObjectWrap<DX::Surface>		pDepthStencilTarget_;

	bool				reuseZ_;

	D3DFORMAT			depthFormat_;
	D3DFORMAT			pixelFormat_;
	bool				autoClear_;
	Colour				clearColour_;

	RenderTargetPtr		pDepthStencilParent_;

	RenderTarget(const RenderTarget&);
	RenderTarget& operator=(const RenderTarget&);

	friend std::ostream& operator<<(std::ostream&, const RenderTarget&);
};

}

#ifdef CODE_INLINE
#include "render_target.ipp"
#endif




#endif
/*render_target.hpp*/