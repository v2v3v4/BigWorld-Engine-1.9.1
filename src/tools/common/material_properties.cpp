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
#include "material_properties.hpp"
#include "resmgr/bwresource.hpp"
#include "moo/texture_manager.hpp"
#include "resmgr/auto_config.hpp"

#define EDCALL __stdcall
#define WORLDEDITORDLL_API

DECLARE_DEBUG_COMPONENT2( "Common", 0 );

static AutoConfigString s_notFoundBmp( "system/notFoundBmp" );

/**
 *	The MaterialProperty list, and associated macros
 *	makes it easy to associate a Property constructor with
 *	a D3DXPARAMETER_CLASS, D3DXPARAMETER_TYPE pair.
 */
MaterialProperties g_editors;

class PropertyInsertor
{
public:
	PropertyInsertor(
        D3DXPARAMETER_CLASS a,
        D3DXPARAMETER_TYPE b,
        MPECreatorFn c )
	{
        MPEKeyType key = std::make_pair( a, b );
		g_editors.insert( std::make_pair(key,c) );
		INFO_MSG( "Registering material property type %d %d\n", a, b );
	}
};

#define REGISTER_MATERIAL_PROPERTY( a, b, c ) static PropertyInsertor Property_##a##b( a, b, c );


bool MaterialTextureProxy::apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
{
	if (!value_.hasObject())
		return SUCCEEDED( pEffect->SetTexture( hProperty, NULL ) );
	return SUCCEEDED( pEffect->SetTexture( hProperty, value_->pTexture() ) );
}

void EDCALL MaterialTextureProxy::set( std::string value, bool transient )
{
	resourceID_ = BWResolver::dissolveFilename( value );
	value_ = Moo::TextureManager::instance()->get(resourceID_, true, true, true, "texture/material");
}

void MaterialTextureProxy::save( DataSectionPtr pSection )
{
	pSection->writeString( "Texture", resourceID_ );
}


class TextureProxyFunctor : public Moo::EffectPropertyFunctor
{
public:
	virtual Moo::EffectPropertyPtr create( DataSectionPtr pSection )
	{
		MaterialTextureProxy* prop = new MaterialTextureProxy;
        //DEBUG_MSG( "created texture proxy %lx\n", (int)prop );
		prop->set( pSection->asString(), false );
		return prop;
	}
	virtual Moo::EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		MaterialTextureProxy* prop = new MaterialTextureProxy;
		return prop;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return (propertyDesc->Class == D3DXPC_OBJECT &&
			(propertyDesc->Type == D3DXPT_TEXTURE ||
			propertyDesc->Type == D3DXPT_TEXTURE1D ||
			propertyDesc->Type == D3DXPT_TEXTURE2D ||
			propertyDesc->Type == D3DXPT_TEXTURE3D ||
			propertyDesc->Type == D3DXPT_TEXTURECUBE ));
	}
private:
};

GeneralProperty* createTextureEditor(
	const std::string & name,
	Moo::EffectPropertyPtr& property)
{
    Moo::EffectProperty* pProp = property.getObject();
    MaterialTextureProxy* tp = static_cast<MaterialTextureProxy*>(pProp);
    MF_ASSERT( tp );

	return new TextProperty(name, (StringProxyPtr)( tp->proxy() ) );
}
REGISTER_MATERIAL_PROPERTY( D3DXPC_OBJECT, D3DXPT_TEXTURE, &createTextureEditor )
REGISTER_MATERIAL_PROPERTY( D3DXPC_OBJECT, D3DXPT_TEXTURE1D, &createTextureEditor )
REGISTER_MATERIAL_PROPERTY( D3DXPC_OBJECT, D3DXPT_TEXTURE2D, &createTextureEditor )
REGISTER_MATERIAL_PROPERTY( D3DXPC_OBJECT, D3DXPT_TEXTURE3D, &createTextureEditor )
REGISTER_MATERIAL_PROPERTY( D3DXPC_OBJECT, D3DXPT_TEXTURECUBE, &createTextureEditor )



