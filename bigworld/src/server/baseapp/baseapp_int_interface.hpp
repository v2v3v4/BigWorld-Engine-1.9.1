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
	#undef PROXY_INT_INTERFACE_HPP
#endif

#ifndef PROXY_INT_INTERFACE_HPP
#define PROXY_INT_INTERFACE_HPP

#include "network/basictypes.hpp"
#include "server/anonymous_channel_client.hpp"


// -----------------------------------------------------------------------------
// Section: Helper macros
// -----------------------------------------------------------------------------

#undef MF_BEGIN_BASE_APP_MSG
#define MF_BEGIN_BASE_APP_MSG( NAME )										\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
			BaseAppMessageHandler< BaseAppIntInterface::NAME##Args >,		\
			&BaseApp::NAME )												\

#undef MF_BEGIN_BASE_APP_MSG_WITH_ADDR
#define MF_BEGIN_BASE_APP_MSG_WITH_ADDR( NAME )								\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
		BaseAppMessageWithAddrHandler< BaseAppIntInterface::NAME##Args >,	\
		&BaseApp::NAME )													\

#undef MF_BEGIN_PROXY_MSG
#define MF_BEGIN_PROXY_MSG( NAME )											\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
			NoBlockProxyMessageHandler< BaseAppIntInterface::NAME##Args >,	\
			&Proxy::NAME )													\

#undef MF_BEGIN_BASE_MSG
#define MF_BEGIN_BASE_MSG( NAME )											\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
			BaseMessageHandler< BaseAppIntInterface::NAME##Args >,			\
			&Base::NAME )													\

#undef MF_BEGIN_BASE_MSG_WITH_ADDR
#define MF_BEGIN_BASE_MSG_WITH_ADDR( NAME )									\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
			BaseMessageWithAddrHandler< BaseAppIntInterface::NAME##Args >,	\
			&Base::NAME )													\


#define MF_VARLEN_BASE_APP_MSG( NAME ) 										\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, 								\
			BaseAppVarLenMessageHandler, &BaseApp::NAME )

#define MF_RAW_BASE_APP_MSG( NAME )											\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, 								\
			BaseAppRawMessageHandler, &BaseApp::NAME )

#define MF_BIG_RAW_BASE_APP_MSG( NAME )										\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 4, 								\
			BaseAppRawMessageHandler, &BaseApp::NAME )

#define MF_VARLEN_BASE_MSG( NAME ) 											\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, 								\
			BaseVarLenMessageHandler, &Base::NAME )

#define MF_RAW_BASE_MSG( NAME ) 											\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, 								\
			RawBaseMessageHandler, &Base::NAME )

#define MF_VARLEN_PROXY_MSG( NAME ) 										\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, 								\
			ProxyVarLenMessageHandler<false>, &Proxy::NAME )

// -----------------------------------------------------------------------------
// Section: Includes
// -----------------------------------------------------------------------------

#include "server/common.hpp"
#include "network/interface_minder.hpp"
#include "network/msgtypes.hpp"

// -----------------------------------------------------------------------------
// Section: Proxy Internal Interface
// -----------------------------------------------------------------------------

