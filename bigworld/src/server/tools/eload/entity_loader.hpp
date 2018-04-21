/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ENTITY_LOADER_HPP
#define ENTITY_LOADER_HPP

#include "entitydef/entity_description_map.hpp"
#include "network/nub.hpp"

/**
 *	This class reads a scene file from a DataSection, finds server-side
 *	entities, and creates them on the server by sending create entity messages
 *	to the BaseAppMgr (or CellAppMgr).
 */
class EntityLoader : public Mercury::ReplyMessageHandler
{
public:
	enum Component
	{
		ON_BASE,
		ON_CELL
	};

	EntityLoader( Component component = ON_BASE, SpaceID spaceID = 0,
		int sleepTime = 10 );
	virtual ~EntityLoader();

	/// This method does all the real initialisation work.
	bool startup();

	/// Loads a scene graph, and fires off messages to the Cell App Manager.
	bool loadScene( DataSectionPtr pSection,
			const Matrix & blockTransform = Matrix::identity );

private:

	/// Handles a reply from the Cell App Manager.
	void handleMessage( const Mercury::Address&,
			Mercury::UnpackedMessageHeader&,
			BinaryIStream&, void* );

	/// Handles a Mercury exception
	void handleException( const Mercury::NubException&, void* );

	/// Generates an entity instantiation message
	bool createObject( EntityTypeID entityTypeID,
			const Vector3 & location, const Direction3D & direction,
			DataSectionPtr pProperties );

	/// Parses a single object in the scene graph
	bool parseObject( DataSectionPtr pObject, const Matrix & localToGlobal );

private:
	Mercury::Nub 				nub_;
	Mercury::Address 			addr_;
	EntityDescriptionMap 		entityDescriptionMap_;
	int							pendingCount_;
	int							sleepTime_;

	Component					component_;
	SpaceID						spaceID_;
};

#endif
