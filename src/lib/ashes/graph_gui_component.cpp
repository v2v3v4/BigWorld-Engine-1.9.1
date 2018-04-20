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

#pragma warning(disable:4786)	// turn off truncated identifier warnings

#include "romp/geometrics.hpp"
#include "math/boundbox.hpp"

#include "graph_gui_component.hpp"
#include "simple_gui.hpp"

#include "cstdmf/debug.hpp"
#include "moo/shader_manager.hpp"

DECLARE_DEBUG_COMPONENT2( "2DComponents", 0 )


PY_TYPEOBJECT( GraphGUIComponent )

PY_BEGIN_METHODS( GraphGUIComponent )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( GraphGUIComponent )

	/*~	attribute GraphGUIComponent.input
	 *	@type Vector4Provider
	 *
	 *	This stores the information to be displayed on the screen.
	 *	The Vector4Provider is represented as 4 line graphs, one for
	 *	each of the x,y,z,w components.
	 *	 
	 *	Example:
	 *	@{	 
	 *	import GUI
	 *	import Math
	 *	g = GUI.Graph( "" )
	 *	GUI.addRoot(g)
	 *	g.input = Math.Vector4Product()
	 *	g.input.a = Math.Vector4LFO()
	 *	g.input.b = ( 1.0, 0.8, 0.4, 0.2 )
	 *	g.frequency = 0.015
	 *	g.nPoints = 100
	 *	g.size = (1.5, 1.5)
	 *	@}
	 */
	PY_ATTRIBUTE( input )

	/*~	attribute GraphGUIComponent.nPoints
	 *	unsigned int
	 *
	 *	This stores the number of points that will be displayed in the graph.
	 *	Defaults to 25.
	 */
	PY_ATTRIBUTE( nPoints )

	/*~	attribute GraphGUIComponent.frequency
	 *	@type float
	 *
	 *	This specifies the rate at which the graph will be updated.
	 *	If frequency is less than 0, the update rate will be in frames (ticks).
	 *	If it is greater than 0, it will be in seconds.
	 *	Defaults to 0.033
	 */
	PY_ATTRIBUTE( frequency )

	/*~	attribute GraphGUIComponent.minY
	 *	@type float
	 *
	 *	Specifies the minimum height that the graph will be displayed on the screen.
	 *	Value should be between maxY and 0.0 (bottom of screen).
	 *	Defaults to 0.0
	 */
	PY_ATTRIBUTE( minY )

	/*~	attribute GraphGUIComponent.maxY
	 *	@type float
	 *
	 *	Specifies the maximum height that the graph will be displayed on the screen.
	 *	Value should be between minY and 1.0 (top of screen).
	 *	Defaults to 1.0
	 */
	PY_ATTRIBUTE( maxY )

PY_END_ATTRIBUTES()

COMPONENT_FACTORY( GraphGUIComponent )

/*~ function GUI.Graph
 *
 *	Creates and returns a new GraphGUIComponent, which is used to graph a Vector4Provider on the screen.
 *
 *	@return	The new GraphGUIComponent.
 */
PY_FACTORY_NAMED( GraphGUIComponent, "Graph", GUI )



GraphGUIComponent::GraphGUIComponent( PyTypePlus * pType )
:SimpleGUIComponent( "", pType ),
 input_( NULL ),
 minY_( 0.f ),
 maxY_( 1.f ),
 nPoints_( 25 ),
 frequency_( 0.033f ),
 head_( 0 ),
 yValues_( NULL ),
 accumTime_( 0.f )
{	
	materialFX( FX_ADD );
	memset( mesh_, 0, sizeof( CustomMesh<Moo::VertexXYZDUV>* ) * 4 );
	//force creation of buffers
	this->nPoints( nPoints_ );
}


GraphGUIComponent::~GraphGUIComponent()
{
	delete[] yValues_;
	for (size_t i=0; i < 4; i++)
	{
		delete mesh_[i];
	}
}


void GraphGUIComponent::nPoints( int n )
{
	nPoints_ = max(n,1);
	head_ = 0;

	if (yValues_)
	{
		delete[] yValues_;
	}

	yValues_ = new Vector4[nPoints_];	

	for (size_t i=0; i < nPoints_; i++)
	{
		yValues_[i].setZero();
	}

	for (size_t i=0; i < 4; i++)
	{
		if (mesh_[i])
		{
			delete mesh_[i];
		}
		mesh_[i] = new CustomMesh< Moo::VertexXYZDUV >( D3DPT_LINESTRIP );
		mesh_[i]->resize( nPoints_ );
	}	
}


/// Get an attribute for python
PyObject * GraphGUIComponent::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return SimpleGUIComponent::pyGetAttribute( attr );
}


/// Set an attribute for python
int GraphGUIComponent::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return SimpleGUIComponent::pySetAttribute( attr, value );
}


/**
 *	Static python factory method
 */
