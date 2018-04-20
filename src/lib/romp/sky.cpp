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

#include "sky.hpp"
#include "cstdmf/stdmf.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/memory_counter.hpp"

#include "moo/render_context.hpp"
#include "moo/camera.hpp"
#include "moo/texture_manager.hpp"
#include "moo/texturestage.hpp"
#include "moo/texture_exposer.hpp"
#include "moo/fog_helper.hpp" 

#include "moo/dynamic_vertex_buffer.hpp"
#include "moo/dynamic_index_buffer.hpp"
#include "moo/vertex_formats.hpp"

#include "math/colour.hpp"

#include "resmgr/bwresource.hpp"
#include "resmgr/auto_config.hpp"

#include "weather.hpp"
#include "sun_and_moon.hpp"
#include "photon_occluder.hpp"
#include "geometrics.hpp"
#include "fog_controller.hpp"
#include "sky_light_map.hpp"
#include "enviro_minder.hpp"
#include "lens_effect_manager.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 0 )

memoryCounterDefine( cloud, Environment );

#include "physics2/worldtri.hpp"


/**
 * TODO: to be documented.
 */
struct CloudSpec
{
	int				texture;	// unused
	float			rotation;	// in radians
	Vector2	radii;
	Vector2	position;
	uint16			midLum;
	uint16			botLum;
	float			lightning;
};

/**
 * TODO: to be documented.
 */
struct CloudStratum
{
	std::vector<CloudSpec>	clouds;
	float					height;
	Vector2					windSpeed;
	float					cover;
	float					cohesion;
	float					colourMin;
	float					colourMax;
};

static const int STRATA_COUNT = 1;

CloudStratum	cloudStrata[STRATA_COUNT];

const float visRange = 1000;
const float fullRange = visRange * 2;
const float fullArea = visRange*visRange*4;

static float			ambColRatio[3] = { 0.f, 1.f, 0.5f };//{ 0.5f, 0.5f, 0.5f };
static float			sunColRatio[3] = { 0.2f, 0.2f, 0.f };//{ 0.f, 0.f, 0.f };
static float			fogColRatio[3] = { 1.0f, 0.f, 1.f };//{ 0.f, 0.f, 0.f };
static bool				sceneFogging = true;
static bool				moveWithCamera = true;
static bool				alphaTestBody = false;

static AutoConfigString s_cloudEdgesBmpName( "environment/cloudEdgesBmpName" );
static AutoConfigString s_cloudBodyBmpName( "environment/cloudBodyBmpName" );
static AutoConfigString s_lightningBmpName( "environment/lightningBmpName" );

/**
 *	Photon occluder for objects in the sky, i.e. clouds
 */
class SkyPhotonOccluder : public PhotonOccluder
{
public:
	SkyPhotonOccluder( float & alpha ) : alpha_(alpha) { }

	virtual float collides(
			const Vector3 & lightSourcePosition,
			const Vector3 & cameraPosition,
			const LensEffect& le )
	{
		Vector3 difference = cameraPosition - lightSourcePosition;
		float farPlane = (Moo::rc().camera().farPlane() - 2.5f);
		if ( difference.lengthSquared() >= (farPlane*farPlane) )
		{
			//If unnatural fog is in effect, then no distance flares
			//get through.  This is like an overcast cloud layer
			if ( FogController::instance().multiplier() > 1.f )
				return 0.f;

			//Return the visibility as a function of the alpha (the three is plucked from the air)
			return (float)pow( (double)alpha_ - 1, 2 );
		}
		else
			return 1.f;
	}

private:
	float & alpha_;
};


//---------------------------------------------------------------------------
// Section: SkyBoxScopedViewport
//---------------------------------------------------------------------------
SkyBoxScopedSetup::SkyBoxScopedSetup()
{
	// Update viewport for background - from background minZ to 1.0f
	D3DVIEWPORT9 vp;
	Moo::rc().getViewport( &oldVp_ );

	// Draw beyond the far plane with enough slack for bad precision
	memcpy( &vp, &oldVp_, sizeof( vp ) );
	vp.MinZ	= 1.0f;
	vp.MaxZ	= 1.1f;

	Moo::rc().setViewport( &vp );
}


SkyBoxScopedSetup::~SkyBoxScopedSetup()
{
	Moo::rc().setViewport( &oldVp_ );
}


//---------------------------------------------------------------------------
// Section: Sky
//---------------------------------------------------------------------------
Sky::Sky()
:sunAlpha_( 0 ),
 avgCover_( 0 ),
 avgDensity_( 0 ),
 precipitation_( 0.f, 0.f, 0.f ),
 conflict_( 0 ),
 settings_( NULL ),
 cloudSet_( NULL ),
 enabled_( false ),
 anyNewClouds_( false ),
 pEdgeMat_( NULL ),
 pBodyMat_( NULL ),
 pBodyPix_( NULL )
{
	const float SKY_EXTENT = 36000;
	const float SKY_HIGH = 1200;
	const float TC1_MIN = 0.f;
	const float TC1_MAX = 6.5f;
	const float TC2_MIN = 0.5f;
	const float TC2_MAX = 8.25f;
	const Vector2 SHADOW_OFFSET( 0.01f, 0.01f );

	pPhotonOccluder_ = new SkyPhotonOccluder( sunAlpha_ );

	// set up the edge material
	pEdgeMat_ = new Moo::Material();
	Moo::TextureStage ts1;

	ts1.pTexture( Moo::TextureManager::instance()->get(s_cloudEdgesBmpName, true, true, true, "texture/environment") );
	ts1.colourOperation( Moo::TextureStage::MODULATE,
		Moo::TextureStage::TEXTURE,
		Moo::TextureStage::DIFFUSE );
	ts1.alphaOperation( Moo::TextureStage::MODULATE,
		Moo::TextureStage::TEXTURE,
		Moo::TextureStage::DIFFUSE );
	ts1.textureWrapMode( Moo::TextureStage::CLAMP );
	pEdgeMat_->addTextureStage( ts1 );

	Moo::TextureStage ts2;
	pEdgeMat_->addTextureStage( ts2 );

	pEdgeMat_->alphaBlended( true );
	pEdgeMat_->srcBlend( Moo::Material::SRC_ALPHA );
	pEdgeMat_->destBlend( Moo::Material::ONE );
	pEdgeMat_->zBufferRead( true );
	pEdgeMat_->zBufferWrite( false );

	// set up the body material
	pBodyMat_ = new Moo::Material();
	ts1 = Moo::TextureStage();

	ts1.pTexture( Moo::TextureManager::instance()->get(s_cloudBodyBmpName, true, true, true, "texture/environment"));
	ts1.colourOperation( Moo::TextureStage::ADDSIGNED,	
		Moo::TextureStage::DIFFUSE,
		Moo::TextureStage::TEXTURE );
	ts1.alphaOperation( Moo::TextureStage::MODULATE,	
		Moo::TextureStage::DIFFUSE,
		Moo::TextureStage::TEXTURE );
	ts1.textureWrapMode( Moo::TextureStage::CLAMP );
	pBodyMat_->addTextureStage( ts1 );

	ts2 = Moo::TextureStage();
	pBodyMat_->addTextureStage( ts2 );

	pBodyMat_->alphaBlended( true );
	pBodyMat_->srcBlend( Moo::Material::SRC_ALPHA );
	pBodyMat_->destBlend( Moo::Material::INV_SRC_ALPHA);	
	pBodyMat_->zBufferRead( true );
	pBodyMat_->zBufferWrite( false );

	// copy out the alpha map
	if (ts1.pTexture() && ts1.pTexture()->pTexture())
	{
		Moo::TextureExposer te( ts1.pTexture() );

		// Make sure the correct dds was used. If it fails here you may have
		// run with no dds for the sky.?
		MF_ASSERT( te.format() == D3DFMT_A8R8G8B8 );

		pBodyPix_ = new PixArray( te.width(), te.height() );
		for (int y = 0; y < te.height(); y++) for (int x = 0; x < te.width(); x++)
		{
			(*pBodyPix_)[y][x] = ((uint32*)te.bits())
				[ y * (te.pitch()/sizeof(uint32)) + x ];
		}
	}
	else
	{
		ERROR_MSG( "sky.cpp::initStatics - Could not find the bitmap for the clouds\n" );
	}

	memoryCounterAdd( cloud );
	memoryClaim( this );
}

