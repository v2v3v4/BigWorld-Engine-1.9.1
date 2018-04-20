/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef STATIC_LIGHT_VALUES_HPP
#define STATIC_LIGHT_VALUES_HPP

#include "cstdmf/stdmf.hpp"
#include "moo/com_object_wrap.hpp"
#include "moo/device_callback.hpp"
#include "moo/vertex_buffer.hpp"
#include "resmgr/forward_declarations.hpp"

#include <d3d9.h>
#include <vector>

class BinaryBlock;
typedef SmartPointer<BinaryBlock> BinaryPtr;

/**
 * This class is a container for static light values,
 * which also loads and saves, and takes care of the
 * vertexbuffer for the static light values.
 */

class StaticLightValues : public Moo::DeviceCallback
{
public:
	typedef std::vector< D3DCOLOR > ColourValueVector;

	StaticLightValues( BinaryPtr pData = NULL);
	~StaticLightValues();
	bool init( BinaryPtr pData );
	bool save( DataSectionPtr binFile, const std::string& sectionName );
	bool saveData( DataSectionPtr section, const std::string& tag );

	ColourValueVector& colours() { dirty_ = true; return colours_; };
	Moo::VertexBuffer vb();

	void dirty( bool state ) { dirty_ = state; };
	bool dirty() const { return dirty_; };

	virtual void deleteManagedObjects();

	uint32 size() { return colours_.size(); }

private:
	bool				dirty_;
	ColourValueVector	colours_;
	Moo::VertexBuffer   vb_;

	StaticLightValues( const StaticLightValues& );
	StaticLightValues& operator=( const StaticLightValues& );
};


#endif // STATIC_LIGHT_VALUES_HPP
