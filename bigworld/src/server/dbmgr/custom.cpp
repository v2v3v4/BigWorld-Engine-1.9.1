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

#include "custom.hpp"
#include "resmgr/xml_section.hpp"
#include "database.hpp"
#include "db_entitydefs.hpp"

/**
 *	This method is used to create a new "unknown" entity. All its properties
 * 	should be default values except its name property which will be set to
 * 	the login name.
 */
DataSectionPtr createNewEntity( EntityTypeID typeID, const std::string& name )
{
	const EntityDefs& entityDefs = Database::instance().getEntityDefs();
	DataSectionPtr pSection = 
			new XMLSection( entityDefs.getEntityDescription( typeID ).name() );
	
	const std::string& nameProp = entityDefs.getNameProperty( typeID );
	if (!nameProp.empty())
	{
		if (entityDefs.getPropertyType( typeID, nameProp ) == "BLOB" )
			pSection->writeBlob( nameProp, name );
		else
			pSection->writeString( nameProp, name );
	}
	return pSection;
}
