/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MATERIAL_PROPERTIES_HPP
#define MATERIAL_PROPERTIES_HPP

#define EDCALL __stdcall
#define WORLDEDITORDLL_API
#include "gizmo/general_properties.hpp"
#include "moo/managed_effect.hpp"

static const char RANGE_MIN[] = "UIMin";
static const char RANGE_MAX[] = "UIMax";
static const char RANGE_DIGITS[] = "UIDigits";

/**
 *	The editor effect property simply adds a save interface to 
 *	the base class.
 */
class EditorEffectProperty : public Moo::EffectProperty
{
public:
	virtual void save( DataSectionPtr pSection ) = 0;
};

/**
 *	By calling runtimeInitMaterialProperties, the section processor
 *	map for Moo::EffectProperties is overridden, and replaced with
 *	EditorEffectPropreties.  This allows both viewing of tweakables
 *	in editors, and the saving of properties via the above interface.
 *
 *	If runtimeInitMaterialProperties is not called, then all materials
 *	will use Moo::EffectProperties instead, and the dynamic_cast calls
 *	in the editor code will fail.
 */
extern bool runtimeInitMaterialProperties();


typedef GeneralProperty* (*MPECreatorFn)(const std::string &, Moo::EffectPropertyPtr&);
typedef std::pair<D3DXPARAMETER_CLASS,D3DXPARAMETER_TYPE> MPEKeyType;
typedef std::map< MPEKeyType, MPECreatorFn > MaterialProperties;
extern MaterialProperties g_editors;

template<typename Outer, typename Parent>
class ProxyHolder
{
public:
	class InnerProxy : public Parent
	{
		Outer& outer_;
	public:
		InnerProxy( Outer& outer ) : outer_( outer )
		{}
		void EDCALL set( typename Parent::Data value, bool transient )
		{
			outer_.set( value, transient );
		};
		typename Parent::Data EDCALL get() const
		{
			return outer_.get();
		}

		void EDCALL getMatrix( Matrix & m, bool world = true )
		{
			outer_.getMatrix( m, world );
		}
		void EDCALL getMatrixContext( Matrix & m )
		{
			outer_.getMatrix( m );
		}
		void EDCALL getMatrixContextInverse( Matrix & m )
		{
			outer_.getMatrixContextInverse( m );
		}
		bool EDCALL setMatrix( const Matrix & m )
		{
			return outer_.setMatrix( m );
		}
		void EDCALL recordState()
		{
			outer_.recordState();
		}
		bool EDCALL commitState( bool revertToRecord = false, bool addUndoBarrier = true )
		{
			return outer_.commitState( revertToRecord, addUndoBarrier );
		}
		bool EDCALL hasChanged()
		{
			return outer_.hasChanged();
		}
        bool getRange( float& min, float& max, int& digits ) const
        {
            return outer_.getRange( min, max, digits );
        }
        bool getRange( int& min, int& max ) const
        {
            return outer_.getRange( min, max );
        }

		typedef SmartPointer<InnerProxy> SP;
	};
	typename InnerProxy::SP proxy()
	{
		if( !ptr_ )
			ptr_ = new InnerProxy( *static_cast< Outer* >( this ) );
		return ptr_;
	}
private:
	typename InnerProxy::SP ptr_;
};

class MaterialTextureProxy : public EditorEffectProperty, public ProxyHolder< MaterialTextureProxy,StringProxy >
{
public:
	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty );
	void EDCALL set( std::string value, bool transient );
	std::string EDCALL get() const { return resourceID_; };
	void save( DataSectionPtr pSection );
	bool getResourceID( std::string & s ) const { s = value_ ? value_->resourceID() : ""; return true; };
protected:
	std::string		resourceID_;
	Moo::BaseTexturePtr value_;
};

class MaterialIntProxy : public EditorEffectProperty, public ProxyHolder< MaterialIntProxy,IntProxy >
{
public:
    MaterialIntProxy();
	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty );
	void EDCALL set( uint32 value, bool transient );
	uint32 EDCALL get() const { return value_; }
	void save( DataSectionPtr pSection );
    bool getRange( int& min, int& max ) const;
    void setRange( int min, int max );
	virtual void attach( D3DXHANDLE hProperty, ID3DXEffect* pEffect );
protected:
	int value_;
    bool ranged_;
    int min_;
    int max_;
};

