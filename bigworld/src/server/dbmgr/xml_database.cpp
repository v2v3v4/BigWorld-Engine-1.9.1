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

#include "xml_database.hpp"

#include "cstdmf/debug.hpp"
#include "entitydef/entity_description_map.hpp"
#include "entitydef/constants.hpp"
#include "resmgr/bwresource.hpp"
#include "database.hpp"
#include "db_entitydefs.hpp"
#include "db_config.hpp"
#include "entity_recoverer.hpp"
#include "pyscript/py_data_section.hpp"
#include "server/bwconfig.hpp"

DECLARE_DEBUG_COMPONENT(0)

static const char* DATABASE_INFO_SECTION = "_BigWorldInfo";
static const char* DATABASE_LOGONMAPPING_SECTION = "LogOnMapping";

// -----------------------------------------------------------------------------
// Section: XMLDatabase
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
XMLDatabase::XMLDatabase() :
	maxID_( 0 ),
	nextID_( 1 ),
	pEntityDefs_( 0 ),
	pNewEntityDefs_( 0 )
{
}


/**
 *	Destructor.
 */
XMLDatabase::~XMLDatabase()
{
	this->shutDown();
}


/*
 *	Override from IDatabase.
 */
bool XMLDatabase::startup( const EntityDefs& entityDefs,
		bool /*isFaultRecovery*/, bool isUpgrade, bool isSyncTablesToDefs )
{
	if (isUpgrade)
	{
		WARNING_MSG( "XMLDatabase::startup: "
						"XML database does not support --upgrade option\n" );
	}

	if (isSyncTablesToDefs)
	{
		WARNING_MSG( "XMLDatabase::startup: "
						"XML database does not support --sync-tables-to-defs "
						"option\n" );
	}

	// Create NameMaps for all entity types
	nameToIdMaps_.resize( entityDefs.getNumEntityTypes() );

	pDB_ = BWResource::openSection( EntityDef::Constants::xmlDatabaseFile() );
	pEntityDefs_ = &entityDefs;

	if (!pDB_)
	{
		ERROR_MSG( "XMLDatabase::startup: Could not open %s: Creating it.\n",
				EntityDef::Constants::xmlDatabaseFile() );
		pDB_ = BWResource::openSection(
				EntityDef::Constants::xmlDatabaseFile(), true );
	}

	DataSectionPtr pInfoSection = pDB_->findChild( DATABASE_INFO_SECTION );
	if (pInfoSection)
	{
		// Read logon mapping info
		pLogonMapSection_ =
				pInfoSection->findChild( DATABASE_LOGONMAPPING_SECTION );
		if (pLogonMapSection_)
		{
			for (DataSection::iterator it = pLogonMapSection_->begin();
					it != pLogonMapSection_->end(); ++it)
			{
				EntityTypeID typeID =
					pEntityDefs_->getEntityType( (*it)->readString( "type" ) );
				if (typeID != INVALID_TYPEID)
				{
					logonMap_[ (*it)->readString( "logOnName" ) ] =
							LogOnMapping( (*it)->readString( "password" ),
										typeID,
										(*it)->readString( "entityName" ) );
				}
				else
				{
					WARNING_MSG( "Database::init: Logon mapping ignored because"
							" '%s' is not a valid entity type\n",
							(*it)->readString( "type" ).c_str() );
				}
			}
		}
		else
		{
			pLogonMapSection_ =
					pInfoSection->newSection( DATABASE_LOGONMAPPING_SECTION );
		}
	}
	else
	{
		pInfoSection = pDB_->newSection( DATABASE_INFO_SECTION );
		pLogonMapSection_ =
				pInfoSection->newSection( DATABASE_LOGONMAPPING_SECTION );
	}

	// We do two loops for backward compatibiliy, where none of the entities
	// had DBIDs stored in the xml file. The first loop reads all the entities
	// with DBIDs and then determines the max DBID. The second loop then
	// assigns DBIDs to the entities with no DBIDs.
	bool hasEntityWithNoDBID = false;
	bool shouldAssignDBIDs = false;
	for ( int i = 0; i < 2; ++i )
	{
		for (DataSection::iterator sectionIter = pDB_->begin();
				sectionIter != pDB_->end(); ++sectionIter)
		{
			DataSectionPtr pCurr = *sectionIter;

			// Check that it is a valid entity type
			EntityTypeID typeID =
					pEntityDefs_->getEntityType( pCurr->sectionName() );
			if (typeID == INVALID_TYPEID)
			{
				// Print warning if it is not our info section and only in
				// the first loop
				if (pCurr.getObject() != pInfoSection.getObject() && (i == 0))
					WARNING_MSG( "Database::init: '%s' is not a valid entity "
							"type - data ignored\n",
							pCurr->sectionName().c_str() );
				continue;
			}

			DatabaseID id = pCurr->read( "databaseID", int64( 0 ) );
			if (id == 0)
			{
				if (shouldAssignDBIDs)
				{
					id = ++maxID_;
					pCurr->write( "databaseID", id );
				}
				else
				{
					hasEntityWithNoDBID = true;
					continue;
				}
			}
			else
			{
				if (shouldAssignDBIDs)
					// Since we have a DBID, we were loaded in the first loop.
					continue;
				else if (id > maxID_)
					maxID_ = id;
			}

			// Check for duplicate DBID
			IDMap::iterator idIter = idToData_.find( id );
			if (idIter != idToData_.end())
			{
				// HACK: Skip -1, -2, -3: ids for bots etc.
				if (id >= 0)
				{
					WARNING_MSG( "Database::init: '%s' and '%s' have same id "
							"(%"FMT_DBID") - second entity ignored\n",
							idIter->second->sectionName().c_str(),
							pCurr->sectionName().c_str(),
							id );
				}
				continue;
			}
			else
			{
				idToData_[id] = pCurr;
			}

			// Find the name of this entity.
			const std::string& nameProperty =
					pEntityDefs_->getNameProperty( typeID );
			if (!nameProperty.empty())
			{
				std::string entityName = pCurr->readString( nameProperty );
				// Check that name is not already taken. This is
				// possible if a different property is chosen as the
				// name property.
				NameMap& nameMap = nameToIdMaps_[ typeID ];
				NameMap::const_iterator it = nameMap.find( entityName );
				if (it == nameMap.end())
					nameMap[entityName] = id;
				else
					WARNING_MSG( "XMLDatabase::startup: Multiple "
							"entities of type '%s' have the same name: '%s' - "
							"second entity will not be retrievable by name\n",
							pCurr->sectionName().c_str(), entityName.c_str() );
			}
		}

		if (hasEntityWithNoDBID)
			shouldAssignDBIDs = true;
		else
			break;	// Don't do second loop.
	}

	// Make sure watcher is initialised by now
	MF_WATCH( "maxID",			maxID_,			Watcher::WT_READ_ONLY );

	// add the DB as an attribute for Python - so executeRawDatabaseCommand()
	// can access the database.
	PyObjectPtr dbRoot( new PyDataSection( pDB_ ),
						PyObjectPtr::STEAL_REFERENCE );
	PyObject* pBigWorldModule = PyImport_AddModule( "BigWorld" );
	PyObject_SetAttrString( pBigWorldModule, "dbRoot", dbRoot.getObject() );
	// Import BigWorld module as user cannot execute "import" using
	// executeRawDatabaseCommand().
	PyObject * pMainModule = PyImport_AddModule( "__main__" );
	if (pMainModule)
	{
		PyObject * pMainModuleDict = PyModule_GetDict( pMainModule );
		if (PyDict_SetItemString( pMainModuleDict, "BigWorld", pBigWorldModule)
				 != 0)
		{
			ERROR_MSG( "XMLDatabase::startup: Can't insert BigWorld module into"
						" __main__ module\n" );
		}
	}
	else
	{
		ERROR_MSG( "XMLDatabase::startup: Can't create Python __main__ "
					"module\n" );
		PyErr_Print();
	}

	return true;
}


