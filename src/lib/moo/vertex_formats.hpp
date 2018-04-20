/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef VERTEX_FORMATS_HPP
#define VERTEX_FORMATS_HPP

#include "moo_math.hpp"

#ifdef MF_SERVER
#define FVF( FORMAT )
#else
#define FVF( FORMAT ) static int fvf() { return FORMAT; }
#endif


namespace Moo
{

#pragma pack(push, 1 )
	/**
	 * Unpack compressed normal into a three float vector.
	 */
	inline Vector3 unpackNormal( uint32 packed )
	{
		int32 z = int32(packed) >> 22;
		int32 y = int32( packed << 10 ) >> 21;
		int32 x = int32( packed << 21 ) >> 21;

		return Vector3( float( x ) / 1023.f, float( y ) / 1023.f, float( z ) / 511.f );
	}

	/**
	 * Pack three float normal (each component clamped to [-1,1]) into a single
	 * unsigned 32bit word ( 11 bits x, 11 bits y, 10 bits z )
	 */
	inline uint32 packNormal( const Vector3& nn )
	{
		Vector3 n = nn;
		n.normalise();

		n.x = Math::clamp(-1.f, n.x, 1.f);
		n.y = Math::clamp(-1.f, n.y, 1.f);
		n.z = Math::clamp(-1.f, n.z, 1.f);


		return	( ( ( (uint32)(n.z * 511.0f) )  & 0x3ff ) << 22L ) |
				( ( ( (uint32)(n.y * 1023.0f) ) & 0x7ff ) << 11L ) |
				( ( ( (uint32)(n.x * 1023.0f) ) & 0x7ff ) <<  0L );
	}

	/**
	 * Position, Normal, UV
	 */
	struct VertexXYZNUV
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;

