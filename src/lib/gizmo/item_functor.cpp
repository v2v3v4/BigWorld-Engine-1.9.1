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
#include "item_functor.hpp"
//#include "item_view.hpp"
#include "general_properties.hpp"

//#include "big_bang.hpp"
#include "tool_manager.hpp"
#include "appmgr/options.hpp"
#include "undoredo.hpp"
#include "snap_provider.hpp"
//#include "../common/snaps.hpp"
//#include "chunk/chunk_manager.hpp"
//#include "chunk/chunk_space.hpp"
//#include "chunk/chunk.hpp"
//#include "chunks/editor_chunk.hpp"
#include "pyscript/py_data_section.hpp"
#include "resmgr/bwresource.hpp"
#include "cstdmf/debug.hpp"
#include "current_general_properties.hpp"

DECLARE_DEBUG_COMPONENT2( "Editor", 0 )



// -----------------------------------------------------------------------------
// Section: MatrixMover
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( MatrixMover )

PY_BEGIN_METHODS( MatrixMover )
	PY_METHOD( setUndoName )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( MatrixMover )
PY_END_ATTRIBUTES()

PY_FACTORY( MatrixMover, Functor )


int MatrixMover::moving_ = 0;
/**
 *	Constructor.
 */
MatrixMover::MatrixMover( MatrixProxyPtr pMatrix, PyTypePlus * pType ) :
	ToolFunctor( pType ),
	lastLocatorPos_( Vector3::zero() ),
	totalLocatorOffset_( Vector3::zero() ),
	gotInitialLocatorPos_( false ),
	snap_( true ),
	rotate_( false ),
	snapMode_( SnapProvider::instance()->snapMode() )
{
	++moving_;
	std::vector<GenPositionProperty*> props = CurrentPositionProperties::properties();
	for (std::vector<GenPositionProperty*>::iterator i = props.begin(); i != props.end(); ++i)
	{
		(*i)->pMatrix()->recordState();
	}
}

MatrixMover::MatrixMover( MatrixProxyPtr pMatrix, bool snap, bool rotate, PyTypePlus * pType ) :
	ToolFunctor( pType ),
	lastLocatorPos_( Vector3::zero() ),
	totalLocatorOffset_( Vector3::zero() ),
	gotInitialLocatorPos_( false ),
	snap_( snap ),
	rotate_( rotate ),
	snapMode_( SnapProvider::instance()->snapMode() )
{
	++moving_;
	std::vector<GenPositionProperty*> props = CurrentPositionProperties::properties();
	for (std::vector<GenPositionProperty*>::iterator i = props.begin(); i != props.end(); ++i)
	{
		(*i)->pMatrix()->recordState();
	}
}

MatrixMover::~MatrixMover()
{
	--moving_;
}

PyObject* MatrixMover::py_setUndoName( PyObject* args )
{
	// parse arguments
	char* str;
	if (!PyArg_ParseTuple( args, "s", &str ))	{
		PyErr_SetString( PyExc_TypeError, "setUndoName() "
			"expects a string argument" );
		return NULL;
	}

	undoName_ = str;

	Py_Return;
}


/**
 *	Update method
 */
