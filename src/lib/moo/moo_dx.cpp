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
#include "moo_dx.hpp"
#include <map>


uint32	DX::surfaceSize( const D3DSURFACE_DESC& desc )
{
	float scale = 0.0f;
	switch(desc.Format)
	{
		case D3DFMT_R8G8B8:			scale =  3.0f; break;
		case D3DFMT_A8R8G8B8:		scale =  4.0f; break;
		case D3DFMT_X8R8G8B8:		scale =  4.0f; break;
		case D3DFMT_R5G6B5:			scale =  2.0f; break;
		case D3DFMT_X1R5G5B5:		scale =  2.0f; break;
		case D3DFMT_A1R5G5B5:		scale =  2.0f; break;
		case D3DFMT_A4R4G4B4:		scale =  2.0f; break;
		case D3DFMT_R3G3B2:			scale =  1.0f; break;
		case D3DFMT_A8:				scale =  1.0f; break;
		case D3DFMT_A8R3G3B2:		scale =  2.0f; break;
		case D3DFMT_X4R4G4B4:		scale =  2.0f; break;
		case D3DFMT_A2B10G10R10:	scale =  4.0f; break;
		case D3DFMT_A8B8G8R8:		scale =  4.0f; break;
		case D3DFMT_X8B8G8R8:		scale =  4.0f; break;
		case D3DFMT_G16R16:			scale =  4.0f; break;
		case D3DFMT_A2R10G10B10:	scale =  4.0f; break;
		case D3DFMT_A16B16G16R16:	scale =  8.0f; break;
		case D3DFMT_A8P8:			scale =  2.0f; break;
		case D3DFMT_P8:				scale =  1.0f; break;
		case D3DFMT_L8:				scale =  1.0f; break;
		case D3DFMT_A8L8:			scale =  2.0f; break;
		case D3DFMT_A4L4:			scale =  1.0f; break;
		case D3DFMT_V8U8:			scale =  2.0f; break;
		case D3DFMT_L6V5U5:			scale =  2.0f; break;
		case D3DFMT_X8L8V8U8:		scale =  4.0f; break;
		case D3DFMT_Q8W8V8U8:		scale =  4.0f; break;
		case D3DFMT_V16U16:			scale =  4.0f; break;
		case D3DFMT_A2W10V10U10:	scale =  4.0f; break;
		case D3DFMT_UYVY:			scale =  1.0f; break;
		case D3DFMT_R8G8_B8G8:		scale =  2.0f; break;
		case D3DFMT_YUY2:			scale =  1.0f; break;
		case D3DFMT_G8R8_G8B8:		scale =  2.0f; break;
		case D3DFMT_DXT1: 			scale =  0.5f; break;
		case D3DFMT_DXT2: 			scale =  1.0f; break;
		case D3DFMT_DXT3: 			scale =  1.0f; break;
		case D3DFMT_DXT4: 			scale =  1.0f; break;
		case D3DFMT_DXT5: 			scale =  1.0f; break;
		case D3DFMT_D16_LOCKABLE:	scale =  2.0f; break;
		case D3DFMT_D32:			scale =  4.0f; break;
		case D3DFMT_D15S1: 			scale =  2.0f; break;
		case D3DFMT_D24S8: 			scale =  4.0f; break;
		case D3DFMT_D24X8: 			scale =  4.0f; break;
		case D3DFMT_D24X4S4:		scale =  4.0f; break;
		case D3DFMT_D16:			scale =  2.0f; break;
		case D3DFMT_D32F_LOCKABLE:	scale =  4.0f; break;
		case D3DFMT_D24FS8:			scale =  4.0f; break;
		case D3DFMT_L16:			scale =  2.0f; break;
		case D3DFMT_Q16W16V16U16: 	scale =  8.0f; break;
		case D3DFMT_MULTI2_ARGB8: 	scale =  0.0f; break;
		case D3DFMT_R16F:			scale =  2.0f; break;
		case D3DFMT_G16R16F:		scale =  4.0f; break;
		case D3DFMT_A16B16G16R16F:	scale =  8.0f; break;
		case D3DFMT_R32F:			scale =  4.0f; break;
		case D3DFMT_G32R32F:		scale =  8.0f; break;
		case D3DFMT_A32B32G32R32F:	scale = 16.0f; break;
		case D3DFMT_CxV8U8:			scale =  2.0f; break;
	}

	return uint32(scale * (float)(desc.Width * desc.Height));
}


uint32 DX::textureSize( const Texture *constTexture )
{
	BW_GUARD;
	if (constTexture == NULL)
		return 0;

	// The operations on the texture are really const operations
	Texture *texture = const_cast<Texture *>(constTexture);

	// Determine the mip-map texture size scaling factor
	double mipmapScaler = ( 4 - pow(0.25, (double)texture->GetLevelCount() - 1 ) ) / 3;

	// Get a surface to determine the width, height, and format
	D3DSURFACE_DESC surfaceDesc;
	texture->GetLevelDesc(0, &surfaceDesc);

	// Get the surface size
	uint32 surfaceSize = DX::surfaceSize(surfaceDesc);

	// Track memory usage
	return (uint32)(surfaceSize * mipmapScaler);
}


