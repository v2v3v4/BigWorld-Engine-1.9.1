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

#include "model/super_model.hpp"
#include "model/super_model_dye.hpp"

#include "resmgr/xml_section.hpp"
#include "resmgr/string_provider.hpp"

#include "undo_redo.hpp"

#include "romp/geometrics.hpp"

#include "common/material_utility.hpp"
#include "common/material_properties.hpp"
#include "common/material_editor.hpp"
#include "common/dxenum.hpp"

#include "utilities.hpp"

#include "mutant.hpp"

DECLARE_DEBUG_COMPONENT2( "Mutant_Materials", 0 )

MaterialInfo::MaterialInfo():
	nameData (NULL)
{}
MaterialInfo::MaterialInfo(
	const std::string& cName,
	DataSectionPtr cNameData,
	EffectMaterialSet cEffect,
	std::vector< DataSectionPtr > cData,
	std::string cFormat
):
	name (cName),
	nameData (cNameData),
	effect (cEffect),
	data (cData),
	format (cFormat)
{}

TintInfo::TintInfo()
{}
TintInfo::TintInfo(
	Moo::EffectMaterialPtr cEffect,
	DataSectionPtr cData,
	SuperModelDyePtr cDye,
	std::string cFormat
):
	effect (cEffect),
	data (cData),
	dye (cDye),
	format (cFormat)
{}

std::string Mutant::materialDisplayName( const std::string& materialName )
{
	std::string test = materials_[materialName].name;
	return materials_[materialName].name;
}

void Mutant::setDye( const std::string& matterName, const std::string& tintName, Moo::EffectMaterialPtr & material  )
{
	currDyes_[matterName] = tintName;

	material = tints_[matterName][tintName].effect;

	recreateFashions();
}

void Mutant::getMaterial( const std::string& materialName, EffectMaterialSet & material )
{
	material = materials_[materialName].effect;
}

std::string Mutant::getTintName( const std::string& matterName )
{
	if (currDyes_.find(matterName) != currDyes_.end())
		return currDyes_[matterName];
	else
		return "Default";
}

bool Mutant::setMaterialProperty( const std::string& materialName, const std::string& descName, const std::string& uiName,  const std::string& propType, const std::string& val )
{
	std::vector< DataSectionPtr > pMats = materials_[materialName].data;

	std::vector< DataSectionPtr >::iterator matIt = pMats.begin();
	std::vector< DataSectionPtr >::iterator matEnd = pMats.end();
	while (matIt != matEnd)
	{
		DataSectionPtr data = *matIt++;

		UndoRedo::instance().add( new UndoRedoOp( 0, data, currVisual_ ));

		std::vector< DataSectionPtr > pProps;
		data->openSections( "property", pProps );

		bool done = false;
		
		std::vector< DataSectionPtr >::iterator propsIt = pProps.begin();
		std::vector< DataSectionPtr >::iterator propsEnd = pProps.end();
		while (propsIt != propsEnd)
		{
			DataSectionPtr prop = *propsIt++;

			if (descName == prop->asString())
			{
				DataSectionPtr textureFeed = prop->openSection("TextureFeed");
			
				if (textureFeed)
				{
					textureFeed->writeString( "default", BWResource::dissolveFilename( val ));
				}
				else
				{
					prop->writeString( propType, BWResource::dissolveFilename( val ));
				}

				done = true;
			}
		}

		if (!done)
		{
			data = data->newSection( "property" );
			data->setString( descName );
			data->writeString( propType, BWResource::dissolveFilename( val ));
		}
	}

	UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/CHANGE_TO", uiName ), true );

	return true;

}

void Mutant::instantiateMFM( DataSectionPtr data )
{
	std::string mfmFile = data->readString( "mfm", "" );

	if (mfmFile != "")
	{
		DataSectionPtr mfmData = BWResource::openSection( mfmFile, false );

		if (!mfmData) return;

		std::string temp = data->readString("identifier", "");
		if (temp != "")
			mfmData->writeString( "identifier", temp );
		temp = data->readString("fx", "");
		if (temp != "")
			mfmData->writeString( "fx", temp );

		mfmData->writeInt( "collisionFlags",
			data->readInt("collisionFlags", 0) );

		mfmData->writeInt( "materialKind",
			data->readInt("materialKind", 0) );
		
		std::vector< DataSectionPtr > pSrcProps;
		data->openSections( "property", pSrcProps );

		std::vector< DataSectionPtr > pDestProps;
		mfmData->openSections( "property", pDestProps );

		std::vector< DataSectionPtr >::iterator destPropsIt = pDestProps.begin();
		std::vector< DataSectionPtr >::iterator destPropsEnd = pDestProps.end();
		while (destPropsIt != destPropsEnd)
		{
			DataSectionPtr destProp = *destPropsIt++;

			std::vector< DataSectionPtr >::iterator srcPropsIt = pSrcProps.begin();
			std::vector< DataSectionPtr >::iterator srcPropsEnd = pSrcProps.end();
			while (srcPropsIt != srcPropsEnd)
			{
				DataSectionPtr srcProp = *srcPropsIt++;

				if (destProp->asString() == srcProp->asString())
				{
					destProp->copy( srcProp );
				}
			}
		}

		data->copy( mfmData );
		data->delChild("mfm");
	}
}

void Mutant::overloadMFM( DataSectionPtr data, DataSectionPtr mfmData )
{
	std::string temp = mfmData->readString("fx", "");
	if (temp != "")
		data->writeString( "fx", temp );
	
	data->writeInt( "collisionFlags",
		mfmData->readInt("collisionFlags", 0) );

	data->writeInt( "materialKind",
		mfmData->readInt("materialKind", 0) );
		
	std::vector< DataSectionPtr > pSrcProps;
	mfmData->openSections( "property", pSrcProps );

	std::vector< DataSectionPtr > pDestProps;
	data->openSections( "property", pDestProps );

	std::vector< DataSectionPtr >::iterator srcPropsIt = pSrcProps.begin();
	std::vector< DataSectionPtr >::iterator srcPropsEnd = pSrcProps.end();
	while (srcPropsIt != srcPropsEnd)
	{
		DataSectionPtr srcProp = *srcPropsIt++;

		bool placed = false;
		
		std::vector< DataSectionPtr >::iterator destPropsIt = pDestProps.begin();
		std::vector< DataSectionPtr >::iterator destPropsEnd = pDestProps.end();
		while (destPropsIt != destPropsEnd)
		{
			DataSectionPtr destProp = *destPropsIt++;

			if (destProp->asString() == srcProp->asString())
			{
				destProp->copy( srcProp );
				placed = true;
			}
		}

		if (!placed)
		{
			data->newSection("property")->copy( srcProp );
		}
	}
}