class MaterialColourProxy : public EditorEffectProperty, public ProxyHolder< MaterialColourProxy,ColourProxy >
{
public:
	MaterialColourProxy():
	  value_(0.f,0.f,0.f,0.f)
	{
	}

	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
	{
		return SUCCEEDED( pEffect->SetVector( hProperty, &value_ ) );
	}
	
	Moo::Colour EDCALL get() const { return Moo::Colour(value_) / 255.f; }

	void EDCALL set( Moo::Colour f, bool transient )
	{
		value_ = Vector4( static_cast<float*>(f) ) * 255.f;
	}
    
    Vector4 EDCALL getVector4() const { return value_; }

    void EDCALL setVector4( Vector4 f, bool transient )
	{
		value_ = f;
	}

	void save( DataSectionPtr pSection )
	{
		pSection->writeVector4( "Colour", value_ );
	}

protected:
	Vector4 value_;
};

class ColourProxyFunctor : public Moo::EffectPropertyFunctor
{
public:
	virtual Moo::EffectPropertyPtr create( DataSectionPtr pSection )
	{
		MaterialColourProxy* proxy = new MaterialColourProxy;
        //DEBUG_MSG( "created colour proxy %lx\n", (int)proxy );
		proxy->setVector4( pSection->asVector4(), false );
		return proxy;
	}
	virtual Moo::EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		MaterialColourProxy* proxy = new MaterialColourProxy;
		Vector4 v;
        pEffect->GetVector( hProperty, &v );
		proxy->setVector4( v, false );
		return proxy;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return (propertyDesc->Class == D3DXPC_VECTOR &&
			propertyDesc->Type == D3DXPT_FLOAT);
	}
private:
};

GeneralProperty* createColourEditor(
	const std::string & name,
	Moo::EffectPropertyPtr& property )
{
    Moo::EffectProperty* pProp = property.getObject();
    MaterialColourProxy* cp = static_cast<MaterialColourProxy*>(pProp);
    MF_ASSERT( cp );

	return new ColourProperty(name, (ColourProxyPtr)( cp->proxy() ) );
}
//can't do this, must use a Vector4 editor for now
//REGISTER_MATERIAL_PROPERTY( D3DXPC_VECTOR, D3DXPT_FLOAT, &createColourEditor )

class Vector4ProxyFunctor : public Moo::EffectPropertyFunctor
{
public:
	virtual Moo::EffectPropertyPtr create( DataSectionPtr pSection )
	{
		MaterialVector4Proxy* proxy = new MaterialVector4Proxy;
        //DEBUG_MSG( "created Vector4 proxy %lx\n", (int)proxy );
		proxy->set( pSection->asVector4(), false );
		return proxy;
	}
	virtual Moo::EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		MaterialVector4Proxy* proxy = new MaterialVector4Proxy;
		Vector4 v;
        pEffect->GetVector( hProperty, &v );
		proxy->set( v, false );
		return proxy;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return (propertyDesc->Class == D3DXPC_VECTOR &&
			propertyDesc->Type == D3DXPT_FLOAT);
	}
private:
};

GeneralProperty* createVector4Editor(
	const std::string & name,
	Moo::EffectPropertyPtr& property )
{
    Moo::EffectProperty* pProp = property.getObject();
    MaterialVector4Proxy* vp = static_cast<MaterialVector4Proxy*>(pProp);
    MF_ASSERT( vp );

	return new Vector4Property(name, (Vector4ProxyPtr)( vp->proxy() ) );
}
REGISTER_MATERIAL_PROPERTY( D3DXPC_VECTOR, D3DXPT_FLOAT, &createVector4Editor )

