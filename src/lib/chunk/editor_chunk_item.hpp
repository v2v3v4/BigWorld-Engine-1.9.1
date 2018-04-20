/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef EDITOR_CHUNK_ITEM_HPP
#define EDITOR_CHUNK_ITEM_HPP

#ifndef EDITOR_ENABLED
#error EDITOR_ENABLED must be defined
#endif

#include <vector>
#include <string>
#include "resmgr/string_provider.hpp"
#include "../../tools/common/bw_message_info.hpp"

class EditorGroup;

/**
 *	This class declares the extra data and methods that the editor requires all
 *	its chunk items to have.
 */
class EditorChunkItem : public ChunkItemBase
{
public:
	explicit EditorChunkItem( WantFlags wantFlags = WANTS_NOTHING );
	virtual ~EditorChunkItem();

	/** Load function called after the chunk has been bound */
	virtual void edMainThreadLoad() {}
	/** Called when the chunk is bound, calls through to edMainThreadLoad() */
	void edChunkBind();


	bool edCommonSave( DataSectionPtr pSection );
	bool edCommonLoad( DataSectionPtr pSection );

	/**
	 *	Save to the given data section
	 *	May be called at any time (generally by the item onitself), not related
	 *	to the containing chunk being saved
	 */
	virtual bool edSave( DataSectionPtr pSection )	{ return false; }

	/**
	 *	Called when the parent chunk is saving itself
	 *
	 *	Any changed we've made to our DataSection will be automatically
	 *	save it any case, this is only needed to save external resources,
	 *	such as the static lighting data for EditorChunkModel
	 */
	virtual void edChunkSave() {}
	virtual void edChunkSaveCData(DataSectionPtr cData) {}


	virtual void toss( Chunk * pChunk );

	/**
	 *	Access and mutate matricies for items that have them
	 */
	virtual const Matrix & edTransform()	{ return Matrix::identity; }
	virtual bool edTransform( const Matrix & m, bool transient = false )
	{
		transient_ = transient;
		return false;
	}

	/**
	 *	Is this item currently moving?
	 */
	virtual bool edIsTransient() { return transient_; }

	/**
	 *	Get the local spec (in edTransform's space) coords of this item
	 */
	virtual void edBounds( BoundingBox & bbRet ) const { }

	/**
	 *	Get the local bounding box (in edTransform's space) to use when marking
	 *  as selected
	 */
	virtual void edSelectedBox( BoundingBox & bbRet ) const { return edBounds( bbRet ); }

	/**
	 *	Whether the chunk item affects the ray tracing in shadow calculation
	 */
	virtual bool edAffectShadow() const {	return true; }

	/**
	 *	Whether this item is editable according to locks in bwlockd ( it is always editable if bwlockd is not present )
	 */
	virtual bool edIsEditable();

	/**
	 *	Access the class name. Do NOT be tempted to use this in
	 *	switch statements... make a virtual function for it!
	 *	This should only be used for giving the user info about the item.
	 */
	virtual const char * edClassName()		{ return L("CHUNK/EDITOR/EDITOR_CHUNK_ITEM/UNKNOWN"); }

	/**
	 *	Get a nice description for this item. Most items will not need
	 *	to override this method.
	 */
	virtual std::string edDescription();

	/**
	 *	Edit this item, by adding its properties to the given object
	 */
	virtual bool edEdit( class ChunkItemEditor & editor )
		{ return false; }

	virtual std::vector<std::string> edCommand( const std::string& path ) const
		{	return std::vector<std::string>();	}
	virtual bool edExecuteCommand(  const std::string& path , std::vector<std::string>::size_type index )
		{	return false;	}
	/**
	 *	Find which chunk this item has been dropped in if its local
	 *	position has changed to that given. Complains and returns
	 *	NULL if the drop chunk can't be found.
	 */
	virtual Chunk * edDropChunk( const Vector3 & lpos );


	/**
	 *	Access the group of the chunk item
	 */
	EditorGroup * edGroup()					{ return pGroup_; }
	void edGroup( EditorGroup * pGp );

	/**
	 * The DataSection of the chunk item, to enable copying
	 *
	 * NULL is a valid value to indicate that no datasection is exposed
	 */
	virtual DataSectionPtr pOwnSect()		{ return NULL; }
	virtual const DataSectionPtr pOwnSect()	const { return NULL; }

	/**
	 * If this ChunkItem is the interior mesh for it's chunk
	 */
	virtual bool isShellModel()				{ return false; }

	/**
	 * If this ChunkItem is a portal
	 */
	virtual bool isPortal() const				{ return false; }

    /**
     * If this ChunkItem is an entity.
     */
    virtual bool isEditorEntity() const { return false; }
	 /**
     * If this ChunkItem is a User Data Object
     */
    virtual bool isEditorUserDataObject() const { return false; }