void Mutant::cleanMaterials()
{
	std::map< std::string, MaterialInfo >::iterator it = materials_.begin();
	std::map< std::string, MaterialInfo >::iterator end = materials_.end();

	for (; it != end; ++it)
	{
		std::string materialName = it->first;
		std::vector< DataSectionPtr > data = it->second.data;
		Moo::EffectMaterialPtr effect = *(it->second.effect.begin());
		if (tintedMaterials_.find( materialName ) != tintedMaterials_.end())
		{
			effect = tints_[ tintedMaterials_[materialName] ]["Default"].effect;
		}

		if (data.size() == 0) continue;

		instantiateMFM( data[0] );
		
		// Make a backup of the old material data
		XMLSectionPtr materialData = new XMLSection("old_state");
		materialData->copy( data[0] );
		
		// Erase all the material data
		for (unsigned i=0; i<data.size(); i++)
		{
			data[i]->delChildren();
			
			// Copy the default fields first
			std::string temp = materialData->readString("identifier", "");
			if (temp != "")
				data[i]->writeString( "identifier", temp );

			// Write all effect references
			std::vector< std::string > fxs;
			materialData->readStrings( "fx", fxs );
			for ( unsigned j=0; j<fxs.size(); j++)
			{
				data[i]->newSection( "fx" )->setString( fxs[j] );
			}

			data[i]->writeInt( "collisionFlags",
				materialData->readInt("collisionFlags", 0) );

			data[i]->writeInt( "materialKind",
				materialData->readInt("materialKind", 0) );
		}

		//Now add the material's own properties.
		effect->replaceDefaults();

		std::vector< std::string > existingProps;

		if ( effect->pEffect() )
		{
			ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( effect );
			if ( !pEffect )
				continue;

			Moo::EffectMaterial::Properties& properties = effect->properties();
			Moo::EffectMaterial::Properties::iterator propIt = properties.begin();
			Moo::EffectMaterial::Properties::iterator propEnd = properties.end();

			for (; propIt != propEnd; ++propIt )
			{
				MF_ASSERT( propIt->second );
				D3DXHANDLE hParameter = propIt->first;
				Moo::EffectPropertyPtr& pProperty = propIt->second;
				
				if ( MaterialUtility::artistEditable( &*pEffect, hParameter ) )
				{
					D3DXPARAMETER_DESC desc;
					HRESULT hr = pEffect->GetParameterDesc( hParameter, &desc );
					if ( SUCCEEDED(hr) )
					{
						std::string descName = desc.Name;

						// Find all the material properties
						std::vector< DataSectionPtr > srcProps;
						materialData->openSections( "property", srcProps );

						// Copy the material properties if they exist
						std::vector< DataSectionPtr >::iterator srcPropsIt = srcProps.begin();
						std::vector< DataSectionPtr >::iterator srcPropsEnd = srcProps.end();
						for (; srcPropsIt != srcPropsEnd; ++srcPropsIt)
						{
							if (descName == (*srcPropsIt)->asString())
							{
								// Skip over properties that we have already added.  This can occur
								// when using multi-layer effects - there will most likely be
								// shared properties referenced by both effects.
								std::vector< std::string >::iterator fit =
									std::find(existingProps.begin(),existingProps.end(),descName);
								if ( fit == existingProps.end() )
								{
									// For all the material sections add the property
									for (unsigned i=0; i<data.size(); i++)
									{
										data[i]->newSection("property")->copy( (*srcPropsIt) );
									}
									existingProps.push_back( descName );
									continue;
								}
							}
						}
					}
				}
			}
		}
	}
	dirty( currVisual_ );
}

void Mutant::cleanTints()
{
	std::map< std::string, std::map < std::string, TintInfo > >::iterator dyeIt = tints_.begin();
	std::map< std::string, std::map < std::string, TintInfo > >::iterator dyeEnd = tints_.end();

	for(; dyeIt != dyeEnd; ++dyeIt)
	{
		std::string matterName = (*dyeIt).first;
		std::map < std::string, TintInfo >::iterator tintIt = (*dyeIt).second.begin();
		std::map < std::string, TintInfo >::iterator tintEnd = (*dyeIt).second.end();

		for(; tintIt != tintEnd; ++tintIt)
		{
			std::string tintName = tintIt->first;
			DataSectionPtr data = tintIt->second.data;
			Moo::EffectMaterialPtr effect = tintIt->second.effect;

			if (data)
				data = data->openSection("material");

			if (!data) 
				continue;

			instantiateMFM( data );

			// Make a backup of the old material data
			XMLSectionPtr materialData = new XMLSection("old_state");
			materialData->copy( data );
			
			// Erase all the material data
			data->delChildren();
				
			// Copy the default fields first
			std::string temp = materialData->readString("identifier", "");
			if (temp != "")
				data->writeString( "identifier", temp );
			
			// Write all effect references
			std::vector< std::string > fxs;
			materialData->readStrings( "fx", fxs );
			for ( unsigned j=0; j<fxs.size(); j++)
			{
				data->newSection( "fx" )->setString( fxs[j] );
			}

			data->writeInt( "collisionFlags",
				materialData->readInt("collisionFlags", 0) );

			data->writeInt( "materialKind",
				materialData->readInt("materialKind", 0) );

			//Now add the material's own properties.
			effect->replaceDefaults();

			std::vector<Moo::EffectPropertyPtr> existingProps;

			if ( effect->pEffect() )
			{
				ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( effect );
				if ( !pEffect )
					continue;

				Moo::EffectMaterial::Properties& properties = effect->properties();
				Moo::EffectMaterial::Properties::iterator propIt = properties.begin();
				Moo::EffectMaterial::Properties::iterator propEnd = properties.end();

				for (; propIt != propEnd; ++propIt )
				{
					MF_ASSERT( propIt->second );
					D3DXHANDLE hParameter = propIt->first;
					Moo::EffectPropertyPtr& pProperty = propIt->second;

					// Skip over properties that we have already added.  This can occur
					// when using multi-layer effects - there will most likely be
					// shared properties referenced by both effects.
					std::vector<Moo::EffectPropertyPtr>::iterator fit =
						std::find(existingProps.begin(),existingProps.end(),pProperty);
					if ( fit != existingProps.end() )
					{
						continue;
					}

					existingProps.push_back(pProperty);

					if ( MaterialUtility::artistEditable( &*pEffect, hParameter ) )
					{
						D3DXPARAMETER_DESC desc;
						HRESULT hr = pEffect->GetParameterDesc( hParameter, &desc );
						if ( SUCCEEDED(hr) )
						{
							std::string descName = desc.Name;

							// Find all the material properties
							std::vector< DataSectionPtr > srcProps;
							materialData->openSections( "property", srcProps );

							// Copy the material properties if they exist
							std::vector< DataSectionPtr >::iterator srcPropsIt = srcProps.begin();
							std::vector< DataSectionPtr >::iterator srcPropsEnd = srcProps.end();
							for (; srcPropsIt != srcPropsEnd; ++srcPropsIt)
							{
								if (descName == (*srcPropsIt)->asString())
								{
									data->newSection("property")->copy( (*srcPropsIt) );
								}
							}
						}
					}
				}
			}
		}
	}
	dirty( currModel_ );
}