//---------------------------------------------------------------------------
Sky::~Sky( )
{
	if (pPhotonOccluder_)
		delete pPhotonOccluder_;

	if (pEdgeMat_)
		delete pEdgeMat_;

	if (pBodyMat_)
		delete pBodyMat_;

	if (pBodyPix_)
		delete pBodyPix_;

	memoryCounterSub( cloud );
	memoryClaim( this );
}


/**
 *	This method is called by the envirominder when it
 *	is activated, e.g. when the camera has moved to a
 *	new space.
 */
void Sky::activate( const EnviroMinder& em, DataSectionPtr pSpaceSettings, SkyLightMap* skyLightMap )
{
	if (!pSpaceSettings->readBool( "oldClouds", true ))
	{
		enabled_ = false;
		return;
	}

	//OK, lets enable clouds then...
	enabled_ = true;

	if (!EnviroMinder::primitiveVideoCard())
		cloudSet_ = ShaderManager::instance().shaderSet( "xyzlsuv", "sky" );	

	settings_ = pSpaceSettings;	

	// prepare the clouds for use
	this->init3();

	if ( settings_ )
	{
		DataSectionPtr pWatcherSection =
			settings_->openSection( "watcherValues/Client Settings/Clouds" );

		if (pWatcherSection)
		{
			pWatcherSection->setWatcherValues( "Client Settings/Clouds" );
		}
	}

	// add our own photon occluder for use
	LensEffectManager::instance().addPhotonOccluder(pPhotonOccluder_);

	// add ourselves as a sky light map contributor
	skyLightMap->addContributor( *this );
}


/**
 *	This method is called by the envirominder when it
 *	is deactivated, e.g. when the camera has moved to a
 *	new space and this environment is no longer in use.
 */
void Sky::deactivate( const EnviroMinder& em, SkyLightMap* skyLightMap )
{
	// remove ourselves as a sky light map contributor
	skyLightMap->delContributor( *this );

	// remove our own photon occluder from use
	LensEffectManager::instance().delPhotonOccluder(pPhotonOccluder_);

	cloudSet_ = NULL;
}


// OK, forget all that - let's just try some sprite-based clouds

static bool s_drawEdges = true;
static bool s_drawbodis = true;

/**
 *	Initialise the sky for the third style of drawing
 */
void Sky::init3()
{
	// set up the first stratum
	cloudStrata[0].height = 300.f;
	cloudStrata[0].windSpeed = Vector2(0,-10);
	cloudStrata[0].colourMin = 0.9f;
	cloudStrata[0].colourMax = 1.0f;
	cloudStrata[0].cover = 0.5;
	cloudStrata[0].cohesion = 0.2f;
/*
	cloudStrata[1].height = 200.f;
	cloudStrata[1].windSpeed = Vector2(0,-4);
	cloudStrata[1].colourMin = 0.9f;
	cloudStrata[1].colourMax = 1.0f;
	cloudStrata[1].cover = 0.5;
	cloudStrata[1].cohesion = 0.2f;

	cloudStrata[2].height = 300.f;
	cloudStrata[2].windSpeed = Vector2(0,-4);
	cloudStrata[2].colourMin = 0.9f;
	cloudStrata[2].colourMax = 1.0f;
	cloudStrata[2].cover = 0.5;
	cloudStrata[2].cohesion = 0.2f;
*/

	// seed clouds in 20 easy steps!
	for (float f=0; f < 20; f++)
	{
		float delt = ((visRange*2) / -cloudStrata[0].windSpeed.y) / 20.f;
		this->generateCloudSpecs( delt );
		for (uint c = 0; c < cloudStrata[0].clouds.size(); c++)
		{
			cloudStrata[0].clouds[c].position +=
				cloudStrata[0].windSpeed * delt;
		}
	}

	// watch some stuff
	static bool watchingClouds = false;
	if (!watchingClouds)
	{
		MF_WATCH( "Client Settings/Clouds/colourMin",
			cloudStrata[0].colourMin,
			Watcher::WT_READ_WRITE,
			"Minimum colour of clouds." );

		MF_WATCH( "Client Settings/Clouds/colourMax",
			cloudStrata[0].colourMax,
			Watcher::WT_READ_WRITE,
			"Maximum colour of clouds." );

		MF_WATCH( "Client Settings/Clouds/cover",
			cloudStrata[0].cover,
			Watcher::WT_READ_WRITE,
			"Amount of cloud cover." );

		MF_WATCH( "Client Settings/Clouds/cohesion",
			cloudStrata[0].cohesion,
			Watcher::WT_READ_WRITE,
			"Cohesion of clouds." );

		MF_WATCH( "Client Settings/Clouds/wind y",
			cloudStrata[0].windSpeed.y,
			Watcher::WT_READ_WRITE,
			"Wind speed with respect to clouds." );

		MF_WATCH( "Client Settings/Clouds/draw edges_",
			s_drawEdges,
			Watcher::WT_READ_WRITE,
			"Toggle to draw clouds edges_." );

		MF_WATCH( "Client Settings/Clouds/draw bodis_",
			s_drawbodis,
			Watcher::WT_READ_WRITE,
			"Toggle to draw clouds bodies." );

		MF_WATCH( "Client Settings/Clouds/HUD",
			drawHUD_,
			Watcher::WT_READ_WRITE,
			"Draw the cloud Heads-up display." );

		MF_WATCH( "Client Settings/Clouds/sun alpha",
			sunAlpha_,
			Watcher::WT_READ_ONLY,
			"Current sun alpha value, or how much cloud cover there is in front"
			" of the sun at present." );

		MF_WATCH( "Client Settings/Clouds/cur cover",
			avgCover_,
			Watcher::WT_READ_ONLY,
			"Current average cloud cover, used to determine intensity of "
			"sunlight." );

		MF_WATCH( "Client Settings/Clouds/cur density",
			avgDensity_,
			Watcher::WT_READ_ONLY,
			"Current average cloud density, used to determine intensity of "
			"sunlight." );

		MF_WATCH( "Client Settings/Clouds/precipitation",
			precipitation_,
			Watcher::WT_READ_ONLY,
			"Amount of precipitation currently generated by clouds." );

		MF_WATCH( "Client Settings/Clouds/ambientColourRatio_top",
			ambColRatio[0],
			Watcher::WT_READ_WRITE,
			"Ambient colour ratio (at the top)" );

		MF_WATCH( "Client Settings/Clouds/sunColourRatio_top",
			sunColRatio[0],
			Watcher::WT_READ_WRITE,
			"Sun colour ratio (at the top)" );

		MF_WATCH( "Client Settings/Clouds/fogColourRatio_top",
			fogColRatio[0],
			Watcher::WT_READ_WRITE,
			"Fog colour ratio (at the top)" );

		MF_WATCH( "Client Settings/Clouds/ambientColourRatio_mid",
			ambColRatio[1],
			Watcher::WT_READ_WRITE,
			"Ambient colour ratio (at the middle)" );

		MF_WATCH( "Client Settings/Clouds/sunColourRatio_mid",
			sunColRatio[1],
			Watcher::WT_READ_WRITE,
			"Sun colour ratio (at the middle)" );

		MF_WATCH( "Client Settings/Clouds/fogColourRatio_mid",
			fogColRatio[1],
			Watcher::WT_READ_WRITE,
			"Fog colour ratio (at the middle)" );

		MF_WATCH( "Client Settings/Clouds/ambientColourRatio_bot",
			ambColRatio[2],
			Watcher::WT_READ_WRITE,
			"Ambient colour ratio (at the bottom)" );

		MF_WATCH( "Client Settings/Clouds/sunColourRatio_bot",
			sunColRatio[2],
			Watcher::WT_READ_WRITE,
			"Sun colour ratio (at the bottom)" );

		MF_WATCH( "Client Settings/Clouds/fogColourRatio_bot",
			fogColRatio[2],
			Watcher::WT_READ_WRITE,
			"Fog colour ratio (at the bottom)" );

		MF_WATCH( "Client Settings/Clouds/sceneStyleFogging",
			sceneFogging,
			Watcher::WT_READ_WRITE,
			"Enable scene-style fogging, instead of special-case fogging." );

		MF_WATCH( "Client Settings/Clouds/moveWithCamera",
			moveWithCamera,
			Watcher::WT_READ_WRITE,
			"Toggle whether or not clouds move with camera movement." );

		MF_WATCH( "Client Settings/Clouds/alphaTestBodyMaps",
			alphaTestBody,
			Watcher::WT_READ_WRITE,
			"Enable body maps alpha testing." );

		watchingClouds = true;
	}
}


