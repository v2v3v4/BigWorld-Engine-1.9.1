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

#include "tint.hpp"

#include "moo/material.hpp"
#include "moo/effect_material.hpp"

#include "model.hpp"


DECLARE_DEBUG_COMPONENT2( "Model", 0 )


/**
 *	The constructor for the tint
 */
Tint::Tint( const std::string & name, bool defaultTint /*= false*/ ) :
	name_( name ),
	effectMaterial_( NULL ),
	default_( defaultTint )
{
}


/**
 *	The destructor for the tint
 */
Tint::~Tint()
{
}


/**
 *	Tint the material with the current settings of our properties
 */
void Tint::applyTint()
{
	if (properties_.empty())
		return;

	SimpleMutexHolder smh( Model::propCatalogueLock() );

	for (uint i = 0; i < properties_.size(); i++)
	{
		DyeProperty & dp = properties_[i];

		Vector4 & value =
			(Model::propCatalogueRaw().begin() + dp.index_)->second;

		if (dp.mask_ == -1)
		{
			// look for the property in this EffectMaterial...
			if ( effectMaterial_->pEffect() )
			{
				Moo::EffectMaterial::Properties & eProps = effectMaterial_->properties();
				Moo::EffectMaterial::Properties::iterator found =
					eProps.find( (D3DXHANDLE)dp.controls_ );
				// it might not always be there, as this property might
				// be defined in a parent model that uses a different Effect
				if (found != eProps.end())
				{
					// kind of sucks that we're looking it up all the time here too!
					// but this will do for the first version of it anyhow
					found->second->be( value );
					continue;
				}
			}
		}

		// now apply to the field of material_ indicated by
		// dp.controls_ and dp.mask_. Currently assume it's
		// the whole of the texture factor
		
		// TODO: Remove DyeProperty::controls_
		switch( dp.controls_ )
		{
		case DyePropSetting::PROP_TEXTURE_FACTOR:
			//if (oldMaterial_)
			//{
			//	oldMaterial_->textureFactor( Moo::Colour( value ) );
			//}
			break;
		case DyePropSetting::PROP_UV:
			//if (oldMaterial_)
			//{
			//	oldMaterial_->uvTransform( value );
			//}
			break;
		}
	}
}



// model_tint.cpp