void Mutant::cleanMaterialNames()
{
	if (materialNameDataToRemove_.empty()) return;
	
	DataSectionPtr data = currModel_->openSection( "materialNames" );

	if (data == NULL) return;
	
	std::vector< DataSectionPtr>::iterator it = materialNameDataToRemove_.begin();
	std::vector< DataSectionPtr>::iterator end = materialNameDataToRemove_.end();
	for (; it != end; ++it)
	{
		data->delChild( *it );
	}
	
	materialNameDataToRemove_.clear();
	
	dirty( currModel_ );
}

bool Mutant::setTintProperty( const std::string& matterName, const std::string& tintName, const std::string& descName,  const std::string& uiName,  const std::string& propType, const std::string& val )
{
	DataSectionPtr data = tints_[matterName][tintName].data;

	if (data == NULL) return false;

	DataSectionPtr materialData = data->openSection("material");

	if (materialData == NULL) return false;

	UndoRedo::instance().add( new UndoRedoOp( 0, materialData, currModel_ ));
	UndoRedo::instance().barrier( "Change to " + uiName, true );

	instantiateMFM( materialData );

	std::vector<DataSectionPtr> pProps;
	materialData->openSections( "property", pProps );

	bool done = false;

	std::vector< DataSectionPtr >::iterator propsIt = pProps.begin();
	std::vector< DataSectionPtr >::iterator propsEnd = pProps.end();
	while (propsIt != propsEnd)
	{
		DataSectionPtr prop = *propsIt++;

		if (descName == prop->asString())
		{
			DataSectionPtr textureFeed = prop->openSection("TextureFeed");
			
			if (textureFeed)
			{
				textureFeed->writeString( "default", BWResource::dissolveFilename( val ));
			}
			else
			{
				prop->writeString( propType, BWResource::dissolveFilename( val ));
			}

			done = true;
		}
	}

	if (!done)
	{
		materialData = materialData->newSection( "property" );
		materialData->setString( descName );
		materialData->writeString( propType, BWResource::dissolveFilename( val ));
	}

	//Now do nearly the exact same thing for the parent's "property" values for exposed properties.
	//We do this to keep the python exposed value the same as the materials.

	data->openSections( "property", pProps );
	
	propsIt = pProps.begin();
	propsEnd = pProps.end();
	while (propsIt != propsEnd)
	{
		DataSectionPtr prop = *propsIt++;

		if (descName == prop->readString( "name", "" ))
		{
			prop->writeVector4( "default", getExposedVector4( matterName, tintName, descName, propType, val ) );
		}
	}

	return true;

}

bool Mutant::materialName( const std::string& materialName, const std::string& new_name )
{
	//Exit if we have already set this name
	if (new_name == materials_[materialName].name) return true;

	std::map< std::string, MaterialInfo >::iterator it = materials_.begin();

	//Determine whether that material name is being used and exit if it is
	for (;it != materials_.end(); ++it)
	{
		if (new_name == it->second.name) return false;
	}

	DataSectionPtr pNameData = materials_[materialName].nameData;

	if (pNameData != NULL)
	{

		UndoRedo::instance().add( new UndoRedoOp( 0, pNameData, currModel_ ));
		UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/CHANGING_MATERIAL_NAME"), true );

	}
	else
	{
		pNameData = currModel_->openSection( "materialNames", true );

		UndoRedo::instance().add( new UndoRedoOp( 0, pNameData, currModel_ ));
		UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/CHANGING_MATERIAL_NAME"), true );
		
		pNameData = pNameData->newSection( "material" );
		materials_[materialName].nameData = pNameData;
	}

	pNameData->writeString( "original", materialName );
	pNameData->writeString( "display", new_name );
	materials_[materialName].name = new_name;

	//Update the tinted material map to use the new material name
	if ( tintedMaterials_.find( materialName ) != tintedMaterials_.end())
	{
		tintedMaterials_[new_name] = tintedMaterials_[materialName];
		tintedMaterials_.erase( materialName );
	}

	triggerUpdate( "Object" );

	return true;
}

bool Mutant::matterName( const std::string& matterName, const std::string& new_name, bool undoRedo )
{
	if (matterName == new_name) return true;
	
	if (undoRedo && dyes_.find( new_name ) != dyes_.end()) return false;

	//If we are curently using this matter make sure to update the reference
	if (currDyes_.find( matterName ) != currDyes_.end())
	{
		currDyes_[new_name] = currDyes_[matterName];
		currDyes_.erase( matterName );
	}

	if (undoRedo)
	{
		UndoRedo::instance().add( new UndoRedoMatterName( new_name, matterName ) );
		UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/CHANGING_DYE_NAME"), true );
	}
	
	if (dyes_.find( matterName ) != dyes_.end())
	{
		dyes_[matterName]->writeString("matter", new_name);

		dyes_[new_name] = dyes_[matterName];
		dyes_.erase(matterName);
	}

	if (tints_.find( matterName ) != tints_.end())
	{
		tints_[new_name] = tints_[matterName];
		tints_.erase(matterName);
	}

	//Update the tinted material map to use the new matter name
	std::map< std::string , std::string >::iterator tintIt = tintedMaterials_.begin();
	std::map< std::string , std::string >::iterator tintEnd = tintedMaterials_.end();
	for (; tintIt != tintEnd; ++tintIt)
	{
		if ( tintIt->second == matterName )
		{
			tintedMaterials_[tintIt->first] = new_name;
		}
	}

	this->reloadAllLists();

	return true;
}

bool Mutant::tintName( const std::string& matterName, const std::string& tintName, const std::string& new_name, bool undoRedo )
{
	if (tintName == new_name) return true;
	
	if (undoRedo)
	{
		TintMap::iterator tintIt = tints_.begin();
		TintMap::iterator tintEnd = tints_.end();
		for (; tintIt != tintEnd; ++tintIt)
		{
			if (tintIt->second.find( new_name ) != tintIt->second.end()) return false;
		}
	}
	//If we are curently using this matter make sure to update the reference
	if (currDyes_.find( matterName ) != currDyes_.end())
	{
		currDyes_[matterName] = new_name;
	}

	DataSectionPtr data = tints_[matterName][tintName].data;

	if (undoRedo)
	{
		UndoRedo::instance().add( new UndoRedoTintName( matterName, new_name, tintName ) );
		UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/CHANGING_TINT_NAME"), true );
	}

	data->writeString( "name", new_name );
	
	tints_[matterName][new_name] = tints_[matterName][tintName];
	tints_[matterName].erase(tintName);

	this->reloadAllLists();

	return true;
}

