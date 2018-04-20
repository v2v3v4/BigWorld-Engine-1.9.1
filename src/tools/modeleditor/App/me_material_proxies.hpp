/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#pragma once

//TODO: Remove this...
#include "guimanager/gui_manager.hpp"

#include "material_preview.hpp"
#include "me_error_macros.hpp"

typedef SmartPointer< class MatrixProxy > MatrixProxyPtr;
typedef SmartPointer< class FloatProxy > FloatProxyPtr;
typedef SmartPointer< class StringProxy > StringProxyPtr;

template <class CL, class DT> class MaterialProxy: public ReferenceCount
{
public:

	MaterialProxy( SmartPointer<CL> proxy )
	{
		proxies_.push_back( proxy );
		MaterialPreview::instance().needsUpdate( true );
	}

	void addProperty( SmartPointer<CL> proxy )
	{
		proxies_.push_back( proxy );
	}

	DT get() const
	{
		return proxies_[0]->get();
	}

	void set( DT val )
	{
		for ( std::vector< SmartPointer<CL> >::iterator it = proxies_.begin(); it != proxies_.end(); it++ )
			if (*it)
				(*it)->set( val, true );
		MaterialPreview::instance().needsUpdate( true );
	}

	bool getRange( int& min, int& max )
	{
		return proxies_[0]->getRange( min, max );
	}

	bool getRange( float& min, float& max, int& digits )
	{
		return proxies_[0]->getRange( min, max, digits );
	}

private:

	std::vector< SmartPointer<CL> > proxies_;
};

class MeMaterialFlagProxy: public IntProxy
{
public:
	MeMaterialFlagProxy( const std::string& flagName, std::string materialName, std::string* matterName = NULL, std::string* tintName = NULL):
	  flagName_(flagName),
	  materialName_(materialName),
	  matterName_(matterName),
	  tintName_(tintName)
	{}
	
	virtual uint32 EDCALL get() const
	{
		if ((matterName_) && (*matterName_ != "") && (tintName_) && (*tintName_ != "Default"))
		{
			return MeApp::instance().mutant()->tintFlag( *matterName_, *tintName_, flagName_ );
		}
		else
		{
			return MeApp::instance().mutant()->materialFlag( materialName_, flagName_ );
		}
	}
	
	virtual void EDCALL set( uint32 v, bool transient )
	{
		// set it
		if ((matterName_) && (*matterName_ != "") && (tintName_) && (*tintName_ != "Default"))
		{
			MeApp::instance().mutant()->tintFlag( *matterName_, *tintName_, flagName_, v );
		}
		else
		{
			MeApp::instance().mutant()->materialFlag( materialName_, flagName_, v );
		}
	}
private:
	std::string materialName_;
	std::string* matterName_;
	std::string* tintName_;
	std::string flagName_;
};

