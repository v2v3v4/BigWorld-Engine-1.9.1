/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"
#include "worldeditor/world/items/editor_chunk_user_data_object_link.hpp"
#include "worldeditor/world/items/editor_chunk_user_data_object.hpp"
#include "worldeditor/world/items/editor_chunk_entity.hpp"
#include "worldeditor/world/world_manager.hpp"
#include "worldeditor/world/editor_chunk_item_linker_manager.hpp"


std::vector<std::string> EditorChunkUserDataObjectLink::edCommand(
	std::string const &path ) const
{
	std::vector<std::string> commands;
	commands.push_back(L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_LINK/DELETE"));

	ChunkItem* startCI =
		static_cast<ChunkItem*>(startItem().getObject());
	ChunkItem* endCI =
		static_cast<ChunkItem*>(endItem().getObject());

	if (!startCI->isEditorUserDataObject() || !endCI->isEditorUserDataObject())
		return commands;

	EditorChunkUserDataObject* start =
		static_cast<EditorChunkUserDataObject*>( startItem().getObject() );
	EditorChunkUserDataObject* end =
		static_cast<EditorChunkUserDataObject*>( endItem().getObject() );
	// At the moment, it doesn't matter if "getLinkCommands" is called from
	// start or end, and it only works if both UDOs are the same type.
	start->getLinkCommands( commands, end );

	return commands;
}


bool EditorChunkUserDataObjectLink::edExecuteCommand(
	std::string const &path, std::vector<std::string>::size_type index )
{
	ChunkItem* startCI =
		static_cast<ChunkItem*>(startItem().getObject());
	ChunkItem* endCI =
		static_cast<ChunkItem*>(endItem().getObject());

	if (startCI->isEditorEntity() && endCI->isEditorUserDataObject())
	{
		// Links to entities can only be deleted
		deleteCommand();

		std::vector<ChunkItemPtr> emptyList;
		WorldManager::instance().setSelection(emptyList, true);
		return true;
	}
	else if (startCI->isEditorUserDataObject() && endCI->isEditorUserDataObject())
	{
		if (index == 0)
		{
			// Clicked "Delete", so delete the link
			deleteCommand();
		}
		else if ( index >= 1 )
		{
			// Clicked a python command, so execute it.
			EditorChunkUserDataObject* start =
				static_cast<EditorChunkUserDataObject*>( startItem().getObject() );
			EditorChunkUserDataObject* end =
				static_cast<EditorChunkUserDataObject*>( endItem().getObject() );
			// At the moment, it doesn't matter if "executeLinkCommand" is called from
			// start or end, and it only works if both UDOs are the same type.
			start->executeLinkCommand( index - 1, end );
		}
		else
		{
			return false;
		}

		std::vector<ChunkItemPtr> emptyList;
		WorldManager::instance().setSelection(emptyList, true);
		return true;
	}

	return false;
}


void EditorChunkUserDataObjectLink::deleteCommand()
{
	ChunkItem* startCI =
		static_cast<ChunkItem*>(startItem().getObject());
	ChunkItem* endCI =
		static_cast<ChunkItem*>(endItem().getObject());

	EditorChunkItemLinkable* startLinker = 0;
	EditorChunkItemLinkable* endLinker = 0;

	if (startCI->isEditorEntity())
		startLinker = static_cast<EditorChunkEntity*>(startCI)->chunkItemLinker();
	else if (startCI->isEditorUserDataObject())
		startLinker = static_cast<EditorChunkUserDataObject*>(startCI)->chunkItemLinker();

	if (endCI->isEditorEntity())
		endLinker = static_cast<EditorChunkEntity*>(endCI)->chunkItemLinker();
	else if (endCI->isEditorUserDataObject())
		endLinker = static_cast<EditorChunkUserDataObject*>(endCI)->chunkItemLinker();

	// Inform the linker manager that all links are to be deleted
	WorldManager::instance().linkerManager().deleteAllLinks(startLinker, endLinker);

	UndoRedo::instance().barrier( 
		L( "WORLDEDITOR/WORLDEDITOR/PROPERTIES/STATION_NODE_LINK_PROXY/LINK_NODES" ), false);
}


/**
 *	This method overrides the base class' draw method to set a shader constant
 *	when the link has to be drawn red (i.e. when one of its end points is in a
 *	read-only chunk).
 */
/*virtual*/ void EditorChunkUserDataObjectLink::draw()
{
	if ( materialEffect()->pEffect() && materialEffect()->pEffect()->pEffect() )
	{
		if (highlight_)
			materialEffect()->pEffect()->pEffect()->SetBool("highlight", TRUE);
		else
			materialEffect()->pEffect()->pEffect()->SetBool("highlight", FALSE);
	}

	highlight_ = false;

	EditorChunkLink::draw();
}


/**
 *  This method prevents collisions against this kind of links
 *
 *	@param source	Starting point of the collision ray
 *	@param dir		Direction of the collision ray
 *	@param wt		Triangle to test, in world coordinates
 *	@return			distance from 'source' to the collision point.
 */
/*virtual*/ float EditorChunkUserDataObjectLink::collide(
    const Vector3& source, const Vector3& dir, WorldTriangle& wt ) const
{
	// check to see that both sides of the link are writeable
	EditorChunkItem* start =
		static_cast<EditorChunkItem*>(startItem().getObject());
	EditorChunkItem* end =
		static_cast<EditorChunkItem*>(endItem().getObject());
	if ( start == NULL || end == NULL ||
		!EditorChunkCache::instance( *(start->chunk()) ).edIsWriteable() ||
		!EditorChunkCache::instance( *(end->chunk()) ).edIsWriteable())
		return std::numeric_limits<float>::max();

	return EditorChunkLink::collide( source, dir, wt );
}
