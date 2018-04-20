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

#include "gobo_component.hpp"
#include "ashes/simple_gui.hpp"
#include "cstdmf/debug.hpp"
#include "moo/render_context.hpp"
#include "moo/effect_material.hpp"
#include "moo/texture_compressor.hpp"
#include "moo/vertex_formats.hpp"
#include "resmgr/auto_config.hpp"
#include "romp/texture_feeds.hpp"
#include "romp/custom_mesh.hpp"
#include "romp/bloom_effect.hpp"

DECLARE_DEBUG_COMPONENT2( "2DComponents", 0 )

#ifndef CODE_INLINE
#include "gobo_component.ipp"
#endif

/**
 *	This global method specifies the resources required by this file
 */
static AutoConfigString s_mfmName( "system/goboMaterial" );


// -----------------------------------------------------------------------------
// Section: GoboComponent
// -----------------------------------------------------------------------------

#undef PY_ATTR_SCOPE
#define PY_ATTR_SCOPE GoboComponent::

PY_TYPEOBJECT( GoboComponent )

PY_BEGIN_METHODS( GoboComponent )
	/*~ function GoboComponent.freeze
	 *
	 *	This method stows the current state of the bloom buffer in
	 *	another texture, and displays it; essentially freezing the
	 *  current state of the bloom buffer.  As a side effect, the texture
	 *  is saved to disk as temp_gobo.dds in the root of your resources folder.
	 */
	PY_METHOD( freeze )
	/*~ function GoboComponent.unfreeze
	 *
	 *	This method undoes the freeze method, it restores the GoboComponent
	 *	to using the engine's dynamic bloom texture.
	 */	
	PY_METHOD( unfreeze )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( GoboComponent )
PY_END_ATTRIBUTES()

COMPONENT_FACTORY( GoboComponent )

/*~ function GUI.Gobo
 *
 *	This function creates a new Gobo component.
 *	The GoboComponent accesses the render target used by blooming, and blends
 *	in that texture based on the alpha channel of the gobo component's texture.
 *	By turning off bloom + blur, the blooming render target becomes a blurred
 *	version of the scene, meaning that by using the gobo component, you can
 *	selectively display a blurred version of the scene.
 *
 *	This is for example perfect for binoculars and sniper scopes.
 *
 *	For example, in the following code:
 *
 *	@{
 *	comp = GUI.Gobo( "gui/maps/gobo_binoculars.tga" )
 *	comp.materialFX="SOLID"
 *	GUI.addRoot( comp )
 *	BigWorld.selectBloomPreset(1)
 *	@}
 *
 *	This example will display a binocular gobo, and where the alpha channel is
 *	relatively opaque in the binocular texture map, a blurred version of the scene
 *	is drawn. 
 *
 *	@param	textureName				The filename of the gobo texture.  The alpha
 *									channel determines how much of the blurred
 *									screen should be blended in.
 */
PY_FACTORY_NAMED( GoboComponent, "Gobo", GUI )

GoboComponent::GoboComponent( const std::string& textureName, PyTypePlus * pType ):
	SimpleGUIComponent( textureName, pType ),
	pDiffuseMap_( NULL ),
	pBlurMap_( NULL ),
	pBackBuffer_( NULL )
{	
	material_ = NULL;
	
	buildMaterial();
}


GoboComponent::~GoboComponent()
{
}


/**
 *	Section - SimpleGUIComponent methods
 */


bool GoboComponent::TextureSetter::operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
{
	if (map_)
		pEffect->SetTexture( constantHandle, map_->pTexture() );
	return true;
}

void GoboComponent::TextureSetter::map(Moo::BaseTexturePtr pTexture )
{
	map_ = pTexture;
}

Moo::BaseTexturePtr GoboComponent::TextureSetter::map()
{
	return map_;
}

void GoboComponent::setConstants()
{
	if ( pDiffuseMap_ == NULL )
	{
		pDiffuseMap_= Moo::EffectConstantValue::get( "DiffuseMap" );
		pBlurMap_	= Moo::EffectConstantValue::get( "BloomMap" );
		pBackBuffer_ = Moo::EffectConstantValue::get( "BackBuffer" );
		blurTexture_= TextureFeeds::instance().get("bloom2");		
	}
	
	diffuseMapSetterPtr_->map( texture_ );
	backBufferSetterPtr_->map( TextureFeeds::instance().get("backBuffer") );
	if (Bloom::isSupported() && blurTexture_)
		blurMapSetterPtr_->map( blurTexture_ );
	else
		blurMapSetterPtr_->map( texture_ );

	*pDiffuseMap_	= diffuseMapSetterPtr_;
	*pBlurMap_		= blurMapSetterPtr_;
	*pBackBuffer_	= backBufferSetterPtr_;

	Moo::rc().setFVF( Moo::VertexXYZDUV2::fvf() );
}

/**
 *	This method implements the PyAttachment::draw interface.  Since
 *	this gui component draws in the world, this is where we do our
 *	actual drawing.
 */