template <class CL> class MeMaterialTextureProxy: public StringProxy
{
public:
	typedef typename std::string (CL::*GetFn)() const;
	typedef void (CL::*SetFn)( std::string v );

	MeMaterialTextureProxy( SmartPointer<CL> valPtr, GetFn getFn, SetFn setFn, const std::string& uiName, const std::string& materialName, const std::string& matterName, const std::string& tintName, const std::string& descName ):
	  valPtr_(valPtr),
	  getFn_(getFn),
	  setFn_(setFn),
	  uiName_(uiName),
	  materialName_(materialName),
	  matterName_(matterName),
	  tintName_(tintName),
	  descName_(descName)
	{}
	
	virtual std::string EDCALL get() const
	{
		return ((*valPtr_).*getFn_)();
	}
	
	virtual void EDCALL set( std::string v, bool transient )
	{
#if MANAGED_CUBEMAPS
		// Make sure there isn't a mismatch between the supplied texture and the expected
		// texture type
		//
		// Get the managed effect from the mutant
		Moo::EffectMaterialPtr pEffectMaterial;
		if ( !(pEffectMaterial =
				MeApp::instance().mutant()->getEffectForTint( matterName_, tintName_, materialName_ )) )
			return;
		
		// Get the managed effect
		Moo::ManagedEffectPtr pManagedEffect = pEffectMaterial->pEffect();

		// Get a handle to parameter descName_
		D3DXHANDLE param = pManagedEffect->pEffect()->GetParameterByName( descName_.c_str(), 0 );

		// Get the annotation, if any
		const char* UIWidget = 0;
		D3DXHANDLE hAnnot = pManagedEffect->pEffect()->GetAnnotationByName( param, "UIWidget" );
		if (hAnnot)
			pManagedEffect->pEffect()->GetString( hAnnot, &UIWidget );
		std::string widgetType;
		if (UIWidget)
			widgetType = std::string(UIWidget);
		else
			widgetType = "";

		
		// Check the type of the new texture
		bool isCubeMap = false;
		std::string             newResID = BWResolver::dissolveFilename( v );
		Moo::BaseTexturePtr		newBaseTex = Moo::TextureManager::instance()->get( newResID ).getObject();

		// Is this texture a cube map
		if ( newBaseTex )
			isCubeMap = newBaseTex->isCubeMap();

		// Check if the types match up
		if ( newBaseTex && widgetType == "CubeMap" && !isCubeMap )
		{
			ME_WARNING_MSG(	"Warning - You have attempted to assign a non-cube map texture to a\n"
							"cube map texture slot!  This is not permitted.")

			return;
		}
		if ( newBaseTex && widgetType != "CubeMap" && isCubeMap )
		{
			ME_WARNING_MSG(	"Warning - You have attempted to assign a cube map texture to a\n"
							"non-cube map texture slot!  This is not permitted.")

			return;
		}
#endif
		if ( !v.empty() )
		{
			std::string sfilename = BWResource::resolveFilename(v);
			std::replace(sfilename.begin(), sfilename.end(), '/', '\\');
			std::wstring wfilename(sfilename.begin(), sfilename.end());

			//Create a PIDL from the filename:
			HRESULT hr = S_OK;
			ITEMIDLIST *pidl = NULL;
			DWORD flags = 0;
			hr = ::SHILCreateFromPath(wfilename.c_str(), &pidl, &flags); 

			if (SUCCEEDED(hr))
			{
			   // Convert the PIDL back to a filename (now corrected for case):
			   char buffer[MAX_PATH];
			   ::SHGetPathFromIDList(pidl, buffer);
			   ::ILFree(pidl); 

				sfilename = buffer;
			}
			
			v = BWResource::dissolveFilename(sfilename);
		}

		((*valPtr_).*setFn_)( v );

		if (transient) return;

		if ((matterName_ != "") && (tintName_ != "Default"))
		{
			MeApp::instance().mutant()->setTintProperty( matterName_, tintName_, descName_, uiName_, "Texture", v );
		}
		else
		{
			MeApp::instance().mutant()->setMaterialProperty( materialName_, descName_, uiName_, "Texture", v );
		}
		MeApp::instance().mutant()->recalcTextureMemUsage(); // This could have changed
	}

private:
	SmartPointer<CL> valPtr_;
	GetFn getFn_;
	SetFn setFn_;
	std::string uiName_;
	std::string materialName_;
	std::string matterName_;
	std::string tintName_;
	std::string descName_;
};

template <class CL> class MeMaterialBoolProxy: public BoolProxy
{
public:
	typedef typename bool (CL::*GetFn)() const;
	typedef void (CL::*SetFn)( bool v );

	MeMaterialBoolProxy( SmartPointer<CL> valPtr, GetFn getFn, SetFn setFn, const std::string& uiName, const std::string& materialName, const std::string& matterName, const std::string& tintName, const std::string& descName ):
	  valPtr_(valPtr),
	  getFn_(getFn),
	  setFn_(setFn),
	  uiName_(uiName),
	  materialName_(materialName),
	  matterName_(matterName),
	  tintName_(tintName),
	  descName_(descName)
	{}
	
	virtual bool EDCALL get() const
	{
		return ((*valPtr_).*getFn_)();
	}
	
	virtual void EDCALL set( bool v, bool transient )
	{
		((*valPtr_).*setFn_)( v );

		if (transient) return;

		if ((matterName_ != "") && (tintName_ != "Default"))
		{
			MeApp::instance().mutant()->setTintProperty( matterName_, tintName_, descName_, uiName_, "Bool", v ? "true" : "false" );
		}
		else
		{
			MeApp::instance().mutant()->setMaterialProperty( materialName_, descName_, uiName_, "Bool", v ? "true" : "false" );
		}
	}

private:
	SmartPointer<CL> valPtr_;
	GetFn getFn_;
	SetFn setFn_;
	std::string uiName_;
	std::string materialName_;
	std::string matterName_;
	std::string tintName_;
	std::string descName_;
};