void Sky::prepareClouds( float dTime, Vector3 sunDir, uint32 sunCol, float sunAngle )
{
	if (!enabled_)
		return;

	sunAngle_ = sunAngle;

	static Vector3		lastView = Moo::rc().invView().applyToOrigin();
	float fon = Moo::rc().fogNear();	float foe = Moo::rc().fogFar();

	Vector3 nowView = Moo::rc().invView().applyToOrigin();
	Vector3 deltaView = nowView - lastView;
	lastView = nowView;

	// adjust deltaView from World space to Cloud space
	float worldToCloud = visRange / Moo::rc().camera().farPlane();
	deltaView *= worldToCloud;

	// create any new clouds
	anyNewClouds_ = this->generateCloudSpecs( dTime );

	// figure out where the sun shines from
	Vector3 sunUp(0,0.02f,0);
    if ( almostZero( sunDir.y, 0.0001f ) )
    	sunDir.y = 0.0001f;
	Vector3 sunProj = sunUp + sunDir * (sunUp.y / sunDir.y);
	sunProj.x = Math::clamp( .04f, sunProj.x );	// we assume a maximum 1/25 overlap
	sunProj.z = Math::clamp( .04f, sunProj.z );	//(i.e. 10 pixels in a 256^2 texture)
	if (sunDir.y>0) sunProj.x *= -1.f;
	Vector3 toTheSun( sunDir.x, -sunDir.y, sunDir.z );
	Vector3 sunColVec = Colour::getVector3( sunCol );

	uint32 ambCol = Moo::rc().lightContainer() ?
		Moo::rc().lightContainer()->ambientColour():
		Moo::Colour( 0, 0, 0, 0);
	Vector3 ambColVec = Colour::getVector3( ambCol );

	uint32 fogCol = Moo::rc().fogColour();
	Vector3 fogColVec = Colour::getVector3( fogCol );

	Vector3 resColVec[3];
	for (int i = 0; i < 3; i++)
	{
		resColVec[i] =
			ambColVec * ambColRatio[i] +
			sunColVec * sunColRatio[i] +
			fogColVec * fogColRatio[i];
	}

	float oldAvgDensity = avgDensity_;

	sunAlpha_ = 0.f;
	avgCover_ = 0.f;
	avgDensity_ = 0.f;
	precipitation_ = Vector3(0,0,0);
	float precipite = 0.f;

	// update the strata
	for (int s = STRATA_COUNT-1; s >= 0; s--)
	{		
		edges_.clear();
		bodis_.clear();

		float strataHeight = cloudStrata[s].height;
		for (uint c = 0; c < cloudStrata[s].clouds.size(); c++)
		{
			CloudSpec & cs = cloudStrata[s].clouds[c];
			Vector4 centre( cs.position.x, strataHeight, cs.position.y, 1 );
			float sinrot = sinf( cs.rotation ) * 1.f;
			float cosrot = cosf( cs.rotation ) * 1.f;

			Vector3 quad[4];
			Vector2 quadUV[4];

			// figure out the vertices
			for (int v=0; v < 12; v++)
			{
				SkyVertexVector & vec = (v>>2) ? bodis_ : edges_;
				vec.push_back( Moo::VertexXYZDSUV() );
				Moo::VertexXYZDSUV & tlv = vec.back();

				Vector4 point = centre + Vector4(
					(((v&1)?-cosrot:cosrot) + ((v&2)?-sinrot:sinrot) +
						(v>>2)*sunProj.x) * cs.radii.x, 0,
					(((v&2)?-cosrot:cosrot) + ((v&1)?sinrot:-sinrot) +
						(v>>2)*sunProj.z) * cs.radii.y, 0 );
				point.y = (1-Vector2(point.x/visRange,point.z/visRange).length())*
					strataHeight - (v>>2)*strataHeight/100.f;

				*(Vector3*)&tlv = Vector3(point.x,point.y,point.z);

				uint32 alphaFromDist = 0xff000000;
				if (sceneFogging)
				{
					float plen = Vector3((float*)point).length();
					if (plen - fon > foe * 0.8f)
					{
						float scaled = (plen - (fon + foe*0.8f)) / (foe * 0.2f);
						alphaFromDist = 0xff000000 - (uint32(min(scaled,1.f) * 255) << 24);
					}
				}

				Vector3 acol = (v>>2)==0?Vector3(255,255,255):(v>>2)==1?
					Vector3(cs.midLum,cs.midLum,cs.midLum):
					Vector3(cs.botLum,cs.botLum,cs.botLum);
				Vector3 & resColVecRef = resColVec[v>>2];
				Vector3 rcol = Vector3( acol[0]*resColVecRef[0],
					acol[1]*resColVecRef[1], acol[2]*resColVecRef[2] ) / 128.f;
				float rcolMax = max(rcol[0],max(rcol[1],rcol[2])) / 255.f;
				tlv.colour = Colour::getUint32( rcol / max(1.0f,rcolMax) ) & 0x00ffffff |
					alphaFromDist;
				tlv.spec = 0xFFFFFFFF;

				if ((v>>2) == 1 && cs.lightning > 0.5f ) tlv.colour = 0x00ffffff;

				tlv.tu = (v&1)?1.f:0.f;
				tlv.tv = (v&2)?1.f:0.f;

				if (v<4)
				{
					quad[v].set( point.x, point.y, point.z );
					quadUV[v].set( tlv.tu, tlv.tv );
				}
			}

			// see if the vector to the sun intersects this quad
			WorldTriangle triA( quad[0], quad[1], quad[2] );
			WorldTriangle triB( quad[1], quad[3], quad[2] );
			Vector2 *triAUV[3] = { &quadUV[0], &quadUV[1], &quadUV[2] };
			Vector2 *triBUV[3] = { &quadUV[1], &quadUV[3], &quadUV[2] };
			for (int t = 0; t < 2; t++)
			{
				WorldTriangle & tri = t?triB:triA;
				float dist = strataHeight*2;
				if (tri.intersects( Vector3(0,0,0), toTheSun, dist ))
				{
					Vector2 ** triUV = t?triBUV:triAUV;
					Vector2 st = tri.project( toTheSun * dist );
					Vector2 uv = (*triUV[0]) + st[0] * (*triUV[1] - *triUV[0]) +
						st[1] * (*triUV[2] - *triUV[0]);

					// look up that pixel in the alpha map (finally!)
					if (pBodyPix_ != NULL)
					{
						uint32 tx = uint32(Math::clamp(0.f,uv.x,1.f) * (pBodyPix_->width-1));
						uint32 ty = uint32(Math::clamp(0.f,uv.y,1.f) * (pBodyPix_->height-1));
						sunAlpha_ += float((*pBodyPix_)[ty][tx] >> 24) / 255.f;
					}
				}
			}

			// update the cover and density
			float avgRad = sqrtf(cs.radii.x * cs.radii.y);
			float csCovers =	// clip it to the fullArea (no quick changes)
				max( 0.f, min( cs.position.x + avgRad, visRange ) -
					max( cs.position.x - avgRad, -visRange )) *
				max( 0.f, min( cs.position.y + avgRad, visRange ) -
					max( cs.position.y - avgRad, -visRange ));
			avgCover_ += csCovers;
			avgDensity_ += csCovers * Math::clamp(0.f,(((128+192) - cs.midLum)/192.f),1.f);

			// see if there's any precipitation worth a mention
			if (cs.midLum < 128+192/2)
			{
				Vector3	toTheRain(0,1,0);
				float rainMul = (((128+192/2) - cs.midLum) / 192.f) * 2.f;
				float rainVal = 0.f;

				for (int t = 0; t < 2; t++)
				{
					WorldTriangle & tri = t?triB:triA;
					float dist = strataHeight*2;
					if (tri.intersects( Vector3(0,0,0), toTheRain, dist ))
					{
						Vector2 ** triUV = t?triBUV:triAUV;
						Vector2 st = tri.project( toTheRain * dist );
						Vector2 uv = (*triUV[0]) + st[0] * (*triUV[1] - *triUV[0]) +
							st[1] * (*triUV[2] - *triUV[0]);

						// look up that pixel in the alpha map (finally!)
						if (pBodyPix_ != NULL)
						{
							uint32 tx = uint32(Math::clamp(0.f,uv.x,1.f) * (pBodyPix_->width-1));
							uint32 ty = uint32(Math::clamp(0.f,uv.y,1.f) * (pBodyPix_->height-1));
							rainVal += float((*pBodyPix_)[ty][tx] >> 24) / 255.f;
						}
					}
				}

				precipite += rainVal * rainMul;
			}

			// blow it on by the wind
			cs.position += cloudStrata[s].windSpeed * dTime;

			if (moveWithCamera)
			{
				// move it as if were in the scene
				cs.position.x -= deltaView.x;
				cs.position.y -= deltaView.z;
			}

			// cancel any lightning
			cs.lightning = 0.f;
		}

		while (indxs_.size()/6 < max(edges_.size(),bodis_.size())/4)
		{
			uint16 first = indxs_.size()/6*4;
			indxs_.push_back(first);
			indxs_.push_back(first+1);
			indxs_.push_back(first+2);
			indxs_.push_back(first+1);
			indxs_.push_back(first+3);
			indxs_.push_back(first+2);
		}		
	}

	frameOffset_ = Vector2( cloudStrata[0].windSpeed * dTime );
	if (moveWithCamera)
	{
		frameOffset_.x -= deltaView.x;
		frameOffset_.y -= deltaView.z;
	}

	avgCover_   /= fullArea * STRATA_COUNT * 4;
	avgDensity_ /= fullArea * STRATA_COUNT * 4;

	if (oldAvgDensity == -1.f) avgDensity_ = -1.f;	

	// distribute the precipitation according to the weather conditions
	if (temperature_ < 0.f)
	{
		precipitation_.x = 0.f;
		precipitation_.y = precipite;
	}
	else if (temperature_ < 5.f)
	{
		precipitation_.x = precipite * (temperature_/5.f);
		precipitation_.y = precipite * (1.f - (temperature_/5.f));
	}
	else
	{
		precipitation_.x = precipite;
		precipitation_.y = 0.f;
	}	
}