std::string Mutant::newTint( const std::string& materialName, const std::string& matterName, const std::string& oldTintName, const std::string& newTintName, const std::string& fxFile, const std::string& mfmFile )
{
	std::string newMatterName = matterName;
		
	DataSectionPtr data;
	DataSectionPtr pMFMSec;

	if (mfmFile != "")
	{
		pMFMSec = BWResource::openSection( mfmFile, false );
		if (!pMFMSec)
		{
			ERROR_MSG( "Cannot open MFM file: %s", mfmFile.c_str() );
			newMatterName.clear();
			return newMatterName;
		}
	}
		
	if (matterName == "")
	{
		UndoRedo::instance().add( new UndoRedoOp( 0, currModel_, currModel_ ));
		UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/ADDING_TINT"), true );

		data = currModel_->newSection("dye");

		newMatterName = Utilities::pythonSafeName( materialName );
		data->writeString( "matter", newMatterName );
		data->writeString( "replaces", materialName );
	}
	else
	{
		data = dyes_[matterName];

		UndoRedo::instance().add( new UndoRedoOp( 0, currModel_, currModel_ ));
		UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/ADDING_TINT"), true );

	}
	
	data = data->newSection("tint");
	data->writeString( "name", newTintName );
	data = data->newSection("material");
	if (fxFile != "")
	{
		if ((matterName == "") || (oldTintName == "Default")) // A material
		{
			if ((materials_.find( materialName ) != materials_.end()) &&
				( materials_[materialName].data[0]))
			{
				data->copy( materials_[materialName].data[0] );
			}
		}
		else // A tint
		{
			if ((tints_.find( matterName ) != tints_.end()) &&
				(tints_[matterName].find( oldTintName ) != tints_[matterName].end()) &&
				(tints_[matterName][oldTintName].data))
			{
				DataSectionPtr materialData( tints_[matterName][oldTintName].data->openSection("material") );
				if (materialData)
				{
					data->copy( materialData );
				}
			}
		}
		
		
		data->writeString( "fx", fxFile );
	}
	else
	{
		data->copy( pMFMSec );
	}

	reloadAllLists();

	return newMatterName;

}

bool Mutant::saveMFM( const std::string& materialName, const std::string& matterName, const std::string& tintName, const std::string& mfmFile )
{
	DataSectionPtr data;
	
	if ((matterName == "") || (tintName == "Default")) // We are a material
	{
		if (materials_.find( materialName ) == materials_.end())
			return false;
	
		for (unsigned i=0; i<materials_[materialName].data.size(); i++ )
		{
			data = materials_[materialName].data[i];
			instantiateMFM( data );
		}
	}
	else // We are a tint
	{
		data = tints_[matterName][tintName].data;

		if (data == NULL) return false;

		data = data->openSection("material");

		if (data == NULL) return false;

		instantiateMFM( data );
	}

	DataSectionPtr mfmData;
	mfmData = BWResource::openSection( mfmFile, true );
	if (mfmData == NULL) return false;
	mfmData->copy( data );
	mfmData->delChild("identifier");
	return mfmData->save();

	//Even though this method could have dirtied the model (through the instantiation of the mfm)
	//we do not mark it as dirty since the user did not expressly request this change.
	//If any further changes are made then this will be saved.

}

void Mutant::deleteTint( const std::string& matterName, const std::string& tintName )
{
	if (dyes_.find( matterName ) == dyes_.end())
		return;
	
	DataSectionPtr data = dyes_[matterName];

	UndoRedo::instance().add( new UndoRedoOp( 0, currModel_, currModel_ ));
	UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/DELETING_TINT"), true );
	
	if (tints_.find( matterName ) == tints_.end())
		return;

	if (tints_[matterName].find( tintName ) == tints_[matterName].end())
		return;
	
	data->delChild( tints_[matterName][tintName].data );

	if (data->findChild( "tint" ) == NULL) // If there are no more tints...
	{
		currModel_->delChild( data ); // Remove the dye
	}

	currDyes_.erase( matterName );

	reloadAllLists();

}

bool Mutant::ensureShaderCorrect( const std::string& fxFile, const std::string& format )
{
	if (fxFile == "") return true; // No shader can apply to any format

	bool softskinned = (format == "xyznuviiiww") || (format == "xyznuviiiwwtb");
	bool hardskinned = (format == "xyznuvi") || (format == "xyznuvitb");
	
	if ( fxFile.find("hardskinned") != -1 )
	{
		if (softskinned)
		{
			ERROR_MSG("Unable to apply a hardskinned shader to a softskinned object.\n");
			return false;
		}
	}
	else if ( fxFile.find("skinned") != -1 )
	{
		if (hardskinned)
		{
			ERROR_MSG("Unable to apply a softskinned shader to a hardskinned object.\n");
			return false;
		}
		else if (!softskinned)
		{
			ERROR_MSG("Unable to apply a softskinned shader to an unskinned object.\n");
			return false;
		}
	}
	else
	{
		if (softskinned)
		{
			WARNING_MSG("Applying an unskinned shader to a softskinned object.\n");
			if (::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
				L("MODELEDITOR/MODELS/MUTANT_MATERIALS/SOFTSKINNED_WARNING_MSG"),
				L("MODELEDITOR/MODELS/MUTANT_MATERIALS/SOFTSKINNED_WARNING"), MB_OKCANCEL ) == IDCANCEL)
				{
					return false;
				}
		}
		else if (hardskinned)
		{
			WARNING_MSG("Applying an unskinned shader to a hardskinned object.\n");
			if (::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
				L("MODELEDITOR/MODELS/MUTANT_MATERIALS/HARDSKINNED_WARNING_MSG"),
				L("MODELEDITOR/MODELS/MUTANT_MATERIALS/HARDSKINNED_WARNING"), MB_OKCANCEL ) == IDCANCEL)
			{
				return false;
			}
		}
	}
	return true;
}