class FloatProxyFunctor : public Moo::EffectPropertyFunctor
{
public:
	virtual Moo::EffectPropertyPtr create( DataSectionPtr pSection )
	{
		MaterialFloatProxy* proxy = new MaterialFloatProxy;
        //DEBUG_MSG( "created float proxy %lx\n", (int)proxy );
		proxy->set( pSection->asFloat(), false );
		return proxy;
	}
	virtual Moo::EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		MaterialFloatProxy* proxy = new MaterialFloatProxy;
		float v;
        pEffect->GetFloat( hProperty, &v );
		proxy->set( v, false );

		proxy->attach( hProperty, pEffect );

		return proxy;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return (propertyDesc->Class == D3DXPC_SCALAR &&
			propertyDesc->Type == D3DXPT_FLOAT);
	}
private:
};

GeneralProperty* createFloatEditor(
	const std::string & name,
	Moo::EffectPropertyPtr& property )
{
    Moo::EffectProperty* pProp = property.getObject();
    MaterialFloatProxy* fp = static_cast<MaterialFloatProxy*>(pProp);
    MF_ASSERT( fp );

	return new GenFloatProperty(name, (FloatProxyPtr)( fp->proxy() ) );
}
REGISTER_MATERIAL_PROPERTY( D3DXPC_SCALAR, D3DXPT_FLOAT, &createFloatEditor )

/*class MaterialBoolProxy : public EditorEffectProperty, public ProxyHolder< MaterialBoolProxy,BoolProxy >
{
public:
	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
	{
		return SUCCEEDED( pEffect->SetBool( hProperty, (BOOL)!!value_ ) );
	}
	void EDCALL set( bool value, bool transient ) { value_ = value; };
	bool EDCALL get() const { return value_; }

	void save( DataSectionPtr pSection )
	{
		pSection->writeBool( "Bool", value_ );
	}
protected:
	bool value_;
};*/

class BoolProxyFunctor : public Moo::EffectPropertyFunctor
{
public:
	virtual Moo::EffectPropertyPtr create( DataSectionPtr pSection )
	{
		MaterialBoolProxy* prop = new MaterialBoolProxy;
        //DEBUG_MSG( "created bool proxy %lx\n", (int)prop );
		prop->set( pSection->asBool(), false );
		return prop;
	}
	virtual Moo::EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		MaterialBoolProxy* prop = new MaterialBoolProxy;
		BOOL v;
        pEffect->GetBool( hProperty, &v );
		prop->set( !!v, false );
		return prop;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return (propertyDesc->Class == D3DXPC_SCALAR &&
			propertyDesc->Type == D3DXPT_BOOL);
	}
private:
};

GeneralProperty* createBoolEditor(
	const std::string & name,
	Moo::EffectPropertyPtr& property )
{
    Moo::EffectProperty* pProp = property.getObject();
    MaterialBoolProxy* bp = static_cast<MaterialBoolProxy*>(pProp);
    MF_ASSERT( bp );

	return new GenBoolProperty(name, (BoolProxyPtr)( bp->proxy() ) );
}
REGISTER_MATERIAL_PROPERTY( D3DXPC_SCALAR, D3DXPT_BOOL, &createBoolEditor )



MaterialIntProxy::MaterialIntProxy() : ranged_( false )
{
}

bool MaterialIntProxy::apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
{
	return SUCCEEDED( pEffect->SetInt( hProperty, value_ ) );
}

void EDCALL MaterialIntProxy::set( uint32 value, bool transient )
{
    value_ = value;
}

void MaterialIntProxy::save( DataSectionPtr pSection )
{
	pSection->writeInt( "Int", value_ );
}

bool MaterialIntProxy::getRange( int& min, int& max ) const
{
    min = min_;
    max = max_;
    return ranged_;
}

void MaterialIntProxy::setRange( int min, int max )
{
	ranged_ = true;
    min_ = min;
    max_ = max;
}