void MatrixMover::update( float dTime, Tool& tool )
{
	// see if we want to commit this action
	if (!InputDevices::isKeyDown( KeyEvent::KEY_LEFTMOUSE ) ||
		snapMode_ != SnapProvider::instance()->snapMode() )
	{
		std::vector<GenPositionProperty*> props = CurrentPositionProperties::properties();

		bool addUndoBarrier = (props.size() > 1) || (undoName_ != "");

		bool success = true;
		for (std::vector<GenPositionProperty*>::iterator i = props.begin(); i != props.end(); ++i)
		{
			MatrixProxyPtr pMatrix = (*i)->pMatrix();
			if (pMatrix->hasChanged())
			{
				// set its transform permanently
				if ( !pMatrix->commitState( false, !addUndoBarrier ) )
				{
					success = false;
				}
			}
			else
			{
				pMatrix->commitState( true );
			}
		}

		if (addUndoBarrier)
		{
			if (undoName_ != "")
				UndoRedo::instance().barrier( undoName_, false );
			else
				UndoRedo::instance().barrier( "Move group", true );

			if ( !success )
			{
				UndoRedo::instance().undo();
			}
		}

		// and this tool's job is over
		ToolManager::instance().popTool();
		return;
	}

	// figure out movement
	if (tool.locator())
	{
		std::vector<GenPositionProperty*> props = CurrentPositionProperties::properties();

		Vector3 locatorPos = tool.locator()->transform().applyToOrigin();

		if (!gotInitialLocatorPos_)
		{
			lastLocatorPos_ = tool.locator()->transform().applyToOrigin();
			gotInitialLocatorPos_ = true;

			if (props.size() == 1)
			{
				Vector3 objPos = props[0]->pMatrix()->get().applyToOrigin();
				Vector3 clipPos = Moo::rc().viewProjection().applyPoint( objPos );
				clipPos.x = ( clipPos.x + 1 ) / 2 * Moo::rc().screenWidth();
				clipPos.y = ( 1 - clipPos.y ) / 2 * Moo::rc().screenHeight();

				POINT pt;
				pt.x = LONG( clipPos.x );
				pt.y = LONG( clipPos.y );
				::ClientToScreen( Moo::rc().windowHandle(), &pt );
				::SetCursorPos( pt.x, pt.y );

				if (Moo::rc().device()) 
				{
					Moo::rc().device()->SetCursorPosition( pt.x, pt.y, 0 );
				}

				lastLocatorPos_ = locatorPos = objPos;
			}
		}

		totalLocatorOffset_ += locatorPos - lastLocatorPos_;
		lastLocatorPos_ = locatorPos;


		for (std::vector<GenPositionProperty*>::iterator i = props.begin(); i != props.end(); ++i)
		{
			MatrixProxyPtr pMatrix = (*i)->pMatrix();

			Matrix oldMatrix;
			pMatrix->getMatrix( oldMatrix );

			// reset the last change we made
			pMatrix->commitState( true );

			Matrix m;
			pMatrix->getMatrix( m );

			Vector3 delta = totalLocatorOffset_;

			if( snap_ )
				SnapProvider::instance()->snapPositionDelta( delta );

			Vector3 newPos = m.applyToOrigin() + delta;

            bool snapPosOK = true;
			if( snap_ )
				snapPosOK = SnapProvider::instance()->snapPosition( newPos );

            if( rotate_ && snapPosOK )
			{
				Vector3 normalOfSnap = SnapProvider::instance()->snapNormal( newPos );
				Vector3 yAxis( 0, 1, 0 );
				yAxis = m.applyVector( yAxis );

				Vector3 binormal = yAxis.crossProduct( normalOfSnap );

				normalOfSnap.normalise();
				yAxis.normalise();
				binormal.normalise();

				float angle = acosf( Math::clamp(-1.0f, yAxis.dotProduct( normalOfSnap ), +1.0f) );

				Quaternion q( binormal.x * sinf( angle / 2.f ),
					binormal.y * sinf( angle / 2.f ),
					binormal.z * sinf( angle / 2.f ),
					cosf( angle / 2.f ) );

				q.normalise();

				Matrix rotation;
				rotation.setRotate( q );

				m.postMultiply( rotation );
			}

            if (snapPosOK)
            {
			    m.translation( newPos );

			    Matrix worldToLocal;
			    pMatrix->getMatrixContextInverse( worldToLocal );

			    m.postMultiply( worldToLocal );

			    pMatrix->setMatrix( m );
            }
            else
            {
                // snapping the position failed, revert back to the previous
                // good matrix:
			    Matrix worldToLocal;
			    pMatrix->getMatrixContextInverse( worldToLocal );
			    oldMatrix.postMultiply( worldToLocal );
                pMatrix->setMatrix( oldMatrix );
            }
		}

	}
}


/**
 *	Key event method
 */
bool MatrixMover::handleKeyEvent( const KeyEvent & event, Tool& tool )
{
	if (event.type() != KeyEvent::KEY_DOWN) return false;

	if (event.key() == KeyEvent::KEY_ESCAPE)
	{
		// Set the items back to their original states
		std::vector<GenPositionProperty*> props = CurrentPositionProperties::properties();
		for (std::vector<GenPositionProperty*>::iterator i = props.begin(); i != props.end(); ++i)
		{
			(*i)->pMatrix()->commitState( true );
		}

		// and we're that's it from us
		ToolManager::instance().popTool();
		return true;
	}

	return false;
}


/**
 *	Factory method
 */
PyObject * MatrixMover::pyNew( PyObject * args )
{

	if (CurrentPositionProperties::properties().empty())
	{
		PyErr_Format( PyExc_ValueError, "MatrixMover() "
			"No current editor" );
		return NULL;
	}

	return new MatrixMover( NULL );
}



// -----------------------------------------------------------------------------
// Section: MatrixScaler
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( MatrixScaler )

PY_BEGIN_METHODS( MatrixScaler )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( MatrixScaler )
PY_END_ATTRIBUTES()

//PY_FACTORY( MatrixScaler, Functor )


/**
 *	Constructor.
 */
MatrixScaler::MatrixScaler( MatrixProxyPtr pMatrix, float scaleSpeedFactor,
		   FloatProxyPtr scaleX, FloatProxyPtr scaleY, FloatProxyPtr scaleZ,
		   PyTypePlus * pType ) :
	ToolFunctor( pType ),
	pMatrix_( pMatrix ),
	scaleSpeedFactor_(scaleSpeedFactor),
	grabOffset_( 0, 0, 0 ),
	grabOffsetSet_( false ),
	scaleX_( scaleX ),
	scaleY_( scaleY ),
	scaleZ_( scaleZ )
{
	pMatrix_->recordState();
	pMatrix_->getMatrix( initialMatrix_, false );

	initialScale_.set( initialMatrix_[0].length(),
		initialMatrix_[1].length(),
		initialMatrix_[2].length() );

	initialMatrix_[0] /= initialScale_.x;
	initialMatrix_[1] /= initialScale_.y;
	initialMatrix_[2] /= initialScale_.z;

	invInitialMatrix_.invert( initialMatrix_ );
}