#define ERRORSTRING( code, ext )											\
	if (hr == code)															\
	{																		\
		bw_snprintf( res, sizeof(res), #code "(0x%08x) : %s", code, ext );	\
	}


std::string DX::errorAsString( HRESULT hr )
{
	char res[1024];
	     ERRORSTRING( D3D_OK, "No error occurred." )
	else ERRORSTRING( D3DOK_NOAUTOGEN, "This is a success code. However, the autogeneration of mipmaps is not supported for this format. This means that resource creation will succeed but the mipmap levels will not be automatically generated." )
	else ERRORSTRING( D3DERR_CONFLICTINGRENDERSTATE, "The currently set render states cannot be used together." )
	else ERRORSTRING( D3DERR_CONFLICTINGTEXTUREFILTER, "The current texture filters cannot be used together." )
	else ERRORSTRING( D3DERR_CONFLICTINGTEXTUREPALETTE, "The current textures cannot be used simultaneously." )
	else ERRORSTRING( D3DERR_DEVICELOST, "The device has been lost but cannot be reset at this time. Therefore, rendering is not possible." )
	else ERRORSTRING( D3DERR_DEVICENOTRESET, "The device has been lost but can be reset at this time." )
	else ERRORSTRING( D3DERR_DRIVERINTERNALERROR, "Internal driver error. Applications should destroy and recreate the device when receiving this error. For hints on debugging this error, see Driver Internal Errors (Direct3D 9)." )
	else ERRORSTRING( D3DERR_DRIVERINVALIDCALL, "Not used." )
	else ERRORSTRING( D3DERR_INVALIDCALL, "The method call is invalid. For example, a method's parameter may not be a valid pointer." )
	else ERRORSTRING( D3DERR_INVALIDDEVICE, "The requested device type is not valid." )
	else ERRORSTRING( D3DERR_MOREDATA, "There is more data available than the specified buffer size can hold." )
	else ERRORSTRING( D3DERR_NOTAVAILABLE, "This device does not support the queried technique." )
	else ERRORSTRING( D3DERR_NOTFOUND, "The requested item was not found." )
	else ERRORSTRING( D3DERR_OUTOFVIDEOMEMORY, "Direct3D does not have enough display memory to perform the operation." )
	else ERRORSTRING( D3DERR_TOOMANYOPERATIONS, "The application is requesting more texture-filtering operations than the device supports." )
	else ERRORSTRING( D3DERR_UNSUPPORTEDALPHAARG, "The device does not support a specified texture-blending argument for the alpha channel." )
	else ERRORSTRING( D3DERR_UNSUPPORTEDALPHAOPERATION, "The device does not support a specified texture-blending operation for the alpha channel." )
	else ERRORSTRING( D3DERR_UNSUPPORTEDCOLORARG, "The device does not support a specified texture-blending argument for color values." )
	else ERRORSTRING( D3DERR_UNSUPPORTEDCOLOROPERATION, "The device does not support a specified texture-blending operation for color values." )
	else ERRORSTRING( D3DERR_UNSUPPORTEDFACTORVALUE, "The device does not support the specified texture factor value. Not used; provided only to support older drivers." )
	else ERRORSTRING( D3DERR_UNSUPPORTEDTEXTUREFILTER, "The device does not support the specified texture filter." )
	else ERRORSTRING( D3DERR_WASSTILLDRAWING, "The previous blit operation that is transferring information to or from this surface is incomplete." )
	else ERRORSTRING( D3DERR_WRONGTEXTUREFORMAT, "The pixel format of the texture surface is not valid." )

	else ERRORSTRING( D3DXERR_CANNOTMODIFYINDEXBUFFER, "The index buffer cannot be modified." )
	else ERRORSTRING( D3DXERR_INVALIDMESH, "The mesh is invalid." )
	else ERRORSTRING( D3DXERR_CANNOTATTRSORT, "Attribute sort (D3DXMESHOPT_ATTRSORT) is not supported as an optimization technique." )
	else ERRORSTRING( D3DXERR_SKINNINGNOTSUPPORTED, "Skinning is not supported." )
	else ERRORSTRING( D3DXERR_TOOMANYINFLUENCES, "Too many influences specified." )
	else ERRORSTRING( D3DXERR_INVALIDDATA, "The data is invalid." )
	else ERRORSTRING( D3DXERR_LOADEDMESHASNODATA, "The mesh has no data." )
	else ERRORSTRING( D3DXERR_DUPLICATENAMEDFRAGMENT, "A fragment with that name already exists." )
	else ERRORSTRING( D3DXERR_CANNOTREMOVELASTITEM, "The last item cannot be deleted." )

	else ERRORSTRING( E_FAIL, "An undetermined error occurred inside the Direct3D subsystem." )
	else ERRORSTRING( E_INVALIDARG, "An invalid parameter was passed to the returning function." )
//	else ERRORSTRING( E_INVALIDCALL, "The method call is invalid. For example, a method's parameter may have an invalid value." )
	else ERRORSTRING( E_NOINTERFACE, "No object interface is available." )
	else ERRORSTRING( E_NOTIMPL, "Not implemented." )
	else ERRORSTRING( E_OUTOFMEMORY, "Direct3D could not allocate sufficient memory to complete the call." )
	else ERRORSTRING( S_OK, "No error occurred." )
	else
	{
		bw_snprintf( res, sizeof(res), "Unknown(0x%08x)", hr );
	}

	return std::string(res);
}