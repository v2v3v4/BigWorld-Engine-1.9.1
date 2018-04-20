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
#include "cstdmf/debug.hpp"
#include "moo/effect_material.hpp"
#include "material_utility.hpp"
#include "material_properties.hpp"
#include "resmgr/string_provider.hpp"

DECLARE_DEBUG_COMPONENT2( "Common", 0 )


/**
 *	This method safely returns the D3DXEffect that a material uses,
 *	or NULL if there is an error.
 *
 *	The return value is a reference counted COM object.
 */
ComObjectWrap<ID3DXEffect> MaterialUtility::effect(
	Moo::EffectMaterialPtr material )
{
	if (material && material->pEffect() )
	{
		Moo::ManagedEffectPtr pEffect = material->pEffect();
		
		if ( pEffect )
		{
			return pEffect->pEffect();
		}
	}

	return NULL;
}


/**
 *	This method returns the number of techniques in the given material.
 */
int MaterialUtility::numTechniques( Moo::EffectMaterialPtr material )
{
	int n = 0;	
	
	ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
	if ( pEffect != NULL )
	{
		D3DXHANDLE hTechnique = pEffect->GetTechnique(n);
		while ( hTechnique != NULL )
		{
			hTechnique = pEffect->GetTechnique(++n);
        }
    }

	return n;
}


/**
 *	This method fills a vector of strings with the list
 *	of techniques for the given effect material.
 *
 *	@param material		The effect material.
 *	@param retVector	The returned vector of strings.
 *
 *	@return				The number of techniques.
 */
int MaterialUtility::listTechniques(	Moo::EffectMaterialPtr material,
										std::vector<std::string> & retVector )
{
	retVector.clear();
	
	//Write out techniques
	ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
	if ( pEffect != NULL )
	{
		int i = 0;
		D3DXHANDLE hTechnique = pEffect->GetTechnique(i++);
		while ( hTechnique != NULL )
		{
			D3DXTECHNIQUE_DESC desc;
			pEffect->GetTechniqueDesc( hTechnique, &desc );
			retVector.push_back( std::string(desc.Name) );
			hTechnique = pEffect->GetTechnique(i++);
        }
    }

	return retVector.size();
}


/**
 *	This method selects the given technique in the given material
 *	for viewing.  It takes a technique index, which is the same
 *	as the index given in the listTechniques method.
 *
 *	@param material		The effect material.
 *	@param index		The index of the desired technique.
 *
 *	@return				Success or Failure.
 */
bool MaterialUtility::viewTechnique(	Moo::EffectMaterialPtr material,
										int index )
{
	if ( !MaterialUtility::isTechniqueValid( material, index ) )
		return false;

	ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
	if ( pEffect != NULL )
	{
		D3DXHANDLE handle = pEffect->GetTechnique( index );
		if ( handle != NULL )
		{
			material->hTechnique( handle );
			return true;
		}
	}

	return false;
}


/**
 *	This method selects the given technique in the given material
 *	for viewing.  It takes a technique name, instead of an index.
 *
 *	@param material		The effect material.
 *	@param name			The name of the desired technique.
 *
 *	@return				Success or Failure.
 */
bool MaterialUtility::viewTechnique(	Moo::EffectMaterialPtr material,
										const std::string & name )
{
	int index = techniqueByName( material, name );
	if ( index >= 0 )
	{
		return viewTechnique( material, index );
	}
	else
	{
		ERROR_MSG( "MaterialUtility::viewTechnique: technique '%s' not found for material %lx.\n", name.c_str(), (int)&*material );
		return false;
	}
}


/**
 *	This method returns the index of a technique, given the name of a
 *	technique.
 *
 *	@param	material		The effect material
 *	@param	name			The name of the desired technique
 *
 *	@return the index of the technique, or -1 to indicate the name was not
 *	found.
 */
int MaterialUtility::techniqueByName(	Moo::EffectMaterialPtr material,
										const std::string & name )
{
	//Write out techniques
	ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
	if ( pEffect != NULL )
	{
		int i = 0;
		D3DXHANDLE hTechnique = pEffect->GetTechnique(i);
		while ( hTechnique != NULL )
		{
			D3DXTECHNIQUE_DESC desc;
			pEffect->GetTechniqueDesc( hTechnique, &desc );
			if ( !stricmp( name.c_str(), desc.Name ) )
				return i;
			hTechnique = pEffect->GetTechnique(++i);
        }
    }

	return -1;
}