/**
 *	Update method
 */
void MatrixScaler::update( float dTime, Tool& tool )
{
	// see if we want to commit this action
	if (!InputDevices::isKeyDown( KeyEvent::KEY_LEFTMOUSE ))
	{
		// set its transform permanently
		pMatrix_->commitState();

		// and this tool's job is over
		ToolManager::instance().popTool();
		return;
	}

	// figure out movement
	if (tool.locator())
	{
		Matrix localPosn;
		pMatrix_->getMatrixContextInverse( localPosn );
		localPosn.preMultiply( tool.locator()->transform() );

		if (!grabOffsetSet_)
		{
			grabOffsetSet_ = true;
			grabOffset_ = localPosn.applyToOrigin();
		}

		Vector3 scale = localPosn.applyToOrigin() - grabOffset_;
		scale *= scaleSpeedFactor_;

		scale = invInitialMatrix_.applyVector( scale );

		Vector3 direction = invInitialMatrix_.applyVector( tool.locator()->direction() );
		direction.normalise();

		scale = scale + initialScale_;

		const float scaleEpsilon = 0.01f;
		scale.x = max( scale.x, scaleEpsilon );
		scale.y = max( scale.y, scaleEpsilon );
		scale.z = max( scale.z, scaleEpsilon );

		if (scaleX_)
		{
			scaleX_->set( scale.x, false );
		}
		if (scaleY_)
		{
			scaleY_->set( scale.y, false );
		}
		if (scaleZ_)
		{
			scaleZ_->set( scale.z, false );
		}

		Matrix curPose;
		curPose.setScale( scale );
		curPose.postMultiply( initialMatrix_ );

		pMatrix_->setMatrix( curPose );
	}
}


/**
 *	Key event method
 */
bool MatrixScaler::handleKeyEvent( const KeyEvent & event, Tool& tool )
{
	if (event.type() != KeyEvent::KEY_DOWN) return false;

	if (event.key() == KeyEvent::KEY_ESCAPE)
	{
		// set the item back to it's original pose
		pMatrix_->commitState( true );

		// and we're that's it from us
		ToolManager::instance().popTool();
		return true;
	}

	return false;
}


/**
 *	Factory method
 */
/*
PyObject * MatrixScaler::pyNew( PyObject * args )
{
	PyObject * pPyRev = NULL;
	if (!PyArg_ParseTuple( args, "O", &pPyRev ) ||
		!ChunkItemRevealer::Check( pPyRev ))
	{
		PyErr_SetString( PyExc_TypeError, "MatrixScaler() "
			"expects a ChunkItemRevealer" );
		return NULL;
	}

	ChunkItemRevealer* pRevealer = static_cast<ChunkItemRevealer*>( pPyRev );

	ChunkItemRevealer::ChunkItems items;
	pRevealer->reveal( items );
	if (items.size() != 1)
	{
		PyErr_Format( PyExc_ValueError, "MatrixScaler() "
			"Revealer must reveal exactly one item, not %d", items.size() );
		return NULL;
	}

	ChunkItemPtr pItem = items[0];
	if (pItem->chunk() == NULL)
	{
		PyErr_Format( PyExc_ValueError, "MatrixScaler() "
			"Item to move is not in the scene" );
		return NULL;
	}

	return new MatrixScaler( MatrixProxy::getChunkItemDefault( pItem ) );
}
*/


// -----------------------------------------------------------------------------
// Section: PropertyScaler
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( PropertyScaler )

PY_BEGIN_METHODS( PropertyScaler )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( PropertyScaler )
PY_END_ATTRIBUTES()


/**
 *	Constructor.
 */
PropertyScaler::PropertyScaler( float scaleSpeedFactor,
			FloatProxyPtr scaleX, FloatProxyPtr scaleY, FloatProxyPtr scaleZ,
			PyTypePlus * pType ) :
	ToolFunctor( pType ),
	scaleX_( scaleX ),
	scaleY_( scaleY ),
	scaleZ_( scaleZ ),
	scaleSpeedFactor_(scaleSpeedFactor)
{
	std::vector<GenScaleProperty*> props = CurrentScaleProperties::properties();
	for (std::vector<GenScaleProperty*>::iterator i = props.begin(); i != props.end(); ++i)
	{
		PropInfo pi;
		pi.grabOffset_ = Vector3( 0, 0, 0 );
		pi.grabOffsetSet_ = false;

		pi.prop_ = *i;
		pi.prop_->pMatrix()->recordState();
		pi.prop_->pMatrix()->getMatrix( pi.initialMatrix_, false );

		pi.prop_->pMatrix()->getMatrixContext( pi.chunkMatrix_ );
		pi.chunkMatrix_[0].normalise();
		pi.chunkMatrix_[1].normalise();
		pi.chunkMatrix_[2].normalise();
		pi.chunkMatrix_[3].setZero();

		pi.initialScale_.set( 1.f, 1.f, 1.f );

		Matrix m = pi.initialMatrix_;
		m[0].normalise();
		m[1].normalise();
		m[2].normalise();

		pi.invInitialMatrix_.invert( m );

		props_.push_back(pi);
	}
}

