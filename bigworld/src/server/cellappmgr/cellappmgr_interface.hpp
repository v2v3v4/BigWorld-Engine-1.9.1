/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CELL_APP_MGR_INTERFACE_HPP_ONCE
#define CELL_APP_MGR_INTERFACE_HPP_ONCE

#include "network/basictypes.hpp"
#include "server/id_generator.hpp"
#include "server/anonymous_channel_client.hpp"

/**
 * Data to use when initialising a CellApp.
 */
struct CellAppInitData
{
	int32 id;			//!< ID of the new CellApp
	TimeStamp time;		//!< Current game time
	Mercury::Address baseAppAddr;	//!< Address of the BaseApp to talk to
	bool isReady;		//!< Flag indicating whether the server is ready
};

typedef uint8 SharedDataType;
const SharedDataType SHARED_DATA_TYPE_CELL_APP = 1;
const SharedDataType SHARED_DATA_TYPE_BASE_APP = 2;
const SharedDataType SHARED_DATA_TYPE_GLOBAL = 3;
const SharedDataType SHARED_DATA_TYPE_GLOBAL_FROM_BASE_APP = 4;

#endif

#if defined( DEFINE_INTERFACE_HERE ) || defined( DEFINE_SERVER_HERE )
	#undef CELL_APP_MGR_INTERFACE_HPP
#endif

#ifndef CELL_APP_MGR_INTERFACE_HPP
#define CELL_APP_MGR_INTERFACE_HPP

// We need this define because DEFINE_SERVER_HERE is undefined in
// interface_minder.hpp.
#ifdef DEFINE_SERVER_HERE
#define SERVER_IS_DEFINED_HERE
#endif

#include "server/common.hpp"
#include "network/interface_minder.hpp"
#include "server/reviver_subject.hpp"

// Temporary defines
#define MF_BEGIN_INTERFACE					BEGIN_MERCURY_INTERFACE
#define MF_END_INTERFACE					END_MERCURY_INTERFACE
#define MF_END_MSG							END_STRUCT_MESSAGE

#define MF_BEGIN_MSG						BEGIN_STRUCT_MESSAGE

#define MF_BEGIN_HANDLED_MSG				BEGIN_HANDLED_STRUCT_MESSAGE


// -----------------------------------------------------------------------------
// Section: Helper macros
// -----------------------------------------------------------------------------

#define MF_BEGIN_CELL_APP_MGR_MSG( NAME )									\
	MF_BEGIN_HANDLED_MSG( NAME,												\
			CellAppMgrMessageHandler< CellAppMgrInterface::NAME##Args >,	\
			&CellAppMgr::NAME )												\

#define MF_BEGIN_CELL_APP_MGR_MSG_WITH_ADDR( NAME )							\
	MF_BEGIN_HANDLED_MSG( NAME,												\
			CellAppMgrMessageWithAddrHandler<								\
					CellAppMgrInterface::NAME##Args >,						\
			&CellAppMgr::NAME )												\

#define MF_BEGIN_CELLAPP_MSG( NAME )										\
	MF_BEGIN_HANDLED_MSG( NAME,												\
			CellAppMessageHandler< CellAppMgrInterface::NAME##Args >,		\
			&CellApp::NAME )												\

#define MF_VARLEN_CELLAPP_MSG( NAME )										\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, CellAppVarLenMessageHandler,	\
			&CellApp::NAME )

#define MF_VARLEN_CELL_APP_MGR_MSG( NAME )									\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2,								\
			CellAppMgrVarLenMessageHandler,	&CellAppMgr::NAME )

#define MF_RAW_CELL_APP_MGR_MSG( NAME )										\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, CellAppMgrRawMessageHandler,	\
			&CellAppMgr::NAME )

// This is a bit of a hack. We want to pass a template type to
// MF_BEGIN_HANDLED_MSG but it has a comma in it. To avoid this, we do a typedef
// only when we "DEFINE_SERVER_HERE".

#ifdef SERVER_IS_DEFINED_HERE
	#undef MF_BEGIN_RETURN_CELL_APP_MGR_MSG
	#undef SERVER_IS_DEFINED_HERE
	#define MF_BEGIN_RETURN_CELL_APP_MGR_MSG( RETURN_TYPE, NAME )			\
		typedef CellAppMgrReturnMessageHandler< RETURN_TYPE,				\
						CellAppMgrInterface::NAME##Args > _BLAH_##NAME; 	\
		MF_BEGIN_HANDLED_MSG( NAME,											\
				_BLAH_##NAME,												\
				&CellAppMgr::NAME )