template <class CL> class MeMaterialIntProxy: public IntProxy
{
public:
	typedef typename uint32 (CL::*GetFn)() const;
	typedef void (CL::*SetFn)( uint32 v );
	typedef bool (CL::*RangeFn)( int& min, int& max );

	MeMaterialIntProxy( SmartPointer<CL> valPtr, GetFn getFn, SetFn setFn, RangeFn rangeFn, const std::string& uiName, const std::string& materialName, const std::string& matterName, const std::string& tintName, const std::string& descName ):
	  valPtr_(valPtr),
	  getFn_(getFn),
	  setFn_(setFn),
	  rangeFn_(rangeFn),
	  uiName_(uiName),
	  materialName_(materialName),
	  matterName_(matterName),
	  tintName_(tintName),
	  descName_(descName)
	{}
	
	virtual uint32 EDCALL get() const
	{
		return ((*valPtr_).*getFn_)();
	}
	
	virtual void EDCALL set( uint32 v, bool transient )
	{
		((*valPtr_).*setFn_)( v );

		if (transient) return;

		char buf[16];
		bw_snprintf( buf, sizeof(buf), "%d", v );
		if ((matterName_ != "") && (tintName_ != "Default"))
		{
			MeApp::instance().mutant()->setTintProperty( matterName_, tintName_, descName_, uiName_, "Int", buf );
		}
		else
		{
			MeApp::instance().mutant()->setMaterialProperty( materialName_, descName_, uiName_, "Int", buf );
		}
	}

	bool getRange( int& min, int& max ) const
	{
		int mind, maxd;
		if( rangeFn_ )
		{
			bool result = ((*valPtr_).*rangeFn_)( mind, maxd );
			min = mind;
			max = maxd;
			return result;
		}
		return false;
	}

private:
	SmartPointer<CL> valPtr_;
	GetFn getFn_;
	SetFn setFn_;
	RangeFn rangeFn_;
	std::string uiName_;
	std::string materialName_;
	std::string matterName_;
	std::string tintName_;
	std::string descName_;
};

template <class CL> class MeMaterialEnumProxy: public IntProxy
{
public:
	typedef typename uint32 (CL::*GetFn)() const;
	typedef void (CL::*SetFn)( uint32 v );
	typedef bool (CL::*RangeFn)( int& min, int& max );

	MeMaterialEnumProxy( SmartPointer<CL> valPtr, GetFn getFn, SetFn setFn, const std::string& uiName, const std::string& materialName, const std::string& matterName, const std::string& tintName, const std::string& descName ):
	  valPtr_(valPtr),
	  getFn_(getFn),
	  setFn_(setFn),
	  uiName_(uiName),
	  materialName_(materialName),
	  matterName_(matterName),
	  tintName_(tintName),
	  descName_(descName)
	{}
	
	virtual uint32 EDCALL get() const
	{
		return ((*valPtr_).*getFn_)();
	}
	
	virtual void EDCALL set( uint32 v, bool transient )
	{
		((*valPtr_).*setFn_)( v );

		if (transient) return;

		char buf[16];
		bw_snprintf( buf, sizeof(buf), "%d", v );
		if ((matterName_ != "") && (tintName_ != "Default"))
		{
			MeApp::instance().mutant()->setTintProperty( matterName_, tintName_, descName_, uiName_, "Int", buf );
		}
		else
		{
			MeApp::instance().mutant()->setMaterialProperty( materialName_, descName_, uiName_, "Int", buf );
		}
	}

private:
	SmartPointer<CL> valPtr_;
	GetFn getFn_;
	SetFn setFn_;
	std::string uiName_;
	std::string materialName_;
	std::string matterName_;
	std::string tintName_;
	std::string descName_;
};