/**
 *	Update method
 */
void PropertyScaler::update( float dTime, Tool& tool )
{
	// see if we want to commit this action
	if (!InputDevices::isKeyDown( KeyEvent::KEY_LEFTMOUSE ))
	{
		// set its transform permanently
		std::avector<PropInfo>::iterator i = props_.begin();
		for (; i != props_.end(); ++i)
			(*i).prop_->pMatrix()->commitState( false, false );

		UndoRedo::instance().barrier( "Scale", false );

		// and this tool's job is over
		ToolManager::instance().popTool();
		return;
	}

	// figure out movement
	if (tool.locator())
	{
		std::avector<PropInfo>::iterator i = props_.begin();
		for (; i != props_.end(); ++i)
		{
			PropInfo& pi = *i;

			if (!pi.grabOffsetSet_)
			{
				pi.grabOffsetSet_ = true;
				pi.grabOffset_ = tool.locator()->transform().applyToOrigin();
			}

			Vector3 scaleWorld = tool.locator()->transform().applyToOrigin() - pi.grabOffset_;
			scaleWorld *= scaleSpeedFactor_;

			Matrix curPose = pi.initialMatrix_;
			curPose.postMultiply( pi.chunkMatrix_ );
			curPose.translation( Vector3( 0.f, 0.f, 0.f ) );

			for( int i = 0; i < 3; ++i )
			{
				Vector3 axis = tool.locator()->transform().applyToUnitAxisVector( i );
				axis.normalise();
				float scale = scaleWorld.dotProduct( axis );
				if( scale < 0.0 )
					scale = - 1 / ( scale - 1 ) - 1;

				const float scaleEpsilon = 0.01f;

				if (i == 0 && scaleX_)
				{
					scaleX_->set( scale + 1, true );
				}
				else if (i == 1 && scaleY_)
				{
					scaleY_->set( scale + 1, true );
				}
				else if (i == 2 && scaleZ_)
				{
					scaleZ_->set( scale + 1, true );
				}


				Vector3 scaleWorld = axis;// weird

				Vector3 xAxis( 1.f, 0.f, 0.f );
				Vector3 binormal = xAxis.crossProduct( scaleWorld );

				if( fabs( xAxis.dotProduct( scaleWorld ) ) < 0.999f )
					binormal = xAxis.crossProduct( scaleWorld );
				else
					binormal = Vector3( 0, 0, 1 ).crossProduct( scaleWorld );
				binormal.normalise();
				float angle = acosf( Math::clamp(-1.0f, scaleWorld[0], +1.0f) );
				Quaternion q( binormal.x * sinf( angle / 2.f ),
					binormal.y * sinf( angle / 2.f ),
					binormal.z * sinf( angle / 2.f ),
					cosf( angle / 2.f ) );
				q.normalise();

				Matrix rotationMatrix;
				rotationMatrix.setRotate( q );
				rotationMatrix.invert();
				curPose.postMultiply( rotationMatrix );

				Matrix scaleMatrix;
				scaleMatrix.setScale( Vector3( scale + 1, 1.f, 1.f ) );
				curPose.postMultiply( scaleMatrix );

				rotationMatrix.invert();
				curPose.postMultiply( rotationMatrix );
			}

			Matrix invChunkMatrix = pi.chunkMatrix_;
			invChunkMatrix.invert();
			curPose.postMultiply( invChunkMatrix );

			curPose.translation( pi.initialMatrix_[3] );

			pi.prop_->pMatrix()->setMatrix( curPose );
		}
	}
}


/**
 *	Key event method
 */
bool PropertyScaler::handleKeyEvent( const KeyEvent & event, Tool& tool )
{
	if (event.type() != KeyEvent::KEY_DOWN) return false;

	if (event.key() == KeyEvent::KEY_ESCAPE)
	{
		// set the item back to it's original pose
		std::avector<PropInfo>::iterator i = props_.begin();
		for (; i != props_.end(); ++i)
			(*i).prop_->pMatrix()->commitState( true );

		// and we're that's it from us
		ToolManager::instance().popTool();
		return true;
	}

	return false;
}

// -----------------------------------------------------------------------------
// Section: MatrixRotator
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( MatrixRotator )

PY_BEGIN_METHODS( MatrixRotator )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( MatrixRotator )
PY_END_ATTRIBUTES()

//PY_FACTORY( MatrixRotator, Functor )



/**
 *	Constructor.
 *  (pMatrix is there only for the factory)
 */
MatrixRotator::MatrixRotator( MatrixProxyPtr pMatrix, PyTypePlus * pType ) :
	ToolFunctor( pType ),
	grabOffset_( 0, 0, 0 ),
	grabOffsetSet_( false ),
	centrePoint_(0.f, 0.f, 0.f)
{
	std::vector<GenRotationProperty*> props = CurrentRotationProperties::properties();
	for (std::vector<GenRotationProperty*>::iterator i = props.begin(); i != props.end(); ++i)
	{
		MatrixProxyPtr matrixProxy = (*i)->pMatrix();
		Matrix temp;
		matrixProxy->recordState();
		matrixProxy->getMatrix( temp, true );
		initialMatrixes_.push_back(temp);
	}
}


