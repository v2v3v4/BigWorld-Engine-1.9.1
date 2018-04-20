/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef VERTEX_DECLARATION_HPP
#define VERTEX_DECLARATION_HPP

#include "moo_dx.hpp"
#include <map>
#include "resmgr/datasection.hpp"
#include "cstdmf/concurrency.hpp"

namespace Moo
{

typedef ComObjectWrap< DX::VertexDeclaration > D3DVertexDeclarationPtr;

/**
 * This class handles the vertex declarations used by moo. Vertex declarations 
 * are stored on disk (in res\\shaders\\formats ), and created using this class.
 *
 * A example vertex declaration looks like this:
 *
 * \verbatim
 *  <root>
 *  	<POSITION>
 *  		<type> FLOAT2 </type>
 *  	</POSITION>
 *  	<TEXCOORD>
 *  		<stream> 1 </stream>
 *  		<offset> 0 </offset>
 *  		<type> FLOAT2 </type>
 *  	</TEXCOORD>
 *  </root>
 * \endverbatim
 *
 * In this example, the declartion defines two streams: an xy position type 
 * stream, and a uv texture coordinate stream. The POSITION and TEXCOORD sections
 * map directly to D3D shader semantics. In the following example:
 *
 * \verbatim
 *  <root>
 *  	<POSITION>
 *  		<type> FLOAT3 </type>
 *  	</POSITION>
 *  	<TEXCOORD>
 *  		<type> SHORT2 </type>
 *  	</TEXCOORD>		
 *  	<BLENDWEIGHT>
 *  		<type> SHORT2 </type>
 *  	</BLENDWEIGHT>	
 *  </root>
 * \endverbatim
 *
 * There is a single stream stream containing interleaved xyz postion, uv texture,
 * and blend weight information.
 */
class VertexDeclaration
{
public:
	typedef std::vector<std::string > Aliases;
	VertexDeclaration();
	~VertexDeclaration();
	bool	load( DataSectionPtr pSection );
	const Aliases& aliases() const { return aliases_; }
	DX::VertexDeclaration* declaration() { return pDecl_.pComObject(); }

	static VertexDeclaration* get( const std::string& declName );
	static void fini();
private:
	D3DVertexDeclarationPtr	pDecl_;
	Aliases					aliases_;
	VertexDeclaration( const VertexDeclaration& );
	VertexDeclaration& operator=( const VertexDeclaration& );

	static SimpleMutex	declarationsLock_;

};

};


#endif // VERTEX_DECLARATION_HPP
