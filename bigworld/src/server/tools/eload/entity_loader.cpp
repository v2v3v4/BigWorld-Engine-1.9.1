/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifdef _WIN32
#pragma warning ( disable: 4786 )
#endif

#include "Python.h"		// See http://docs.python.org/api/includes.html

#include "entity_loader.hpp"

#include "cellappmgr/cellappmgr_interface.hpp"
#include "baseappmgr/baseappmgr_interface.hpp"

#include "entitydef/entity_description_map.hpp"
#include "entitydef/constants.hpp"

#include "network/mercury.hpp"

#include "resmgr/bwresource.hpp"
#include "resmgr/datasection.hpp"

#include "server/stream_helper.hpp"

DECLARE_DEBUG_COMPONENT(0)


/**
 * 	Constructor
 */
EntityLoader::EntityLoader( Component component, SpaceID spaceID,
		int sleepTime ) :
	pendingCount_( 0 ),
	sleepTime_( sleepTime ),
	component_( component ),
	spaceID_( spaceID )
{
}


/**
 * 	Destructor
 */
EntityLoader::~EntityLoader()
{
}


/**
 * 	This method performs all real initialisation.
 *
 * 	@return		true if successful, false otherwise
 */
bool EntityLoader::startup()
{
	const char * interfaceName =
		(component_ == ON_BASE) ? "BaseAppMgrInterface" : "CellAppMgrInterface";
	// Find the Cell App Manager.
	int reason = nub_.findInterface( interfaceName, 0, addr_ );

	if (reason != 0)
	{
		ERROR_MSG( "EntityLoader: Failed to find %s, reason %s\n",
				interfaceName,
				Mercury::reasonToString( (Mercury::Reason)reason ) );
		INFO_MSG( "Is the server running?\n" );
		return false;
	}

	// Parse entity definitions
	if (!entityDescriptionMap_.parse(
		BWResource::openSection( EntityDef::Constants::entitiesFile() ) ))
	{
		ERROR_MSG( "EntityLoader: Failed to parse %s\n",
			EntityDef::Constants::entitiesFile() );
		return false;
	}

	return true;
}


/**
 * 	This method loads a scene from the given data section.
 *
 * 	@param pSection			DataSection containing the scene.
 *	@param blockTransform	A transform from local coordinates to global ones.
 * 	@return					true if successful, false otherwise
 */
bool EntityLoader::loadScene( DataSectionPtr pSection,
	   const Matrix & blockTransform )
{
	if (!pSection)
	{
		ERROR_MSG( "EntityLoader:: pSection is NULL\n" );
		return false;
	}

	Matrix matrix = pSection->readMatrix34( "transform", Matrix::identity );
	matrix.postMultiply( blockTransform );

	int count = pSection->countChildren();

	for (int i = 0; i < count; ++i)
	{
		DataSectionPtr pCurr = pSection->openChild( i );

		if (pCurr->sectionName() == "entity")
		{
			this->parseObject( pCurr, matrix );

			while (pendingCount_)
			{
				nub_.processContinuously();
#ifdef _WIN32
				Sleep( sleepTime_ );
#else
				usleep( sleepTime_ * 1000 );
#endif
			}
		}
	}

	while (pendingCount_)
		nub_.processContinuously();

	return true;
}


/**
 * 	This method loads a single object.
 *
 * 	@param pSection			DataSection containing the object
 *	@param chunkTransform	The transform from local to global coordinates.
 *
 * 	@return					true if successful, false otherwise
 */
bool EntityLoader::parseObject( DataSectionPtr pObject,
	   const Matrix & localToGlobal )
{
	Matrix objToGlobal = pObject->readMatrix34( "transform", Matrix::identity );
	objToGlobal.postMultiply( localToGlobal );

	// Parse out the entity type.
	std::string entityType = pObject->readString( "type" );

	EntityTypeID entityTypeID;

	if (!entityDescriptionMap_.nameToIndex( entityType, entityTypeID ))
	{
		ERROR_MSG( "EntityLoader::parseObject: Invalid entity type %s\n",
				entityType.c_str() );
		return false;
	}

	Direction3D direction;
	direction.roll	= objToGlobal.roll();
	direction.pitch	= objToGlobal.pitch();
	direction.yaw	= objToGlobal.yaw();

	// Finally got all the information we need, so create it.
	return this->createObject( entityTypeID,
			objToGlobal.applyToOrigin(), direction,
			pObject->findChild( "properties" ) );
}


/**
 * 	This method generates the creation message.
 */
bool EntityLoader::createObject( EntityTypeID entityTypeID,
		const Vector3 & location,
		const Direction3D & direction,
		DataSectionPtr pProperties )
{
	const EntityDescription& desc = entityDescriptionMap_.entityDescription(
			entityTypeID );

	Mercury::Bundle bundle;

	if (component_ == ON_CELL)
	{
		bundle.startRequest( CellAppMgrInterface::createEntity, this );

		StreamHelper::addEntity( bundle,
			StreamHelper::AddEntityData( 0,
				location, false, entityTypeID, direction ) );

		// If it is a real type with witnesses, stream on extra arguments.
		desc.addSectionToStream( pProperties,
				bundle, EntityDescription::CELL_DATA );

		// TOKEN_ADD( bundle, "DB End" )

		StreamHelper::addRealEntity( bundle );
		bundle << '-'; // no witnesses
	}
	else
	{
		bundle.startRequest( BaseAppMgrInterface::createBaseEntity, this );

		bundle << EntityID( 0 ) << entityTypeID << DatabaseID( 0 );
		bundle << Mercury::Address( 0, 0 ); // dummy client address.
		bundle << std::string(); // encryptionKey
		bundle << false;	// Not persistent-only.

		desc.addSectionToStream( pProperties, bundle,
				EntityDescription::BASE_DATA | EntityDescription::CELL_DATA );
		bundle << location << direction << SpaceID( spaceID_ );
	}

	nub_.send( addr_, bundle );
	++pendingCount_;

	INFO_MSG( "Creating a %s\n", desc.name().c_str() );

	return true;
}


/**
 * 	This method handles a reply message from the Cell App Manager.
 */
void EntityLoader::handleMessage(const Mercury::Address&,
		Mercury::UnpackedMessageHeader&, BinaryIStream& data, void*)
{
	if (component_ == ON_CELL)
	{
		EntityID entityID;
		data >> entityID;
		INFO_MSG( "Created entity %u on cell\n", entityID );
	}
	else
	{
		EntityMailBoxRef ref;
		data >> ref;
		INFO_MSG( "Created entity %u on base at %s\n",
			ref.id, (char *)ref.addr );
	}

	--pendingCount_;
	nub_.breakProcessing();
}


/**
 * 	This method handles a Mercury exception.
 */
void EntityLoader::handleException( const Mercury::NubException& e, void* )
{
	ERROR_MSG( "Nub Exception: %d\n", e.reason() );
	--pendingCount_;
	nub_.breakProcessing();
}

// entity_loader.cpp