bool Mutant::effectHasNormalMap( const std::string& effectFile )
{
	if (effectFile == "") return false;
		
	static std::map< std::string, bool > s_binormal;

	if (s_binormal.find( effectFile ) != s_binormal.end())
	{
		return s_binormal[ effectFile ];
	}

	Moo::EffectMaterialPtr material = new Moo::EffectMaterial();

	if (!material) return false;
	if (!material->initFromEffect( effectFile )) return false;

	s_binormal[ effectFile ] = false;

	if ( material->pEffect() )
	{
		ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
		if ( !pEffect )
			return false;

		Moo::EffectMaterial::Properties& properties = material->properties();
		Moo::EffectMaterial::Properties::reverse_iterator it = properties.rbegin();
		Moo::EffectMaterial::Properties::reverse_iterator end = properties.rend();

		for (; it != end; ++it )
		{
			MF_ASSERT( it->second );
			D3DXHANDLE hParameter = it->first;
			Moo::EffectPropertyPtr& pProperty = it->second;             
			D3DXPARAMETER_DESC desc;
			HRESULT hr = pEffect->GetParameterDesc( hParameter, &desc );
			if ( SUCCEEDED(hr) )
			{
				if ( desc.Class == D3DXPC_OBJECT && ( desc.Type == D3DXPT_TEXTURE ||
				desc.Type == D3DXPT_TEXTURE1D || desc.Type == D3DXPT_TEXTURE2D ||
				desc.Type == D3DXPT_TEXTURE3D || desc.Type == D3DXPT_TEXTURECUBE ) )
				{
					std::string UIWidget = MaterialUtility::UIWidget( pEffect.pComObject(), hParameter );
					if ((desc.Name == std::string("normalMap")) || (UIWidget == "NormalMap"))
					{
						s_binormal[ effectFile ] = true;
						break;
					}
				}
			}
		}
	}

	return s_binormal[ effectFile ];
}

bool Mutant::doAnyEffectsHaveNormalMap()
{
	bool isNormalMap = false;

	std::map< std::string, MaterialInfo >::iterator it = materials_.begin();
	std::map< std::string, MaterialInfo >::iterator end = materials_.end();

	for (; it != end; ++it)
	{
		std::string materialName = it->first;
		DataSectionPtr data;

		for (unsigned i=0; i < it->second.data.size(); i++ )
		{
			data = it->second.data[i];

			std::vector<std::string> fxs;
			data->readStrings( "fx", fxs );

			for ( uint32 j = 0; j < fxs.size(); j++ )
			{
				isNormalMap |= effectHasNormalMap( fxs[j] );
			}
		}
	}
	return isNormalMap;
}


/**
 *	This method checks if a FX file is a sky box shader (doing the xyww
 *	transform) by checking the bool "isBWSkyBox" in the shader.
 */
bool Mutant::effectIsSkybox( const std::string & effectFile ) const
{
	if (effectFile.empty())
	{
		return false;
	}
		
	Moo::EffectMaterialPtr material = new Moo::EffectMaterial();

	if (!material->initFromEffect( effectFile ))
	{
		return false;
	}

	if (material->pEffect())
	{
		ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( material );
		if (!pEffect)
		{
			return false;
		}

		D3DXHANDLE hParameter = pEffect->GetParameterByName( 0, "isBWSkyBox" );

		if (hParameter)
		{
			D3DXPARAMETER_DESC desc;
			if (SUCCEEDED( pEffect->GetParameterDesc( hParameter, &desc ) ))
			{
				if (desc.Class == D3DXPC_SCALAR && desc.Type == D3DXPT_BOOL)
				{
					BOOL isSkyBox = FALSE;
					if (SUCCEEDED( pEffect->GetBool( hParameter, &isSkyBox )))
					{
						if (isSkyBox)
						{
							return true;
						}
					}

				}
			}
		}
	}

	return false;
}


/**
 *	This method checks all materials in the model and sets up internal flags
 *	and/or states where needed.
 */
void Mutant::checkMaterials()
{
	isSkyBox_ = false;

	std::map< std::string, MaterialInfo >::iterator it = materials_.begin();
	std::map< std::string, MaterialInfo >::iterator end = materials_.end();

	for (; it != end; ++it)
	{
		std::string materialName = it->first;
		DataSectionPtr data;

		for (uint32 i = 0; i < it->second.data.size(); ++i)
		{
			data = it->second.data[i];

			std::vector< std::string > fxs;
			data->readStrings( "fx", fxs );

			for (uint32 j = 0; j < fxs.size(); ++j)
			{
				if (effectIsSkybox( fxs[j] ))
				{
					isSkyBox_ = true;
					return;
				}
			}
		}
	}
}


bool Mutant::materialShader( const std::string& materialName, const std::string& matterName, const std::string& tintName, const std::string& fxFile, bool undoable /* = true */ )
{
	if ((matterName == "") || (tintName == "Default"))
	{
		if (materials_.find( materialName ) == materials_.end())
			return false;

		if ( ! ensureShaderCorrect( fxFile, materials_[materialName].format ))
			return false;

		DataSectionPtr data;
		
		for (unsigned i=0; i<materials_[materialName].data.size(); i++ )
		{
			data = materials_[materialName].data[i];

			if (undoable)
			{
				UndoRedo::instance().add( new UndoRedoOp( 0, data, currVisual_ ));
			}

			instantiateMFM( data );

			// Delete all fx entries initially
			std::vector<DataSectionPtr> fxs;
			data->openSections( "fx", fxs );
			for ( unsigned i=0; i<fxs.size(); i++ ) 
			{
				data->delChild( fxs[i] );
			}

			if (fxFile != "")
			{
				data->writeString( "fx", fxFile );
			}
			else
			{
				data->delChild( "fx" );
			}
		}

		//Special case if we don't have a tint, we have to reload the material by hand
		if (tintName == "")
		{
			std::set< Moo::EffectMaterialPtr >::iterator matIt = materials_[materialName].effect.begin();
			std::set< Moo::EffectMaterialPtr >::iterator matEnd = materials_[materialName].effect.end();

			while (matIt != matEnd)
			{
				Moo::EffectMaterialPtr effect = *matIt++;
				effect->load( data );
			}
		}
	}
	else
	{
		if (tints_.find( matterName ) == tints_.end())
			return false;
		
		if (tints_[matterName].find( tintName ) == tints_[matterName].end())
			return false;

		if ( ! ensureShaderCorrect( fxFile, tints_[matterName][tintName].format ))
			return false;
		
		DataSectionPtr data = tints_[matterName][tintName].data;

		if (data == NULL) return false;

		data = data->openSection("material");

		if (undoable)
		{
			UndoRedo::instance().add( new UndoRedoOp( 0, currModel_, currModel_ ));
		}

		instantiateMFM( data );

		// Delete all fx entries initially
		std::vector<DataSectionPtr> fxs;
		data->openSections( "fx", fxs );
		for ( unsigned i=0; i<fxs.size(); i++ ) 
		{
			data->delChild( fxs[i] );
		}

		if (fxFile != "")
		{
			data->writeString( "fx", fxFile );
		}
		else
		{
			data->delChild( "fx" );
		}
	}

	if (undoable)
	{
		UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/CHANGING_MATERIAL_EFFECT"), true );
	}

	reloadAllLists();

	return true;
}