void MaterialIntProxy::attach( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
{
	D3DXHANDLE minHandle = pEffect->GetAnnotationByName( hProperty, RANGE_MIN );
	D3DXHANDLE maxHandle = pEffect->GetAnnotationByName( hProperty, RANGE_MAX );
	if( minHandle && maxHandle )
	{
		D3DXPARAMETER_DESC minPara, maxPara;
		if( SUCCEEDED( pEffect->GetParameterDesc( minHandle, &minPara ) ) &&
			SUCCEEDED( pEffect->GetParameterDesc( maxHandle, &maxPara ) ) &&
			minPara.Type == D3DXPT_INT &&
			maxPara.Type == D3DXPT_INT )
		{
			int min, max;
			if( SUCCEEDED( pEffect->GetInt( minHandle, &min ) ) &&
				SUCCEEDED( pEffect->GetInt( maxHandle, &max ) ) )
			{
				setRange( min, max );
			}
		}
	}
}

class IntProxyFunctor : public Moo::EffectPropertyFunctor
{
public:
	virtual Moo::EffectPropertyPtr create( DataSectionPtr pSection )
	{
		MaterialIntProxy* prop = new MaterialIntProxy;
        //DEBUG_MSG( "created int proxy %lx\n", (int)prop );
		prop->set( (uint32)pSection->asInt(), false );
		return prop;
	}
	virtual Moo::EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		MaterialIntProxy* prop = new MaterialIntProxy;
		int v;
        pEffect->GetInt( hProperty, &v );

		prop->set( (uint32)v, false );
		prop->attach( hProperty, pEffect );

		return prop;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return (propertyDesc->Class == D3DXPC_SCALAR &&
			propertyDesc->Type == D3DXPT_INT );
	}
private:
};


GeneralProperty* createIntEditor(
	const std::string & name,
	Moo::EffectPropertyPtr& property )
{
    Moo::EffectProperty* pProp = property.getObject();
    MaterialIntProxy* ip = static_cast<MaterialIntProxy*>(pProp);
    MF_ASSERT( ip );

	return new GenIntProperty(name, (IntProxyPtr)( ip->proxy() ) );
}
REGISTER_MATERIAL_PROPERTY( D3DXPC_SCALAR, D3DXPT_INT, &createIntEditor )



#ifdef EDITOR_ENABLED

bool MaterialMatrixProxy::apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
{
	return SUCCEEDED( pEffect->SetMatrix( hProperty, &value_ ) );
}

bool EDCALL MaterialMatrixProxy::setMatrix( const Matrix & m )
{
    value_ = m;
	return true;
}

void MaterialMatrixProxy::save( DataSectionPtr pSection )
{
    pSection = pSection->openSection( "Matrix", true );
    char buf[6] = "row0\0";
    for ( int i=0; i<4; i++ )
    {
        pSection->writeVector4(buf,value_.row(i));
        buf[3]++;
    }
}



class MatrixProxyFunctor : public Moo::EffectPropertyFunctor
{
public:
	virtual Moo::EffectPropertyPtr create( DataSectionPtr pSection )
	{
		MaterialMatrixProxy* prop = new MaterialMatrixProxy;
        //DEBUG_MSG( "created matrix proxy %lx\n", (int)prop );
        Matrix m;
        char buf[6] = "row0\0";
        for ( int i=0; i<4; i++ )
        {
            m.row(i, pSection->readVector4(buf) );
            buf[3]++;
        }
		prop->setMatrix( m );
		return prop;
	}
	virtual Moo::EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		MaterialMatrixProxy* prop = new MaterialMatrixProxy;
		Matrix m;
        pEffect->GetMatrix( hProperty, &m );
		prop->setMatrix( m );
		return prop;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return ((propertyDesc->Class == D3DXPC_MATRIX_ROWS ||
            propertyDesc->Class == D3DXPC_MATRIX_COLUMNS) &&
			propertyDesc->Type == D3DXPT_FLOAT);
	}
private:
};


