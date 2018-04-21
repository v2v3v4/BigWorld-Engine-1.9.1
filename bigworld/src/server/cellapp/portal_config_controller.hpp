/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PORTAL_CONFIG_CONTROLLER_HPP
#define PORTAL_CONFIG_CONTROLLER_HPP

#include "controller.hpp"
#include "cstdmf/time_queue.hpp"
#include "chunk/chunk_item.hpp"
#include "common/chunk_portal.hpp"

#include "pyscript/script.hpp"

/**
 *	This class controls the configuration of the portal over which its
 *	entity sits.
 */
class PortalConfigController : public Controller, public TimeQueueHandler
{
	DECLARE_CONTROLLER_TYPE( PortalConfigController )

public:
	PortalConfigController();

	virtual void writeGhostToStream( BinaryOStream& stream );
	virtual bool readGhostFromStream( BinaryIStream& stream );

	virtual void startGhost();
	virtual void stopGhost();

	static FactoryFnRet New( bool permissive, uint32 triFlags, bool navigable );
	PY_AUTO_CONTROLLER_FACTORY_DECLARE( PortalConfigController,
		ARG( bool, ARG( uint32, ARG( bool, END ) ) ) )

private:
	bool applyToWorld();

	virtual void handleTimeout( TimeQueueId id, void * pUser );
	virtual void onRelease( TimeQueueId id, void * pUser );

	void chunkUnloaded();
	friend class PortalConfigSentry;


	bool			permissive_;
	uint32			triFlags_;
	bool			navigable_;

	bool			started_;

	TimeQueueId		timeQueueID_;
	ChunkItemPtr	pSentry_;
	ChunkPortalPtr	pCPortal_;
};

#endif // PORTAL_CONFIG_CONTROLLER_HPP
