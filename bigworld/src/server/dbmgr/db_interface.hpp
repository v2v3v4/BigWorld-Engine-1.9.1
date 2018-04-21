/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#if defined( DEFINE_INTERFACE_HERE ) || defined( DEFINE_SERVER_HERE )
	#undef DB_INTERFACE_HPP
#endif

#ifndef DB_INTERFACE_HPP
#define DB_INTERFACE_HPP

// -----------------------------------------------------------------------------
// Section: Includes
// -----------------------------------------------------------------------------

#include "server/common.hpp"
#include "server/reviver_subject.hpp"
#include "network/interface_minder.hpp"

// -----------------------------------------------------------------------------
// Section: Helper macros
// -----------------------------------------------------------------------------

#define MF_RAW_DB_MSG( NAME )												\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2,								\
			DBVarLenMessageHandler, &Database::NAME );						\

// Same as above except message length increased to 3 bytes.
#define MF_MEDIUM_RAW_DB_MSG( NAME )										\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 3,								\
			DBVarLenMessageHandler, &Database::NAME );						\

#define MF_BEGIN_SIMPLE_DB_MSG( NAME )										\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
			SimpleDBMessageHandler< DBInterface::NAME##Args >,				\
			&Database::NAME )												\

#define MF_BEGIN_RETURN_DB_MSG( NAME )										\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
			ReturnDBMessageHandler< DBInterface::NAME##Args >,				\
			&Database::NAME )												\

// -----------------------------------------------------------------------------
// Section: Database Interface
// -----------------------------------------------------------------------------

BEGIN_MERCURY_INTERFACE( DBInterface )

	MF_REVIVER_PING_MSG()

	MF_BEGIN_SIMPLE_DB_MSG( handleBaseAppMgrBirth )
		Mercury::Address	addr;
	END_STRUCT_MESSAGE()

	MF_BEGIN_SIMPLE_DB_MSG( shutDown )
		// none
	END_STRUCT_MESSAGE()

	MF_BEGIN_SIMPLE_DB_MSG( controlledShutDown )
		ShutDownStage stage;
	END_STRUCT_MESSAGE()

	MF_BEGIN_SIMPLE_DB_MSG( cellAppOverloadStatus )
		bool anyOverloaded;
	END_STRUCT_MESSAGE()


	MF_RAW_DB_MSG( logOn )
		// std::string logOnName
		// std::string password
		// Mercury::Address addrForProxy
		// bool offChannel
		// MD5::Digest digest

	MF_RAW_DB_MSG( loadEntity )
		// EntityTypeID	entityTypeID;
		// EntityID entityID;
		// bool nameNotID;
		// nameNotID ? (std::string name) : (DatabaseID id );

	MF_MEDIUM_RAW_DB_MSG( writeEntity )
		// int8 flags; (cell? base? log off?)
		// EntityTypeID entityTypeID;
		// DatabaseID	databaseID;
		// properties

	MF_BEGIN_RETURN_DB_MSG( deleteEntity )
		EntityTypeID	entityTypeID;
		DatabaseID		dbid;
	END_STRUCT_MESSAGE()

	MF_RAW_DB_MSG( deleteEntityByName )
		// EntityTypeID	entityTypeID;
		// std::string name;

	MF_BEGIN_RETURN_DB_MSG( lookupEntity )
		EntityTypeID	entityTypeID;
		DatabaseID		dbid;
		bool			offChannel;
	END_STRUCT_MESSAGE()

	MF_RAW_DB_MSG( lookupEntityByName )
		// EntityTypeID		entityTypeID;
		// std::string 		name;
		// bool				offChannel;

	MF_RAW_DB_MSG( lookupDBIDByName )
		// EntityTypeID	entityTypeID;
		// std::string name;

	MF_MEDIUM_RAW_DB_MSG( executeRawCommand )
		// char[] command;

	MF_RAW_DB_MSG( putIDs )
		// EntityID ids[];

	MF_RAW_DB_MSG( getIDs )
		// int numIDs;

	MF_MEDIUM_RAW_DB_MSG( writeSpaces )

	MF_BEGIN_SIMPLE_DB_MSG( writeGameTime )
		TimeStamp gameTime;
	END_STRUCT_MESSAGE()

	MF_BEGIN_SIMPLE_DB_MSG( handleDatabaseBirth )
		Mercury::Address	addr;
	END_STRUCT_MESSAGE()

	MF_RAW_DB_MSG( handleBaseAppDeath )

	MF_RAW_DB_MSG( checkStatus )

	MF_RAW_DB_MSG( secondaryDBRegistration );
	MF_RAW_DB_MSG( updateSecondaryDBs );
	MF_RAW_DB_MSG( getSecondaryDBDetails );

END_MERCURY_INTERFACE()

#endif // DB_INTERFACE_HPP
