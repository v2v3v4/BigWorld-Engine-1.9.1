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
#include "transfer_mesh.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 0 );


// -----------------------------------------------------------------------------
// Section: class DistortionMesh
// -----------------------------------------------------------------------------


/**
 *	Constructor.
 */
DistortionMesh::DistortionMesh( int x, int y )
{
	nVertsX_ = x+1;
	nVertsY_ = y+1;
	nIndicesX_ = nVertsX_ + 1;
	nIndicesY_ = y;
	nIndices_ = nIndicesX_ * nIndicesY_ * 2 * 2;
	xDivisor_ = (float)(nVertsX_-1);
	yDivisor_ = (float)(nVertsY_-1);
	nVerts_ = nVertsX_ * nVertsY_;

	this->createIndexBuffer();
}


void DistortionMesh::createIndexBuffer()
{
	HRESULT hr = indexBuffer_.create( nIndices_ / 2, D3DFMT_INDEX16, D3DUSAGE_WRITEONLY, D3DPOOL_DEFAULT );

	if (SUCCEEDED(hr))
	{
		Moo::IndicesReference ir = indexBuffer_.lock( 0 );
		int offset = 0;
		if (ir.valid())
		{
			uint16 lastIndex = 0;
			for (int y = 0; y < nIndicesY_; y++)
			{
				uint16 rowIndex = y * nVertsX_;
				ir[offset++] = lastIndex;
				ir[offset++] = rowIndex + nVertsX_;
				for (int x = 0; x < nVertsX_; x++)
				{
					lastIndex = x + rowIndex;
					ir[offset++] = lastIndex + nVertsX_;
					ir[offset++] = lastIndex;
				}
			}

			indexBuffer_.unlock();
		}
		else
		{
			CRITICAL_MSG( "DistortionMesh::createIndexBuffer: Unable to lock index buffer" );
		}
	}
	else
	{
		CRITICAL_MSG( "DistortionMesh::createIndexBuffer: Unable create index buffer" );
	}
}


void DistortionMesh::deleteIndexBuffer()
{
	indexBuffer_.release();
}


void DistortionMesh::setIndices()
{
	indexBuffer().set();
}


///dim is the output dimensions
Moo::DynamicVertexBuffer< Moo::VertexTUV >&
DistortionMesh::create( const Vector2& tl, const Vector2& dim, const Vector2& uvDimensions )
{
	Moo::DynamicVertexBuffer< Moo::VertexTUV >& vb = Moo::DynamicVertexBuffer< Moo::VertexTUV >::instance();
	Moo::VertexTUV* pVerts = vb.lock( nVerts_ );

	float xPosStep = dim[0] / xDivisor_;
	float xUVStep = uvDimensions[0] / xDivisor_;

	float yPosStep = float( dim[1] ) / yDivisor_;
	float yUVStep = float ( uvDimensions[1] ) / yDivisor_;

	Vector2 pos( tl );
	Vector2 uv( 0.f, 0.f );
	for (int y = 0; y < nVertsY_; y++)
	{
		pos[0] = tl[0];
		uv[0] = 0.f;

		for (int x = 0; x < nVertsX_; x++)
		{
			pVerts->pos_.set( pos[0], pos[1], 0, 1 );
			pVerts->uv_.set( uv[0], uv[1] );

			pos[0] += xPosStep;
			uv[0] += xUVStep;

			pVerts++;
		}

		pos[1] += yPosStep;
		uv[1] += yUVStep;
	}
	vb.unlock();

	return vb;
}


void DistortionMesh::draw( const Vector2& tl, const Vector2& dimensions, const Vector2& uvDimensions, bool useEffect )
{
	//create an output (distortion) mesh
	Moo::DynamicVertexBuffer< Moo::VertexTUV >&vb = 
		this->create( tl, dimensions, uvDimensions );

	vb.set();

	//copy to back buffer, using distortion mesh
	Moo::rc().drawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, 0, nVerts_, 0, (nIndicesX_ * 2 * nIndicesY_) - 2 );
}


static float s_shimmerPower = 8;
static float s_shimmerSpread = 2.f;

ShimmerMesh::ShimmerMesh( int w, int h )
:DistortionMesh( w,h )
{	
}