std::string Mutant::materialShader( const std::string& materialName, const std::string& matterName, const std::string& tintName )
{
	DataSectionPtr data;
	
	if ((matterName == "") || (tintName == "Default"))
	{
		if (materials_.find( materialName ) == materials_.end())
			return "";

		data = materials_[materialName].data[0];
	}
	else
	{
		if (tints_.find( matterName ) == tints_.end())
			return "";
		
		if (tints_[matterName].find( tintName ) == tints_[matterName].end())
			return "";

		data = tints_[matterName][tintName].data;

		if (data == NULL) return "";

		data = data->openSection("material");
	}

	if (data == NULL) return "";

	instantiateMFM( data );
	
	return data->readString( "fx", "" );
}

bool Mutant::materialMFM( const std::string& materialName, const std::string& matterName, const std::string& tintName, const std::string& mfmFile, std::string* fxFile )
{
	DataSectionPtr mfmData = BWResource::openSection( mfmFile, false );
	
	if (mfmData == NULL) return false;

	std::string mfmFx = mfmData->readString( "fx", "" );

	if (mfmFx == "") return false;

	if ((matterName == "") || (tintName == "Default"))
	{
		if (materials_.find( materialName ) == materials_.end())
			return false;

		if ( ! ensureShaderCorrect( mfmFx, materials_[materialName].format ))
			return false;
	
		DataSectionPtr data;
		
		for (unsigned i=0; i<materials_[materialName].data.size(); i++ )
		{
			data = materials_[materialName].data[i];

			UndoRedo::instance().add( new UndoRedoOp( 0, data, currVisual_ ));

			overloadMFM( data, mfmData );
		}

		//Hmm... Special case if we don't have a tint, we have to reload the material by hand
		if (tintName == "")
		{
			std::set< Moo::EffectMaterialPtr >::iterator matIt = materials_[materialName].effect.begin();
			std::set< Moo::EffectMaterialPtr >::iterator matEnd = materials_[materialName].effect.end();

			while (matIt != matEnd)
			{
				Moo::EffectMaterialPtr effect = *matIt++;
				effect->load( data );
			}
		}
	}
	else
	{
		if (tints_.find( matterName ) == tints_.end())
			return false;
		
		if (tints_[matterName].find( tintName ) == tints_[matterName].end())
			return false;

		if ( ! ensureShaderCorrect( mfmFx, tints_[matterName][tintName].format ))
			return false;
		
		DataSectionPtr data = tints_[matterName][tintName].data;

		if (data == NULL) return false;

		data = data->openSection("material");

		UndoRedo::instance().add( new UndoRedoOp( 0, data, currModel_ ));

		overloadMFM( data, mfmData );
	}

	UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/CHANGING_MATERIAL_MFM"), true );

	reloadAllLists();

	if (fxFile)
		*fxFile = mfmFx;

	return true;
}

void Mutant::tintFlag( const std::string& matterName, const std::string& tintName, const std::string& flagName, uint32 val )
{
	if (tints_.find( matterName ) == tints_.end())
		return;

	if (tints_[matterName].find( tintName ) == tints_[matterName].end())
		return;
	
	DataSectionPtr data = tints_[matterName][tintName].data->openSection("material");

	UndoRedo::instance().add( new UndoRedoOp( 0, currModel_, currModel_ ));

	instantiateMFM( data );
	data->writeInt( flagName, val );

	UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/CHANGING_TINT_FLAG"), true );

	reloadModel();
	reloadBSP();
	triggerUpdate("Materials");
	triggerUpdate("Object");
}

uint32 Mutant::tintFlag( const std::string& matterName, const std::string& tintName, const std::string& flagName )
{
	if (tints_.find( matterName ) == tints_.end())
		return 0xB0B15BAD;
	
	if (tints_[matterName].find( tintName ) == tints_[matterName].end())
		return 0xB0B15BAD;

	DataSectionPtr data = tints_[matterName][tintName].data;

	if (data == NULL) return 0xB0B15BAD;

	uint32 flag = data->readInt( "material/"+flagName, 0xB0B15BAD );

	if ( flag != 0xB0B15BAD ) return flag;

	//Now handle the case where the material is using an MFM which hasn't been instanciated (legacy)

	std::string mfmName = data->readString( "material/mfm", "" );

	data = BWResource::openSection( mfmName, false );

	if (data == NULL) return 0xB0B15BAD;

	return data->readInt( flagName, 0xB0B15BAD );
}

void Mutant::materialFlag( const std::string& materialName, const std::string& flagName, uint32 val )
{
	if (materials_.find( materialName ) == materials_.end())
		return;

	for (unsigned i=0; i<materials_[materialName].data.size(); i++ )
	{
		DataSectionPtr data = materials_[materialName].data[i];

		UndoRedo::instance().add( new UndoRedoOp( 0, data, currVisual_, true ));

		instantiateMFM( data );
		data->writeInt( flagName, val );
	}

	UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/CHANGING_MATERIAL_FLAG"), true );

	reloadModel();
	reloadBSP();
	triggerUpdate("Materials");
	triggerUpdate("Object");
}

uint32 Mutant::materialFlag( const std::string& materialName, const std::string& flagName )
{
	if (materials_.find( materialName ) == materials_.end())
		return 0;

	DataSectionPtr data = materials_[materialName].data[0];
		
	if (data->findChild( flagName ))
		return data->readInt( flagName, 0 );

	//Now handle the case where the material is using an MFM which hasn't been instanciated (legacy)

	std::string mfmName = data->readString( "mfm", "" );

	if (mfmName == "") return 0;

	data = BWResource::openSection( mfmName, false );

	if (data == NULL) return 0;

	return data->readInt( flagName, 0 );
}

void Mutant::tintNames( const std::string& matterName, std::vector< std::string >& names )
{
	if (tints_.find(matterName) == tints_.end()) return;
	
	std::map < std::string, TintInfo >::iterator tintIt = tints_[matterName].begin();
	std::map < std::string, TintInfo >::iterator tintEnd = tints_[matterName].end();
	while ( tintIt != tintEnd )
	{
		names.push_back( (*tintIt++).first );
	}
}

int Mutant::modelMaterial () const
{
	if (currVisual_ == NULL) return 0;
	return currVisual_->readInt( "materialKind" , 0 );
}

void Mutant::modelMaterial ( int id )
{
	if (currVisual_ == NULL) return;

	DataSectionPtr data = currVisual_->openSection( "materialKind", true );

	UndoRedo::instance().add( new UndoRedoOp( 0, data, currVisual_ ));
	
	data->setInt( id );

	UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/CHANGING_MATERIAL_KIND"), false );

	triggerUpdate("Object");
}