void Sky::updateLightMap( SkyLightMap* lightMap )
{
	if (enabled_ && lightMap)
	{		
		lightMap->update( sunAngle_, frameOffset_ );
	}
}

void Sky::setRenderState()
{
	if (!enabled_)
		return;

	float strataHeight = cloudStrata[0].height;
	 
	Moo::FogHelper::setFog( Moo::rc().fogNear(), Moo::rc().fogFar(), 
 		D3DRS_FOGTABLEMODE, D3DFOG_LINEAR ); 

	Moo::rc().setPixelShader(NULL);

	Moo::rc().setFVF( Moo::VertexXYZDSUV::fvf() );

	if (cloudSet_)
		Moo::rc().setVertexShader( cloudSet_->shader( 0, 0, 0, true ) );
	else
		Moo::rc().setVertexShader( NULL );
	Moo::rc().setRenderState( D3DRS_CLIPPING, TRUE );
}


bool Sky::needsUpdate()
{
	return anyNewClouds_;
}


void Sky::render( SkyLightMap* lightMap, Moo::EffectMaterialPtr material, float sunAngle  )
{
	this->setRenderState();
	pBodyMat_->set();

	Matrix m;

	// set the cloud body texture map	
	ComObjectWrap<ID3DXEffect> pEffect = material->pEffect()->pEffect();
	D3DXHANDLE param = pEffect->GetParameterByName(NULL,"Cloud");
	Moo::BaseTexturePtr pTex = pBodyMat_->textureStage(0).pTexture();
	pEffect->SetTexture( param, pTex->pTexture() );

	if ( material->begin() )
	{
		for ( uint32 i=0; i<material->nPasses(); i++ )
		{
			material->beginPass(i);

			lightMap->orthogonalProjection(fullRange,-fullRange,m);
			m.row(3, Vector4( 0.f, 0.f,  0.1f,  1 ) );
			lightMap->setLightMapProjection(m);

			//Draw the clouds into the render target
			if ( bodis_.size() > 0 )
			{
				//DynamicVertexBuffer
				uint32 lockIndex = 0, vertexBase = 0;			
				Moo::DynamicVertexBufferBase2<SkyVertex>& vb = Moo::DynamicVertexBufferBase2< SkyVertex >::instance();
				if ( vb.lockAndLoad( &bodis_.front(), bodis_.size(), vertexBase ) &&
					 SUCCEEDED(vb.set( 0 )) )
				{
					//DynamicIndexBuffer
					Moo::DynamicIndexBufferBase& dynamicIndexBuffer = Moo::rc().dynamicIndexBufferInterface().get( D3DFMT_INDEX16 );
					Moo::IndicesReference ind =
						dynamicIndexBuffer.lock2( indxs_.size() );
					if ( ind.valid() )
					{
						ind.fill( &indxs_.front(), indxs_.size() );
						dynamicIndexBuffer.unlock();
						lockIndex = dynamicIndexBuffer.lockIndex();
						if ( SUCCEEDED(dynamicIndexBuffer.indexBuffer().set()) )
						{
							Moo::rc().drawIndexedPrimitive(
								D3DPT_TRIANGLELIST, 
								vertexBase, 
								0, 
								bodis_.size(), 
								lockIndex, 
								bodis_.size()/2);		
						}
					}
				}
			}
			material->endPass();
		}
		material->end();
	}		
}


