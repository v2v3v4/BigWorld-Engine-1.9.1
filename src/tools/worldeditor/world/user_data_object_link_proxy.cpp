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
#include "worldeditor/world/user_data_object_link_proxy.hpp"
#include "worldeditor/world/editor_chunk_item_linker.hpp"
#include "worldeditor/world/world_manager.hpp"
#include "worldeditor/world/items/editor_chunk_entity.hpp"
#include "worldeditor/world/items/editor_chunk_user_data_object.hpp"
#include "worldeditor/world/editor_chunk_item_linker_manager.hpp"
#include "worldeditor/world/editor_chunk.hpp"
#include "worldeditor/editor/user_data_object_link_locator.hpp"
#include "worldeditor/editor/chunk_item_placer.hpp"
#include "worldeditor/editor/item_properties.hpp"
#include "common/editor_views.hpp"
#include "chunk/user_data_object_link_data_type.hpp"
#include "resmgr/xml_section.hpp"


/**
 *  UserDataObjectLinkProxy constructor.
 *
 *  @param linkName	The id linked.
 *  @param linker	The linker object being linked.
 */
/*explicit*/ UserDataObjectLinkProxy::UserDataObjectLinkProxy(
		const std::string&		linkName,
		EditorChunkItemLinkable*	linker ) :
	linkName_(linkName),
	linker_(linker)
{
	PropertyIndex index =
		linker_->propHelper()->propGetIdx( linkName_ );
	PyObjectPtr ob( linker_->propHelper()->propGetPy( index ), PyObjectPtr::STEAL_REFERENCE );
	linkValue_ = UserDataObjectLinkDataType::asString( ob.getObject() );
}


/**
 *  UserDataObjectLinkProxy destructor.
 */
/*virtual*/ UserDataObjectLinkProxy::~UserDataObjectLinkProxy()
{
}


/**
 *  With entities we only support linking, not the creation of new links.
 *
 *  @returns                LT_LINK
 */
/*virtual*/ LinkProxy::LinkType UserDataObjectLinkProxy::linkType() const
{
	if ( linker_->chunkItem()->isEditorUserDataObject() )
	{
		EditorChunkUserDataObject* udo =
			static_cast<EditorChunkUserDataObject*>( linker_->chunkItem() );

		if ( udo->showAddGizmo( linkName_ ) )
			return (LinkType)( LT_ADD | LT_LINK );
		else
			return LT_LINK;
	}

	// Linker must be an entity.
	return LT_LINK;
}


/**
 *  Create a copy of the EditorChunkUserDataObject that the UserDataObjectLinkProxy 
 *  is working on, link this copy to the original and return a MatrixProxyPtr 
 *  that can set the position/orientation etc of the copy.
 *
 *  @return		A proxy to set the position/orientation of the linked
 *              item.
 */
/*virtual*/ MatrixProxyPtr UserDataObjectLinkProxy::createCopyForLink()
{
	if ( linker_->chunkItem()->isEditorEntity() )
	{
		// Not supported in entities at the moment.
		return NULL;
	}

    // Create a copy of node_:
    EditorChunkUserDataObject *newNode = new EditorChunkUserDataObject();
    DataSectionPtr newSection = new XMLSection("copy");
    newSection->copy( linker_->chunkItem()->pOwnSect() );
	newSection->delChild( "guid" );
	newSection->delChild( "backLinks" );

	// delete the link property, or the whole array if it's an array of links
	DataSectionPtr propsSection = newSection->openSection( "properties" );
	std::string baseLinkName = linkName_;
	int pos = baseLinkName.find_first_of( "[" );
	if ( pos != std::string::npos )
		baseLinkName = baseLinkName.substr( 0, pos );
	if ( propsSection != NULL )
		propsSection->delChild( baseLinkName );

	// and load it.
    newNode->load( newSection, linker_->chunkItem()->chunk() );
    linker_->chunkItem()->chunk()->addStaticItem( newNode );
    newNode->edTransform( linker_->chunkItem()->edTransform(), false );

	// set the link in the current node to point to the newNode
	PropertyIndex propIdx =
		linker_->propHelper()->propGetIdx(linkName_);

	WorldManager::instance().linkerManager().addLink(
		linker_, newNode->chunkItemLinker(), propIdx );
	
	PyObjectPtr ob( linker_->propHelper()->propGetPy( propIdx ), PyObjectPtr::STEAL_REFERENCE );
	linkValue_ = UserDataObjectLinkDataType::asString( ob.getObject() );

    // Set the new node as the selection:
	int curSel = PropTable::table()->propertyList()->GetCurSel();
    std::vector<ChunkItemPtr> items;
    items.push_back( newNode );
    WorldManager::instance().setSelection( items );
	PropTable::table()->propertyList()->selectItem( curSel );

	newNode->propHelper()->resetSelUpdate( true );

    // Return a ChunkItemMatrix for the new node so that its position can be
    // edited:
    ChunkItemMatrix *result = new ChunkItemMatrix( newNode );
    result->recordState();
    return result;
}