std::string Mutant::materialTextureFeedName( const std::string& materialName, const std::string& matterName, const std::string& tintName, const std::string& propName )
{
	DataSectionPtr data;
	
	if ((matterName == "") || (tintName == "Default"))
	{
		if (materials_.find( materialName ) == materials_.end())
			return "";

		data = materials_[materialName].data[0];
	}
	else
	{
		if (tints_.find( matterName ) == tints_.end())
			return "";
		
		if (tints_[matterName].find( tintName ) == tints_[matterName].end())
			return "";
		
		data = tints_[matterName][tintName].data;

		if (data == NULL) return "";

		data = data->openSection("material");
	}

	if (data == NULL) return "";

	instantiateMFM( data );

	std::string feedName = "";

	std::vector<DataSectionPtr> pProps;
	data->openSections( "property", pProps );

	std::vector< DataSectionPtr >::iterator propsIt = pProps.begin();
	std::vector< DataSectionPtr >::iterator propsEnd = pProps.end();
	while (propsIt != propsEnd)
	{
		DataSectionPtr prop = *propsIt++;

		if (propName == prop->asString())
		{
			feedName = prop->readString( "TextureFeed", feedName);
		}
	}

	return feedName;
}

std::string Mutant::materialPropertyVal( const std::string& materialName, const std::string& matterName, const std::string& tintName, const std::string& propName, const std::string& dataType )
{
	DataSectionPtr data;
	
	if ((matterName == "") || (tintName == "Default"))
	{
		if (materials_.find( materialName ) == materials_.end())
			return "";

		data = materials_[materialName].data[0];
	}
	else
	{
		if (tints_.find( matterName ) == tints_.end())
			return "";
		
		if (tints_[matterName].find( tintName ) == tints_[matterName].end())
			return "";
		
		data = tints_[matterName][tintName].data;

		if (data == NULL) return "";

		data = data->openSection("material");
	}

	if (data == NULL) return "";

	instantiateMFM( data );

	std::string val = "";

	std::vector<DataSectionPtr> pProps;
	data->openSections( "property", pProps );

	std::vector< DataSectionPtr >::iterator propsIt = pProps.begin();
	std::vector< DataSectionPtr >::iterator propsEnd = pProps.end();
	while (propsIt != propsEnd)
	{
		DataSectionPtr prop = *propsIt++;

		if (propName == prop->asString())
		{
			val = prop->readString( dataType, val);
		}
	}

	return val;
}

void Mutant::changeMaterialFeed( DataSectionPtr data, const std::string& propName, const std::string& feedName )
{
	if (data == NULL) return;
	
	UndoRedo::instance().add( new UndoRedoOp( 0, data ));

	instantiateMFM( data );

	std::vector<DataSectionPtr> pProps;
	data->openSections( "property", pProps );

	std::vector< DataSectionPtr >::iterator propsIt = pProps.begin();
	std::vector< DataSectionPtr >::iterator propsEnd = pProps.end();
	while (propsIt != propsEnd)
	{
		DataSectionPtr prop = *propsIt++;

		if (propName == prop->asString())
		{
			if (prop->readString( "TextureFeed", "" ) != "")
			{
				if ( feedName != "")
				{
					prop->writeString( "TextureFeed", feedName );
				}
				else
				{
					std::string textureName = prop->readString( "TextureFeed/default", "" );
					prop->delChild( "TextureFeed" );
					prop->writeString( "Texture", textureName );
				}
			}
			else
			{
				std::string textureName = prop->readString( "Texture", "" );
				prop->delChild( "Texture" );
				prop->writeString( "TextureFeed", feedName );
				data = prop->openSection( "TextureFeed" );
				data->writeString( "default", textureName );
			}
		}
	}
}

void Mutant::materialTextureFeedName( const std::string& materialName, const std::string& matterName, const std::string& tintName, const std::string& propName, const std::string& feedName )
{
	DataSectionPtr data;
	
	if ((matterName == "") || (tintName == "Default"))
	{
		if (materials_.find( materialName ) == materials_.end())
			return;
		
		for (unsigned i=0; i<materials_[materialName].data.size(); i++ )
		{
			data = materials_[materialName].data[i];

			changeMaterialFeed( data, propName, feedName );
		}

		dirty( currVisual_ );
	}
	else
	{
		if (tints_.find( matterName ) == tints_.end())
			return;
		
		if (tints_[matterName].find( tintName ) == tints_[matterName].end())
			return;
		
		data = tints_[matterName][tintName].data;

		if (data == NULL) return;

		data = data->openSection("material");

		changeMaterialFeed( data, propName, feedName );

		dirty( currModel_ );
	}

	UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/CHANGING_TEXTURE_FEED_NAME"), false );

	reloadModel();
	triggerUpdate("Materials");
}

std::string Mutant::exposedToScriptName( const std::string& matterName, const std::string& tintName, const std::string& propName )
{
	if ((matterName == "") || (tintName == "Default"))
		return "";
		
	if (tints_.find( matterName ) == tints_.end())
		return "";
	
	if (tints_[matterName].find( tintName ) == tints_[matterName].end())
		return "";
	
	DataSectionPtr data = tints_[matterName][tintName].data;

	if (data == NULL) return "";
	
	std::vector<DataSectionPtr> pProps;
	data->openSections( "property", pProps );

	std::vector< DataSectionPtr >::iterator propsIt = pProps.begin();
	std::vector< DataSectionPtr >::iterator propsEnd = pProps.end();
	while (propsIt != propsEnd)
	{
		DataSectionPtr prop = *propsIt++;

		if (propName == prop->readString( "name", "" ))
		{
			return propName;
		}
	}

	return "";
}

void Mutant::toggleExposed( const std::string& matterName, const std::string& tintName, const std::string& propName )
{
	if ((matterName == "") || (tintName == "Default"))
		return;
	
	if (tints_.find( matterName ) == tints_.end())
		return;
	
	if (tints_[matterName].find( tintName ) == tints_[matterName].end())
		return;
	
	DataSectionPtr data = tints_[matterName][tintName].data;

	if (data == NULL) return;

	UndoRedo::instance().add( new UndoRedoOp( 0, data, currModel_ ));

	DataSectionPtr materialData = data->openSection("material");

	if (materialData == NULL) return;
	
	instantiateMFM( materialData );

	std::vector<DataSectionPtr> pProps;
	data->openSections( "property", pProps );
	std::vector< DataSectionPtr >::iterator propsIt = pProps.begin();
	std::vector< DataSectionPtr >::iterator propsEnd = pProps.end();
	while (propsIt != propsEnd)
	{
		DataSectionPtr prop = *propsIt++;

		if (propName == prop->readString( "name", "" ))
		{
			//Remove the "property" section
			data->delChild( prop );

			UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/DISABLING_PYTHON"), false );

			return;
		}
	}

	//Add a new "property" section
	data = data->newSection( "property" );
	data->writeString( "name", propName );
	data->writeVector4( "default", getExposedVector4( matterName, tintName, propName, "", "" ) );

	UndoRedo::instance().barrier( L("MODELEDITOR/MODELS/MUTANT_MATERIALS/ENABLING_PYTHON"), false );
}