/**
 *	Draw the sky on the screen
 */
void Sky::draw()
{
	if (!enabled_)
		return;

	this->setRenderState();
	float fon = Moo::rc().fogNear();	float foe = Moo::rc().fogFar();
	float strataHeight = cloudStrata[0].height;

	Moo::rc().push();
	Moo::rc().world( Matrix::identity );	

	float oldFarPlane = Moo::rc().camera().farPlane();
	Moo::rc().camera().farPlane( visRange * 2.f );
	Moo::rc().updateProjectionMatrix();
	Matrix projMatrix = Moo::rc().projection();
	Moo::rc().camera().farPlane( oldFarPlane );
	Moo::rc().updateProjectionMatrix();
	Matrix viewMatrix = Moo::rc().view();
	viewMatrix.translation( Vector3(0,0,0) );

	//Make tiny.  scale everything down by this amount,
	//just so that we ensure the clouds are drawn in front
	//of the far-z plane.
	//
	//All environment effects draw in front of the far-z plane
	//but have their z-values clamped to 1.0 by the viewport.
	//
	//This allows us to occlusion cull these fill-rate hungry
	//effects.
	const float make_tiny = 0.05f;
	Matrix tiny;
	tiny.setScale( make_tiny, make_tiny, make_tiny );
	tiny.postMultiply( Moo::rc().world() );
	Moo::rc().device()->SetTransform( D3DTS_WORLD, &tiny );
	Moo::rc().device()->SetTransform( D3DTS_VIEW, &viewMatrix );
	Moo::rc().device()->SetTransform( D3DTS_PROJECTION, &projMatrix );
	Moo::rc().setRenderState( D3DRS_LIGHTING, FALSE );
	Moo::rc().setRenderState( D3DRS_SPECULARENABLE, FALSE );

	//1,2,3,4 - viewProjection
	Matrix worldViewProj( tiny );
	worldViewProj.postMultiply( viewMatrix );
	worldViewProj.postMultiply( projMatrix );
	XPMatrixTranspose( &worldViewProj, &worldViewProj );
	Moo::rc().device()->SetVertexShaderConstantF( 1, (const float*)&worldViewProj, 4 );		

	float fogVal;
	float fogNear;
	float fogFar;

	pEdgeMat_->set();

	if (!sceneFogging)
	{
		fogVal = Vector2(visRange-visRange/4,strataHeight/4).length();
		fogVal *= make_tiny;
		fogNear = fogVal;

		fogVal = Vector2(visRange,0).length();
		fogVal *= make_tiny;
		fogFar = fogVal;

		Moo::FogHelper::setFog( fogNear, fogFar, D3DRS_FOGTABLEMODE, D3DFOG_LINEAR ); 
	}
	else
	{
		fogNear = Moo::rc().fogNear();
		fogVal = Moo::rc().fogFar() * make_tiny;
		Moo::FogHelper::setFogEnd( fogVal ); 
		fogFar = fogVal;
	}
	
	uint32 oldFogColour = Moo::rc().fogColour();
	Moo::FogHelper::setFogColour( 0x00000000 ); 

	Moo::rc().device()->SetVertexShaderConstantF(  15,
		(const float*)Vector4( - (1.f/(fogFar-fogNear) ),
		fogFar/(fogFar-fogNear), 0, 0 ),  1 );

	uint32 lockIndex = 0;
	// DynamicIndexBuffer
	Moo::DynamicIndexBufferBase& dynamicIndexBuffer = Moo::rc().dynamicIndexBufferInterface().get( D3DFMT_INDEX16 );
	Moo::IndicesReference ind =
		dynamicIndexBuffer.lock2( indxs_.size() );
	if ( ind.valid() )
	{
		ind.fill( &indxs_.front(), indxs_.size() );
		dynamicIndexBuffer.unlock();
		lockIndex = dynamicIndexBuffer.lockIndex();
		if ( SUCCEEDED(dynamicIndexBuffer.indexBuffer().set()) )
		{
			if ( s_drawEdges && edges_.size() > 0 )
			{
				uint32 vertexBase = 0;
				Moo::DynamicVertexBufferBase2<SkyVertex>& vb = Moo::DynamicVertexBufferBase2< SkyVertex >::instance();
				if ( vb.lockAndLoad( &edges_.front(), edges_.size(), vertexBase ) &&
					 SUCCEEDED(vb.set( 0 )) )
				{
					Moo::rc().drawIndexedPrimitive(
						D3DPT_TRIANGLELIST, 
						vertexBase,
						0, 
						edges_.size(), 
						lockIndex, 
						edges_.size()/2);
				}
			}

			if (alphaTestBody)
			{
				pBodyMat_->alphaTestEnable( true );
				pBodyMat_->alphaReference( 0x01 );
			}
			else
			{
				pBodyMat_->alphaTestEnable( false );
			}

			pBodyMat_->set();

			if (!sceneFogging)
			{
				Moo::FogHelper::setFogTableMode( D3DFOG_LINEAR ); 
				fogVal = Vector2(visRange-visRange/4,strataHeight/4).length();
				fogVal *= make_tiny;
				Moo::FogHelper::setFogStart( fogVal ); 
				fogVal = Vector2(visRange,0).length();
				fogVal *= make_tiny;
				Moo::FogHelper::setFogEnd( fogVal ); 
			}
			else
			{
				fogVal = Moo::rc().fogFar() * make_tiny;
				Moo::FogHelper::setFogEnd( fogVal ); 
			}		

			Moo::FogHelper::setFogColour( oldFogColour ); 

			if ( s_drawbodis && bodis_.size() > 0 )
			{
				uint32 vertexBase = 0;
				Moo::DynamicVertexBufferBase2<SkyVertex>& vb = Moo::DynamicVertexBufferBase2< SkyVertex >::instance();
				if ( vb.lockAndLoad( &bodis_.front(), bodis_.size(), vertexBase ) &&
					 SUCCEEDED(vb.set( 0 )) )
				{
					Moo::rc().drawIndexedPrimitive(
						D3DPT_TRIANGLELIST, 
						vertexBase, 
						0, 
						bodis_.size(), 
						lockIndex, 
						bodis_.size()/2);
				}
			}
		}
	}

	Moo::FogHelper::setFogStart( Moo::rc().fogNear() ); 
 	Moo::FogHelper::setFogEnd( Moo::rc().fogFar() ); 

	Moo::rc().pop();

	Moo::rc().setVertexShader( NULL );
	Moo::rc().setRenderState( D3DRS_SPECULARENABLE, TRUE );
	Moo::rc().setRenderState( D3DRS_CLIPPING, TRUE  );
}


