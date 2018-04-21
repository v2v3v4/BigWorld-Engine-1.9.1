/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#if defined(DEFINE_INTERFACE_HERE) || defined(DEFINE_SERVER_HERE)
	#undef CELL_INTERFACE_HPP
	#define SECOND_PASS
#endif


#ifndef CELL_INTERFACE_HPP
#define CELL_INTERFACE_HPP

// The following macro is to get around the problem with putting an argument in
// a macro that has a comma in it. In this case, the template class.
#ifdef DEFINE_SERVER_HERE
#define CELL_TYPEDEF_MACRO( TYPE, NAME )									\
	typedef MessageHandler< TYPE, CellAppInterface::NAME##Args >			\
													TYPE##_##NAME##_Handler;
#else
#define CELL_TYPEDEF_MACRO( TYPE, NAME )
#endif

#include "entitydef/entity_description.hpp"
#include "server/common.hpp"
#include "server/anonymous_channel_client.hpp"
#include "network/interface_minder.hpp"
#include "network/msgtypes.hpp"

// Temporary defines
#define MF_BEGIN_INTERFACE					BEGIN_MERCURY_INTERFACE
#define MF_END_INTERFACE					END_MERCURY_INTERFACE
#define MF_END_MSG							END_STRUCT_MESSAGE

#define MF_BEGIN_MSG						BEGIN_STRUCT_MESSAGE

#define MF_BEGIN_HANDLED_PREFIXED_MSG		BEGIN_HANDLED_PREFIXED_MESSAGE
#define MF_BEGIN_HANDLED_MSG				BEGIN_HANDLED_STRUCT_MESSAGE



// -----------------------------------------------------------------------------
// Section: Helper macros
// -----------------------------------------------------------------------------

#ifndef SECOND_PASS
enum EntityReality
{
	GHOST_ONLY,
	REAL_ONLY,
	WITNESS_ONLY
};

#endif


// For Cell Application
#define MF_BEGIN_CELL_APP_MSG( NAME )										\
	CELL_TYPEDEF_MACRO( CellApp, NAME )										\
	MF_BEGIN_HANDLED_MSG( NAME,												\
			CellApp_##NAME##_Handler,										\
			&CellApp::NAME )												\

#define MF_VARLEN_CELL_APP_MSG( NAME ) 										\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2,								\
			VarLenMessageHandler< CellApp >,								\
			&CellApp::NAME )

#define MF_RAW_CELL_APP_MSG( NAME ) 										\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, RawMessageHandler<CellApp>,	\
			&CellApp::NAME )

#define MF_RAW_CELL_APP_SIGNAL( NAME ) 										\
	HANDLER_STATEMENT( NAME, RawMessageHandler<CellApp>, &CellApp::NAME )	\
	MERCURY_EMPTY_MESSAGE( NAME, HANDLER_ARGUMENT( NAME ) )					\

// For Space
#define MF_BEGIN_SPACE_MSG( NAME ) 											\
	CELL_TYPEDEF_MACRO( Space, NAME )										\
	MF_BEGIN_HANDLED_PREFIXED_MSG( NAME,									\
			SpaceID,														\
			Space_##NAME##_Handler,											\
			&Space::NAME )													\

#define MF_VARLEN_SPACE_MSG( NAME ) 										\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2,								\
			VarLenMessageHandler< Space >,									\
			&Space::NAME )

// For Cell
#define MF_BEGIN_CELL_MSG( NAME ) 											\
	CELL_TYPEDEF_MACRO( Cell, NAME )										\
	MF_BEGIN_HANDLED_PREFIXED_MSG( NAME,									\
			SpaceID,														\
			Cell_##NAME##_Handler,											\
			&Cell::NAME )													\

#define MF_VARLEN_CELL_MSG( NAME ) 											\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, VarLenMessageHandler< Cell >,\
			&Cell::NAME )

#define MF_RAW_CELL_MSG( NAME ) 											\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, RawMessageHandler<Cell>,		\
			&Cell::NAME )

// For Cell referenced by Entity
#define MF_RAW_CELL_BY_ENTITY_MSG( NAME, METHOD_NAME ) 						\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2,								\
			CellRawByEntityMessageHandler, &Cell::METHOD_NAME )

// For Entity
#define MF_BEGIN_ENTITY_MSG( NAME, IS_REAL_ONLY ) 							\
	MF_BEGIN_HANDLED_PREFIXED_MSG( NAME, EntityID,							\
			CellEntityMessageHandler< CellAppInterface::NAME##Args >, 		\
			std::make_pair( &Entity::NAME, IS_REAL_ONLY ) )

#define MF_VARLEN_ENTITY_MSG( NAME, IS_REAL_ONLY )							\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, EntityVarLenMessageHandler,	\
			std::make_pair( &Entity::NAME, IS_REAL_ONLY) )

#define MF_RAW_VARLEN_ENTITY_MSG( NAME, IS_REAL_ONLY )						\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, 								\
			RawEntityVarLenMessageHandler,									\
			std::make_pair( &Entity::NAME, IS_REAL_ONLY) )