template <class CL> class MeMaterialFloatProxy: public FloatProxy
{
public:
	typedef typename float (CL::*GetFn)() const;
	typedef void (CL::*SetFn)( float v );
	typedef bool (CL::*RangeFn)( float& min, float& max, int& digits );

	MeMaterialFloatProxy( SmartPointer<CL> valPtr, GetFn getFn, SetFn setFn, RangeFn rangeFn, const std::string& uiName, const std::string& materialName, const std::string& matterName, const std::string& tintName, const std::string& descName ):
	  valPtr_(valPtr),
	  getFn_(getFn),
	  setFn_(setFn),
	  rangeFn_(rangeFn),
	  uiName_(uiName),
	  materialName_(materialName),
	  matterName_(matterName),
	  tintName_(tintName),
	  descName_(descName)
	{}
	
	virtual float EDCALL get() const
	{
		return ((*valPtr_).*getFn_)();
	}
	
	virtual void EDCALL set( float v, bool transient )
	{
		((*valPtr_).*setFn_)( v );

		if (transient) return;
		
		char buf[16];
		bw_snprintf( buf, sizeof(buf), "%f", v );
		if ((matterName_ != "") && (tintName_ != "Default"))
		{
			MeApp::instance().mutant()->setTintProperty( matterName_, tintName_, descName_, uiName_, "Float", buf );
		}
		else
		{
			MeApp::instance().mutant()->setMaterialProperty( materialName_, descName_, uiName_, "Float", buf );
		}
	}

	bool getRange( float& min, float& max, int& digits ) const
	{
		float mind, maxd;
		if( rangeFn_ )
		{
			bool result =  ((*valPtr_).*rangeFn_)( mind, maxd, digits );
			min = mind;
			max = maxd;
			return result;
		}
		return false;
	}

private:
	SmartPointer<CL> valPtr_;
	GetFn getFn_;
	SetFn setFn_;
	RangeFn rangeFn_;
	std::string uiName_;
	std::string materialName_;
	std::string matterName_;
	std::string tintName_;
	std::string descName_;
};

template <class CL> class MeMaterialVector4Proxy: public Vector4Proxy
{
public:
	typedef typename Vector4 (CL::*GetFn)() const;
	typedef void (CL::*SetFn)( Vector4 v );

	MeMaterialVector4Proxy( SmartPointer<CL> valPtr, GetFn getFn, SetFn setFn, const std::string& uiName, const std::string& materialName, const std::string& matterName, const std::string& tintName, const std::string& descName ):
	  valPtr_(valPtr),
	  getFn_(getFn),
	  setFn_(setFn),
	  uiName_(uiName),
	  materialName_(materialName),
	  matterName_(matterName),
	  tintName_(tintName),
	  descName_(descName)
	{}
	
	virtual Vector4 EDCALL get() const
	{
		return ((*valPtr_).*getFn_)();
	}
	
	virtual void EDCALL set( Vector4 v, bool transient )
	{
		((*valPtr_).*setFn_)( v );

		if (transient) return;

		char buf[256];
		bw_snprintf( buf, sizeof(buf), "%f %f %f %f", v.x, v.y, v.z, v.w );
		if ((matterName_ != "") && (tintName_ != "Default"))
		{
			MeApp::instance().mutant()->setTintProperty( matterName_, tintName_, descName_, uiName_, "Vector4", buf );
		}
		else
		{
			MeApp::instance().mutant()->setMaterialProperty( materialName_, descName_, uiName_, "Vector4", buf );
		}
	}

private:
	SmartPointer<CL> valPtr_;
	GetFn getFn_;
	SetFn setFn_;
	std::string uiName_;
	std::string materialName_;
	std::string matterName_;
	std::string tintName_;
	std::string descName_;
};
