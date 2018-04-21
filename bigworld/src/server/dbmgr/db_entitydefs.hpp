/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef DATABASE_ENTITYDEF_HPP
#define DATABASE_ENTITYDEF_HPP

#include "entitydef/entity_description_map.hpp"
#include "cstdmf/md5.hpp"

const EntityTypeID INVALID_TYPEID = std::numeric_limits<uint16>::max();

/**
 *	This class represents the entity definitions in DbMgr.
 */
class EntityDefs
{
	EntityDescriptionMap	entityDescriptionMap_;
	std::vector<bool>		entityPasswordBits_;
	MD5::Digest				md5Digest_;
	MD5::Digest				persistentPropertiesMD5Digest_;
	EntityTypeID			defaultTypeID_;
	std::vector< std::string >	nameProperties_;
	std::string				defaultNameProperty_;

public:
	EntityDefs() :
		entityDescriptionMap_(), entityPasswordBits_(), md5Digest_(),
		persistentPropertiesMD5Digest_(),
		defaultTypeID_( INVALID_ENTITY_TYPE_ID ), nameProperties_(),
		defaultNameProperty_()
	{}

	bool init( DataSectionPtr pEntitiesSection,
		const std::string& defaultTypeName,
		const std::string& defaultNameProperty );

	const MD5::Digest& getDigest() const 		{	return md5Digest_;	}
	const MD5::Digest& getPersistentPropertiesDigest() const
	{	return persistentPropertiesMD5Digest_;	}
	const std::string& getNameProperty( EntityTypeID index ) const
	{	return nameProperties_[ index ];	}
	const std::string& getDefaultNameProperty() const
	{ 	return defaultNameProperty_;	}
	const EntityTypeID getDefaultType() const	{	return defaultTypeID_; 	}
	const std::string& getDefaultTypeName() const;

	bool entityTypeHasPassword( EntityTypeID typeID ) const
	{
		return entityPasswordBits_[typeID];
	}
	bool isValidEntityType( EntityTypeID typeID ) const
	{
		return (typeID < entityDescriptionMap_.size()) &&
				!entityDescriptionMap_.entityDescription( typeID ).
					isClientOnlyType();
	}
	EntityTypeID getEntityType( const std::string& typeName ) const
	{
		EntityTypeID typeID = INVALID_TYPEID;
		entityDescriptionMap_.nameToIndex( typeName, typeID );
		return typeID;
	}
	size_t getNumEntityTypes() const
	{
		return entityDescriptionMap_.size();
	}
	const EntityDescription& getEntityDescription( EntityTypeID typeID ) const
	{
		return entityDescriptionMap_.entityDescription( typeID );
	}
	//	This function returns the type name of the given property.
	std::string getPropertyType( EntityTypeID typeID,
		const std::string& propName ) const;

	void debugDump( int detailLevel ) const;
};

#endif /*DATABASE_ENTITYDEF_HPP*/