/**
 *	Update method
 */
void MatrixRotator::update( float dTime, Tool& tool )
{
	// see if we want to commit this action
	if (!InputDevices::isKeyDown( KeyEvent::KEY_LEFTMOUSE ))
	{
		// set its transform permanently
		std::vector<GenRotationProperty*> props = CurrentRotationProperties::properties();

		bool addUndoBarrier = props.size() > 1;

		bool success = true;
		for (std::vector<GenRotationProperty*>::iterator i = props.begin(); i != props.end(); ++i)
		{
			MatrixProxyPtr pMatrix = (*i)->pMatrix();
			if (pMatrix->hasChanged())
			{
				// set its transform permanently
				if ( !pMatrix->commitState( false, !addUndoBarrier ) )
				{
					success = false;
				}
			}
			else
			{
				pMatrix->commitState( true );
			}
		}

		if (addUndoBarrier)
		{
			UndoRedo::instance().barrier( "Rotate", true );

			if ( !success )
			{
				UndoRedo::instance().undo();
			}
		}

		// and this tool's job is over
		ToolManager::instance().popTool();
		return;
	}


	// figure out rotation
	if (tool.locator())
	{
		Matrix localPosn = tool.locator()->transform();

		if (!grabOffsetSet_)
		{
			grabOffsetSet_ = true;
			centrePoint_ = CurrentRotationProperties::averageOrigin();
			grabOffset_ = localPosn.applyToOrigin() - centrePoint_;
			grabOffset_.normalise();

		}

		Vector3 offset = localPosn.applyToOrigin() - centrePoint_;
		offset.normalise();

		Vector3 n;
		float a = acosf( Math::clamp(-1.0f, offset.dotProduct( grabOffset_ ), +1.0f) );

		float snapAmount = SnapProvider::instance()->angleSnapAmount() / 180 * MATH_PI;

		if( snapAmount != 0.0f )
		{
			float angle = int( a / snapAmount ) * snapAmount;
			if( a - angle >= snapAmount / 2 )
				a = angle + snapAmount;
			else
				a = angle;
		}

		{
			n.crossProduct( grabOffset_, offset );
			Quaternion q;
			q.fromAngleAxis( a, n );

			Matrix rotMat;
			rotMat.setRotate( q );

			// rotate around the gizmo position
			const Vector3 invCentrePoint(-centrePoint_);
			Matrix invCentrePointMatrix;
			invCentrePointMatrix.setTranslate(invCentrePoint);

			rotMat.translation(centrePoint_);
			rotMat.multiply(invCentrePointMatrix, rotMat);

			std::vector<GenRotationProperty*> props = CurrentRotationProperties::properties();
			int j;
			std::vector<GenRotationProperty*>::iterator i;
			for ( i = props.begin(), j = 0;
				i != props.end();
				++i, ++j)
			{
				Matrix newMatrix;
				newMatrix.multiply(initialMatrixes_[j], rotMat);

				Vector3 pos = newMatrix.applyToOrigin();
				Vector3 newPos = pos;
				if( SnapProvider::instance()->snapMode() != SnapProvider::SNAPMODE_OBSTACLE )
					SnapProvider::instance()->snapPosition( newPos );
				Matrix mover;
				mover.setTranslate( newPos - pos );
				newMatrix.postMultiply( mover );

				Matrix worldToChunk;
				(*i)->pMatrix()->getMatrixContextInverse( worldToChunk );
				newMatrix.multiply(newMatrix, worldToChunk);

				(*i)->pMatrix()->setMatrix( newMatrix );
			}
		}
	}
}


/**
 *	Key event method
 */
bool MatrixRotator::handleKeyEvent( const KeyEvent & event, Tool& tool )
{
	if (event.type() != KeyEvent::KEY_DOWN) return false;

	if (event.key() == KeyEvent::KEY_ESCAPE)
	{
		// set the item back to it's original pose
		std::vector<GenRotationProperty*> props = CurrentRotationProperties::properties();
		for (std::vector<GenRotationProperty*>::iterator i = props.begin(); i != props.end(); ++i)
		{
			(*i)->pMatrix()->commitState( true );
		}

		// and we're that's it from us
		ToolManager::instance().popTool();
		return true;
	}

	return false;
}


/**
 *	Factory method
 */
/*
PyObject * MatrixRotator::pyNew( PyObject * args )
{
	PyObject * pPyRev = NULL;
	if (!PyArg_ParseTuple( args, "O", &pPyRev ) ||
		!ChunkItemRevealer::Check( pPyRev ))
	{
		PyErr_SetString( PyExc_TypeError, "MatrixRotator() "
			"expects a ChunkItemRevealer" );
		return NULL;
	}

	ChunkItemRevealer* pRevealer = static_cast<ChunkItemRevealer*>( pPyRev );

	ChunkItemRevealer::ChunkItems items;
	pRevealer->reveal( items );
	if (items.size() != 1)
	{
		PyErr_Format( PyExc_ValueError, "MatrixRotator() "
			"Revealer must reveal exactly one item, not %d", items.size() );
		return NULL;
	}

	ChunkItemPtr pItem = items[0];
	if (pItem->chunk() == NULL)
	{
		PyErr_Format( PyExc_ValueError, "MatrixRotator() "
			"Item to move is not in the scene" );
		return NULL;
	}

	return new MatrixRotator( MatrixProxy::getChunkItemDefault( pItem ) );
}
*/