/*
 *	Override from IDatabase.
 */
bool XMLDatabase::shutDown()
{
	if (pDB_)
	{
		BWResource::instance().save( EntityDef::Constants::xmlDatabaseFile() );
		pDB_ = (DataSection *)NULL;
	}

	return true;
}


/**
 *	Override from IDatabase.
 */
void XMLDatabase::mapLoginToEntityDBKey(
	const std::string & logOnName, const std::string & password,
	IDatabase::IMapLoginToEntityDBKeyHandler& handler )
{
	LogonMap::const_iterator it = logonMap_.find( logOnName );
	if (it != logonMap_.end())
	{
		if (password == it->second.password)
		{
			handler.onMapLoginToEntityDBKeyComplete(
					DatabaseLoginStatus::LOGGED_ON,
					EntityDBKey( it->second.typeID, 0, it->second.entityName ) );
		}
		else
		{
			handler.onMapLoginToEntityDBKeyComplete(
					DatabaseLoginStatus::LOGIN_REJECTED_INVALID_PASSWORD,
					EntityDBKey( 0, 0 ) );
		}
	}
	else
	{
		handler.onMapLoginToEntityDBKeyComplete(
				DatabaseLoginStatus::LOGIN_REJECTED_NO_SUCH_USER,
				EntityDBKey( 0, 0 ) );
	}
}