#else
	#undef MF_BEGIN_RETURN_CELL_APP_MGR_MSG
	#define MF_BEGIN_RETURN_CELL_APP_MGR_MSG( RETURN_TYPE, NAME )			\
		MF_BEGIN_HANDLED_MSG( NAME, a, b )
#endif


// -----------------------------------------------------------------------------
// Section: Cell App Manager interface
// -----------------------------------------------------------------------------

#pragma pack(push, 1)
MF_BEGIN_INTERFACE( CellAppMgrInterface )

	BW_ANONYMOUS_CHANNEL_CLIENT_MSG( DBInterface )

	// The arguments are the same as for Cell::createEntity.
	// It assumes that the first two arguments are:
	// 	EntityID		- The id of the new entity
	// 	Position3D		- The position of the new entity
	//
	MF_RAW_CELL_APP_MGR_MSG( createEntity )
	MF_RAW_CELL_APP_MGR_MSG( createEntityInNewSpace )

	MF_RAW_CELL_APP_MGR_MSG( prepareForRestoreFromDB )

	MF_RAW_CELL_APP_MGR_MSG( startup )

	MF_BEGIN_CELL_APP_MGR_MSG( shutDown )
		bool isSigInt;
	MF_END_MSG()

	MF_BEGIN_CELL_APP_MGR_MSG( controlledShutDown )
		ShutDownStage stage;
	END_STRUCT_MESSAGE()

	MF_BEGIN_CELL_APP_MGR_MSG( shouldOffload )
		bool enable;
	MF_END_MSG()

	MF_VARLEN_CELL_APP_MGR_MSG( runScript );

	MF_RAW_CELL_APP_MGR_MSG( addApp );

	MF_VARLEN_CELL_APP_MGR_MSG( recoverCellApp );

	MF_BEGIN_CELL_APP_MGR_MSG( delApp )
		Mercury::Address addr;
	MF_END_MSG()

	MF_BEGIN_CELL_APP_MGR_MSG( setBaseApp )
		Mercury::Address addr;
	MF_END_MSG()

	MF_BEGIN_CELL_APP_MGR_MSG( handleCellAppMgrBirth )
		Mercury::Address addr;
	MF_END_MSG()

	MF_BEGIN_CELL_APP_MGR_MSG( handleBaseAppMgrBirth )
		Mercury::Address addr;
	MF_END_MSG()

	MF_BEGIN_CELL_APP_MGR_MSG_WITH_ADDR( handleCellAppDeath )
		Mercury::Address addr;
	MF_END_MSG()

	MF_VARLEN_CELL_APP_MGR_MSG( handleBaseAppDeath )

	MF_BEGIN_CELL_APP_MGR_MSG_WITH_ADDR( ackCellAppDeath )
		Mercury::Address deadAddr;
	MF_END_MSG()

	MF_BEGIN_RETURN_CELL_APP_MGR_MSG( double, gameTimeReading )
		double				gameTimeReadingContribution;
	MF_END_MSG()	// double is good for ~100 000 years

	// These could be a space messages
	MF_RAW_CELL_APP_MGR_MSG( updateSpaceData )

	MF_BEGIN_CELL_APP_MGR_MSG( shutDownSpace )
		SpaceID spaceID;
	MF_END_MSG()

	MF_BEGIN_CELL_APP_MGR_MSG( ackBaseAppsShutDown )
		ShutDownStage stage;
	MF_END_MSG()

	MF_RAW_CELL_APP_MGR_MSG( checkStatus )

	// ---- Cell App messages ----
	MF_BEGIN_CELLAPP_MSG( informOfLoad )
		float load;
		int	numEntities;
	MF_END_MSG()

	MF_VARLEN_CELLAPP_MSG( updateBounds );

	MF_BEGIN_CELLAPP_MSG( shutDownApp )
		int8 dummy;
	MF_END_MSG()

	MF_BEGIN_CELLAPP_MSG( ackCellAppShutDown )
		ShutDownStage stage;
	MF_END_MSG()

	MF_REVIVER_PING_MSG()

	MF_VARLEN_CELL_APP_MGR_MSG( setSharedData )
	MF_VARLEN_CELL_APP_MGR_MSG( delSharedData )

MF_END_INTERFACE()
#pragma pack(pop)

#endif // CELL_APP_MGR_INTERFACE_HPP