// -----------------------------------------------------------------------------
// Section: DynamicFloatDevice
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( DynamicFloatDevice )

PY_BEGIN_METHODS( DynamicFloatDevice )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( DynamicFloatDevice )
PY_END_ATTRIBUTES()

/**
 *	Constructor.
 */
DynamicFloatDevice::DynamicFloatDevice( MatrixProxyPtr pCenter,
				   FloatProxyPtr pFloat,
				   float adjFactor,
				   PyTypePlus * pType ) :
	ToolFunctor( pType ),
	pCenter_( pCenter ),
	pFloat_( pFloat ),
	grabOffset_( 0, 0, 0 ),
	grabOffsetSet_( false ),
	adjFactor_( adjFactor )
{
	initialFloat_ = pFloat->get();
	pCenter->getMatrix( initialCenter_, true );
}



/**
 *	Update method
 */
void DynamicFloatDevice::update( float dTime, Tool& tool )
{
	// see if we want to commit this action
	if (!InputDevices::isKeyDown( KeyEvent::KEY_LEFTMOUSE ))
	{
		// set its value permanently
		float finalValue = pFloat_->get();

		pFloat_->set( initialFloat_, true );
		pFloat_->set( finalValue, false );

		if (UndoRedo::instance().barrierNeeded())
			UndoRedo::instance().barrier( "Scale", false );

		// and this tool's job is over
		ToolManager::instance().popTool();
		return;
	}

	// figure out radius
	if (tool.locator())
	{
		// Raymond, for bug 4734, the scale function has been changed
		// Now it will be smooth near 1:1 and quick when far away from 1:1
		// Also it meets 0 in the middle of the RadiusGizmo
		if (!grabOffsetSet_)
		{
			grabOffsetSet_ = true;
			grabOffset_ = tool.locator()->transform().applyToOrigin() - initialCenter_.applyToOrigin();

			Vector4 v4Locator( tool.locator()->transform().applyToOrigin(), 1. );

			Moo::rc().viewProjection().applyPoint( v4Locator, v4Locator );
			grabOffset_[0] = v4Locator[0] / v4Locator[3];
			grabOffset_[1] = v4Locator[1] / v4Locator[3];
			grabOffset_[2] = v4Locator[2] / v4Locator[3];

			Vector4 v4InitialCenter( initialCenter_.applyToOrigin(), 1 );

			Moo::rc().viewProjection().applyPoint( v4InitialCenter, v4InitialCenter );
			grabOffset_[0] -= v4InitialCenter[0] / v4InitialCenter[3];
			grabOffset_[1] -= v4InitialCenter[1] / v4InitialCenter[3];
			grabOffset_[2] -= v4InitialCenter[2] / v4InitialCenter[3];
			grabOffset_[2] = 0;
		}

		Vector3 offset = (tool.locator()->transform().applyToOrigin() - initialCenter_.applyToOrigin());

		Vector4 v4Locator( tool.locator()->transform().applyToOrigin(), 1. );

		Moo::rc().viewProjection().applyPoint( v4Locator, v4Locator );
		offset[0] = v4Locator[0] / v4Locator[3];
		offset[1] = v4Locator[1] / v4Locator[3];
		offset[2] = v4Locator[2] / v4Locator[3];

		Vector4 v4InitialCenter( initialCenter_.applyToOrigin(), 1 );

		Moo::rc().viewProjection().applyPoint( v4InitialCenter, v4InitialCenter );
		offset[0] -= v4InitialCenter[0] / v4InitialCenter[3];
		offset[1] -= v4InitialCenter[1] / v4InitialCenter[3];
		offset[2] -= v4InitialCenter[2] / v4InitialCenter[3];
		offset[2] = 0;

		float ratio = offset.length() / grabOffset_.length();
		if (initialFloat_ == 0.0f) // Bug 5153 fix: There was a problem if the initial radius was 0.0.
		{
			pFloat_->set( initialFloat_ + ((offset.length() - grabOffset_.length()) * adjFactor_), true );
		}
		else if( ratio < 1.0f )
		{
			pFloat_->set( initialFloat_ * ( 1 - ( ratio - 1 ) * ( ratio - 1 ) ), true );
		}
		else
		{
			pFloat_->set( initialFloat_ * ratio * ratio, true );
		}
//		pFloat_->set( initialFloat_ + ((offset.length() - grabOffset_.length()) * adjFactor_), true );
		// }		
	}
}


/**
 *	Key event method
 */