/**
 *	Override from IDatabase
 */
void XMLDatabase::setLoginMapping( const std::string & username,
	const std::string & password, const EntityDBKey& ekey,
	ISetLoginMappingHandler& handler )
{
	// ekey must be a full and valid key.
	IDMap::const_iterator iterData = idToData_.find( ekey.dbID );
	MF_ASSERT( iterData != idToData_.end() )
	MF_ASSERT( this->findEntityByName( ekey.typeID, ekey.name ) == ekey.dbID );

	// Try to find existing section.
	DataSectionPtr pSection;
	if ( logonMap_.find( username ) != logonMap_.end() )
	{
		// Using linear search... yuk but this should occur rarely.
		for ( DataSection::iterator it = pLogonMapSection_->begin();
				it != pLogonMapSection_->end(); ++it )
		{
			if ((*it)->readString( "logOnName" ) == username)
			{
				pSection = *it;
				break;
			}
		}
	}

	logonMap_[ username ] = LogOnMapping( password, ekey.typeID, ekey.name );

	const std::string& typeName =
			pEntityDefs_->getEntityDescription( ekey.typeID ).name();
	if (!pSection)
		pSection = pLogonMapSection_->newSection( "item" );
	pSection->writeString( "logOnName", username );
	pSection->writeString( "password", password );
	pSection->writeString( "type", typeName );
	pSection->writeString( "entityName", ekey.name );

	handler.onSetLoginMappingComplete();
}

/**
 *	Override from IDatabase
 */
