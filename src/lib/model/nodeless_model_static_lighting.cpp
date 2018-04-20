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

#include "nodeless_model_static_lighting.hpp"

#include "moo/visual.hpp"
#include "romp/static_light_values.hpp"


DECLARE_DEBUG_COMPONENT2( "Model", 0 )


NodelessModelStaticLighting::NodelessModelStaticLighting(
										Moo::VisualPtr bulk,
										StaticLightValues * pSLV ) :
	bulk_( bulk ),
	pSLV_( pSLV )
{
}


NodelessModelStaticLighting::~NodelessModelStaticLighting()
{
	delete pSLV_;
}


/**
 *	This method sets the static lighting for the main bulk of the nodeless
 *	model from which it was loaded.
 */
void NodelessModelStaticLighting::set()
{
	bulk_->staticVertexColours( pSLV_->vb() );
}


void NodelessModelStaticLighting::unset()
{
	bulk_->staticVertexColours( Moo::VertexBuffer() );
}


StaticLightValues * NodelessModelStaticLighting::staticLightValues()
{
	return pSLV_;
}


// nodeless_model_static_lighting.cpp