/**
 *	This function decides whether or not there will be any lightning,
 *	and what form it will take if there will be. If it can draw it,
 *	it does.
 *
 *	@return The source (x,y,z) and remoteness (w) of any thunder.
 *		A remoteness of >= 1 means no thunder.
 */
Vector4 Sky::decideLightning( float dTime )
{
	Vector4	thunder( 0, 0, 0, 100 );

	// if we were doing lightning before, we're not any more
	if (avgDensity_ == -1) avgDensity_ = 0.f;

	// when conflict is full on, we have a 70% chance of
	// lighting/thunder every second.
	if (rand() * conflict_ < RAND_MAX * 0.7 * dTime ) return thunder;

	// find a dark cloud
	std::vector<CloudSpec*> possibles;
	for (uint c = 0; c < cloudStrata[0].clouds.size(); c++)
	{
		if (cloudStrata[0].clouds[c].midLum < 128+192/4)
		{
			possibles.push_back( &cloudStrata[0].clouds[c] );
		}
	}

	if (!possibles.size()) return thunder;

	CloudSpec & cs = *possibles[rand() % possibles.size()];

	// choose the type of lighting - sheet/intracloud/forked, and do it
	bool shouldFlashAmbient = false;
	float litype = float(rand() & 0xF);
	if (litype >= 13)	// sheet lightning / forked lightning
	{
		if (litype == 15)
		{
			// create an omni light at the centre of it
			// set something to draw it later... intersect with collision
			// scene if it exists :)

			float straySz = ((cs.radii.x + cs.radii.y)/2) * 0.5f;
			Vector2 topPos(
				cs.position.x + rand()*straySz/RAND_MAX,
				cs.position.y + rand()*straySz/RAND_MAX );

			if (topPos.length() < 500)	// TODO: should be set to 80% of farplane
				shouldFlashAmbient = true;

			Vector3 flashpoint(
				topPos[0],
				(1-topPos.length()/visRange)*cloudStrata[0].height,
				topPos[1] );

			//dprintf( "Doing a lightning strike...\n" );
			this->lightningStrike( flashpoint );
			//dprintf( "Done.\n" );

			// just make this cloud light
			cs.lightning = 1;

			// and we'll have some thunder too, thanks
			thunder = Vector4(
				flashpoint + Moo::rc().invView().applyToOrigin(),
				topPos.length() / visRange );
		}
		else
		{
			// make all the clouds light...
			for (uint c = 0; c < cloudStrata[0].clouds.size(); c++)
				if (cloudStrata[0].clouds[c].midLum < 128+192/4)
					cloudStrata[0].clouds[c].lightning = 1;
		}

		// flash the whole scene too if we have sufficient cover
		if (avgCover_ > 0.7 && litype == 15 && shouldFlashAmbient)
		{
			// basically want to set sun colour to fluorescent white
			// .. and something like that to the sky gradient
			avgDensity_ = -1.f;
		}
	}
	else					// intracloud lightning
	{
		cs.lightning = 1;
	}

	return thunder;
}


struct LightningFork
{
	Vector3	pos;
	Vector3	dir;
	float			width;
};

/**
 *	Create a lightning strike from the given point down
 */