bool DynamicFloatDevice::handleKeyEvent( const KeyEvent & event, Tool& tool )
{
	if (event.type() != KeyEvent::KEY_DOWN) return false;

	if (event.key() == KeyEvent::KEY_ESCAPE)
	{
		// set the item back to it's original pose
		pFloat_->set( initialFloat_, true );

		// and we're that's it from us
		ToolManager::instance().popTool();
		return true;
	}

	return false;
}



// -----------------------------------------------------------------------------
// Section: WheelRotator
// -----------------------------------------------------------------------------


PY_TYPEOBJECT( WheelRotator )

PY_BEGIN_METHODS( WheelRotator )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( WheelRotator )
PY_END_ATTRIBUTES()

PY_FACTORY( WheelRotator, Functor )

WheelRotator::WheelRotator( PyTypePlus * pType ) :
	timeSinceInput_(0.0f),
	rotAmount_(0.0f)
{
	std::vector<GenRotationProperty*> props = CurrentRotationProperties::properties();
	for (std::vector<GenRotationProperty*>::iterator i = props.begin(); i != props.end(); ++i)
	{
		MatrixProxyPtr matrixProxy = (*i)->pMatrix();
		Matrix temp;
		matrixProxy->recordState();
		matrixProxy->getMatrix( temp, true );
		initialMatrixes_.push_back(temp);
	}

	centrePoint_ = CurrentPositionProperties::averageOrigin();
}

void WheelRotator::update( float dTime, Tool& tool )
{
	if (CurrentRotationProperties::properties().size() == 0)
	{
		ToolManager::instance().popTool();
		return;
	}

	timeSinceInput_ += dTime;

	// Automatically commit after 750ms of no input
	if (timeSinceInput_ > 0.75f)
	{
		commitChanges();

		ToolManager::instance().popTool();
	}
}

void WheelRotator::rotateBy( float degs, bool useLocalYaxis )
{
	float snapAmount = SnapProvider::instance()->angleSnapAmount();
	if (degs > 0 && degs < snapAmount)
		degs = snapAmount;
	if (degs < 0 && degs > -snapAmount)
		degs = -snapAmount;

	rotAmount_ += degs;

	Matrix rotMat;

	if ( (CurrentRotationProperties::properties().size() == 1) && useLocalYaxis )
	{
		Quaternion q;
		q.fromAngleAxis( DEG_TO_RAD(-rotAmount_), initialMatrixes_[0].applyToUnitAxisVector(1) );
		rotMat.setRotate( q );
	}
	else
		rotMat.setRotateY( DEG_TO_RAD(-rotAmount_) );

	// rotate around the centre point
	const Vector3 invCentrePoint(-centrePoint_);
	Matrix invCentrePointMatrix;
	invCentrePointMatrix.setTranslate(invCentrePoint);

	rotMat.translation(centrePoint_);
	rotMat.multiply(invCentrePointMatrix, rotMat);

	std::vector<GenRotationProperty*> props = CurrentRotationProperties::properties();
	int j;
	std::vector<GenRotationProperty*>::iterator i;
	for ( i = props.begin(), j = 0;
		i != props.end();
		++i, ++j)
	{
		Matrix newMatrix;
		newMatrix.multiply(initialMatrixes_[j], rotMat);

		Vector3 pos = newMatrix.applyToOrigin();
		Vector3 newPos = pos;
		if( SnapProvider::instance()->snapMode() != SnapProvider::SNAPMODE_OBSTACLE )
			SnapProvider::instance()->snapPosition( newPos );
		Matrix mover;
		mover.setTranslate( newPos - pos );
		newMatrix.postMultiply( mover );

		Matrix worldToChunk;
		(*i)->pMatrix()->getMatrixContextInverse( worldToChunk );
		newMatrix.multiply(newMatrix, worldToChunk);

		(*i)->pMatrix()->setMatrix( newMatrix );
	}
}

bool WheelRotator::handleMouseEvent( const MouseEvent & event, Tool& tool )
{
	if (CurrentRotationProperties::properties().size() == 0)
		return false;


	if (event.dz() != 0)
	{
		timeSinceInput_ = 0.0f;

		// Get the direction only, we don't want the magnitude
		float amnt = (event.dz() > 0) ? -1.0f : 1.0f;

		// Move at 1deg/click with the button down, 5degs/click otherwise
		if (!InputDevices::instance().isKeyDown( KeyEvent::KEY_MIDDLEMOUSE ))
			amnt *= 15.0f;

		rotateBy( amnt, !InputDevices::instance().isShiftDown() );

		return true;
	}
	else
	{
		// commit the rotation now, so we don't have to wait for the timeout
		commitChanges();

		ToolManager::instance().popTool();
	}

	return false;
}