#define MF_VARLEN_ENTITY_REQUEST( NAME, IS_REAL_ONLY )						\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, EntityVarLenRequestHandler,	\
			std::make_pair( &Entity::NAME, IS_REAL_ONLY) )



// -----------------------------------------------------------------------------
// Section: Cell interface
// -----------------------------------------------------------------------------

MF_BEGIN_INTERFACE( CellAppInterface )

	BW_ANONYMOUS_CHANNEL_CLIENT_MSG( DBInterface )

	// -------------------------------------------------------------------------
	// CellApp messages
	// -------------------------------------------------------------------------
	MF_VARLEN_CELL_APP_MSG( addCell )
		// SpaceID spaceID;

	MF_BEGIN_CELL_APP_MSG( startup )
		Mercury::Address baseAppAddr;
	MF_END_MSG()

	MF_BEGIN_CELL_APP_MSG( setGameTime )
		TimeStamp		gameTime;
	MF_END_MSG()

	MF_BEGIN_CELL_APP_MSG( handleCellAppMgrBirth )
		Mercury::Address addr;
	MF_END_MSG()

	MF_BEGIN_CELL_APP_MSG( handleCellAppDeath )
		Mercury::Address addr;
	MF_END_MSG()

	MF_VARLEN_CELL_APP_MSG( handleBaseAppDeath )

	MF_BEGIN_CELL_APP_MSG( shutDown )
		bool isSigInt; // Not used.
	MF_END_MSG()

	MF_BEGIN_CELL_APP_MSG( controlledShutDown )
		ShutDownStage	stage;
		TimeStamp		shutDownTime;
	MF_END_MSG()

	MERCURY_HANDLED_VARIABLE_MESSAGE( sendEntityPositions, 2, \
			EntityPositionSender, NULL )

	MF_VARLEN_CELL_APP_MSG( createSpaceIfNecessary )
	//	SpaceID			spaceID
	//	int32			count
	//	count of:
	//		SpaceEntryID	entryID
	//		uint16			key;
	//		string			value;

	MF_VARLEN_CELL_APP_MSG( runScript );

	MF_VARLEN_CELL_APP_MSG( setSharedData );

	MF_VARLEN_CELL_APP_MSG( delSharedData );

	MF_BEGIN_CELL_APP_MSG( setBaseApp )
		Mercury::Address baseAppAddr;
	MF_END_MSG()

	MF_RAW_CELL_APP_MSG( onloadTeleportedEntity )

	// -------------------------------------------------------------------------
	// Space messages
	// -------------------------------------------------------------------------

	// The arguments are as follows:
	//  EntityID			The id of the entity
	//  Position3D			The position of the entity
	//  EntityTypeID		The type of the entity
	//  Mercury::Address	The address of the entity's owner
	//  Variable script state data
	MF_VARLEN_SPACE_MSG( createGhost )

	MF_VARLEN_SPACE_MSG( spaceData )
	//	SpaceEntryID	entryID
	//	uint16			key;
	//	char[]			value;		// rest of message

	MF_VARLEN_SPACE_MSG( allSpaceData )
	//	int				numEntries;
	//	numEntries of:
	//	SpaceEntryID	entryID;
	//	uint16			key;
	//	std::string		value;

	MF_VARLEN_SPACE_MSG( updateGeometry )

	#define SPACE_GEOMETRY_LOADED_BOOTSTRAP_FLAG	0x01
	MF_VARLEN_SPACE_MSG( spaceGeometryLoaded )
	//	uint8 flags;
	//	std::string lastGeometryPath

	MF_BEGIN_SPACE_MSG( shutDownSpace )
		uint8 info;	// Not used yet.
	MF_END_MSG()

	// -------------------------------------------------------------------------
	// Cell messages
	// -------------------------------------------------------------------------

	// Entity creation

	// The arguments are as follows:
	//	ChannelVersion 	The channel version
	//	bool			IsRestore flag
	// 	EntityID		The id for the new entity
	// 	Position3D		The position of the new entity
	//	bool			IsOnGround flag
	// 	EntityTypeID	The type for the new entity
	// 	Variable script initialisation data
	// 	Variable real entity data
	MF_RAW_CELL_MSG( createEntity )
	MF_RAW_CELL_BY_ENTITY_MSG( createEntityNearEntity, createEntity )

	// Miscellaneous
	MF_BEGIN_CELL_MSG( shouldOffload )
		bool enable;
	MF_END_MSG()

	// Called from CellAppMgr
	MF_BEGIN_CELL_MSG( setRetiringCell )
		bool isRetiring;
		bool isRemoved;
	MF_END_MSG()


	// -------------------------------------------------------------------------
	// Entity messages
	// -------------------------------------------------------------------------

	// Destined for real entity only

	// Fast-track avatar update
	MF_BEGIN_ENTITY_MSG( avatarUpdateImplicit, REAL_ONLY )
		Coord			pos;
		YawPitchRoll	dir;
		uint8			refNum;
	MF_END_MSG()

	// Brisk-track avatar update
	MF_BEGIN_ENTITY_MSG( avatarUpdateExplicit, REAL_ONLY )
		SpaceID			spaceID;
		EntityID		vehicleID;
		Coord			pos;
		YawPitchRoll	dir;
		bool			onGround;
		uint8			refNum;
	MF_END_MSG()

	MF_BEGIN_ENTITY_MSG( ackPhysicsCorrection, REAL_ONLY )
	MF_END_MSG()

	MF_VARLEN_ENTITY_MSG( enableWitness, REAL_ONLY )

	MF_BEGIN_ENTITY_MSG( witnessCapacity, WITNESS_ONLY )
		EntityID			witness;
		uint32				bps;
	MF_END_MSG()

	// requestEntityUpdate:
	//	EntityID	id;
	//	Array of event numbers;
	MF_VARLEN_ENTITY_MSG( requestEntityUpdate, WITNESS_ONLY )

	// This is used by ghost entities to inform the real entity that it is being
	// witnessed.
	MF_BEGIN_ENTITY_MSG( witnessed, REAL_ONLY )
	MF_END_MSG()

	MF_VARLEN_ENTITY_REQUEST( writeToDBRequest, REAL_ONLY )

	MF_BEGIN_ENTITY_MSG( destroyEntity, REAL_ONLY )
		int					flags; // Currently not used.
	MF_END_MSG()

	// Destined for ghost entity only
	MF_RAW_VARLEN_ENTITY_MSG( onload, GHOST_ONLY )

	MF_BEGIN_ENTITY_MSG( ghostAvatarUpdate, GHOST_ONLY )
		Coord				pos;
		YawPitchRoll		dir;
		bool				isOnGround;
		VolatileNumber		updateNumber;
	MF_END_MSG()

	MF_VARLEN_ENTITY_MSG( ghostHistoryEvent, GHOST_ONLY )

	MF_BEGIN_ENTITY_MSG( ghostSetReal, GHOST_ONLY )
		Mercury::Address	owner;
	MF_END_MSG()

	MF_BEGIN_ENTITY_MSG( ghostSetNextReal, GHOST_ONLY )
		Mercury::Address	nextRealAddr;
	MF_END_MSG()

	MF_BEGIN_ENTITY_MSG( delGhost, GHOST_ONLY )
	MF_END_MSG()

	MF_BEGIN_ENTITY_MSG( ghostVolatileInfo, GHOST_ONLY )
		VolatileInfo	volatileInfo;
	MF_END_MSG()

	MF_VARLEN_ENTITY_MSG( ghostControllerExist, GHOST_ONLY )
	MF_VARLEN_ENTITY_MSG( ghostControllerUpdate, GHOST_ONLY )

	// for non-OtherClient data, see ghostOtherClientDataUpdate below
	MF_VARLEN_ENTITY_MSG( ghostedDataUpdate, GHOST_ONLY )
		// EventNumber (int32) eventNumber
		// data for ghostDataUpdate

	// for OtherClient data
	MF_VARLEN_ENTITY_MSG( ghostedOtherClientDataUpdate, GHOST_ONLY )
		// EventNumber (int32) eventNumber
		// data for ghostOtherClientDataUpdate

	// The real entity uses this to query whether there are any entities
	// witnessing its ghost entities.
	MF_BEGIN_ENTITY_MSG( checkGhostWitnessed, GHOST_ONLY )
	MF_END_MSG()

	MF_BEGIN_ENTITY_MSG( aoiPriorityUpdate, GHOST_ONLY )
		float aoiPriority;
	MF_END_MSG()

	// Message to run cell script.
	MF_VARLEN_ENTITY_MSG( runScriptMethod, REAL_ONLY )

	// Message to run base method via a cell mailbox
	MF_VARLEN_ENTITY_MSG( callBaseMethod, REAL_ONLY )

	// Message to run client method via a cell mailbox
	MF_VARLEN_ENTITY_MSG( callClientMethod, REAL_ONLY )

	MF_BEGIN_ENTITY_MSG( delControlledBy, REAL_ONLY )
		EntityID deadController;
	MF_END_MSG()

	// CellApp's EntityChannelFinder uses this to forward base entity packets
	// from the ghost to the real
	MF_VARLEN_ENTITY_MSG( forwardedBaseEntityPacket, REAL_ONLY )

	// 128 to 254 are messages destined for our entities.
	// They all look like this:
	MERCURY_VARIABLE_MESSAGE( runExposedMethod, 2, NULL )


	// -------------------------------------------------------------------------
	// Watcher messages
	// -------------------------------------------------------------------------

	// Message to forward watcher requests via
	MF_RAW_CELL_APP_MSG( callWatcher )


MF_END_INTERFACE()

#undef CELL_TYPEDEF_MACRO
#undef SECOND_PASS

#endif // CELL_INTERFACE_HPP
