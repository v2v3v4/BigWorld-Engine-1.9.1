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
#include "player_fader.hpp"

#include "entity_manager.hpp"
#include "player.hpp"
#include "camera/base_camera.hpp"
#include "romp/geometrics.hpp"

DECLARE_DEBUG_COMPONENT2( "App", 0 )

namespace
{
	// Watch variables
	static bool s_checkPlayerClip = true;
	
	static class WatchIniter
	{
	public:
		WatchIniter()
		{
			BW_GUARD;
			MF_WATCH( "Client Settings/playerClip", s_checkPlayerClip,
				Watcher::WT_READ_WRITE, "Enable player clipping" );
		}
	} s_initer_;
}

/**
 *	This helper function is used by PlayerFader.
 *
 *	This function returns the rectangle that is the intersection of the
 *	near-plane with the view frustum.
 *
 *	@param rc			The render context (used to calculate the near-plane).
 *	@param corner		Is set to the position of the bottom-left corner of the
 *							rectangle.
 *	@param xAxis		Is set to the length of the horizontal edges.
 *	@param yAxis		Is set to the length of the vertical edges.
 *
 *	@note	The invView matrix must be correct before this method is called. You
 *			may need to call updateViewTransforms.
 */
void getNearPlaneRect( const Moo::RenderContext & rc, Vector3 & corner,
		Vector3 & xAxis, Vector3 & yAxis )
{
	BW_GUARD;
	// TODO: May want to make this function available to others.

	const Matrix & matrix = rc.invView();
	const Moo::Camera & camera = rc.camera();

	// zAxis is the vector from the camera position to the centre of the
	// near plane of the camera.
	Vector3 zAxis = matrix.applyToUnitAxisVector( Z_AXIS );
	zAxis.normalise();

	// xAxis is the vector from the centre of the near plane to its right edge.
	xAxis = matrix.applyToUnitAxisVector( X_AXIS );
	xAxis.normalise();

	// yAxis is the vector from the centre of the near plane to its top edge.
	yAxis = matrix.applyToUnitAxisVector( Y_AXIS );
	yAxis.normalise();

	const float fov = camera.fov();
	const float nearPlane = camera.nearPlane();
	const float aspectRatio = camera.aspectRatio();

	const float yLength = nearPlane * tanf( fov / 2.0f );
	const float xLength = yLength * aspectRatio;

	xAxis *= xLength;
	yAxis *= yLength;
	zAxis *= nearPlane;

	Vector3 nearPlaneCentre( matrix.applyToOrigin() + zAxis );
	corner = nearPlaneCentre - xAxis - yAxis;
	xAxis *= 2.f;
	yAxis *= 2.f;
}

BW_SINGLETON_STORAGE( PlayerFader );
PlayerFader s_playerFader;

/**
 *	Constructor. In the constructor, this object checks whether the near-plane
 *	clips the player. If so, it adjusts the visibility of the player's model.
 */
PlayerFader::PlayerFader() :	
	transparency_( 0.f ),
	ptp_( 2.f ),
	maxPt_( 0.85f )
{
	BW_GUARD;
	FullScreenBackBuffer::addUser(this);

}

void PlayerFader::init()
{
	MF_WATCH( "Client Settings/fx/Player Fader/transparency power",
		ptp_,
		Watcher::WT_READ_WRITE,
		"Mathematical power for the player transparency effect (when "
		"the player models fades as it nears the camera)" );

	MF_WATCH( "Client Settings/fx/Player Fader/maximum transparency",
		maxPt_,
		Watcher::WT_READ_WRITE,
		"The maximum value of player transparency is clamped to this value." );
}


void PlayerFader::fini()
{
}


/**
 *	Destructor. In the destructor, the visibility of the player's model is
 *	restored.
 */
PlayerFader::~PlayerFader()
{
	BW_GUARD;
	FullScreenBackBuffer::removeUser(this);
}


void PlayerFader::update()
{
	BW_GUARD;
	if (!s_checkPlayerClip)
		return;	

	transparency_ = 0.f;

	Entity * pPlayer = Player::entity();
	if (pPlayer == NULL)
		return;	

	PyModel * pModel = pPlayer->pPrimaryModel();
	if (pModel == NULL)
		return;

	if (!BaseCamera::checkCameraTooClose())
		return;

	BaseCamera::checkCameraTooClose( false );	

	//allow the player to fade out smoothly before completely disappearing
	//the transparency_ member is set with a value between 0 and 1.
	//For any value > 0, the player is fading or faded out.
	//
	//For a smooth transition, the near plane check no longer uses the
	//bounding box because it has sharp corners (and would thus create
	//discontinuities in the transparency value when the camera circles
	//around the player).
	//Instead, an ellipsoid is fitted around the bounding box and the distance
	//from the camera to it is calculated.
	if (pPlayer->pPrimaryModel()->visible())
	{
		//The distance is calculated by transforming the camera position into
		//unit-bounding-box space.  A sphere is fitted around the unit bounding
		//box and the distance is calculated.
		BoundingBox bb;
		pPlayer->pPrimaryModel()->boundingBoxAcc( bb, true );
		//1.414 is so the sphere fits around the unit cube, instead of inside.
		const Vector3 s = (bb.maxBounds() - bb.minBounds())*1.414f;
		Matrix m( pPlayer->fallbackTransform() );
		m.invert();
		Vector3 origin( Moo::rc().invView().applyToOrigin() );
		m.applyPoint( origin, origin );
		origin -= bb.centre();
		origin = origin * Vector3(1.f/s.x,1.f/s.y,1.f/s.z);

		//1 is subtracted so if camera is inside unit sphere then we are fully
		//faded out.
		float d = origin.length() - 1.f;
		transparency_ = 1.f - min( max( d,0.f ),1.f );

		//Check the player itself for transparency	
		transparency_ = max( pPlayer->transparency(), transparency_ );

		//And adjust the values for final output.
		transparency_ = powf(transparency_,ptp_);
		transparency_ = min( transparency_, maxPt_ );
	}
}


bool PlayerFader::isEnabled()
{
	BW_GUARD;
	return (transparency_ > 0.f);
}


void PlayerFader::beginScene()
{	
	BW_GUARD;
	Entity * pPlayer = Player::entity();
	pPlayer->pPrimaryModel()->visible( false );
}


void PlayerFader::endScene()
{
	BW_GUARD;
	Entity * pPlayer = Player::entity();
	pPlayer->pPrimaryModel()->visible( true );
}

void PlayerFader::doPostTransferFilter()
{
	BW_GUARD;
	//now, draw the player if the near plane clipper removed the player for fading
	if ( 0.f < transparency_ && transparency_ < 1.f )
	{
		if ( Player::instance().drawPlayer( &FullScreenBackBuffer::renderTarget(), 
									!FullScreenBackBuffer::reuseZBuffer() ) )
		{
			Moo::rc().setTexture( 0, FullScreenBackBuffer::renderTarget().pTexture() );
			Moo::rc().device()->SetPixelShader( NULL );

			Geometrics::texturedRect( Vector2(0.f,0.f),
				Vector2(Moo::rc().screenWidth(),Moo::rc().screenHeight()),
				Vector2(0,0),
				Vector2(FullScreenBackBuffer::uSize(),FullScreenBackBuffer::vSize()),
				Moo::Colour( 1.f, 1.f, 1.f,
				transparency_ ), true );
		}
	}
}


// player_fader.cpp