#pragma pack(push, 1)
BEGIN_MERCURY_INTERFACE( BaseAppIntInterface )

	BW_ANONYMOUS_CHANNEL_CLIENT_MSG( DBInterface )

	MF_RAW_BASE_APP_MSG( createBaseWithCellData )

	MF_RAW_BASE_APP_MSG( createBaseFromDB )

	MF_RAW_BASE_APP_MSG( logOnAttempt )

	MF_VARLEN_BASE_APP_MSG( addGlobalBase )
	MF_VARLEN_BASE_APP_MSG( delGlobalBase )

	MF_RAW_BASE_APP_MSG( runScript )

	MF_BEGIN_BASE_APP_MSG( handleCellAppMgrBirth )
		Mercury::Address	addr;
	END_STRUCT_MESSAGE()

	MF_BEGIN_BASE_APP_MSG( handleBaseAppMgrBirth )
		Mercury::Address	addr;
	END_STRUCT_MESSAGE()

	MF_VARLEN_BASE_APP_MSG( handleCellAppDeath )

	MF_BEGIN_BASE_APP_MSG( startup )
		bool				bootstrap;
	END_STRUCT_MESSAGE()

	MF_BEGIN_BASE_APP_MSG( shutDown )
		bool	isSigInt;
	END_STRUCT_MESSAGE()

	MF_RAW_BASE_APP_MSG( controlledShutDown )

	MF_VARLEN_BASE_APP_MSG( setCreateBaseInfo )

	// *** messages called on apps that back up other base apps.
	// Sets who is backing us up.
	MF_BEGIN_BASE_APP_MSG( old_setBackupBaseApp )
		Mercury::Address	addr;
	END_STRUCT_MESSAGE()

	// Real BaseApp tells the backup to start backing us up.
	MF_BEGIN_BASE_APP_MSG( old_startBaseAppBackup )
		Mercury::Address	addr;
	END_STRUCT_MESSAGE()

	// Real BaseApp tells the backup to stop backing us up.
	MF_BEGIN_BASE_APP_MSG( old_stopBaseAppBackup )
		Mercury::Address	addr;
	END_STRUCT_MESSAGE()

	// Update the backup with the latest information for all base entities.
	MF_BIG_RAW_BASE_APP_MSG( old_backupBaseEntities )

	// Backup tells the real BaseApp it is still alive
	MF_BEGIN_BASE_APP_MSG( old_backupHeartbeat )
		Mercury::Address	addr;
	END_STRUCT_MESSAGE()

	// Backup takes over from a dead, real BaseApp.
	MF_BEGIN_BASE_APP_MSG( old_restoreBaseApp )
		Mercury::Address	intAddr;
		Mercury::Address	extAddr;
	END_STRUCT_MESSAGE()

	// *** messages for the new style BaseApp backup. Base entities are now
	// backed up individually ***
	MF_BEGIN_BASE_APP_MSG( startBaseEntitiesBackup )
		Mercury::Address	realBaseAppAddr;
		uint32				index;
		uint32				hashSize;
		uint32				prime;
		bool				isInitial;
	END_STRUCT_MESSAGE()

	MF_BEGIN_BASE_APP_MSG( stopBaseEntitiesBackup )
		Mercury::Address	realBaseAppAddr;
		uint32				index;
		uint32				hashSize;
		uint32				prime;
		bool				isPending;
	END_STRUCT_MESSAGE()

	MF_BIG_RAW_BASE_APP_MSG( backupBaseEntity )

	MF_BEGIN_BASE_APP_MSG_WITH_ADDR( stopBaseEntityBackup )
		EntityID entityID;
	END_STRUCT_MESSAGE()

	MF_RAW_BASE_APP_MSG( handleBaseAppDeath )

	MF_RAW_BASE_APP_MSG( setBackupBaseApps )

	// *** messages related to shared data ***
	MF_VARLEN_BASE_APP_MSG( setSharedData )

	MF_VARLEN_BASE_APP_MSG( delSharedData )

	// *** messages from the cell concerning the client ***

	// identify the client that future messages
	//  (in this bundle) are destined for.
	MF_BEGIN_BASE_APP_MSG( setClient )
		EntityID			id;
	END_STRUCT_MESSAGE()

	// set the cell that owns this base
	MF_BEGIN_BASE_MSG_WITH_ADDR( currentCell )
		SpaceID				newSpaceID;
		Mercury::Address	newCellAddr;
	END_STRUCT_MESSAGE()

	// set the cell that owns this base, in an emergency. This will be called by
	// an old ghost.
	MF_RAW_BASE_APP_MSG( emergencySetCurrentCell )
	//	SpaceID				newSpaceID;
	//	Mercury::Address	newCellAddr;

	MF_BEGIN_PROXY_MSG( sendToClient )
		uint8				dummy; // Can't have 0 length struct message
	END_STRUCT_MESSAGE()

	// Start of messages to forward from cell to client...
	MF_VARLEN_PROXY_MSG( createCellPlayer )

	MF_VARLEN_PROXY_MSG( spaceData )
	//	EntityID		spaceID
	//	SpaceEntryID	entryID
	//	uint16			key;
	//	char[]			value;		// rest of message

	MF_BEGIN_PROXY_MSG( enterAoI )
		EntityID			id;
		IDAlias				idAlias;
	END_STRUCT_MESSAGE()

	MF_BEGIN_PROXY_MSG( enterAoIOnVehicle )
		EntityID			id;
		EntityID			vehicleID;
		IDAlias				idAlias;
	END_STRUCT_MESSAGE()

	MF_VARLEN_PROXY_MSG( leaveAoI )
	//	EntityID		id;
	//	EventNumber[]	lastEventNumbers;	// rest

	MF_VARLEN_PROXY_MSG( createEntity )

	MF_VARLEN_PROXY_MSG( updateEntity )


	// The interface that is shared between this interface and the client
	// interface. This includes messages such as all of the avatarUpdate
	// messages.
#define MF_BEGIN_COMMON_RELIABLE_MSG MF_BEGIN_PROXY_MSG
#define MF_BEGIN_COMMON_PASSENGER_MSG MF_BEGIN_PROXY_MSG
#define MF_BEGIN_COMMON_UNRELIABLE_MSG MF_BEGIN_PROXY_MSG
#include "common/common_client_interface.hpp"

	// This message is used to send an accurate position of an entity down to
	// the client. It is usually sent when the volatile information of an entity
	// becomes less volatile.
	MF_BEGIN_PROXY_MSG( detailedPosition )
		EntityID		id;
		Position3D		position;
		Direction3D		direction;
	END_STRUCT_MESSAGE()


	MF_BEGIN_PROXY_MSG( forcedPosition )	// gotta ack this one
		EntityID		id;
		SpaceID			spaceID;
		EntityID		vehicleID;
		Position3D		position;
		Direction3D		direction;
	END_STRUCT_MESSAGE()

	MF_BEGIN_PROXY_MSG( modWard )
		EntityID		id;
		bool			on;
	END_STRUCT_MESSAGE()


	MF_VARLEN_PROXY_MSG( callClientMethod )
	// ... end of messages to forward from cell to client.




	// *** Message from the cell not concerning the client ***

	// Messages for proxies

	MF_VARLEN_BASE_MSG( backupCellEntity )

	MF_VARLEN_BASE_MSG( writeToDB )
	MF_RAW_BASE_MSG( cellEntityLost )

	/**
	 *	This message is used to signal to the base that it should be kept alive
	 *	for at least the specified interval, even if the client disconnects or
	 *	is not connected.
	 */
	MF_BEGIN_BASE_MSG( startKeepAlive )
		uint32			interval; // in seconds
	END_STRUCT_MESSAGE()

	MF_RAW_BASE_MSG( callBaseMethod )

	MF_VARLEN_BASE_MSG( callCellMethod )

	// 128 to 254 are messages destined either for our mailboxes
	// or for the client's entities. They all look like this:
	MERCURY_VARIABLE_MESSAGE( entityMessage, 2, NULL )

	// -------------------------------------------------------------------------
	// Watcher messages
	// -------------------------------------------------------------------------

	// Message to forward watcher requests via
	MF_RAW_BASE_APP_MSG( callWatcher )

END_MERCURY_INTERFACE()

#pragma pack(pop)

#endif // PROXY_INT_INTERFACE_HPP
