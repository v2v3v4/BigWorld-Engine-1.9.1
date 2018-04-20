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
#include "chunk_attachment.hpp"

#include "chunk/chunk_space.hpp"
#include "chunk/chunk.hpp"

#include "moo/render_context.hpp"

#include "chunk/umbra_config.hpp"

#if UMBRA_ENABLE
#include <umbraModel.hpp>
#include <umbraObject.hpp>
#endif

DECLARE_DEBUG_COMPONENT2( "Duplo", 0 )

PROFILER_DECLARE( ChunkAttachment_tick, "ChunkAttachment Tick" );

// -----------------------------------------------------------------------------
// Section: ChunkAttachment
// -----------------------------------------------------------------------------


/**
 *	Constructor
 */
ChunkAttachment::ChunkAttachment() :
	ChunkDynamicEmbodiment( (WantFlags)(WANTS_DRAW | WANTS_TICK) ),
	needsSync_( false ),
	worldTransform_( Matrix::identity ),
	inited_( false )
{
}


/**
 *	Constructor
 */
ChunkAttachment::ChunkAttachment( PyAttachmentPtr pAttachment ) :
	ChunkDynamicEmbodiment( pAttachment.get(), (WantFlags)(WANTS_DRAW | WANTS_TICK) ),
	needsSync_( false ),
	worldTransform_( Matrix::identity ),
	inited_( true )
{
	BW_GUARD;
	IF_NOT_MF_ASSERT_DEV( pPyObject_ )
	{
		MF_EXIT( "attach to NULL object" );
	}

	this->pAttachment()->attach( this );
}

/**
 *	Destructor
 */
ChunkAttachment::~ChunkAttachment()
{
	BW_GUARD;
	if ( inited_ )
	{
		this->pAttachment()->detach();
	}
}


/**
 *	Chunk item tick method
 */
void ChunkAttachment::tick( float dTime )
{
	BW_GUARD_PROFILER( ChunkAttachment_tick );

	if ( !inited_ )
	{
		inited_ = true;
		this->pAttachment()->attach( this );
	}

	this->pAttachment()->tick( dTime );
#if UMBRA_ENABLE
	// Update umbra object
	if (!pUmbraModel_.exists())
	{
		// Set up our unit boundingbox for the attachment, we use a unit bounding box
		// and scale it using transforms since this is a dynamic object.
		pUmbraModel_ = UmbraModelProxy::getObbModel( Vector3(0,0,0), Vector3(1,1,1) );
		pUmbraObject_ = UmbraObjectProxy::get( pUmbraModel_ );
		pUmbraObject_->object()->setUserPointer( (void*)this );
	}
	
	if (pChunk_ != NULL && pUmbraObject_.hasObject())
	{
		// Get the object to cell transform
		Matrix m = Matrix::identity;
		m.preMultiply( worldTransform_ );

		BoundingBox bb;
		visibilityBox( bb );

		// Create the scale transform from the bounding box
		Vector3 scale(0,0,0);
		if (!bb.insideOut())
			scale = bb.maxBounds() - bb.minBounds();

		// Const number to see if the bounding box is too big for umbra.
		const float TOO_BIG_FOR_UMBRA = 100000.f;

		if (scale.x != 0.f && scale.y != 0.f && scale.z != 0.f &&
			scale.x < TOO_BIG_FOR_UMBRA && scale.y < TOO_BIG_FOR_UMBRA && scale.z < TOO_BIG_FOR_UMBRA)
		{
			// Set up our transform, the transform includes the scale of the bounding box
			Matrix m2;
			m2.setTranslate( bb.minBounds().x, bb.minBounds().y, bb.minBounds().z );
			m.preMultiply( m2 );

			m2.setScale( scale.x, scale.y, scale.z );
			m.preMultiply( m2 );
			pUmbraObject_->object()->setCell( pChunk_->getUmbraCell() );
			pUmbraObject_->object()->setObjectToCellMatrix( (Umbra::Matrix4x4&)m );
		}
		else
		{			
			pUmbraObject_->object()->setCell( NULL );
		}
	}
	else if (pUmbraObject_.hasObject())
	{
		pUmbraObject_->object()->setCell( NULL );
	}
#endif
}


/**
 *	Chunk item draw method
 */
void ChunkAttachment::draw()
{
	BW_GUARD;
	// check that it fits in the BB first
	BoundingBox bb = BoundingBox::s_insideOut_;
	this->pAttachment()->boundingBoxAcc( bb );
	BoundingBox maxbb = BoundingBox::s_insideOut_;
	this->pAttachment()->visibilityBoxAcc( maxbb );

	if (bb != BoundingBox::s_insideOut_ && maxbb != BoundingBox::s_insideOut_)
	{
		Matrix spaceTrans;
		if (pSpace_)
			spaceTrans.multiply( worldTransform_, pSpace_->commonInverse() );
		else	// some subclasses do not call our enter and leave space method
			spaceTrans = worldTransform_;

		Matrix clipTrans;
		clipTrans.multiply( spaceTrans, Moo::rc().viewProjection() );
		maxbb.calculateOutcode( clipTrans );

		if (!maxbb.combinedOutcode())
		{
			float distance = 
				(spaceTrans.applyToOrigin() -
				Moo::rc().invView().applyToOrigin()).length();
			this->pAttachment()->draw( spaceTrans, distance );
#ifdef EDITOR_ENABLED
			drawBoundingBoxes( bb, maxbb, spaceTrans );
#endif
		}
	}
}