void Sky::lightningStrike( const Vector3 & top )
{
	static Moo::Material	mat;

	if (mat.fogged())
	{
		Moo::TextureStage	ts1, nots;

		ts1.pTexture( Moo::TextureManager::instance()->get(s_lightningBmpName, true, true, true, "texture/environment") );
			//"maps/system/Col_white.bmp" ) );
		ts1.colourOperation( Moo::TextureStage::MODULATE,
			Moo::TextureStage::CURRENT,
			Moo::TextureStage::TEXTURE );
		ts1.alphaOperation( Moo::TextureStage::MODULATE,
			Moo::TextureStage::CURRENT,
			Moo::TextureStage::TEXTURE );
		mat.addTextureStage( ts1 );

		mat.addTextureStage( nots );
		mat.fogged( false );

		mat.alphaBlended( true );
		mat.srcBlend( Moo::Material::ONE );
		mat.destBlend( Moo::Material::ONE );
	}

	Matrix		vprg = Moo::rc().view();
	vprg.translation( Vector3(0,0,0) );
	vprg.postMultiply( Moo::rc().projection() );

	Vector3 zrot = Moo::rc().view().applyToUnitAxisVector(2);
	zrot[1] = 0.f;
	zrot.normalise();

	// define and seed a stack of lightning forks
	static VectorNoDestructor<LightningFork>	stack;
	stack.clear();

	LightningFork seed;
	seed.pos = top;
	seed.dir = Vector3( 0, -1, 0 );
	seed.width = 10.f + rand() * 6.f / RAND_MAX;
	stack.push_back( seed );

	float	forkWidths[16];

	int forkTotal = 0;

	// fork until the stack is empty.
	//  termination guarantee is that width always decreases
	while (!stack.empty())
	{
		forkTotal++;
		if (forkTotal > 1024) break;

		LightningFork lf = stack.back();
		stack.pop_back();

		// figure out how many forks we're going to make.
		//  this depends on width. for large width, only 1 or two, with
		//  a strong bias to one being much bigger than the other
		//  for medium width, anywhere from 1 to 3
		//  for small width, 0 or 1
		int nforks = 0;
		if (lf.width > 8.f)
		{
			nforks = int(1.f + rand() * 1.9f / RAND_MAX);
		}
		else if (lf.width > 2.f)
		{
			nforks = int(1.f + rand() * 2.9f / RAND_MAX);
		}
		else
		{
			nforks = int(rand() * 1.9f / RAND_MAX);
		}

		// make up some unscaled width elements
		float	sumFWs = 0.f;
		for (int f = 0; f < nforks; f++)
		{
			float newFW = float(rand()) / RAND_MAX;
			forkWidths[f] = newFW;
			sumFWs += newFW;
		}

		// make sure width adds up to a little less than it is now
		if (lf.width > 1.f)
		{
			sumFWs *= lf.width/(lf.width-0.2f);
		}

		// invent and draw each fork
		for (int f = 0; f < nforks; f++)
		{
			// figure a direction
			Vector3 newdir(
				lf.dir[0] + (rand() * 0.5f / RAND_MAX) - 0.25f,
				lf.dir[1] + (rand() * 0.5f / RAND_MAX) - 0.25f,
				lf.dir[2] + (rand() * 0.5f / RAND_MAX) - 0.25f );
			newdir.normalise();

			// and a length (from 6 to 10m per segment)
			float newlen = 6.f + rand() * 4.f / RAND_MAX;

			// make a new record
			LightningFork nlf;
			nlf.pos = lf.pos + newdir * newlen;
			nlf.dir = newdir;
			nlf.width = forkWidths[f] * lf.width / sumFWs;
			if (f==0 && lf.width > 8.f)
			{
				nlf.width = lf.width;
				if (nlf.dir[1] > -0.5) nlf.dir = Vector3(0,-1,0);
			}

			// draw it from lf.pos to nlf.pos
			Moo::VertexTDSUV2	v[4];
			for (int i = 0; i < 4; i++)
			{
				vprg.applyPoint( v[i].pos_, Vector4(
					((i&2)?nlf.pos[0]:lf.pos[0]) +
						((i&1)?0.5f:-0.5f) * ((i&2)?nlf.width:lf.width) * zrot[2],
					((i&2)?nlf.pos[1]:lf.pos[1]),
					((i&2)?nlf.pos[2]:lf.pos[2]) +
						((i&1)?0.5f:-0.5f) * ((i&2)?nlf.width:lf.width) * zrot[0],
					1 ) );

				float maxz = v[i].pos_.w * 0.999f;
				v[i].pos_.z = min( maxz, v[i].pos_.z );

				v[i].colour_ = 0xffffffff;
				v[i].specular_ = 0xffffffff;

				v[i].uv_.x = (i&2)?1.f:0.f;
				v[i].uv_.y = (i&1)?1.f:0.f;
			}

			//Todo: find a better solution for this.
//			Moo::rc().addSortedTriangle( SortedTriangle( &v[0], &v[1], &v[2], &mat ) );
//			Moo::rc().addSortedTriangle( SortedTriangle( &v[3], &v[2], &v[1], &mat ) );

			// and add it to the stack if it's worthy
			if (nlf.width >= 1.f && nlf.pos[1] > -100.f) stack.push_back(nlf);
			// TODO: don't add it to the stack if it's under the terrain...
			//  or if the line to it intersects the scene...
			// (actually, kinda cool to leave it intersecting the scene :)
		}
	}
}


/**
 *	Update our internal parameters based on the input weather settings
 */
void Sky::update( const struct WeatherSettings & ws, float dTime, Vector3 sunDir, uint32 sunCol, float sunAngle )
{
	if (!enabled_)
		return;
	cloudStrata[0].colourMin = ws.colourMin;
	cloudStrata[0].colourMax = ws.colourMax;
	cloudStrata[0].cover = ws.cover;
	cloudStrata[0].cohesion = ws.cohesion;
	conflict_ = ws.conflict;
	temperature_ = ws.temperature;
	this->prepareClouds(dTime, sunDir, sunCol, sunAngle);
}


/**
 *	An inclusive interval from start to end
 */
struct Interval
{
	float	start;	// [
	float	end;	// ]

	Interval( float s, float e ) : start(s), end(e) {}
};

/**
 *	This class is a sorted list of non-overlapping continuous intervals.
 *
 *	Intervals can be added and deleted from the list, and traversed in
 *	the usual vector way. The 'sorted non-overlapping' constraint is
 *	defined by the condition that for every two adjacent intervals,
 *	i and j, i->end is strictly less than j->start.
 */
class Intervals : public std::vector<Interval>
{
public:
	void add( const Interval & iv );
	void del( const Interval & iv );
};

/**
 * TODO: to be documented.
 */
struct Line2D
{
	Vector2		a,	b;
	uint32		colour;
	Line2D( Vector2 ia, Vector2 ib, uint32 ic = 0x00FF0000 ) :
		a(ia), b(ib), colour(ic) {}
};