Vector4 Mutant::getExposedVector4( const std::string& matterName, const std::string& tintName, const std::string& descName,  const std::string& propType, const std::string& val )
{
	// if propType and val are empty then that means we want the default value
	Vector4 exposedVector4( 0.f, 0.f, 0.f, 0.f );

	if ((matterName == "") || (tintName == "Default"))
		return exposedVector4;
	
	if (tints_.find( matterName ) == tints_.end())
		return exposedVector4;
	
	if (tints_[matterName].find( tintName ) == tints_[matterName].end())
		return exposedVector4;

	if ( propType.empty() && val.empty() )
	{
		Moo::EffectMaterialPtr effect = tints_[matterName][tintName].effect;
		if ( effect->pEffect() )
		{
			ComObjectWrap<ID3DXEffect> pEffect = MaterialUtility::effect( effect );
			if ( !pEffect )
				return exposedVector4;

			D3DXHANDLE hParameter = pEffect->GetParameterByName( 0, descName.c_str() );
			if ( MaterialUtility::artistEditable( &*pEffect, hParameter ) )
			{
				D3DXPARAMETER_DESC desc;
				HRESULT hr = pEffect->GetParameterDesc( hParameter, &desc );
				if ( SUCCEEDED(hr) )
				{
					if ( desc.Class == D3DXPC_SCALAR && desc.Type == D3DXPT_BOOL )
					{
						BOOL b;
						if ( SUCCEEDED( pEffect->GetBool( hParameter, &b )) )
						{
							if ( b == TRUE )
								exposedVector4[0] = 1.f;
						}
					}
					else if ( desc.Class == D3DXPC_SCALAR && desc.Type == D3DXPT_INT )
					{
						int i;
						if ( SUCCEEDED(pEffect->GetInt( hParameter, &i )) )
							exposedVector4[0] = (float)i;
					}
					else if ( desc.Class == D3DXPC_SCALAR && desc.Type == D3DXPT_FLOAT )
					{
						float f;
						if ( SUCCEEDED(pEffect->GetFloat( hParameter, &f )) )
							exposedVector4[0] = f;
					}
					else if ( desc.Class == D3DXPC_VECTOR && desc.Type == D3DXPT_FLOAT )
					{
						Vector4 v;
						if ( SUCCEEDED(pEffect->GetVector( hParameter, &v )) )
							exposedVector4 = v;
					
					}
				}
			}
		}
	}
	else
	{
		DataSectionPtr tempSection = new XMLSection("temp");
		tempSection->writeString( propType, val );
		if ( propType == "Bool" )
			exposedVector4[0] = tempSection->readBool( propType, false ) ? 1.f : 0.f;
		else if ( propType == "Int" )
			exposedVector4[0] = (float)tempSection->readInt( propType, 0 );
		else if ( propType == "Float" )
			exposedVector4[0] = tempSection->readFloat( propType, 0.f );
		else
			exposedVector4 = tempSection->readVector4( propType, exposedVector4 );
	}
	return exposedVector4;
}

uint32 Mutant::materialSectionTextureMemUsage( DataSectionPtr data, std::set< std::string >& texturesDone )
{
	uint32 size = 0;

	std::string name;

	instantiateMFM( data );

	std::vector< DataSectionPtr > props;

	data->openSections( "property", props );

	for (unsigned i=0; i<props.size(); i++)
	{
		name = props[i]->readString( "Texture", "" );
		if ( name != "" )
		{
			if ( texturesDone.find( name ) != texturesDone.end() )
				continue;
			texturesDone.insert( name );
			Moo::BaseTexturePtr baseTexture = Moo::TextureManager::instance()->get( name, true, false, false );
			if (baseTexture)
			{
				size += baseTexture->textureMemoryUsed();
			}
		}
		else
		{
			DataSectionPtr textureFeed = props[i]->openSection( "TextureFeed" );

			if (textureFeed != NULL) 
			{
				name = textureFeed->readString( "default", "" );
				if (( name == "" ) || ( texturesDone.find( name ) != texturesDone.end() ))
					continue;
				texturesDone.insert( name );
				Moo::BaseTexturePtr baseTexture = Moo::TextureManager::instance()->get( name, true, false, false );
				if (baseTexture)
				{
					size += baseTexture->textureMemoryUsed();
				}
			}
		}
	}

	return size;
}

uint32 Mutant::recalcTextureMemUsage()
{
	texMem_ = 0;
	DataSectionPtr data;
	std::set< std::string > texturesDone;

	std::map< std::string, MaterialInfo >::iterator matIt = materials_.begin();
	std::map< std::string, MaterialInfo >::iterator matEnd = materials_.end();

	for ( ;matIt != matEnd; ++matIt )
	{
		data = matIt->second.data[0];

		if (data == NULL) continue;

		texMem_ += materialSectionTextureMemUsage( data, texturesDone );
	}

	TintMap::iterator dyeIt = tints_.begin();
	TintMap::iterator dyeEnd = tints_.end();

	for (; dyeIt != dyeEnd; ++dyeIt)
	{
		std::map< std::string, TintInfo >::iterator tintIt = dyeIt->second.begin();
		std::map< std::string, TintInfo >::iterator tintEnd = dyeIt->second.end();
		for (; tintIt != tintEnd; ++tintIt)
		{
			data = tintIt->second.data;

			if (data == NULL) continue;

			data = data->openSection( "material" );

			if (data == NULL) continue;

			texMem_ += materialSectionTextureMemUsage( data, texturesDone );
		}
	}

	texMemDirty_ = true;

	return texMem_;
}


Moo::EffectMaterialPtr Mutant::getEffectForTint(
	const std::string& matterName, const std::string& tintName, const std::string& materialName )
{
	if ((matterName == "") || (tintName == ""))
	{
		if ( materialName != "" )
		{
			return *materials_[materialName].effect.begin();
		}
		else
			return NULL;
	}
	
	TintMap::iterator matterIt;
	matterIt = tints_.find( matterName );
	if ( matterIt == tints_.end() )
		return NULL;

	std::map < std::string, TintInfo >::iterator tintIt;
	tintIt = tints_[matterName].find( tintName );
	if ( tintIt == tints_[matterName].end() )
		return NULL;
	
	return tintIt->second.effect;
}
