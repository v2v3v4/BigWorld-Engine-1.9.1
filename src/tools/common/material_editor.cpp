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
#include "material_editor.hpp"
#include "cstdmf/debug.hpp"
#include "moo/effect_material.hpp"
#include "physics2/material_kinds.hpp"
#include "material_utility.hpp"
#include "resmgr/string_provider.hpp"

#define EDCALL __stdcall
#define WORLDEDITORDLL_API

DECLARE_DEBUG_COMPONENT2( "Common", 0 )

/**************************************************************
 *	Section - Material Editor
 **************************************************************/

//----------------------------------------------------
// make the standard python stuff
PY_TYPEOBJECT( MaterialEditor )

PY_BEGIN_METHODS( MaterialEditor )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( MaterialEditor )
PY_END_ATTRIBUTES()

//----------------------------------------------------

/**
 *	Constructor.
 */
MaterialEditor::MaterialEditor( Moo::EffectMaterialPtr m, PyTypePlus * pType ):
	GeneralEditor( pType )
{
	edit(m);
}


/**
 *	Destructor.
 */
MaterialEditor::~MaterialEditor()
{
}


/**
 *	This method instatiates all registered editors for all
 *	properties of the given material.
 *
 *	All existing properties being edited are cleared.
 */
void MaterialEditor::edit( Moo::EffectMaterialPtr material )
{
	ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
	if ( !pEffect )
		return;

	//Add the two default properties; material kind and collision flags
    DataSectionPtr pFile = BWResource::openSection( "resources/flags.xml" );
    DataSectionPtr pSect = pFile->openSection( "collisionFlags" );
    MF_ASSERT( pSect.hasObject() );
    SmartPointer<CollisionFlagsProxy> ip2 = new CollisionFlagsProxy(material);
    ChoiceProperty* cp2 = new ChoiceProperty( "Collision Flags", ip2, pSect, true );
    this->addProperty( cp2 );

    // load the material kinds
	DataSectionPtr pKinds = pFile->newSection( "materialKinds" );
	pKinds->writeInt( "(Use Visual's)", 0 );
	MaterialKinds::instance().populateDataSection( pKinds );
    SmartPointer<MaterialKindProxy> ip1 = new MaterialKindProxy(material);
    ChoiceProperty* cp1 = new ChoiceProperty( "Material Kind", ip1, pKinds );
    this->addProperty( cp1 );

	//Now add the material's own properties.
	material->replaceDefaults();

    std::vector<Moo::EffectPropertyPtr> existingProps;

    if ( material->pEffect() )
    {
        ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect(material);
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

            //Skip over properties that we have already added.  This can occur
            //when using multi-layer effects - there will most likely be
            //shared properties referenced by both effects.
            std::vector<Moo::EffectPropertyPtr>::iterator fit =
                std::find(existingProps.begin(),existingProps.end(),pProperty);
            if ( fit != existingProps.end() )
            {
                it++;
                continue;
            }

            existingProps.push_back(pProperty);
            DXEnum dxenum( s_dxenumPath );

            if ( MaterialUtility::artistEditable( &*pEffect, hParameter ) )
            {
                D3DXPARAMETER_DESC desc;
                HRESULT hr = pEffect->GetParameterDesc( hParameter, &desc );
                if ( SUCCEEDED(hr) )
                {
                    MPEKeyType key = std::make_pair( desc.Class, desc.Type );
                    MaterialProperties::iterator it = g_editors.find( key );
                    if ( it != g_editors.end() )
                    {
                        DEBUG_MSG( L("COMMON/MATERIAL_EDITOR/TRYING_CREATE", desc.Name ) );
                        MF_ASSERT( it->second );
                        D3DXHANDLE enumHandle = pEffect->GetAnnotationByName( hParameter, "EnumType" );
                        D3DXPARAMETER_DESC enumPara;
                        LPCSTR enumType;
                        if( desc.Class == D3DXPC_SCALAR && desc.Type == D3DXPT_INT &&
                                enumHandle &&
                                SUCCEEDED( pEffect->GetParameterDesc( enumHandle, &enumPara ) ) &&
                                enumPara.Type == D3DXPT_STRING &&
                                SUCCEEDED( pEffect->GetString( enumHandle, &enumType ) ) &&
                                dxenum.isEnum( enumType ) )
                        {
                                DataSectionPtr enumSec = pFile->newSection( enumType );
                                for( DXEnum::size_type i = 0;
                                        i != dxenum.size( enumType ); ++i)
                                {
                                        std::string name = dxenum.entry( enumType, i );
                                        enumSec->writeInt( name,
                                                dxenum.value( enumType, name ) );
                                }
				std::string uiName = MaterialUtility::UIName( pEffect.pComObject(), hParameter );
				if( uiName.size() == 0 )
					uiName = desc.Name;
                                GeneralProperty* ed = new ChoiceProperty
                                        (
                                                uiName,
                                                new MaterialEnumProxy(
                                                        enumType,
                                                        (MaterialIntProxy*)pProperty.getObject()
                                                        ),
                                                enumSec
                                        );
                                this->addProperty( ed );
                        }
                        else
                        {
                                GeneralProperty* ed = it->second(desc.Name,pProperty);

                                if (ed)
                                {
                                    DEBUG_MSG( L("COMMON/MATERIAL_EDITOR/CREATED", Formatter( (int)ed, "%lx" ) ) );

                                    ed->UIName( MaterialUtility::UIName( &*pEffect, hParameter ) );

                                    ed->WBEditable( MaterialUtility::worldBuilderEditable( &*pEffect, hParameter ) );

                                    this->addProperty( ed );
                                }
                                else
                                {
                                    WARNING_MSG( L("COMMON/MATERIAL_EDITOR/NOT_CREATED") );
                                }
                        }
                    }
                    else
                    {
                        ERROR_MSG( L("COMMON/MATERIAL_EDITOR/NOT_FOUND", desc.Name) );
                    }
                }
                else
                {
                    ERROR_MSG( L("COMMON/MATERIAL_EDITOR/DX_FAILED", Formatter( hr, "%lx" ) ) );
                }
            }

            it++;
        }
    }
}
