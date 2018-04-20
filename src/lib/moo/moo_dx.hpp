/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MOO_DX_HPP
#define MOO_DX_HPP

#include <cstdmf/stdmf.hpp>

#include <d3d9.h>
#include <d3d9types.h>
#include <string>

namespace DX
{
	typedef IDirect3D9				Interface;
	typedef IDirect3DDevice9		Device;
	typedef IDirect3DResource9		Resource;
	typedef IDirect3DBaseTexture9	BaseTexture;
	typedef IDirect3DTexture9		Texture;
	typedef IDirect3DCubeTexture9	CubeTexture;
	typedef IDirect3DSurface9		Surface;
	typedef IDirect3DVertexBuffer9	VertexBuffer;
	typedef IDirect3DIndexBuffer9	IndexBuffer;
	typedef IDirect3DPixelShader9	PixelShader;
	typedef IDirect3DVertexShader9	VertexShader;
	typedef IDirect3DVertexDeclaration9 VertexDeclaration;
	typedef IDirect3DQuery9			Query;
	typedef D3DLIGHT9				Light;
	typedef D3DVIEWPORT9			Viewport;
	typedef D3DMATERIAL9			Material;
	uint32	surfaceSize( const D3DSURFACE_DESC& desc );
	uint32	textureSize( const Texture *texture );
	std::string errorAsString( HRESULT hr );
}

#endif // MOO_DX_HPP