PyObject * GraphGUIComponent::pyNew( PyObject * args )
{
	return new GraphGUIComponent;
}


/**
 *	This method draws the graph gui component.
 */
void GraphGUIComponent::update( float dTime, float relParentWidth, float relParentHeight )
{
	dTime_ = dTime;
	SimpleGUIComponent::update( dTime, relParentWidth, relParentHeight );
}


/**
 *	This method draws the graph gui component.
 */
void GraphGUIComponent::draw( bool overlay )
{
	Moo::VertexXYZDUV v;

	bool takeASample = true;

	if ( input_ )
	{
		//grab one more sample
		if ( frequency_ > 0.f )
		{
			//frequency > 0 - means real time, granular updates.
			accumTime_ += dTime_;
			if ( accumTime_ > frequency_ )
			{
				input_->tick( frequency_ );
				accumTime_ -= frequency_;
			}
			else
			{
				takeASample = false;
			}
		}
		else if ( frequency_ < 0.f )
		{
			//frequency < 0 - this means per-frame updates of exactly the right amount of time.
			input_->tick( -frequency_ );
		}
		//else frequency == 0 ( we do not animate the vector4 provider )

		if ( takeASample )
		{
			Vector4 out;
			input_->output( out );
			for (size_t idx=0; idx<4; idx++)
			{				
				yValues_[head_][idx] = out[idx];
			}
			head_  = (head_+1)%nPoints_;
		}


		//draw the graph into the mesh buffer.
        float yRange = (maxY_ - minY_);
		float dx = ( width_ / ((float)nPoints_ - 1) );
		float yScale = ( height_ / yRange );
		float yMin = position_.y - height_ / 2.f;
		int idx = head_-1;
		if ( idx < 0 )
			idx = nPoints_-1;

		uint32 colours[4] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff };

		for ( uint32 c=0; c<4; c++ )
		{
			float x = position_.x + width_ / 2.f;
			CustomMesh< Moo::VertexXYZDUV >& mesh = *mesh_[c];
			for ( uint32 i=0; i<nPoints_; i++  )
			{
				v.uv_.set( 1.f-((float)i / (float)nPoints_), 1.f - (yValues_[idx][c] - minY_) / yRange );
				v.pos_.set( x, (yValues_[idx][c] - minY_) * yScale + yMin, position_.z );
				v.colour_ = colours[c];
				mesh[i] = v;

				x -= dx;
				idx = (idx-1);
				if ( idx < 0 )
					idx = nPoints_-1;
			}
		}
	}


	if ( visible() )
	{
		Moo::rc().push();
		Moo::rc().preMultiply( runTimeTransform() );
		Moo::rc().device()->SetTransform( D3DTS_WORLD, &Moo::rc().world() );

		//if ( !momentarilyInvisible() )
		{			
			if ( nPoints_ && mesh_[0]->size() )
			{
				SimpleGUI::instance().setConstants(runTimeColour(), pixelSnap());
				material_->begin();
				for ( uint32 i=0; i<material_->nPasses(); i++ )
				{
					material_->beginPass(i);
					Moo::rc().setTexture( 0, texture_ ? texture_->pTexture() : NULL );
					Moo::rc().setSamplerState( 0, D3DSAMP_BORDERCOLOR, 0x00000000 );
					if ( !overlay )
					{
						Moo::rc().setRenderState( D3DRS_ZENABLE, TRUE );
						Moo::rc().setRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
						Moo::rc().setRenderState( D3DRS_ZFUNC, D3DCMP_LESS );
					}
					for ( uint32 idx=0; idx<4; idx++ )
					{
						mesh_[idx]->draw();
					}

					material_->endPass();
				}
				material_->end();

				Moo::rc().setVertexShader( NULL );
				Moo::rc().setFVF( GUIVertex::fvf() );
			}
		}

		SimpleGUIComponent::drawChildren(overlay);

		Moo::rc().pop();
		Moo::rc().device()->SetTransform( D3DTS_WORLD, &Moo::rc().world() );
	}

	momentarilyInvisible( false );
}


bool GraphGUIComponent::load( DataSectionPtr pSect, LoadBindings & bindings )
{
	if (!this->SimpleGUIComponent::load( pSect, bindings )) return false;

	this->nPoints( pSect->readInt( "nPoints", nPoints_ ) );
	this->minY_ = pSect->readFloat( "minY", minY_ );
	this->maxY_ = pSect->readFloat( "maxY", maxY_ );
	this->frequency_ = pSect->readFloat( "frequency", frequency_ );

	return true;
}


void GraphGUIComponent::save( DataSectionPtr pSect, SaveBindings & bindings )
{
	pSect->writeInt( "nPoints", nPoints_ );
	pSect->writeFloat( "minY", minY_ );
	pSect->writeFloat( "maxY", maxY_ );
	pSect->writeFloat( "frequency", frequency_ );

	this->SimpleGUIComponent::save( pSect, bindings );
}

// graph_gui_component.cpp
