/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "Python.h"		// See http://docs.python.org/api/includes.html

#include "db_entitydefs.hpp"

#include "entitydef/entity_description_debug.hpp"
#include "entitydef/constants.hpp"
#include "cstdmf/md5.hpp"

// -----------------------------------------------------------------------------
// Section: Constants
// -----------------------------------------------------------------------------
static const std::string EMPTY_STR;

// -----------------------------------------------------------------------------
// Section: EntityDefs
// -----------------------------------------------------------------------------
/**
 * 	This function initialises EntityDefs. Must be called once and only once for
 * 	each instance of EntityDefs.
 *
 * 	@param	pEntitiesSection	The entities.xml data section.
 * 	@param	defaultTypeName		The default entity type.
 * 	@param	defaultNameProperty		The default name property.
 * 	@return	True if successful.
 */
bool EntityDefs::init( DataSectionPtr pEntitiesSection,
	const std::string& defaultTypeName, const std::string& defaultNameProperty )
{
	struct LocalHelperFunctions
	{
		// Sets output to dataDesc.name() only if it is a STRING or BLOB.
		void setNameProperty( std::string& output,
				const DataDescription& dataDesc,
				const EntityDescription& entityDesc )
		{
			const DataType* pDataType = dataDesc.dataType();
			if ((strcmp( pDataType->pMetaDataType()->name(), "BLOB" ) == 0) ||
				(strcmp( pDataType->pMetaDataType()->name(), "STRING" ) == 0))
			{
				output = dataDesc.name();
			}
			else
			{
				ERROR_MSG( "EntityDefs::init: Identifier must be of "
						"STRING or BLOB type. Identifier '%s' "
						"for entity type '%s' is ignored\n",
						dataDesc.name().c_str(),
						entityDesc.name().c_str() );
			}
		}
	} helper;

	MF_ASSERT( (entityDescriptionMap_.size() == 0) && pEntitiesSection );
	if (!entityDescriptionMap_.parse( pEntitiesSection ))
	{
		ERROR_MSG( "EntityDefs::init: Could not parse '%s'\n",
			EntityDef::Constants::entitiesFile() );
		return false;
	}

	defaultNameProperty_ = defaultNameProperty;

	// Set up some entity def stuff specific to DbMgr
	nameProperties_.resize( entityDescriptionMap_.size() );
	entityPasswordBits_.resize( entityDescriptionMap_.size() );
	for ( EntityTypeID i = 0; i < entityDescriptionMap_.size(); ++i )
	{
		const EntityDescription& entityDesc =
				entityDescriptionMap_.entityDescription(i);
		// Remember whether it has a password property.
		entityPasswordBits_[i] = (entityDesc.findProperty( "password" ) != 0);

		// Find its name property.
		const DataDescription* 	pDefaultNameDataDesc = NULL;
		std::string&			nameProperty = nameProperties_[i];
		for ( unsigned int i = 0; i < entityDesc.propertyCount(); ++i)
		{
			const DataDescription* pDataDesc = entityDesc.property( i );
			if (pDataDesc->isIdentifier())
			{
				if (nameProperty.empty())
				{
					helper.setNameProperty( nameProperty, *pDataDesc,
							entityDesc );
				}
				else // We don't support having multiple to name columns.
				{
					ERROR_MSG( "EntityDefs::init: Multiple identifiers for "
							"one entity type not supported. Identifier '%s' "
							"for entity type '%s' is ignored\n",
							pDataDesc->name().c_str(),
							entityDesc.name().c_str() );
				}
			}
			else if ( pDataDesc->name() == defaultNameProperty )
			{	// For backward compatiblity, we use the default name property
				// if none of the properties of the entity is an identifier.
				pDefaultNameDataDesc = pDataDesc;
			}
		}
		// Backward compatibility.
		if (nameProperty.empty() && pDefaultNameDataDesc)
		{
			helper.setNameProperty( nameProperty, *pDefaultNameDataDesc,
					entityDesc );
		}
	}

	entityDescriptionMap_.nameToIndex( defaultTypeName, defaultTypeID_ );

	MD5 md5;
	entityDescriptionMap_.addToMD5( md5 );
	md5.getDigest( md5Digest_ );

	MD5 persistentPropertiesMD5;
	entityDescriptionMap_.addPersistentPropertiesToMD5(
			persistentPropertiesMD5 );
	persistentPropertiesMD5.getDigest( persistentPropertiesMD5Digest_ );

	return true;
}

const std::string& EntityDefs::getDefaultTypeName() const
{
	return (this->isValidEntityType( this->getDefaultType() )) ?
				this->getEntityDescription( this->getDefaultType() ).name() :
				EMPTY_STR;
}

/**
 *	This function returns the type name of the given property.
 */
std::string EntityDefs::getPropertyType( EntityTypeID typeID,
	const std::string& propName ) const
{
	const EntityDescription& entityDesc = this->getEntityDescription(typeID);
	DataDescription* pDataDesc = entityDesc.findProperty( propName );
	return ( pDataDesc ) ? pDataDesc->dataType()->typeName() : std::string();
}

/**
 *	This function prints out information about the entity defs.
 */
void EntityDefs::debugDump( int detailLevel ) const
{
	EntityDescriptionDebug::dump( entityDescriptionMap_, detailLevel );
}