#ifdef EDITOR_ENABLED
///MatrixProxy base class has a lot of functions we aren't using in the
///borland editor, so most of this class is a stub.
class MaterialMatrixProxy : public EditorEffectProperty, public ProxyHolder< MaterialMatrixProxy,MatrixProxy >
{
public:
	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty );
	bool EDCALL setMatrix( const Matrix & m );
	void EDCALL getMatrix( Matrix& m, bool world = true ) { m = value_; }
    void EDCALL getMatrixContext( Matrix& m )   {};
    void EDCALL getMatrixContextInverse( Matrix& m )   {};
	void save( DataSectionPtr pSection );
    void EDCALL recordState()   {};
	bool EDCALL commitState( bool revertToRecord = false, bool addUndoBarrier = true )
    {
        return true;
    }
    bool EDCALL hasChanged()    { return true; }
protected:
	Matrix value_;
};
#endif//EDITOR_ENABLED

class MaterialBoolProxy : public EditorEffectProperty, public ProxyHolder< MaterialBoolProxy,BoolProxy >
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
};

class MaterialFloatProxy : public EditorEffectProperty, public ProxyHolder< MaterialFloatProxy,FloatProxy >
{
public:
	MaterialFloatProxy():
	  value_(0.f), ranged_(false)
	{
	}

	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
	{
		return SUCCEEDED( pEffect->SetFloat( hProperty, value_ ) );
	}

	float EDCALL get() const { return value_; }

	void EDCALL set( float f, bool transient )
	{
		value_ = f;
	}

	void save( DataSectionPtr pSection )
	{
		pSection->writeFloat( "Float", value_ );
	}

    bool getRange( float& min, float& max, int& digits ) const
	{
        min = min_;
        max = max_;
        digits = digits_;
		return ranged_;
	}

    void setRange( float min, float max, int digits )
    {
        ranged_ = true;
        min_ = min;
        max_ = max;
        digits_ = digits;
    }

	virtual void attach( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		D3DXHANDLE minHandle = pEffect->GetAnnotationByName( hProperty, RANGE_MIN );
		D3DXHANDLE maxHandle = pEffect->GetAnnotationByName( hProperty, RANGE_MAX );
		if( minHandle && maxHandle )
		{
			D3DXPARAMETER_DESC minPara, maxPara;
			if( SUCCEEDED( pEffect->GetParameterDesc( minHandle, &minPara ) ) &&
				SUCCEEDED( pEffect->GetParameterDesc( maxHandle, &maxPara ) ) &&
				minPara.Type == D3DXPT_FLOAT &&
				maxPara.Type == D3DXPT_FLOAT )
			{
				float min, max;
				if( SUCCEEDED( pEffect->GetFloat( minHandle, &min ) ) &&
					SUCCEEDED( pEffect->GetFloat( maxHandle, &max ) ) )
				{
					int digits = 0;
					D3DXPARAMETER_DESC digitsPara;
					D3DXHANDLE digitsHandle = pEffect->GetAnnotationByName( hProperty, RANGE_DIGITS );

					if( digitsHandle &&
						SUCCEEDED( pEffect->GetParameterDesc( digitsHandle, &digitsPara ) ) &&
						digitsPara.Type == D3DXPT_INT &&
						SUCCEEDED( pEffect->GetInt( digitsHandle, &digits ) ) )
						setRange( min, max, digits );
					else
					{
						float range = max - min;
						if( range < 0.0 )
							range = -range;
						while( range <= 99.9999 )// for range, normally include 2 valid digits
						{
							range *= 10;
							++digits;
							setRange( min, max, digits );
						}
					}
				}
			}
		}
	}
protected:
	float value_;
    bool ranged_;
    float min_;
    float max_;
    int digits_;
};

class MaterialVector4Proxy : public EditorEffectProperty, public ProxyHolder< MaterialVector4Proxy,Vector4Proxy >
{
public:
	MaterialVector4Proxy():
	  value_(0.f,0.f,0.f,0.f)
	{
	}

	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
	{
		return SUCCEEDED( pEffect->SetVector( hProperty, &value_ ) );
	}
	
	Vector4 EDCALL get() const { return value_; }

	void EDCALL set( Vector4 f, bool transient )
	{
		value_ = f;
	}

	void save( DataSectionPtr pSection )
	{
		pSection->writeVector4( "Vector4", value_ );
	}

protected:
	Vector4 value_;
};

class MaterialTextureFeedProxy : public EditorEffectProperty, public ProxyHolder< MaterialTextureFeedProxy, StringProxy >
{
public:
	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty );
	void EDCALL set( std::string value, bool transient );
	std::string EDCALL get() const { return resourceID_; };
	void save( DataSectionPtr pSection );
	void setTextureFeed( std::string value );
protected:
	std::string		resourceID_;
	std::string		textureFeed_;
	Moo::BaseTexturePtr value_;
};

#endif