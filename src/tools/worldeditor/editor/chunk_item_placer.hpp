/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_ITEM_PLACER_HPP
#define CHUNK_ITEM_PLACER_HPP


#include "worldeditor/config.hpp"
#include "worldeditor/forward.hpp"
#include "chunk/chunk_item.hpp"
#include "gizmo/undoredo.hpp"


class ChunkItemExistenceOperation : public UndoRedo::Operation
{
public:
	ChunkItemExistenceOperation( ChunkItemPtr pItem, Chunk * pOldChunk ) :
		UndoRedo::Operation( 0 ),
		pItem_( pItem ),
		pOldChunk_( pOldChunk )
	{
		addChunk( pOldChunk_ );
		if( pItem_ )
			addChunk( pItem_->chunk() );
	}

protected:

	virtual void undo();

	virtual bool iseq( const UndoRedo::Operation & oth ) const
	{
		// these operations never replace each other
		return false;
	}

	ChunkItemPtr	pItem_;
	Chunk			* pOldChunk_;
};


class LinkerExistenceOperation : public ChunkItemExistenceOperation
{
public:
	LinkerExistenceOperation( ChunkItemPtr pItem, Chunk * pOldChunk ) :
		ChunkItemExistenceOperation( pItem, pOldChunk )	{}

protected:
	/*virtual*/ void undo();
};


class CloneNotifier
{
	static std::set<CloneNotifier*>* notifiers_;
public:
	CloneNotifier()
	{
		if( !notifiers_ )
			notifiers_ = new std::set<CloneNotifier*>;
		notifiers_->insert( this );
	}
	~CloneNotifier()
	{
		notifiers_->erase( this );
		if( notifiers_->empty() )
		{
			delete notifiers_;
			notifiers_ = NULL;
		}
	}
	virtual void begin() = 0;
	virtual void end() = 0;

	static void beginClone()
	{
		if( notifiers_ )
		{
			for( std::set<CloneNotifier*>::iterator iter = notifiers_->begin();
				iter != notifiers_->end(); ++iter )
			{
				(*iter)->begin();
			}
		}
	}
	static void endClone()
	{
		if( notifiers_ )
		{
			for( std::set<CloneNotifier*>::iterator iter = notifiers_->begin();
				iter != notifiers_->end(); ++iter )
			{
				(*iter)->end();
			}
		}
	}
	class Guard
	{
	public:
		Guard()
		{
			CloneNotifier::beginClone();
		}
		~Guard()
		{
			CloneNotifier::endClone();
		}
	};
};


#endif // CHUNK_ITEM_PLACER_HPP
