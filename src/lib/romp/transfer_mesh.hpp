/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TRANSFER_MESH_HPP
#define TRANSFER_MESH_HPP

#include "romp/custom_mesh.hpp"
#include "math/vector2.hpp"
#include "moo/dynamic_vertex_buffer.hpp"
#include "moo/com_object_wrap.hpp"
#include "moo/moo_dx.hpp"
#include "moo/index_buffer.hpp"
#include "moo/vertex_formats.hpp"


/**
 *	This interface transfers the texture onto the screen.
 */
class TransferMesh
{
public:
	virtual ~TransferMesh()	{};
	virtual void setIndices() = 0;
	virtual void draw( const Vector2& tl, const Vector2& dimensions, const Vector2& uvDimensions, bool useEffect = false ) = 0;
};


/**
 *	This simple transfer copies from one buffer to another.
 */
class SimpleTransfer : public TransferMesh
{
public:
	SimpleTransfer();
	virtual void setIndices();
	virtual void draw( const Vector2& tl, const Vector2& dimensions, const Vector2& uvDimensions, bool useEffect = false );
private:
	CustomMesh<Moo::VertexTUV> screenCopyMesh_;
};


/**
 *	This transfer mesh smears 4 surrounding pixels
 */
class Smear : public TransferMesh
{
public:
	Smear();
	virtual void setIndices();
	virtual void draw( const Vector2& tl, const Vector2& dimensions, const Vector2& uvDimensions, bool useEffect = false );
private:
	CustomMesh<Moo::VertexUV4> screenCopyMesh_;
};


/**
 *	This class uses a wobbly mesh to distort a texture and draw it on the screen.
 */
class DistortionMesh : public TransferMesh
{
public:
	DistortionMesh( int w = 64, int h = 48 );
	virtual void createIndexBuffer();
	void deleteIndexBuffer();

	virtual void setIndices();
	virtual void draw( const Vector2& tl, const Vector2& dimensions, const Vector2& uvDimensions, bool useEffect = false );

	virtual Moo::DynamicVertexBuffer< Moo::VertexTUV >& create( const Vector2& tl, const Vector2& dim, const Vector2& uvDimensions );
	Moo::IndexBuffer indexBuffer()	{ return indexBuffer_; }

protected:
	Moo::IndexBuffer indexBuffer_;

	int	nVertsX_;
	int nVertsY_;
	int nVerts_;
	int nIndicesX_;
	int nIndicesY_;
	int nIndices_;
	float xDivisor_;
	float yDivisor_;
};


/**
 *	This class uses a shimmering mesh to distort a texture and draw it on the screen.
 */
class ShimmerMesh : public DistortionMesh
{
public:
	ShimmerMesh( int w = 64, int h = 48 );
	virtual Moo::DynamicVertexBuffer< Moo::VertexTUV >& create( const Vector2& tl, const Vector2& dim, const Vector2& uvDimensions );
};


#endif