GeneralProperty* createMatrixEditor(
	const std::string & name,
	Moo::EffectPropertyPtr& property )
{
    Moo::EffectProperty* pProp = property.getObject();
    MaterialMatrixProxy* ip = static_cast<MaterialMatrixProxy*>(pProp);
    MF_ASSERT( ip );

	return new GenMatrixProperty(name, (MatrixProxyPtr)( ip->proxy() ) );
}
REGISTER_MATERIAL_PROPERTY( D3DXPC_MATRIX_ROWS, D3DXPT_FLOAT, &createMatrixEditor )
REGISTER_MATERIAL_PROPERTY( D3DXPC_MATRIX_COLUMNS, D3DXPT_FLOAT, &createMatrixEditor )

#endif//EDITOR_ENABLED

bool MaterialTextureFeedProxy::apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
{
	if (!value_.hasObject())
		return SUCCEEDED( pEffect->SetTexture( hProperty, NULL ) );
	return SUCCEEDED( pEffect->SetTexture( hProperty, value_->pTexture() ) );
}

void EDCALL MaterialTextureFeedProxy::set( std::string value, bool transient )
{
	resourceID_ = value;
	if( resourceID_.size() )
		value_ = Moo::TextureManager::instance()->get( resourceID_, true, true, true, "texture/material" );
	else
		value_ = Moo::TextureManager::instance()->get( s_notFoundBmp, true, true, true, "texture/material" );
}

void MaterialTextureFeedProxy::save( DataSectionPtr pSection )
{
	pSection = pSection->newSection( "TextureFeed" );
	pSection->setString( textureFeed_ );
	if( ! resourceID_.empty() )
		pSection->writeString( "default", resourceID_ );
}

void MaterialTextureFeedProxy::setTextureFeed( std::string value )
{
	textureFeed_ = value;
}

class TextureFeedProxyFunctor : public Moo::EffectPropertyFunctor
{
public:
	virtual Moo::EffectPropertyPtr create( DataSectionPtr pSection )
	{
		MaterialTextureFeedProxy* prop = new MaterialTextureFeedProxy;
		prop->set( pSection->readString( "default", "" ), false );
		prop->setTextureFeed( pSection->asString() );
		return prop;
	}
	virtual Moo::EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		MaterialTextureFeedProxy* prop = new MaterialTextureFeedProxy;
		return prop;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return (propertyDesc->Class == D3DXPC_OBJECT &&
			(propertyDesc->Type == D3DXPT_TEXTURE ||
			propertyDesc->Type == D3DXPT_TEXTURE1D ||
			propertyDesc->Type == D3DXPT_TEXTURE2D ||
			propertyDesc->Type == D3DXPT_TEXTURE3D ||
			propertyDesc->Type == D3DXPT_TEXTURECUBE ));
	}
private:
};

GeneralProperty* createTextureFeedEditor(
	const std::string & name,
	Moo::EffectPropertyPtr& property)
{
    Moo::EffectProperty* pProp = property.getObject();
    MaterialTextureFeedProxy* tp = static_cast<MaterialTextureFeedProxy*>(pProp);
    MF_ASSERT( tp );

	return new TextProperty(name, (StringProxyPtr)( tp->proxy() ) );
}

/**
 *	Important - this must be called at runtime, before you begin
 *	editing material properties.  The reason is that in
 *	moo/managed_effect the property processors are set up in the
 *	g_effectPropertyProcessors map at static initialisation time;
 *	our own processors are meant to override the default ones.
 */

bool runtimeInitMaterialProperties()
{
#define EP(x) Moo::g_effectPropertyProcessors[#x] = new x##ProxyFunctor
	EP(Vector4);
#ifdef EDITOR_ENABLED
	EP(Matrix);
#endif//EDITOR_ENABLED
	EP(Float);
	EP(Bool);
	EP(Texture);
	EP(Int);
	EP(TextureFeed);

	return true;
}