void XMLDatabase::getEntity( IDatabase::IGetEntityHandler& handler )
{
	const EntityDefs& entityDefs = *pEntityDefs_;

	EntityDBKey&		ekey = handler.key();
	EntityDBRecordOut&	erec = handler.outrec();

	MF_ASSERT( pDB_ );

	bool isOK = true;

	const EntityDescription& desc =
		entityDefs.getEntityDescription( ekey.typeID );

	bool lookupByName = (!ekey.dbID);
	if (lookupByName)
		ekey.dbID = this->findEntityByName( ekey.typeID, ekey.name );

	isOK = (ekey.dbID != 0);
	if (isOK)
	{	// Get entity data
		IDMap::const_iterator iterData = idToData_.find( ekey.dbID );
		if ( iterData != idToData_.end() )
		{
			DataSectionPtr pData = iterData->second;
			if (!lookupByName)
			{	// Set ekey.name
				const std::string& nameProperty =
						entityDefs.getNameProperty( ekey.typeID );
				if (!nameProperty.empty())
					ekey.name = pData->readString( nameProperty );
			}

			if (erec.isStrmProvided())
			{	// Put entity data into stream.
				// See if need to override password field
				const std::string* pPasswordOverride = handler.getPasswordOverride();
                if (pPasswordOverride)
				{
					bool isBlobPasswd = (entityDefs.
						getPropertyType( ekey.typeID, "password" ) == "BLOB");
					DataSectionPtr pPasswordSection = pData->findChild( "password" );
					if (pPasswordSection)
					{
						std::string existingPassword = pPasswordSection->asString();
						if (isBlobPasswd)
							pPasswordSection->setBlob( *pPasswordOverride );
						else
							pPasswordSection->setString( *pPasswordOverride );
						desc.addSectionToStream( pData, erec.getStrm(),
							EntityDescription::BASE_DATA |
							EntityDescription::CELL_DATA |
							EntityDescription::ONLY_PERSISTENT_DATA	);
						pPasswordSection->setString( existingPassword );
					}
					else
					{
						if (isBlobPasswd)
							pData->writeBlob( "password", *pPasswordOverride );
						else
							pData->writeString( "password", *pPasswordOverride );
						desc.addSectionToStream( pData, erec.getStrm(),
							EntityDescription::BASE_DATA |
							EntityDescription::CELL_DATA |
							EntityDescription::ONLY_PERSISTENT_DATA );
						pData->delChild( "password" );
					}
				}
				else
				{
					desc.addSectionToStream( pData, erec.getStrm(),
						EntityDescription::BASE_DATA |
						EntityDescription::CELL_DATA |
						EntityDescription::ONLY_PERSISTENT_DATA	);
				}

				if (desc.hasCellScript())
				{
					Vector3 position = pData->readVector3( "position" );
					Direction3D direction = pData->readVector3( "direction" );
					SpaceID spaceID = pData->readInt( "spaceID" );
					erec.getStrm() << position << direction << spaceID;
				}
			}
		}
		else
		{
			isOK = false;
		}

		if (isOK && erec.isBaseMBProvided() && erec.getBaseMB())
		{
			ActiveSet::iterator iter = activeSet_.find( ekey.dbID );

			if (iter != activeSet_.end())
				erec.setBaseMB(&iter->second.baseRef);
			else
				erec.setBaseMB( 0 );
		}
	}

	handler.onGetEntityComplete( isOK );
}

/**
 *	Override from IDatabase
 */
void XMLDatabase::putEntity( const EntityDBKey& ekey, EntityDBRecordIn& erec,
							 IDatabase::IPutEntityHandler& handler )
{
	MF_ASSERT( pDB_ );

	const EntityDefs& entityDefs = *pEntityDefs_;
	const EntityDescription& desc =
		entityDefs.getEntityDescription( ekey.typeID );
	const std::string& nameProperty = entityDefs.getNameProperty( ekey.typeID );

	bool isOK = true;
	bool definitelyExists = false;
	bool isExisting = (ekey.dbID != 0);
	DatabaseID dbID = ekey.dbID;

	if (erec.isStrmProvided())
	{
		// Find the existing entity (if any)
		DataSectionPtr 	pOldProps;
		std::string		oldName;
		if (isExisting)	// Existing entity
		{
			IDMap::const_iterator iterData = idToData_.find( dbID );
			if (iterData != idToData_.end())
			{
				pOldProps = iterData->second;
				if (!nameProperty.empty())
					oldName = pOldProps->readString( nameProperty );
			}
			else
			{
				isOK = false;
			}
		}
		else
		{
			dbID = ++maxID_;
		}

		// Read stream into new data section
		DataSectionPtr pProps = pDB_->newSection( desc.name() );;
		desc.readStreamToSection( erec.getStrm(),
			EntityDescription::BASE_DATA | EntityDescription::CELL_DATA |
			EntityDescription::ONLY_PERSISTENT_DATA, pProps );

		if (desc.hasCellScript())
		{
			Vector3				position;
			Direction3D			direction;
			SpaceID				spaceID;

			erec.getStrm() >> position >> direction >> spaceID;
			pProps->writeVector3( "position", position );
			pProps->writeVector3( "direction", *(Vector3 *)&direction );
			pProps->writeInt( "spaceID", spaceID );
		}

		// Used in MySQL only
		TimeStamp gameTime;
		erec.getStrm() >> gameTime;

		// Check name if this type has a name property
		if (isOK && !nameProperty.empty())
		{
			NameMap& 	nameMap = nameToIdMaps_[ ekey.typeID ];
			std::string newName = pProps->readString( nameProperty );

			// New entity or existing entity's name has changed.
			if (!isExisting || (oldName != newName))
			{
				// Check that entity name isn't already taken
				NameMap::const_iterator it = nameMap.find( newName );
				if (it == nameMap.end())
				{
					if (isExisting)	// Existing entity's name has changed
					 	nameMap.erase(oldName);
					nameMap[newName] = dbID;
				}
				else	// Name already taken.
				{
					WARNING_MSG( "XMLDatabase::putEntity: '%s' entity named"
						" '%s' already exists\n", desc.name().c_str(),
						newName.c_str() );
					isOK = false;
				}
			}
		}

		if (isOK)
		{
			pProps->write( "databaseID", dbID );

			if (isExisting)
				pDB_->delChild( pOldProps );

			idToData_[dbID] = pProps;

			definitelyExists = true;
		}
		else
		{
			pDB_->delChild( pProps );
		}
	}

	if (isOK && erec.isBaseMBProvided())
	{	// Update base mailbox.
		if (!definitelyExists)
		{
			isOK = (idToData_.find( dbID ) != idToData_.end());
		}

		if (isOK)
		{
			ActiveSet::iterator iter = activeSet_.find( dbID );
			EntityMailBoxRef* pBaseMB = erec.getBaseMB();

			if (pBaseMB)
			{
				if (iter != activeSet_.end())
					iter->second.baseRef = *pBaseMB;
				else
					activeSet_[ dbID ].baseRef = *pBaseMB;
			}
			else
			{	// Set base mailbox to null.
				if (iter != activeSet_.end())
					activeSet_.erase( iter );
			}
		}
	}

	handler.onPutEntityComplete( isOK, dbID );
}