bool WheelRotator::handleKeyEvent( const KeyEvent & event, Tool& tool )
{
	if (CurrentRotationProperties::properties().size() == 0)
		return false;

	if (event.type() != KeyEvent::KEY_DOWN) return false;

	if (event.key() == KeyEvent::KEY_ESCAPE)
	{
		// set the item back to it's original pose
		std::vector<GenRotationProperty*> props = CurrentRotationProperties::properties();
		for (std::vector<GenRotationProperty*>::iterator i = props.begin(); i != props.end(); ++i)
		{
			(*i)->pMatrix()->commitState( true );
		}

		// and we're that's it from us
		ToolManager::instance().popTool();
		return true;
	}

	if (event.key() == KeyEvent::KEY_LEFTMOUSE || event.key() == KeyEvent::KEY_RIGHTMOUSE)
	{
		// commit the rotation now, so we don't have to wait for the timeout
		std::vector<GenRotationProperty*> props = CurrentRotationProperties::properties();
		for (std::vector<GenRotationProperty*>::iterator i = props.begin(); i != props.end(); ++i)
		{
			(*i)->pMatrix()->commitState();
		}

		ToolManager::instance().popTool();
	}

	return false;
}

void WheelRotator::commitChanges()
{
	std::vector<GenRotationProperty*> props = CurrentRotationProperties::properties();

	bool addUndoBarrier = props.size() > 1;

	bool success = true;
	for (std::vector<GenRotationProperty*>::iterator i = props.begin(); i != props.end(); ++i)
	{
		MatrixProxyPtr pMatrix = (*i)->pMatrix();
		if (pMatrix->hasChanged())
		{
			// set its transform permanently
			if ( !pMatrix->commitState( false, !addUndoBarrier ) )
			{
				success = false;
			}
		}
		else
		{
			pMatrix->commitState( true );
		}
	}

	if (addUndoBarrier)
	{
		UndoRedo::instance().barrier( "Rotate", true );

		if ( !success )
		{
			UndoRedo::instance().undo();
		}
	}
}

PyObject * WheelRotator::pyNew( PyObject * args )
{
	return new WheelRotator();
}



// -----------------------------------------------------------------------------
// Section: MatrixShaker
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( MatrixPositioner )

PY_BEGIN_METHODS( MatrixPositioner )
	PY_METHOD( setUndoName )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( MatrixPositioner )
PY_END_ATTRIBUTES()

PY_FACTORY( MatrixPositioner, Functor )



/**
 *	Constructor.
 */
MatrixPositioner::MatrixPositioner( MatrixProxyPtr pMatrix, PyTypePlus * pType ) :
	ToolFunctor( pType ),
	lastLocatorPos_( Vector3::zero() ),
	totalLocatorOffset_( Vector3::zero() ),
	gotInitialLocatorPos_( false )
{
	std::vector<GenPositionProperty*> props = CurrentPositionProperties::properties();
	for (std::vector<GenPositionProperty*>::iterator i = props.begin(); i != props.end(); ++i)
	{
		(*i)->pMatrix()->recordState();
	}

	matrix_ = pMatrix;
}



PyObject* MatrixPositioner::py_setUndoName( PyObject* args )
{
	// parse arguments
	char* str;
	if (!PyArg_ParseTuple( args, "s", &str ))	{
		PyErr_SetString( PyExc_TypeError, "setUndoName() "
			"expects a string argument" );
		return NULL;
	}

	undoName_ = str;

	Py_Return;
}


/**
 *	Update method
 */
void MatrixPositioner::update( float dTime, Tool& tool )
{
	// see if we want to commit this action
	if (!InputDevices::isKeyDown( KeyEvent::KEY_LEFTMOUSE ))
	{
		if (matrix_->hasChanged())
		{
			// set its transform permanently
			matrix_->commitState( false );
		}
		else
		{
			matrix_->commitState( true );
		}


		// and this tool's job is over
		ToolManager::instance().popTool();
		return;
	}

	// figure out movement
	if (tool.locator())
	{
		if (!gotInitialLocatorPos_)
		{
			lastLocatorPos_ = tool.locator()->transform().applyToOrigin();
			gotInitialLocatorPos_ = true;
		}

		totalLocatorOffset_ += tool.locator()->transform().applyToOrigin() - lastLocatorPos_;
		lastLocatorPos_ = tool.locator()->transform().applyToOrigin();


		// reset the last change we made
		matrix_->commitState( true );


		Matrix m;
		matrix_->getMatrix( m );

		Vector3 newPos = m.applyToOrigin() + totalLocatorOffset_;

		SnapProvider::instance()->snapPosition( newPos );

		m.translation( newPos );

		Matrix worldToLocal;
		matrix_->getMatrixContextInverse( worldToLocal );

		m.postMultiply( worldToLocal );


		matrix_->setMatrix( m );
	}
}


/**
 *	Key event method
 */
bool MatrixPositioner::handleKeyEvent( const KeyEvent & event, Tool& tool )
{
	if (event.type() != KeyEvent::KEY_DOWN) return false;

	if (event.key() == KeyEvent::KEY_ESCAPE)
	{
		// Set the items back to their original states
		matrix_->commitState( true );

		// and we're that's it from us
		ToolManager::instance().popTool();
		return true;
	}

	return false;
}


/**
 *	Factory method
 */
PyObject * MatrixPositioner::pyNew( PyObject * args )
{
	if (CurrentPositionProperties::properties().empty())
	{
		PyErr_Format( PyExc_ValueError, "MatrixPositioner()  No current editor" );
		return NULL;
	}

	return new MatrixPositioner( NULL );
}

// item_functor.cpp