    /**
     * If this ChunkItem is a EditorChunkStationNode.
     */
    virtual bool isEditorChunkStationNode() const { return false; }

    /**
     * If this ChunkItem is a EditorChunkLink.
     */
    virtual bool isEditorChunkLink() const { return false; }

	/**
	 * Ask the item if we can snap other items to it, for example, when in
	 * obstacle mode.
	 */
	virtual bool edIsSnappable() { return true; }

	/**
	 * Ask the item if we can delete it
	 */
	virtual bool edCanDelete() { return true; }

    /**
     * Can the item be added to the selection?
     */
    virtual bool edCanAddSelection() const { return true; }

	/**
	 * Tell the item we're about to delete it.
	 *
	 * Will only be called if edCanDelete() returned true
	 */
	virtual void edPreDelete()		
	{
		#if UMBRA_ENABLE
		pUmbraModel_ = NULL;
		pUmbraObject_ = NULL;
		#endif
	}

	/**
	 * Tell the item it was just cloned from srcItem
	 *
	 * srcItem will be NULL if they shell we were in was cloned, rather than
	 * us directly.
	 */
	virtual void edPostClone( EditorChunkItem* srcItem )	
	{ 
		this->syncInit();
	}

	/**
	 * get the DataSection for clone
	 */
	virtual void edCloneSection( Chunk* destChunk, const Matrix& destMatrixInChunk, DataSectionPtr destDS )
	{
		if( pOwnSect() )
		{
			destDS->copy( pOwnSect() );
			if (destDS->openSection( "transform" ))
				destDS->writeMatrix34( "transform", destMatrixInChunk );
			if (destDS->openSection( "position" ))
				destDS->writeVector3( "position", destMatrixInChunk.applyToOrigin() );
			if (destDS->openSection( "direction" ))
				destDS->writeVector3( "direction", -destMatrixInChunk.applyToUnitAxisVector( 1 ) );
		}
	}

	/**
	 * refine the DataSection for chunk clone
	 */
	virtual bool edPreChunkClone( Chunk* srcChunk, const Matrix& destChunkMatrix, DataSectionPtr chunkDS )
	{
		return true;
	}

	/**
	 * return whether this item's position is relative to the chunk
	 */
	virtual bool edIsPositionRelativeToChunk()
	{
		return true;
	}

	/**
	 * return whether this item belongs to the chunk
	 */
	virtual bool edBelongToChunk()
	{
		return true;
	}

	/**
	 * Tell the item it was just created (doesn't trigger on clone nor load)
	 *
	 * The item will either be a new one, or deleting it was just undone
	 */
	virtual void edPostCreate() 
	{
		this->syncInit();
	}

	/**
	 * Return the binary data used by this item, if any.
	 *
	 * Used by terrain items to expose the terrain block data.
	 */
	virtual BinaryPtr edExportBinaryData() { return 0; }

	/**
	 * If the chunk item should be drawn
	 */
	virtual bool edShouldDraw();

	virtual bool edCheckMark(uint32 mark) { return true; }

	/**
	 * Always on minimum values this item can be moved by
	 */
	virtual Vector3 edMovementDeltaSnaps() { return Vector3( 0.f, 0.f, 0.f ); }
	/**
	 * Always on snap value for this item, in degrees
	 */
	virtual float edAngleSnaps() { return 0.f; }

	void recordMessage( BWMessageInfo * message );
	void deleteMessage( BWMessageInfo * message );
	
	static void hideAllOutside( bool hide );
	static bool hideAllOutside();
private:
	EditorChunkItem( const EditorChunkItem& );
	EditorChunkItem& operator=( const EditorChunkItem& );

	bool hasLoaded_;

	void doItemDeleted();
	void doItemRemoved();
	void doItemRestored();

	std::set< BWMessageInfo *> linkedMessages_;

protected:
	bool groupMember_;
	bool transient_;
	std::string groupName_;
	EditorGroup * pGroup_;
	static bool hideAllOutside_;
};

/**
 *	SpecialChunkItem is a type definition that is the application specific base
 *	class of ChunkItem. When making the client, it is defined as
 *	ClientChunkItem.
 */
typedef EditorChunkItem SpecialChunkItem;


/**
 *	This macro should be used in place of DECLARE_CHUNK_ITEM for the editor
 *	versions of chunk item types
 *
 *	@see DECLARE_CHUNK_ITEM
 */
#define DECLARE_EDITOR_CHUNK_ITEM( CLASS )						\
	DECLARE_CHUNK_ITEM( CLASS )									\
	virtual const char * edClassName() { return 6+#CLASS; }		\


#ifdef CODE_INLINE
#include "editor_chunk_item.ipp"
#endif

#endif // EDITOR_CHUNK_ITEM_HPP