Moo::DynamicVertexBuffer< Moo::VertexTUV >& ShimmerMesh::create( const Vector2& tl, const Vector2& dim, const Vector2& uvDimensions )
{
	Moo::DynamicVertexBuffer< Moo::VertexTUV >& vb = Moo::DynamicVertexBuffer< Moo::VertexTUV >::instance();
	Moo::VertexTUV* pVerts = vb.lock( nVerts_ );

	float xPosStep = dim[0] / xDivisor_;
	float xUVStep = uvDimensions[0] / xDivisor_;

	float yPosStep = float( dim[1] ) / yDivisor_;
	float yUVStep = float ( uvDimensions[1] ) / yDivisor_;

	float slowFactor = powf( 2.f, s_shimmerPower );

	Vector2 pos( tl );
	Vector2 uv( 0.f, 0.f );
	for (int y = 0; y < nVertsY_; y++)
	{
		pos[0] = tl[0];
		uv[0] = 0.f;

		for (int x = 0; x < nVertsX_; x++)
		{
			pVerts->pos_.set( pos[0], pos[1], 0, 1 );
			float t = (float)::GetTickCount();
			t /= slowFactor;
			pVerts->uv_.set( uv[0], uv[1] + ( cosf(t+(float)y) * s_shimmerSpread ) );

			pos[0] += xPosStep;
			uv[0] += xUVStep;

			pVerts++;
		}

		pos[1] += yPosStep;
		uv[1] += yUVStep;
	}
	vb.unlock();

	return vb;
}


/**
 *	Section - SimpleTransfer
 */
SimpleTransfer::SimpleTransfer()
:screenCopyMesh_( D3DPT_TRIANGLESTRIP )
{
	//create the screen copy mesh
	Moo::VertexTUV v;
	v.pos_ = Vector4( 0.f, 0.f, 0.f, 1.f );
	screenCopyMesh_.resize(4);
	for (uint i = 0; i < 4; i++) screenCopyMesh_[i] = v;
}


void SimpleTransfer::setIndices()
{
	Moo::rc().setIndices( NULL );
}


void SimpleTransfer::draw( const Vector2& tl, const Vector2& dim, const Vector2& uvDimensions, bool useEffect )
{
	//mesh position is to ( where do we render to in the back buffer )
	*(Vector2*)(&screenCopyMesh_[0].pos_) = tl;
	screenCopyMesh_[1].pos_.x = tl.x + dim.x;
	screenCopyMesh_[1].pos_.y = tl.y;
	screenCopyMesh_[2].pos_.x = tl.x;
	screenCopyMesh_[2].pos_.y = tl.y + dim.y;
	*(Vector2*)(&screenCopyMesh_[3].pos_) = tl + dim;

	//mesh UV is from ( where in the texture do we render from )
	screenCopyMesh_[0].uv_.set(0.f,0.f);
	screenCopyMesh_[1].uv_.set(uvDimensions[0],0.f);
	screenCopyMesh_[2].uv_.set(0.f,uvDimensions[1]);
	screenCopyMesh_[3].uv_.set(uvDimensions[0],uvDimensions[1]);

	if ( useEffect )
		screenCopyMesh_.drawEffect();
	else
		screenCopyMesh_.draw();
}


static float s_bloomFetchStretch = 4.f;

/**
 *	Section - Smear
 */
Smear::Smear()
:screenCopyMesh_( D3DPT_TRIANGLESTRIP )
{
	//create the screen copy mesh
	Moo::VertexUV4 v;
	v.pos_ = Vector4( 0.f, 0.f, 0.f, 1.f );
	screenCopyMesh_.resize(4);
	for (uint i = 0; i < 4; i++) screenCopyMesh_[i] = v;
}


void Smear::setIndices()
{
	Moo::rc().setIndices( NULL );
}


void Smear::draw( const Vector2& tl, const Vector2& dim, const Vector2& uvDimensions, bool useEffect )
{
	//mesh position is to ( where do we render to in the back buffer )
	*(Vector2*)(&screenCopyMesh_[0].pos_) = tl;
	screenCopyMesh_[1].pos_.x = tl.x + dim.x;
	screenCopyMesh_[1].pos_.y = tl.y;
	screenCopyMesh_[2].pos_.x = tl.x;
	screenCopyMesh_[2].pos_.y = tl.y + dim.y;
	*(Vector2*)(&screenCopyMesh_[3].pos_) = tl + dim;

	//mesh UV is from ( where in the texture do we render from )
	float stretch = s_bloomFetchStretch / 4.f;
	for ( int x=0; x<4; x++ )
	{
		Vector2 offset;
		switch(x)
		{
		case 0:
			offset.set(-stretch,-stretch);
			break;
		case 1:
			offset.set(stretch,-stretch);
			break;
		case 2:
			offset.set(stretch,stretch);
			break;
		case 3:
			offset.set(-stretch,stretch);
			break;
		}
		
		screenCopyMesh_[0].uv_[x] = Vector2(0.f,0.f) + offset;
		screenCopyMesh_[1].uv_[x] = Vector2(uvDimensions[0],0.f) + offset;
		screenCopyMesh_[2].uv_[x] = Vector2(0.f,uvDimensions[1]) + offset;
		screenCopyMesh_[3].uv_[x] = Vector2(uvDimensions[0],uvDimensions[1]) + offset;
	}

	if ( useEffect )
		screenCopyMesh_.drawEffect();
	else
		screenCopyMesh_.draw();
}
