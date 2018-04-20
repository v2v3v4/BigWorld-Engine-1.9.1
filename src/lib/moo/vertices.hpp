/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef VERTICES_HPP
#define VERTICES_HPP

#include "moo_math.hpp"
#include "moo_dx.hpp"
#include "vertex_buffer.hpp"
//#include "dxcontext.hpp"
#include "com_object_wrap.hpp"
#include "cstdmf/smartpointer.hpp"
#include "vertex_declaration.hpp"

namespace Moo
{

typedef SmartPointer< class Vertices > VerticesPtr;
typedef SmartPointer< class BaseSoftwareSkinner > BaseSoftwareSkinnerPtr;
typedef SmartPointer< class Node > NodePtr;
typedef std::vector< NodePtr > NodePtrVector;
class VertexSnapshot;
typedef SmartPointer< VertexSnapshot > VertexSnapshotPtr;

// enable this to enable debugging of mesh normals
//#define EDITOR_ENABLED
// NOTE: it's not a robust solution for drawning normals
// so use it at your own risk :P
// also needs to be defined in : "romp\super_model.cpp"

/**
 *  Vertices is the class used to load and access a vertex buffer, with one of 
 *  several Vertex formats. A Vertices object is created from a resourceID. This 
 *	in general is a reference to a subsection of a .primitives file ( for 
 *	example "objects/MyBipedObject.primitive/FeetVertices" ).
 *
 *  A Vertices object is created using the VerticesManager::get function.
 *
 *  Before rendering a primitive based on this set of Vertices, call the 
 *	function setVertices, which will load the verts from the file if necessary, 
 *	and present them to Direct3D as the current StreamSource.
 *
 *  The string description of format of the vertices is specified in the 
 *	resource header.
 *
 *  See vertex_formats.hpp for a list of valid formats.
 *
 */
class Vertices  : public SafeReferenceCount
{
public:
	Vertices( const std::string& resourceID, int numNodes );
	virtual ~Vertices();

	virtual HRESULT		load( );	
	virtual HRESULT		release( );
	virtual HRESULT		setVertices( bool software, bool staticLighting = false );
	virtual HRESULT		setTransformedVertices( bool tb, 
		const NodePtrVector& nodes );

	virtual VertexSnapshotPtr	getSnapshot( const NodePtrVector& nodes, bool skinned, bool bumpMapped );
	virtual VertexSnapshotPtr	getSnapshot( const std::avector<Matrix>& transforms, bool skinned, bool bumpMapped );

	const std::string&	resourceID( ) const;
	void				resourceID( const std::string& resourceID );
	uint32				nVertices( ) const;

	const std::string&	format( ) const;	

	uint32				vertexStride() const { return vertexStride_; };

	Moo::VertexBuffer	vertexBuffer( ) const;

	typedef std::vector< Vector3 > VertexPositions;
	virtual const VertexPositions& vertexPositions() { return vertexPositions_; };
	#ifdef EDITOR_ENABLED
	typedef std::vector< uint32 > VertexNormals;
	virtual const VertexNormals& vertexNormals() { return vertexNormals_; };
	virtual const VertexPositions& vertexNormals2() { return vertexNormals2_; };
	virtual const VertexNormals& vertexNormals3() { return vertexNormals3_; };
	#endif //EDITOR_ENABLED
	const VertexDeclaration* pDecl() const {return pDecl_; };
	void				clearSoftwareSkinner() { pSkinnerVertexBuffer_.release(); }
	BaseSoftwareSkinner*	softwareSkinner() { return pSoftwareSkinner_.getObject(); }

	bool resaveHardskinnedVertices();

	// return true if the format has 'bumped' info.
	bool bumpedFormat() const
	{ 
		return (format() == "xyznuvtb" ||
				format() == "xyznuviiiwwtb" ||
				format() == "xyznuvitb");
	}

protected:
	bool openSourceFiles( DataSectionPtr& pPrimFile, BinaryPtr& vertices, std::string& partName );

	Moo::VertexBuffer  vertexBuffer_;
	VertexDeclaration* pDecl_;
	VertexDeclaration* pStaticDecl_;
	uint32			nVertices_;
	std::string		resourceID_;

	std::string		format_;
	uint32			vertexStride_;
	mutable VertexPositions vertexPositions_;
#ifdef EDITOR_ENABLED	
	mutable VertexNormals vertexNormals_;
	mutable VertexPositions vertexNormals2_;
	mutable VertexNormals vertexNormals3_;
#endif //EDITOR_ENABLED
	BaseSoftwareSkinnerPtr pSoftwareSkinner_;

	Moo::VertexBuffer	pSkinnerVertexBuffer_;
	bool				vbBumped_;

	//  This parameter is used to verify bone indices against the number of
	//  bones. A value less or equal to zero means no verification is done.
	int					numNodes_;

	/**
	 * This class holds an array of vertices on the heap, destroying them when
	 * object goes out of scope.
	 */
	template<typename VertexType>
	class ScopedVertexArray
	{
	public:
		ScopedVertexArray() : ptr_( NULL )
		{
		}
		~ScopedVertexArray()
		{
			delete [] ptr_;
		}
		VertexType* init( const VertexType* src, int count )
		{
			ptr_ = new VertexType[ count ];
			memcpy( ptr_, src, sizeof(*ptr_)*count );
			return ptr_;
		}
	private:
		VertexType* ptr_;
	};

	template<typename VertexType>
	bool verifyIndices1( typename VertexType* vertices, int numVertices )
	{
		BW_GUARD;
		if ( numNodes_ <= 0 )
			return true;

		bool ok = true;
		for ( int i = 0; i < numVertices; ++i )
		{
			if ( vertices[i].index_ < 0 || vertices[i].index_ >= numNodes_ )
			{
				vertices[i].index_ = 0;
				ok = false;
			}
		}
		return ok;
	}

	template<typename VertexType>
	bool verifyIndices3( typename VertexType* vertices, int numVertices )
	{
		BW_GUARD;
		if ( numNodes_ <= 0 )
			return true;

		bool ok = true;
		for ( int i = 0; i < numVertices; ++i )
		{
			if ( vertices[i].index_ < 0 || vertices[i].index_ >= numNodes_ )
			{
				vertices[i].index_ = 0;
				ok = false;
			}

			if ( vertices[i].index2_ < 0 || vertices[i].index2_ >= numNodes_ )
			{
				vertices[i].index2_ = 0;
				ok = false;
			}

			if ( vertices[i].index3_ < 0 || vertices[i].index3_ >= numNodes_ )
			{
				vertices[i].index3_ = 0;
				ok = false;
			}
		}
		return ok;
	}
};

/**
 *	This is the interface used to store vertex state for delayed rendering.
 */
class VertexSnapshot : public ReferenceCount
{
public:
	virtual ~VertexSnapshot() {}

	/**
	 * This method is called by the delayed renderer (within channel) to enable
	 * the triangles to be sorted.
	 * returns true if the resource has been updated 
	 *		(so we know if we can the last buffer again or not)
	 */
	virtual bool getVertexDepths( uint32 startVertex, uint32 nVertices, 
								float* pOutDepths ) = 0;

	/**
	 * This method is called by the delayed renderer to prepare vertices for
	 * drawing.
	 */
	virtual uint32 setVertices( uint32 startVertex, uint32 nVertices, 
								bool staticLighting ) = 0;

	/**
	 * This method is used to update the status of the vertex buffer
	 * caching.
	 */
	virtual void resetUsage() {}
private:
};

}

#ifdef CODE_INLINE
#include "vertices.ipp"
#endif

#endif
/*vertices.hpp*/