/**
 *  This function is used to determine whether the given locator's position
 *  can link to something.
 *
 *  @param pLocator     The locator to test.
 *	@return				TS_CAN_LINK if it can be linked, TS_CANT_LINK if it
 *						cannot be linked, TS_NO_TARGET if there is no valid
 *						object to link to under the locator.
 */
/*virtual*/ LinkProxy::TargetState UserDataObjectLinkProxy::canLinkAtPos(ToolLocatorPtr pLocator) const
{
	UserDataObjectLinkLocator *locator =
		static_cast<UserDataObjectLinkLocator*>( pLocator.getObject() );

    if ( locator->chunkItem() == NULL ||
		!EditorChunkCache::instance( *linker_->chunkItem()->chunk() ).edIsWriteable() )
		return TS_NO_TARGET;

	if ( locator->chunkItem().getObject() == linker_->chunkItem() )
		return TS_NO_TARGET;	// avoid linking to ourselves

	EditorChunkItem *item = static_cast<EditorChunkItem*>( locator->chunkItem().getObject() );
	if ( item == NULL || !item->isEditorUserDataObject() )
		return TS_NO_TARGET;

	EditorChunkUserDataObject *ecudo = static_cast<EditorChunkUserDataObject*>( item );
	if ( linker_->chunkItem()->isEditorEntity() )
	{
		EditorChunkEntity* entity =
			static_cast<EditorChunkEntity*>( linker_->chunkItem() );

		// Get info for the target
		PyObjectPtr ecudoInfo( ecudo->infoDict(), PyObjectPtr::STEAL_REFERENCE );

		if ( !entity->canLinkTo( linkName_, ecudoInfo.getObject() ) )
			return TS_CANT_LINK;
	}
	else if ( linker_->chunkItem()->isEditorUserDataObject() )
	{
		EditorChunkUserDataObject* udo =
			static_cast<EditorChunkUserDataObject*>( linker_->chunkItem() );

		if ( !udo->canLinkTo( linkName_, ecudo ) )
			return TS_CANT_LINK;
	}

	return TS_CAN_LINK;
}


/**
 *  This links the udo to the udo at the locator's position.
 *
 *  @param pLocator          The locator that has an item to link to.
 */
/*virtual*/ void UserDataObjectLinkProxy::createLinkAtPos(ToolLocatorPtr pLocator)
{
    UserDataObjectLinkLocator* locator =
        (UserDataObjectLinkLocator*)pLocator.getObject();
    ChunkItemPtr chunkItem = locator->chunkItem();
    if (chunkItem == NULL)
        return;

    // The linked udo
    EditorChunkItem* item = static_cast<EditorChunkItem*>( chunkItem.getObject() );
    if (!item->isEditorUserDataObject())
        return;
    EditorChunkUserDataObject* ecudo = static_cast<EditorChunkUserDataObject*>( item );

    if( !EditorChunkCache::instance( *ecudo->chunk() ).edIsWriteable() )
		return;

	PropertyIndex propIdx =
		linker_->propHelper()->propGetIdx(linkName_);
	if ( propIdx.empty() )
		//TODO: have a warning or error especially if its not a selection
		return;

	// Add a link from the linker to ecudo in property propIdx
	WorldManager::instance().linkerManager().addLink(
		linker_, ecudo->chunkItemLinker(), propIdx );
		
	// Update the property representation
	PyObjectPtr ob( linker_->propHelper()->propGetPy( propIdx ), PyObjectPtr::STEAL_REFERENCE );
	linkValue_ = UserDataObjectLinkDataType::asString( ob.getObject() );

	UndoRedo::instance().barrier(
		L( "WORLDEDITOR/WORLDEDITOR/PROPERTIES/STATION_NODE_LINK_PROXY/LINK_NODES" ), false);
}


/**
 *  This funciton is used to create a tool locator appropriate to this linker.
 *  In this case we create a UserDataObjectLinkLocator and set it to only
 *  locator nodes (not entities).
 *
 *  @returns                The tool locator to use.
 */
/*virtual*/ ToolLocatorPtr UserDataObjectLinkProxy::createLocator() const
{
    return 
        ToolLocatorPtr
        (
            new  UserDataObjectLinkLocator(linkValue_, UserDataObjectLinkLocator::LOCATE_USER_DATA_OBJECTS), 
            true
        );
}
