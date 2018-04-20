/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef VERTEX_BUFFER_HPP
#define VERTEX_BUFFER_HPP

#include "render_context.hpp"
#include "cstdmf/guard.hpp"

namespace Moo
{

/**
 *	This class is a vertex buffer helper class used to create
 *	and fill vertex buffers
 */
class VertexBuffer
{
	ComObjectWrap<DX::VertexBuffer> vertexBuffer_;
public:
	VertexBuffer()
	{}
	HRESULT create( uint32 size, DWORD usage, DWORD FVF, D3DPOOL pool, 
		const char* allocator = "vertex buffer/unknown" )
	{
		BW_GUARD;
		release();
		HRESULT hr;

		ComObjectWrap<DX::VertexBuffer> temp;

		// Try to create the vertexbuffer with maxIndices_ number of indices
		if( SUCCEEDED( hr = Moo::rc().device()->CreateVertexBuffer(
			size, usage, FVF, pool, &temp, NULL ) ) )
		{
			vertexBuffer_ = temp;
			vertexBuffer_.addAlloc(allocator);
		}
		return hr;
	}
	HRESULT set( UINT streamNumber = 0, UINT offsetInBytes = 0, 
		UINT stride = 0 ) const
	{
		BW_GUARD;
		return Moo::rc().device()->SetStreamSource( streamNumber, 
			vertexBuffer_.pComObject(), offsetInBytes, stride );
	}
	bool valid() const
	{
		return vertexBuffer_.pComObject() != NULL;
	}
	void release()
	{
		vertexBuffer_.pComObject( NULL );
	}
	HRESULT lock( UINT offset, UINT size, VOID** data, DWORD flags )
	{
		BW_GUARD;
		return vertexBuffer_.pComObject()->Lock( offset, size, data, flags );
	}
	HRESULT unlock()
	{
		BW_GUARD;
		return vertexBuffer_.pComObject()->Unlock();
	}
	HRESULT getDesc( D3DVERTEXBUFFER_DESC* desc )
	{
		BW_GUARD;
		return vertexBuffer_.pComObject()->GetDesc( desc );
	}

	void preload()
	{
		BW_GUARD;
		vertexBuffer_->PreLoad();
	}

	/**
	 * This method adds the vertex buffer to the preload list in
	 * the render context. This causes the buffer to have its 
	 * preload method called in the next few frames, so that it
	 * is uploaded to video memory. This is only useful for
	 * managed pool resources.
	 */
	void addToPreloadList()
	{
		BW_GUARD;
		if (vertexBuffer_.hasComObject())
		{
			rc().addPreloadResource(vertexBuffer_.pComObject());
		}
	}
};

/**
 *	This class is a helper class that helps with locking vertex buffers
 */
template<typename VertexType>
class VertexLock
{
protected:
	void* vertices_;
	VertexBuffer& vb_;
public:
	VertexLock( VertexBuffer& vb )
		: vertices_( 0 ), vb_( vb )
	{
		if( FAILED( vb.lock( 0, 0, &vertices_, 0 ) ) )
			vertices_ = NULL;// shouldn't it?
	}
	VertexLock( VertexBuffer& vb, UINT offset, UINT size, DWORD flags )
		: vertices_( 0 ), vb_( vb )
	{
		if( FAILED( vb.lock( offset, size, &vertices_, flags ) ) )
			vertices_ = NULL;// shouldn't it?
	}
	~VertexLock()
	{
		if(vertices_)
			vb_.unlock();
	}
	operator void*() const
	{
		return vertices_;
	}
	void fill( const void* buffer, uint32 size )
	{
		memcpy( vertices_, buffer, size );
	}
	void pull( void* buffer, uint32 size ) const
	{
		memcpy( buffer, vertices_, size );
	}
	VertexType& operator[]( int index )
	{
		return ((VertexType*)vertices_)[ index ];
	}
	VertexType& operator[]( int index ) const
	{
		return ((VertexType*)vertices_)[ index ];
	}
};

typedef VertexLock<unsigned char> SimpleVertexLock;

};

#endif//VERTEX_BUFFER_HPP