/**
 *	This method checks whether the given technique is valid.
 *
 *	@param material		The effect material.
 *	@param index		The index of the desired technique.
 *
 *	@return				If the technique is valid.
 */
bool MaterialUtility::isTechniqueValid(
	Moo::EffectMaterialPtr material,
	int index )
{
	ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
	if ( pEffect != NULL )
	{
		D3DXHANDLE handle = pEffect->GetTechnique( index );
		if ( handle != NULL )
		{
			return (pEffect->ValidateTechnique(handle) == D3D_OK);
		}
	}

	return false;
}


/**
 *	This method returns the index of the technique currently selected
 *	into a material.
 *
 *	If anything is wrong, it returns -1
 */
int MaterialUtility::currentTechnique( Moo::EffectMaterialPtr material )
{
	int nTechniques = MaterialUtility::numTechniques( material );

	ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
	if ( pEffect != NULL )
	{
		D3DXHANDLE current = pEffect->GetCurrentTechnique();
		if ( current != NULL )
		{
			for ( int i=0; i<nTechniques; i++ )
			{
				D3DXHANDLE handle = pEffect->GetTechnique( i );
				if ( handle == current )
					return i;
			}
		}
	}

	return -1;
}


/**
 *	This method returns the number of tweakable properties
 *	the material has.
 */
int MaterialUtility::numProperties( Moo::EffectMaterialPtr material )
{
	return material->properties().size();
}


/**
 *	This method fills a vector of strings with the list
 *	of editable properties for the given effect material.
 *
 *	@param material		The effect material.
 *	@param retVector	The returned vector of strings.
 *
 *	@return				The number of properties.
 */
int MaterialUtility::listProperties(
	Moo::EffectMaterialPtr material,
	std::vector<std::string> & retVector )
{
	ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
	if ( !pEffect )
		return 0;

	Moo::EffectMaterial::Properties& properties = material->properties();
    Moo::EffectMaterial::Properties::iterator it = properties.begin();
    Moo::EffectMaterial::Properties::iterator end = properties.end();
    while ( it != end )
    {
        D3DXHANDLE hParameter = it->first;
        Moo::EffectPropertyPtr& pProperty = it->second;

        D3DXPARAMETER_DESC desc;
        HRESULT hr = pEffect->GetParameterDesc( hParameter, &desc );
        if ( SUCCEEDED(hr) )
        {
			retVector.push_back( std::string(desc.Name) );
        }
		else
		{
			ERROR_MSG( "MaterialUtility::listProperties: DirectX error %lx.\n", (int)&*material );
		}

        it++;
    }

	return retVector.size();
}


/**
 *	This method saves the given material's tweakable properties
 *	to the given datasection.
 *
 *	Material saving does not support recursion / inherited properties.
 *
 *	@param	m			The material to save
 *	@param	pSection	The data section to save to.
 *
 *	@return Success or Failure.
 */
void MaterialUtility::save(
	Moo::EffectMaterialPtr material,
	DataSectionPtr pSection,
	bool worldBuilderEditableOnly /*= false*/ )
{
	ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
	if ( !pEffect )
		return;

    pSection->deleteSections( "property" );
    pSection->deleteSections( "fx" );

    std::vector<std::string> effects;
    
	if ( material->pEffect() )
    {
        effects.push_back( material->pEffect()->resourceID() );
    }

    if (effects.size() > 0&&!worldBuilderEditableOnly)
        pSection->writeStrings( "fx", effects );

	Moo::EffectMaterial::Properties& properties = material->properties();
    Moo::EffectMaterial::Properties::iterator it = properties.begin();
    Moo::EffectMaterial::Properties::iterator end = properties.end();
    
    while ( it != end )
    {
		D3DXHANDLE hParameter = it->first;
        Moo::EffectPropertyPtr& pProperty = it->second;

        if( ( !worldBuilderEditableOnly && artistEditable( &*pEffect, hParameter ) ) ||
			worldBuilderEditableOnly && worldBuilderEditable( &*pEffect, hParameter ) )
        {
            D3DXPARAMETER_DESC desc;
            HRESULT hr = pEffect->GetParameterDesc( hParameter, &desc );
            if ( SUCCEEDED(hr) )
            {
                Moo::EffectProperty* pProp = pProperty.getObject();
                EditorEffectProperty* eep =
                    static_cast<EditorEffectProperty*>(pProp);
                //If the following assertion is hit, then
                //runtimeInitMaterialProperties() was not called before this effect
                //material was created.
                MF_ASSERT( eep )
                if ( eep )
                {
                    std::string name(desc.Name);
                    DataSectionPtr pChild = pSection->newSection( "property" );
                    pChild->setString( name );
                    eep->save( pChild );
                }
            }
        }
		it++;
	}

	if(!worldBuilderEditableOnly)
	{
		pSection->writeInt( "collisionFlags", material->collisionFlags() );
		pSection->writeInt( "materialKind", material->materialKind() );
	}
}