void GoboComponent::draw( bool overlay )
{
	CustomMesh<Moo::VertexXYZDUV2> mesh( D3DPT_TRIANGLEFAN );
	float w = 1.f;
	float h = 1.f;

	Moo::VertexXYZDUV2 v;
	v.colour_ = 0xffffffff;

	Vector3 fixup( -0.5f / SimpleGUI::instance().screenWidth(), 0.5f / SimpleGUI::instance().screenHeight(), 0.f );
	
	v.pos_.set(-1.f,-1.f,0.1f);
	v.uv_.set(0.f,0.f);
	v.uv2_.set(0.f,h);
	mesh.push_back(v);

	v.pos_.set(-1.f,1.f,0.1f);
	v.uv_.set(0.f,1.f);
	v.uv2_.set(0.f,0.f);
	mesh.push_back(v);

	v.pos_.set(1.f,1.f,0.1f);
	v.uv_.set(1.f,1.f);
	v.uv2_.set(w,0.f);
	mesh.push_back(v);

	v.pos_.set(1.f,-1.f,0.1f);
	v.uv_.set(1.f,0.f);
	v.uv2_.set(w,h);
	mesh.push_back(v);

	for (size_t i=0; i<4; i++)
	{
		mesh[i].pos_ = mesh[i].pos_ + fixup;
	}

	//Use a custom mesh to blend the linear texture onto the screen
	if ( visible() )
	{
		Moo::rc().push();
		Moo::rc().preMultiply( runTimeTransform() );
		Moo::rc().device()->SetTransform( D3DTS_WORLD, &Moo::rc().world() );

		if ( !momentarilyInvisible() )
		{		
			SimpleGUI::instance().setConstants(runTimeColour(), pixelSnap());
			this->setConstants();
			material_->begin();
			for ( uint32 i=0; i<material_->nPasses(); i++ )
			{
				material_->beginPass(i);
				if ( !overlay )
				{
					Moo::rc().setRenderState( D3DRS_ZENABLE, TRUE );
					Moo::rc().setRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
					Moo::rc().setRenderState( D3DRS_ZFUNC, D3DCMP_LESS );
				}				
				mesh.drawEffect();				
				material_->endPass();
			}
			material_->end();
			Moo::rc().setVertexShader( NULL );
			Moo::rc().setFVF( GUIVertex::fvf() );
		}
		
		SimpleGUIComponent::drawChildren(overlay);

		Moo::rc().pop();
		Moo::rc().device()->SetTransform( D3DTS_WORLD, &Moo::rc().world() );
	}
	
	momentarilyInvisible( false );
}


/**
 *	This method stows the current state of the bloom buffer in
 *	another texture, and displays it.
 */
void GoboComponent::freeze( void )
{
	const std::string name( "temp_gobo.dds" );	
	if (Bloom::isSupported() && blurTexture_)
	{
		DX::BaseTexture* pTex = TextureFeeds::instance().get("bloom2")->pTexture();
		TextureCompressor tc( static_cast<DX::Texture*>(pTex), D3DFMT_X8R8G8B8, 1 );
		tc.save( name );
		BWResource::instance().purge( name );
		blurTexture_ = Moo::TextureManager::instance()->get(name);	
		blurTexture_->reload();
	}
}


/**
 *	This method undoes the freeze method, it restores the GoboComponent
 *	to using the engine's dynamic bloom texture.
 */
void GoboComponent::unfreeze( void )
{
	if (Bloom::isSupported() && blurTexture_)
	{
		blurTexture_ = TextureFeeds::instance().get("bloom2");
	}
}


/**
 *	This method overrides SimpleGUIComponent's method and makes sure the
 *	linear background texture is set into the second texture stage.
 *
 *	@returns	true if successful
 */
bool GoboComponent::buildMaterial( void )
{
	bool ret = false;

	if ( !material_ )
	{
		material_ = new Moo::EffectMaterial();
		material_->load( BWResource::openSection( s_mfmName ) );
	}

	if ( material_->pEffect() && material_->pEffect()->pEffect() )
	{
		ComObjectWrap<ID3DXEffect> pEffect = material_->pEffect()->pEffect();

		uint32 i = materialFX()-FX_ADD;
		D3DXHANDLE handle = pEffect->GetTechnique( i );
		material_->hTechnique( handle );

		ret = true;
	}
	else
	{
		ERROR_MSG(" GoboComponent::buildMaterial - material is invalid.");
	}
	
	diffuseMapSetterPtr_	= new TextureSetter;
	blurMapSetterPtr_		= new TextureSetter;
	backBufferSetterPtr_	= new TextureSetter;

	return ret;
}


/**
 *	Get an attribute for python
 */
PyObject * GoboComponent::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();
	return SimpleGUIComponent::pyGetAttribute( attr );
}

/**
 *	Set an attribute for python
 */
int GoboComponent::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();
	return SimpleGUIComponent::pySetAttribute( attr, value );
}


/**
 *	Factory method
 */
PyObject * GoboComponent::pyNew( PyObject * args )
{
	char * textureName;
	if (!PyArg_ParseTuple( args, "s", &textureName ))
	{
		PyErr_SetString( PyExc_TypeError, "GUI.Gobo: "
			"Argument parsing error: Expected a texture name" );
		return NULL;
	}

	return new GoboComponent( textureName );
}