/**
 *	Override from IDatabase
 */
void XMLDatabase::delEntity( const EntityDBKey & ekey,
							 IDatabase::IDelEntityHandler& handler )
{
	DatabaseID dbID = ekey.dbID;
	// look up the id if we don't already know it
	if (dbID == 0)
		dbID = findEntityByName( ekey.typeID, ekey.name );

	bool isOK = (dbID != 0);
	if (isOK)
	{
		// try to delete it
		isOK = this->deleteEntity( dbID, ekey.typeID );

		// Remove from active set
		if (isOK)
		{
			ActiveSet::iterator afound = activeSet_.find( dbID );
			if (afound != activeSet_.end())
				activeSet_.erase( afound );
		}
	}

	handler.onDelEntityComplete(isOK);
}

/*
 *	commit the database to disk.
 */
bool XMLDatabase::commit()
{
	if (pDB_)
	{
		return BWResource::instance().save(
				EntityDef::Constants::xmlDatabaseFile() );
	}

	return false;
}

/**
 *	Private delete method
 */
bool XMLDatabase::deleteEntity( DatabaseID id, EntityTypeID typeID )
{
	MF_ASSERT( pDB_ );

	// find the record
	IDMap::iterator dfound = idToData_.find( id );
	if (dfound == idToData_.end()) return false;
	DataSectionPtr pSect = dfound->second;

	// get rid of the name
	const std::string& nameProperty = pEntityDefs_->getNameProperty( typeID );
	if (!nameProperty.empty())
	{
		std::string name = pSect->readString( nameProperty );
		nameToIdMaps_[ typeID ].erase( name );
	}

	// get rid of the id
	idToData_.erase( dfound );

	// and finally get rid of the data section
	pDB_->delChild( pSect );

	return true;
}


/**
 *	Private find method
 */
DatabaseID XMLDatabase::findEntityByName( EntityTypeID entityTypeID,
		const std::string & name ) const
{
	MF_ASSERT( pDB_ );
	const NameMap& nameMap = nameToIdMaps_[entityTypeID];
	NameMap::const_iterator it = nameMap.find( name );

	return (it != nameMap.end()) ?  it->second : 0;
}

/**
 *	Override from IDatabase
 */
void XMLDatabase::getBaseAppMgrInitData(
		IGetBaseAppMgrInitDataHandler& handler )
{
	// We don't remember game time or have secondary database registration.
	// Always return 0.
	handler.onGetBaseAppMgrInitDataComplete( 0, 0 );
}

/**
 *	Override from IDatabase
 */
