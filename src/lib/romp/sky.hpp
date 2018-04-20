/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SKY_HPP
#define SKY_HPP

#include "cstdmf/stdmf.hpp"

#include "moo/vertex_formats.hpp"

#include "moo/managed_texture.hpp"
#include "moo/material.hpp"
#include "sky_light_map.hpp"
#include <d3d9types.h>
#include <vector>

typedef std::vector<struct CloudEdge> CloudEdges;

typedef std::vector<int> CloudLevels;
typedef Moo::VertexXYZDSUV SkyVertex;
typedef VectorNoDestructor<SkyVertex> SkyVertexVector;

class EnviroMinder;
class PhotonOccluder;
class SkyLightMap;

typedef SmartPointer< class ShaderSet > ShaderSetPtr;

/**
 * TODO: to be documented.
 */
struct PixArray
{
	PixArray( int iwidth, int iheight ) : width(iwidth), height(iheight)
	{
		pData = new uint32[iwidth*iheight];
	}
	~PixArray()
	{
		delete [] pData;
	}

	uint32 * operator[]( int y )
	{
		return pData + y * width;
	}

	int width;
	int height;
	uint32 * pData;
};


/**
 *  This class encapsulates the graphics setup required for rendering sky boxes
 *	and similar elements. It's used in both EnviroMinder and in ModelEditor
 *	directly when loading skyboxes as models, so changes here need to be tested
 *	when editing skyboxes in ModelEditor.
 */
class SkyBoxScopedSetup
{
public:
	SkyBoxScopedSetup();
	~SkyBoxScopedSetup();

private:
	D3DVIEWPORT9 oldVp_;
};


/**
 * TODO: to be documented.
 */
class Sky : public SkyLightMap::IContributor
{
public:
	Sky( );
    ~Sky( );	

	void			activate( const EnviroMinder&, DataSectionPtr, SkyLightMap* );
	void			deactivate( const EnviroMinder&, SkyLightMap* );

	void			draw();	

	Vector4	decideLightning( float dTime );

	//void			newCloud( int size );

	void			update( const struct WeatherSettings & ws, float dTime, Vector3 sunDir, uint32 sunCol, float sunAngle );
	void			updateLightMap( SkyLightMap* l );

	float			avgDensity() const		{ return avgDensity_; }
	const Vector3 &	precipitation() const	{ return precipitation_; }
	float			windSpeed( int stratum = 0 ) const;

	//SkyLightMap::Contributor interface
	bool needsUpdate();
	void render( SkyLightMap* lightMap,
		Moo::EffectMaterialPtr material,
		float sunAngle );

private:	
	float			avgCover() const		{ return avgCover_; }
	void			prepareClouds( float dTime, Vector3 sunDir, uint32 sunCol, float sunAngle );
//	Moo::Material*	material1_;
//	Moo::Material*	material2_;
//	Moo::BaseTexturePtr clouds_;

	//void generateCloud( CloudEdges & cloud, CloudLevels & levels, int growSteps );

	void			setRenderState();
	void			init3();
	void			lightningStrike( const Vector3 & top );
	bool			generateCloudSpecs( float dTime );	

	float			sunAlpha_;
	PhotonOccluder	*pPhotonOccluder_;
	static bool		drawHUD_;

	float			avgCover_;
	float			avgDensity_;
	Vector3	precipitation_;		// rain, snow, hail
	float			conflict_;
	float			temperature_;
	ShaderSetPtr	cloudSet_;
	SkyVertexVector edges_;
	SkyVertexVector bodis_;
	std::vector<uint16> indxs_;
	bool			enabled_;

	//these are needed just for sky light map update.  remove and make
	//local to update fn, once sky light map update is made generic.
	bool			anyNewClouds_;	//any new clouds created in last update?
	Vector2			frameOffset_;	//cloud movement for the last frame.
	float			sunAngle_;

	Moo::Material	*pEdgeMat_;
	Moo::Material	*pBodyMat_;
	PixArray		*pBodyPix_;

	DataSectionPtr	settings_;
};
//---------------------------------------------------------------------------
#endif