// Generate some cloud specifications
bool Sky::generateCloudSpecs( float dTime )
{
	bool anyChanges = false;
	std::vector<Line2D>	remlines;

	for (int s = 0; s < STRATA_COUNT; s++)
	{
		CloudStratum & stratum = cloudStrata[s];
		{
			memoryCounterSub( cloud );
			memoryClaim( stratum.clouds );
		}

		float propOverlap = stratum.cohesion * 5 / 6;

		Intervals overline;
		//overline.add( Interval( -visRange - propOverlap*visRange/2,
		//	+visRange + propOverlap*visRange/2 ) );
		overline.add( Interval( -visRange, +visRange ) );

		// figure out how much cover we have now,
		float curCover = 0;
		// and see if there are any clouds waiting in the wings,
		bool busy = false;
		// or that have now passed out of sight and should be removed
		for (uint c = 0; c < stratum.clouds.size(); c++)
		{
			CloudSpec & cs = stratum.clouds[c];

			float sizeMax = max(cs.radii.x,cs.radii.y);
			Vector2 closestPoint = cs.position + Vector2( sizeMax, sizeMax );
			if ((closestPoint.y + sizeMax) < -visRange)
			{	
				stratum.clouds.erase( stratum.clouds.begin() + c );
				c--;
				continue;
			}
			if (closestPoint.y-sizeMax*propOverlap*2 > visRange)
			{				
				overline.del( Interval( cs.position.x - sizeMax*(1.f-propOverlap),
					cs.position.x + sizeMax*(1.f-propOverlap) ) );				
				busy = true;
			}
			//dprintf( "Closest point of %f: %f\n", cs.rotation, closestPoint.y );

			float avgLen = sqrtf( cs.radii.x * cs.radii.y );
			float overLen = (-visRange) - (cs.position.y - avgLen);
			curCover += avgLen * (overLen > 0 ? max(0.f,avgLen-overLen) : avgLen);

			if (drawHUD_)
			{
				remlines.push_back( Line2D(cs.position+Vector2(-sizeMax,-sizeMax),
					cs.position+Vector2(-sizeMax,+sizeMax) ) );
				remlines.push_back( Line2D(cs.position+Vector2(-sizeMax,+sizeMax),
					cs.position+Vector2(+sizeMax,+sizeMax) ) );
				remlines.push_back( Line2D(cs.position+Vector2(+sizeMax,+sizeMax),
					cs.position+Vector2(+sizeMax,-sizeMax) ) );
				remlines.push_back( Line2D(cs.position+Vector2(+sizeMax,-sizeMax),
					cs.position+Vector2(-sizeMax,-sizeMax) ) );
			}
		}
		curCover /= fullArea;

		//dprintf( "cover %f of %f, %s\n", curCover, stratum.cover, busy?"busy":"ready" );

		if (drawHUD_) for (Intervals::iterator it = overline.begin();
			it != overline.end();
			it++)
		{
			remlines.push_back( Line2D( Vector2(it->start,visRange),
				Vector2(it->end,visRange), 0x00ffff00 ) );
		}

		// if it's too small then we should add some clouds
		if (curCover < stratum.cover)
		{
			CloudSpec cs;
			cs.texture = 0;
			cs.rotation = float(rand()) * MATH_PI * 2.f / RAND_MAX;
			float maxCloudRadius = (expf(stratum.cohesion+1)/expf(2))*visRange*0.7f;
			cs.radii.x = (expf(1+float(rand())/RAND_MAX)/expf(2))*maxCloudRadius,
			cs.radii.y = cs.radii.x/2 + rand()*
				min(cs.radii.x,maxCloudRadius-cs.radii.x/2)/RAND_MAX;
			float sizeMax = max(cs.radii.x,cs.radii.y);
			cs.position = Vector2(
				-visRange + rand()*(visRange*2-sizeMax*2)/RAND_MAX + sizeMax,
				visRange + sizeMax*2 );
			cs.midLum = uint16( 128 + 192 * (stratum.colourMin +
				rand()*(stratum.colourMax-stratum.colourMin)/RAND_MAX) );
			cs.botLum = cs.midLum > 32 ? cs.midLum-32 : 0;
			cs.lightning = 0.f;

			// if we're adding a cloud, try to fit it in to the biggest interval
			if (busy)
			{
				// find the biggest interval
				Interval maxi(0,0);
				for (Intervals::iterator it = overline.begin();
					it != overline.end();
					it++)
				{
					if (it->end - it->start > maxi.end - maxi.start) maxi = *it;
				}
				// does it fit?
				if (sizeMax*2 < maxi.end - maxi.start)
				{
					// yay! move it over then
					cs.position.x = maxi.start + rand()*
						((maxi.end-maxi.start)-sizeMax*2)/RAND_MAX + sizeMax;
					busy = false;
				}
			}

			if (!busy)
			{
				stratum.clouds.push_back( cs );
				anyChanges = true;
			}			
		}

		{
			memoryCounterAdd( cloud );
			memoryClaim( stratum.clouds );
		}
	}

	if (drawHUD_)
	{
		Matrix	invVP = Moo::rc().viewProjection();
		invVP.invert();
		for (uint i=0; i < remlines.size(); i++)
		{
			Geometrics::drawLine(
				invVP.applyPoint( Vector3( remlines[i].a[0], remlines[i].a[1], 0 )/(visRange*2) ),
				invVP.applyPoint( Vector3( remlines[i].b[0], remlines[i].b[1], 0 )/(visRange*2) ),
				remlines[i].colour );
		}
	}

	return anyChanges;
}


float Sky::windSpeed(int stratum) const
{
	return cloudStrata[stratum % STRATA_COUNT].windSpeed.y;
}


bool Sky::drawHUD_ = false;


void Intervals::add( const Interval & iv )
{
	if (!this->size())
	{
		this->insert( this->begin(), iv );
		return;
	}

	uint i = 0;
	for (; i < this->size(); i++)
	{
//		Interval * pI = this->begin()+i;
		std::vector< Interval >::iterator pI = this->begin() + i;
		if (pI->start >= iv.start)			// starts before this segment
		{
			if (pI->start > iv.end)			// wholly between segments
			{
				this->insert( pI, iv );
				return;
			}
			// overlaps this segment
			pI->start = iv.start;		// extend to start
			break;
		}
		else if (pI->end >= iv.start)		// starts inside this segment
		{
			// overlaps this segment
			break;
		}
	}

	// now extend segment i to iv.end, eating anything in the way
	while ((this->begin()+i)->end < iv.end)
	{
//		Interval * pI = this->begin()+i;
		std::vector< Interval >::iterator pI = this->begin()+i;

		if (i < this->size()-1)				// there's another segment
		{
			std::vector< Interval >::iterator pJ = pI+1;
			if (pJ->start <= iv.end)		// overlaps this segment
			{
				pI->end = pJ->end;
				this->erase( pJ );
			}
			else							// ends before this segment
			{
				pI->end = iv.end;
			}
		}
		else								// no more segments
		{
			pI->end = iv.end;
		}
	}
}



void Intervals::del( const Interval & iv )
{
	uint i = 0;

	// find the segment that the deletion begins at
	while (i < this->size() && (this->begin()+i)->end < iv.start) i++;

	// if it falls between two then there's nothing to do
	if (i >= this->size() || (this->begin()+i)->start >= iv.end) return;

	// if we start inside this interval then insert a truncated copy
	if ((this->begin()+i)->start < iv.start)
	{
		Interval civ = *(this->begin()+i);
		civ.end = iv.start;
		this->insert( this->begin()+i, civ );
		i++;
	}

	// eat all the intervals between i and iv.end
	while (i < this->size())
	{
//		Interval * pI = this->begin()+i;
		std::vector<Interval>::iterator pI = this->begin()+i;

		if (pI->end <= iv.end)			// we delete this interval completely
		{
			this->erase( pI );
		}
		else							// we keep this interval
		{
			if (pI->start < iv.end)			// but we truncate it
			{
				pI->start = iv.end;
			}
			// ok, we're done then
			break;
		}
	}
}
