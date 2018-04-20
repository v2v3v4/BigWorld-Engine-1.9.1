/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef EDITOR_CHUNK_BINDING_HPP
#define EDITOR_CHUNK_BINDING_HPP


#include "worldeditor/config.hpp"
#include "worldeditor/forward.hpp"
#include "worldeditor/world/items/editor_chunk_substance.hpp"
#include "chunk/chunk_marker.hpp"
#include "resmgr/string_provider.hpp"


/** NOTE **
 *
 *	This class is not being utilised (and is not finished).
 *
 *	Binding is currently implemented as a property and is a one way attribute
 *	(i.e. only the origin item is awear of the binding)
 *
 */


/**
 *	This class implements two way bindings, where both the bindee and binder
 *	is awear of the binding.  A binding provides a way for the engine to 
 *	send messages between items.
 */
class EditorChunkBinding : public EditorChunkSubstance<ChunkBinding>
{
	DECLARE_EDITOR_CHUNK_ITEM( EditorChunkBinding )
public:
	EditorChunkBinding();
	~EditorChunkBinding();

	virtual void draw();

	bool load( DataSectionPtr pSection );

	virtual bool edSave( DataSectionPtr pSection );

	virtual const Matrix & edTransform();
	virtual bool edTransform( const Matrix & m, bool transient );

	virtual bool edEdit( class ChunkItemEditor & editor );
	virtual bool edCanDelete();
	virtual void edPreDelete();
	virtual void edPostClone( EditorChunkItem* srcItem );

	std::string edDescription() { return L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_BINDING/ED_DESCRIPTION"); }

	void calculateTransform(bool transient = false);

	static void fini();

private:
	EditorChunkBinding( const EditorChunkBinding& );
	EditorChunkBinding& operator=( const EditorChunkBinding& );

	virtual const char * sectName() const { return "binding"; }
	virtual const char * drawFlag() const { return "render/drawEntities"; }

	virtual ModelPtr reprModel() const;

	Matrix transform_;
};


#endif // EDITOR_CHUNK_BINDING_HPP