/**
 *	Chunk item toss method
 */
void ChunkAttachment::toss( Chunk * pChunk )
{
	BW_GUARD;
	this->ChunkDynamicEmbodiment::toss( pChunk );

	this->pAttachment()->tossed(
		pChunk_ == NULL || pChunk_->isOutsideChunk() );
}


/**
 *	Enter the given space. Transient is set to true if switching spaces.
 */
void ChunkAttachment::enterSpace( ChunkSpacePtr pSpace, bool transient )
{
	BW_GUARD;
	this->ChunkDynamicEmbodiment::enterSpace( pSpace, transient );
	needsSync_ = false;

	if (!transient)
		this->pAttachment()->enterWorld();

	BoundingBox bb = BoundingBox::s_insideOut_;
	boundingBox(bb);
}

/**
 *	Leave the current space.
 *	Transient is set to true if switching spaces.
 */
void ChunkAttachment::leaveSpace( bool transient )
{
	BW_GUARD;
	if (!transient)
		this->pAttachment()->leaveWorld();

	this->ChunkDynamicEmbodiment::leaveSpace( transient );
}


/**
 *	This method is called before tick to allow us a chance to move
 *	(which is not permitted during tick since we might change to
 *	a different chunk)
 */
void ChunkAttachment::move( float dTime )
{
	BW_GUARD;
	// promote our transforms if it has such aspirations but is hesitant
	if (needsSync_)
	{
		needsSync_ = false;
		this->sync();
	}

	// let the attachment move about
	this->pAttachment()->move( dTime );

	// and let our base class do its stuff
	this->ChunkDynamicEmbodiment::move( dTime );
}


/**
 *	This method retrieves a representative bounding box (in local coords)
 *	for this embodiment. It is not a strict bounding box, but it's a good
 *	one for displaying or intersecting with or other similar operations.
 */
void ChunkAttachment::boundingBox( BoundingBox & bb ) const
{
	BW_GUARD;
	bb = BoundingBox::s_insideOut_;
	this->pAttachment()->boundingBoxAcc( bb, true );
}


/**
 *	This method retrieves a representative visibility box (in local coords)
 *	for this embodiment. It is used to determine the visibility of the
 *  attachment.
 */
void ChunkAttachment::visibilityBox( BoundingBox & vbb ) const
{
	BW_GUARD;
	vbb = BoundingBox::s_insideOut_;
	this->pAttachment()->visibilityBoxAcc( vbb );
}


/**
 *	This method sets the matrix
 */
bool ChunkAttachment::setMatrix( const Matrix & worldTransform )
{
	BW_GUARD;
	worldTransform_ = worldTransform;

	if (!pSpace_) return true;

	if (!pSpace_->ticking())
		this->sync();
	else
		needsSync_ = true;

	return true;
}


/**
 *	Static method to convert from a PyObject to a ChunkEmbodiment
 */
int ChunkAttachment::convert( PyObject * pObj, ChunkEmbodimentPtr & pNew,
	const char * varName )
{
	BW_GUARD;
	if (!PyAttachment::Check( pObj )) return 0;

	PyAttachment * pAttachment = (PyAttachment*)pObj;
	if (pAttachment->isAttached())
	{
		PyErr_Format( PyExc_TypeError, "%s must be set to an Attachment "
			"that is not attached elsewhere", varName );
		return -1;
	}

	pNew = new ChunkAttachment( pAttachment );
	return 0;
}


bool ChunkAttachment::addYBounds( BoundingBox& bb ) const
{
	BW_GUARD;
	Matrix spaceTrans;
	if (pSpace_)
	{
		spaceTrans.multiply( worldTransform_, pSpace_->commonInverse() );
	}
	else	
	{
		// some subclasses do not call our enter and leave space method
		spaceTrans = worldTransform_;
	}
	
	BoundingBox vbb = BoundingBox::s_insideOut_;
	this->pAttachment()->visibilityBoxAcc(vbb);
	if (!vbb.insideOut())
	{
		vbb.transformBy(spaceTrans);
		bb.addYBounds(vbb.minBounds().y);
		bb.addYBounds(vbb.maxBounds().y);
	}
	
	return true;
}


/// registerer for our type of ChunkEmbodiment
static ChunkEmbodimentRegisterer<ChunkAttachment> registerer;
int ChunkAttachment_token;

// chunk_attachment.cpp