		FVF( D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1 )
	};


	/**
	* Position, Normal, UV, UV2
	*/
	struct VertexXYZNUV2
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;
        Vector2		uv2_;

		FVF( D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX2 )
	};

	/**
	* Position, Normal, Colour, UV
	*/
	struct VertexXYZNDUV
	{
		Vector3		pos_;
		Vector3		normal_;
		DWORD		colour_;
		Vector2		uv_;

		FVF( D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX1 )
	};

	/**
	* Position, Normal
	*/
	struct VertexXYZN
	{
		Vector3		pos_;
		Vector3		normal_;

		FVF( D3DFVF_XYZ|D3DFVF_NORMAL )
	};

	/**
	* Position, Normal, Colour
	*/
	struct VertexXYZND
	{
		Vector3		pos_;
		Vector3		normal_;
		DWORD		colour_;

		FVF( D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE )
	};

	/**
	* Position, Normal, Colour, Specular
	*/
	struct VertexXYZNDS
	{
		Vector3		pos_;
		Vector3		normal_;
		DWORD		colour_;
		DWORD		specular_;

		FVF( D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_SPECULAR )
	};

	/**
	* Position, Colour
	*/
	struct VertexXYZL
	{
		Vector3		pos_;
		DWORD		colour_;

		FVF( D3DFVF_XYZ|D3DFVF_DIFFUSE )
	};

	/**
	* Position only
	*/
	struct VertexXYZ
	{
		Vector3		pos_;

		FVF( D3DFVF_XYZ )
	};

	/**
	* Position, UV
	*/
	struct VertexXYZUV
	{
		Vector3		pos_;
		Vector2		uv_;

		FVF( D3DFVF_XYZ|D3DFVF_TEX1 )
	};

	/**
	* Position, UV, UV2
	*/
    struct VertexXYZUV2
    {
	    Vector3		pos_;
	    Vector2		uv_;
	    Vector2		uv2_;

	    FVF( D3DFVF_XYZ|D3DFVF_TEX2 )
    };

	/**
	* Position, UV, UV2, UV3, UV4
	*/
	struct VertexUV4
	{
		Vector4		pos_;
		Vector2		uv_[4];

		FVF( D3DFVF_XYZRHW|D3DFVF_TEX4 )
	};

	/**
	* Position, Colour, UV
	*/
	struct VertexXYZDUV
	{
		Vector3		pos_;
		DWORD		colour_;
		Vector2		uv_;

		FVF( D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1 )
	};

	/**
	* Position, Colour, UV, UV2
	*/
	struct VertexXYZDUV2
	{
		Vector3		pos_;
		DWORD		colour_;
		Vector2		uv_;
		Vector2		uv2_;

		FVF( D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX2 )
	};

	/**
	* Normal, UV
	*/
	struct VertexNUV
	{
		Vector3		normal_;
		Vector2		uv_;

		FVF( D3DFVF_NORMAL|D3DFVF_TEX1 )
	};

	/**
	 * Normal only
	 */
	struct VertexN
	{
		Vector3		normal_;

		FVF( D3DFVF_NORMAL )
	};

	/**
	* UV only
	*/
	struct VertexUV
	{
		Vector2		uv_;

		FVF( D3DFVF_TEX1 )
	};

	/**
	* Position, Normal, UV, Index
	*/
	struct VertexXYZNUVI
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;
		float		index_;
	};

	/**
	 * Y, Normal
	 */
	struct VertexYN
	{
		float		y_;
		Vector3		normal_;
	};

	/**
	 * Four-component position, Colour
	 */
	struct VertexTL
	{
		Vector4		pos_;
		DWORD		colour_;

		FVF( D3DFVF_XYZRHW|D3DFVF_DIFFUSE )
	};

	/**
	* Four-component position, UV
	*/
	struct VertexTUV
	{
		Vector4		pos_;
		Vector2		uv_;
		FVF( D3DFVF_XYZRHW|D3DFVF_TEX1 )
	};

	/**
	* Four-component position, Colour, UV
	*/
	struct VertexTLUV
	{
		Vector4		pos_;
		DWORD		colour_;
		Vector2		uv_;
		FVF( D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1 )
	};

	/**
	* Four-component position, Colour, Specular, UV, UV2
	*/
	struct VertexTDSUV2
	{
		Vector4		pos_;
		DWORD		colour_;
		DWORD		specular_;
		Vector2		uv_;
		Vector2		uv2_;

		FVF( D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX2 )
	};

	/**
	* Position, Colour, Specular, UV
	*/
	struct VertexXYZDSUV
	{
		float	x;
		float	y;
		float	z;
		uint32	colour;
		uint32	spec;
		float	tu;
		float	tv;

		FVF( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1 )
	};

	/**
	* Position, Colour, Specular, UVUUVV, UVUUVV
	*/
	struct VertexTDSUVUUVV2
	{
		Vector4		pos_;
		DWORD		colour_;
		DWORD		specular_;
		Vector4		uvuuvv_;
		Vector4		uvuuvv2_;
	};

	/**
	* Position, Normal, UV, Packed Tangent, Packed Binormal
	*/
	struct VertexXYZNUVTB
	{
		Vector3		pos_;
		uint32		normal_;
		Vector2		uv_;
		uint32		tangent_;
		uint32		binormal_;
	};

	/**
	* Position, Normal, UV, Tangent, Binormal
	*/
	struct VertexXYZNUVTBPC
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;
		Vector3		tangent_;
		Vector3		binormal_;
		VertexXYZNUVTBPC& operator =(const VertexXYZNUVTB& in)
		{
			pos_ = in.pos_;
			normal_ = unpackNormal( in.normal_ );
			uv_ = in.uv_;
			tangent_ = unpackNormal( in.tangent_ );
			binormal_ = unpackNormal( in.binormal_ );
			return *this;
		}
		VertexXYZNUVTBPC(const VertexXYZNUVTB& in)
		{
			*this = in;
		}
		VertexXYZNUVTBPC& operator =(const VertexXYZNUV& in)
		{
			pos_ = in.pos_;
			normal_ = in.normal_;
			uv_ = in.uv_;
			tangent_.setZero();
			binormal_.setZero();
			return *this;
		}
		VertexXYZNUVTBPC(const VertexXYZNUV& in)
		{
			*this = in;
		}
		VertexXYZNUVTBPC()
		{
		}
	};

	/**
	* Position, Normal, UVx2, Packed Tangent, Packed Binormal
	*/
	struct VertexXYZNUV2TB
	{
		Vector3		pos_;
		uint32		normal_;
		Vector2		uv_;
		Vector2		uv2_;
		uint32		tangent_;
		uint32		binormal_;
	};


	/**
	* Position, Normal, UVx2, Tangent, Binormal
	*/
	struct VertexXYZNUV2TBPC
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;
		Vector2		uv2_;
		Vector3		tangent_;
		Vector3		binormal_;
		VertexXYZNUV2TBPC& operator =(const VertexXYZNUV2TB& in)
		{
			pos_ = in.pos_;
			normal_ = unpackNormal( in.normal_ );
			uv_ = in.uv_;
			uv2_ = in.uv2_;
			tangent_ = unpackNormal( in.tangent_ );
			binormal_ = unpackNormal( in.binormal_ );
			return *this;
		}
		VertexXYZNUV2TBPC(const VertexXYZNUV2TB& in)
		{
			*this = in;
		}
		VertexXYZNUV2TBPC& operator =(const VertexXYZNUV2& in)
		{
			pos_ = in.pos_;
			normal_ = in.normal_;
			uv_ = in.uv_;
			uv2_ = in.uv2_;
			tangent_.setZero();
			binormal_.setZero();
			return *this;
		}
		VertexXYZNUV2TBPC(const VertexXYZNUV2& in)
		{
			*this = in;
		}
		VertexXYZNUV2TBPC()
		{
		}
	};

	/**
	* Position, Normal, UV, Index, Packed Tangent, Packed Binormal
	*/
	struct VertexXYZNUVITB
	{
		Vector3		pos_;
		uint32		normal_;
		Vector2		uv_;
		float		index_;
		uint32		tangent_;
		uint32		binormal_;
	};

	/**
	* Position, Normal, UV, Index, Tangent, Binormal
	*/
	struct VertexXYZNUVITBPC
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;
		float		index_;
		Vector3		tangent_;
		Vector3		binormal_;
		VertexXYZNUVITBPC& operator =(const VertexXYZNUVITB& in)
		{
			pos_ = in.pos_;
			normal_ = unpackNormal( in.normal_ );
			uv_ = in.uv_;
			index_ = in.index_;
			tangent_ = unpackNormal( in.tangent_ );
			binormal_ = unpackNormal( in.binormal_ );
			return *this;
		}
		VertexXYZNUVITBPC(const VertexXYZNUVITB& in)
		{
			*this = in;
		}
		VertexXYZNUVITBPC()
		{
		}
	};

	/**
	* Position, Normal, UV, index, index2, index3, weight, weight2
	*/
	struct VertexXYZNUVIIIWW
	{
		Vector3		pos_;
		uint32		normal_;
		Vector2		uv_;
		uint8		index_;
		uint8		index2_;
		uint8		index3_;
		uint8		weight_;
		uint8		weight2_;
		VertexXYZNUVIIIWW& operator =(const VertexXYZNUVI& in)
		{
			pos_ = in.pos_;
			normal_ = packNormal(in.normal_);
			uv_ = in.uv_;
			weight_ = 255;
			weight2_ = 0;
			index_ = (uint8)in.index_;
			index2_ = index_;
			index3_ = index_;
			return *this;
		}
	};

	/**
	* Position, Normal, UV, index, index2, index3, weight, weight2, 
	*	packed tangent, packed binormal.
	*/
	struct VertexXYZNUVIIIWWTB
	{
		Vector3		pos_;
		uint32		normal_;
		Vector2		uv_;
		uint8		index_;
		uint8		index2_;
		uint8		index3_;
		uint8		weight_;
		uint8		weight2_;
		uint32		tangent_;
		uint32		binormal_;
		VertexXYZNUVIIIWWTB& operator =(const VertexXYZNUVITB& in)
		{
			pos_ = in.pos_;
			normal_ = in.normal_;
			uv_ = in.uv_;
			weight_ = 255;
			weight2_ = 0;
			index_ = (uint8)in.index_;
			index2_ = index_;
			index3_ = index_;
			tangent_ = in.tangent_;
			binormal_ = in.binormal_;
			return *this;
		}
	};

	/**
	* Position, Normal, UV, index3, index2, index, padding, weight2, weight1, 
	*	padding.
	*/
	struct VertexXYZNUVIIIWWPC
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;
		uint8		index3_;
		uint8		index2_;
		uint8		index_;
		uint8		pad_;
		uint8		pad2_;
		uint8		weight2_;
		uint8		weight_;
		uint8		pad3_;
		VertexXYZNUVIIIWWPC& operator =(const VertexXYZNUVIIIWW& in)
		{
			pos_ = in.pos_;
			normal_ = unpackNormal( in.normal_ );
			uv_ = in.uv_;
			weight_ = in.weight_;
			weight2_ = in.weight2_;
			index_ = in.index_;
			index2_ = in.index2_;
			index3_ = in.index3_;
			return *this;
		}
		VertexXYZNUVIIIWWPC& operator =(const VertexXYZNUVI& in)
		{
			pos_ = in.pos_;
			normal_ = in.normal_;
			uv_ = in.uv_;
			weight_ = 255;
			weight2_ = 0;
			index_ = (uint8)in.index_;
			index2_ = index_;
			index3_ = index_;
			return *this;
		}
		VertexXYZNUVIIIWWPC(const VertexXYZNUVIIIWW& in)
		{
			*this = in;
		}
		VertexXYZNUVIIIWWPC()
		{
		}
	};

	/**
	* Position, Normal, UV, index3, index2, index, padding, weight2, weight, 
	*	padding, packed tangent, packed binormal.
	*/
	struct VertexXYZNUVIIIWWTBPC
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;
		uint8		index3_;
		uint8		index2_;
		uint8		index_;
		uint8		pad_;
		uint8		pad2_;
		uint8		weight2_;
		uint8		weight_;
		uint8		pad3_;
		Vector3		tangent_;
		Vector3		binormal_;
		VertexXYZNUVIIIWWTBPC& operator =(const VertexXYZNUVIIIWWTB& in)
		{
			pos_ = in.pos_;
			normal_ = unpackNormal( in.normal_ );
			uv_ = in.uv_;
			weight_ = in.weight_;
			weight2_ = in.weight2_;
			index_ = in.index_;
			index2_ = in.index2_;
			index3_ = in.index3_;
			tangent_ = unpackNormal( in.tangent_ );
			binormal_ = unpackNormal( in.binormal_ );
			return *this;
		}
		VertexXYZNUVIIIWWTBPC& operator =(const VertexXYZNUVITB& in)		
		{
			pos_ = in.pos_;
			normal_ = unpackNormal( in.normal_ );
			uv_ = in.uv_;
			weight_ = 255;
			weight2_ = 0;
			index_ = (uint8)in.index_;
			index2_ = index_;
			index3_ = index_;
			tangent_ = unpackNormal( in.tangent_ );
			binormal_ = unpackNormal( in.binormal_ );			
			return *this;
		}
		VertexXYZNUVIIIWWTBPC(const VertexXYZNUVIIIWWTB& in)
		{
			*this = in;
		}
		VertexXYZNUVIIIWWTBPC()
		{
		}
	};

	struct VertexTex7
	{
		Vector4		tex1;
		Vector4		tex2;
		Vector4		tex3;
		Vector4		tex4;

		Vector4		tex5;
		Vector4		tex6;
		Vector4		tex7;

		static DWORD fvf() { return 0;  }

		static std::string decl()
		{
			return "xyznuv8tb";
		}
	};

	/**
	 * Y, Normal, Diffuse, Shadow
	 */
	struct VertexYNDS
	{
		float		y_;
		uint32		normal_;
		uint32		diffuse_;
		uint16		shadow_;
	};

	/**
	 * 16 bit U, 16 bit V
	 */
	struct VertexUVXB
	{
		int16		u_;
		int16		v_;
	};

	/**
	 * Position, Colour, float size
	 */
	struct VertexXYZDP
	{
		Vector3		pos_;
		uint32		colour_;
		float		size_;

		FVF( D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_PSIZE )
	};
#pragma pack( pop )

}
#endif