void XMLDatabase::executeRawCommand( const std::string & command,
	IExecuteRawCommandHandler& handler )
{
	PyObject * pObj = Script::runString( command.c_str(), false );
	if (pObj == NULL)
	{
		handler.response() << std::string( "Exception occurred" );

		ERROR_MSG( "XMLDatabase::executeRawCommand: encountered exception\n" );
		PyErr_Print();
		handler.onExecuteRawCommandComplete();
		return;
	}

	BinaryOStream& stream = handler.response();
	stream.appendString( "", 0 );	// No error
	stream << int32( 1 );			// 1 column
	stream << int32( 1 );			// 1 row

	PyObject * pString = PyObject_Str( pObj );
	const char * string = PyString_AsString( pString );
	uint sz = PyString_Size( pString );

	stream.appendString( string, sz );

	Py_DECREF( pObj );
	Py_DECREF( pString );
	handler.onExecuteRawCommandComplete();
}


/**
 *	Override from IDatabase.
 */
void XMLDatabase::putIDs( int count, const EntityID * ids )
{
	for (int i=0; i<count; i++)
		spareIDs_.push_back( ids[i] );
}


/**
 *	Override from IDatabase.
 */
void XMLDatabase::getIDs( int count, IGetIDsHandler& handler )
{
	BinaryOStream& strm = handler.idStrm();
	int counted = 0;
	for ( ; (counted < count) && spareIDs_.size(); ++counted )
	{
		strm << spareIDs_.back();
		spareIDs_.pop_back();
	}
	for ( ; counted < count; ++counted )
	{
		strm << nextID_++;
	}

	handler.onGetIDsComplete();
}


/*
 *	Override frrom IDatabase. Archiving of SpaceData is not supported by the
 *	XMLDatabase.
 */
void XMLDatabase::writeSpaceData( BinaryIStream& spaceData )
{
	spaceData.finish();
}

/**
 *	Override from IDatabase.
 */
bool XMLDatabase::getSpacesData( BinaryOStream& strm )
{
	// We don't support restore from DB.
	strm << int( 0 );		// num spaces

	return true;
}

/**
 *	Override from IDatabase.
 */
void XMLDatabase::restoreEntities( EntityRecoverer& recoverer )
{
	// We don't support restore from DB.
	recoverer.start();
}

/**
 *	Override from IDatabase.
 */
void XMLDatabase::remapEntityMailboxes( const Mercury::Address& srcAddr,
		const BackupHash & destAddrs )
{
	for ( ActiveSet::iterator iter = activeSet_.begin();
			iter != activeSet_.end(); ++iter )
	{
		if (iter->second.baseRef.addr == srcAddr)
		{
			const Mercury::Address& newAddr =
					destAddrs.addressFor( iter->second.baseRef.id );
			// Mercury::Address::salt must not be modified.
			iter->second.baseRef.addr.ip = newAddr.ip;
			iter->second.baseRef.addr.port = newAddr.port;
		}
	}
}

/**
 *	Overrides IDatabase method
 */
void XMLDatabase::addSecondaryDB( const SecondaryDBEntry& entry )
{
	CRITICAL_MSG( "XMLDatabase::addSecondaryDb: Not implemented!" );
}

/**
 *	Overrides IDatabase method
 */
void XMLDatabase::updateSecondaryDBs( const BaseAppIDs& ids,
		IUpdateSecondaryDBshandler& handler )
{
	CRITICAL_MSG( "XMLDatabase::updateSecondaryDBs: Not implemented!" );
	handler.onUpdateSecondaryDBsComplete( SecondaryDBEntries() );
}

/**
 *	Overrides IDatabase method
 */
void XMLDatabase::getSecondaryDBs( IGetSecondaryDBsHandler& handler )
{
	CRITICAL_MSG( "XMLDatabase::getSecondaryDBs: Not implemented!" );
	handler.onGetSecondaryDBsComplete( SecondaryDBEntries() );
}

/**
 *	Overrides IDatabase method
 */
uint32 XMLDatabase::getNumSecondaryDBs()
{
	return 0;
}

/**
 *	Overrides IDatabase method
 */
int XMLDatabase::clearSecondaryDBs()
{
	// This always succeeds to simplify code from caller.
	// CRITICAL_MSG( "XMLDatabase::clearSecondaryDBs: Not implemented!" );
	return 0;
}


// xml_database.cpp