bool MaterialUtility::artistEditable( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
{
	if( worldBuilderEditable( pEffect, hProperty ) )
		return TRUE;

	BOOL artistEditable = FALSE;
	D3DXHANDLE hAnnot = pEffect->GetAnnotationByName( hProperty, "artistEditable" );
	if (hAnnot)
		pEffect->GetBool( hAnnot, &artistEditable );
	return artistEditable == TRUE;
}


bool MaterialUtility::worldBuilderEditable( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
{
	BOOL worldBuilderEditable = FALSE;
	D3DXHANDLE hAnnot = pEffect->GetAnnotationByName( hProperty, "worldBuilderEditable" );
	if (hAnnot)
		pEffect->GetBool( hAnnot, &worldBuilderEditable );
	return worldBuilderEditable == TRUE;
}

std::string MaterialUtility::UIName( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
{
    const char* UIName = 0;
	D3DXHANDLE hAnnot = pEffect->GetAnnotationByName( hProperty, "UIName" );
	if (hAnnot)
		pEffect->GetString( hAnnot, &UIName );
        if (UIName)
	        return std::string(UIName);
        else
			return std::string("");
}

std::string MaterialUtility::UIDesc( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
{
    const char* UIDesc = 0;
	D3DXHANDLE hAnnot = pEffect->GetAnnotationByName( hProperty, "UIDesc" );
	if (hAnnot)
		pEffect->GetString( hAnnot, &UIDesc );
        if (UIDesc)
	        return std::string(UIDesc);
        else
			return std::string("");
}

std::string MaterialUtility::UIWidget( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
{
    const char* UIWidget = 0;
	D3DXHANDLE hAnnot = pEffect->GetAnnotationByName( hProperty, "UIWidget" );
	if (hAnnot)
		pEffect->GetString( hAnnot, &UIWidget );
        if (UIWidget)
	        return std::string(UIWidget);
        else
			return std::string("");
}

void MaterialUtility::setTexture( Moo::EffectMaterialPtr material,
	int idx, const std::string& textureName )
{
	ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
	if ( !pEffect )
		return;	

    Moo::EffectMaterial::Properties& properties = material->properties();
    Moo::EffectMaterial::Properties::iterator it = properties.begin();
    Moo::EffectMaterial::Properties::iterator end = properties.end();

    while ( it != end )
    {
        MF_ASSERT( it->second );
        D3DXHANDLE hParameter = it->first;
        Moo::EffectPropertyPtr& pProperty = it->second;

        if ( artistEditable( &*pEffect, hParameter ) )
        {
            D3DXPARAMETER_DESC desc;
            HRESULT hr = pEffect->GetParameterDesc( hParameter, &desc );
            if ( SUCCEEDED(hr) )
            {
                if (desc.Class == D3DXPC_OBJECT &&
                    desc.Type == D3DXPT_TEXTURE)
                {
                    Moo::EffectProperty* pProp = pProperty.getObject();
                    MaterialTextureProxy* tp = static_cast<MaterialTextureProxy*>(pProp);
                    MF_ASSERT(tp);
                    tp->set( textureName, false );
                }
            }
        }
        it++;
    }
}