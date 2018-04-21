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
	#undef BASE_APP_MGR_INTERFACE_HPP
#endif

#ifndef BASE_APP_MGR_INTERFACE_HPP
#define BASE_APP_MGR_INTERFACE_HPP

#define BASE_APP_MGR_INTERFACE_HPP_FIRSTTIME

#ifndef BASE_APP_MGR_INTERFACE_HPP_ONCE
#define BASE_APP_MGR_INTERFACE_HPP_ONCE

#include "network/basictypes.hpp"

/**
 * Data to use when initialising a BaseApp.
 */
#pragma pack( push, 1 )
struct BaseAppInitData
{
	int32 id;			//!< ID of the new BaseApp
	TimeStamp time;		//!< Current game time
	bool isReady;		//!< Flag indicating whether the server is ready
};
#pragma pack( pop )

#else
#undef BASE_APP_MGR_INTERFACE_HPP_FIRSTTIME
#endif // BASE_APP_MGR_INTERFACE_HPP_ONCE

// -----------------------------------------------------------------------------
// Section: Helper macros
// -----------------------------------------------------------------------------

#define MF_BEGIN_BASE_APP_MGR_MSG( NAME )									\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
			BaseAppMgrMessageHandler< BaseAppMgrInterface::NAME##Args >,	\
			&BaseAppMgr::NAME )												\

#define MF_BEGIN_BASE_APP_MGR_MSG_WITH_ADDR( NAME )							\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
		BaseAppMgrMessageHandlerWithAddr< BaseAppMgrInterface::NAME##Args >,\
		&BaseAppMgr::NAME )													\

#define MF_RAW_BASE_APP_MGR_MSG( NAME )										\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, BaseAppMgrRawMessageHandler,	\
			&BaseAppMgr::NAME )

#define MF_VARLEN_BASE_APP_MGR_MSG( NAME )										\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, BaseAppMgrVarLenMessageHandler,	\
			&BaseAppMgr::NAME )

#ifdef DEFINE_SERVER_HERE
	#undef MF_BEGIN_RETURN_BASE_APP_MGR_MSG
	#define MF_BEGIN_RETURN_BASE_APP_MGR_MSG( NAME )						\
		typedef BaseAppMgrReturnMessageHandler<								\
							BaseAppMgrInterface::NAME##Args > _BLAH_##NAME;	\
		BEGIN_HANDLED_STRUCT_MESSAGE( NAME,									\
				_BLAH_##NAME,												\
				&BaseAppMgr::NAME )
#else
	#undef MF_BEGIN_RETURN_BASE_APP_MGR_MSG
	#define MF_BEGIN_RETURN_BASE_APP_MGR_MSG( NAME )							\
		BEGIN_HANDLED_STRUCT_MESSAGE( NAME, 0, 0 )
#endif

// -----------------------------------------------------------------------------
// Section: Includes
// -----------------------------------------------------------------------------

#include "server/common.hpp"
#include "server/anonymous_channel_client.hpp"
#include "server/reviver_subject.hpp"
#include "network/interface_minder.hpp"


// -----------------------------------------------------------------------------
// Section: Base App Manager Interface
// -----------------------------------------------------------------------------

BEGIN_MERCURY_INTERFACE( BaseAppMgrInterface )

	BW_ANONYMOUS_CHANNEL_CLIENT_MSG( DBInterface )

#ifdef BASE_APP_MGR_INTERFACE_HPP_FIRSTTIME
	enum CreateEntityError
	{
		CREATE_ENTITY_ERROR_NO_BASEAPPS = 1,
		CREATE_ENTITY_ERROR_BASEAPPS_OVERLOADED
	};
#endif
	
	MERCURY_HANDLED_VARIABLE_MESSAGE( createEntity,	2,						\
									CreateEntityIncomingHandler, NULL )
									
	MF_BEGIN_RETURN_BASE_APP_MGR_MSG( add )
		Mercury::Address	addrForCells;
		Mercury::Address	addrForClients;
	END_STRUCT_MESSAGE()

	MF_BEGIN_RETURN_BASE_APP_MGR_MSG( addBackup )
		Mercury::Address	addr;
	END_STRUCT_MESSAGE()

	MF_RAW_BASE_APP_MGR_MSG( recoverBaseApp )
		// Mercury::Address		addrForCells;
		// Mercury::Address		addrForClients;
		// Mercury::Address		backupAddress;
		// BaseAppID			id;
		// float				maxLoad;
		// string, MailBoxRef	globalBases; (0 to many)

	MF_RAW_BASE_APP_MGR_MSG( old_recoverBackupBaseApp )
		//  Mercury::Address	addr;
		//  BaseAppID			id;
		//  float				maxLoad;
		//  Mercury::Address	backups; (0 to many)

	MF_BEGIN_BASE_APP_MGR_MSG_WITH_ADDR( del )
		BaseAppID		id;
	END_STRUCT_MESSAGE()

	MF_BEGIN_BASE_APP_MGR_MSG_WITH_ADDR( informOfLoad )
		float load;
		int numBases;
		int numProxies;
	END_STRUCT_MESSAGE()

	MF_BEGIN_BASE_APP_MGR_MSG( shutDown )
		bool		shouldShutDownOthers;
	END_STRUCT_MESSAGE()

	MF_BEGIN_BASE_APP_MGR_MSG( controlledShutDown )
		ShutDownStage stage;
		TimeStamp shutDownTime;
	END_STRUCT_MESSAGE()

	MF_BEGIN_BASE_APP_MGR_MSG( handleBaseAppDeath )
		Mercury::Address addr;
	END_STRUCT_MESSAGE()

	MF_BEGIN_BASE_APP_MGR_MSG( handleCellAppMgrBirth )
		Mercury::Address addr;
	END_STRUCT_MESSAGE()

	MF_BEGIN_BASE_APP_MGR_MSG( handleBaseAppMgrBirth )
		Mercury::Address addr;
	END_STRUCT_MESSAGE()

	MF_RAW_BASE_APP_MGR_MSG( handleCellAppDeath )
	MF_RAW_BASE_APP_MGR_MSG( createBaseEntity )

	MF_RAW_BASE_APP_MGR_MSG( registerBaseGlobally )
	MF_RAW_BASE_APP_MGR_MSG( deregisterBaseGlobally )

	MF_RAW_BASE_APP_MGR_MSG( runScript );

	MF_RAW_BASE_APP_MGR_MSG( requestHasStarted )

	// Sent by DBMgr to initialise game time etc.
	MF_RAW_BASE_APP_MGR_MSG( initData )
	
	// This is called by the DBMgr when it is ready to start the server.
	MF_RAW_BASE_APP_MGR_MSG( startup )

	MF_RAW_BASE_APP_MGR_MSG( checkStatus )

	// This is forwarded to the CellAppMgr.
	MF_RAW_BASE_APP_MGR_MSG( spaceDataRestore )

	MF_VARLEN_BASE_APP_MGR_MSG( setSharedData )
	MF_VARLEN_BASE_APP_MGR_MSG( delSharedData )

	MF_RAW_BASE_APP_MGR_MSG( useNewBackupHash )

	MF_RAW_BASE_APP_MGR_MSG( informOfArchiveComplete )

	MF_REVIVER_PING_MSG()

END_MERCURY_INTERFACE()

#endif // BASE_APP_MGR_INTERFACE_HPP
