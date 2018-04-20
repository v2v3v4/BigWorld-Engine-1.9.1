/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CLOUDS_HPP
#define CLOUDS_HPP

#include "cstdmf/stdmf.hpp"
#include "math/linear_animation.hpp"
#include "resmgr/datasection.hpp"

class EnviroMinder;
class SkyLightMap;
class PhotonOccluder;

namespace Moo
{
	class Visual;
	typedef SmartPointer<Visual> VisualPtr;
}

/**
 * TODO: to be documented.
 */
class Clouds
{
public:
	Clouds( );
    ~Clouds( );

	static void		init();
	static void		fini();

	void			settings( DataSectionPtr pSect );

	void			activate( const EnviroMinder&, DataSectionPtr );
	void			deactivate( const EnviroMinder& );

	void			update( const struct WeatherSettings & ws, float dTime, Vector3 sunDir, uint32 sunCol, float sunAngle );
	void			updateLightMap( SkyLightMap* lightMap );
	void			draw();

	//The clouds own three helper classes, the sky light map,
	//the sky photon occluder, and lightning.
	SkyLightMap &	lightMap();
	PhotonOccluder & photonOccluder();
	float			avgCover() const;
	float			avgDensity() const;
	float			avgFogMultiplier() const;
	const Vector3 & avgColourDim() const;
	const Vector3 &	precipitation() const;
	float			windSpeed( int stratum = 0 ) const;

private:		
	class Rule
	{
	public:
		Rule( DataSectionPtr pSection );
		const std::string& lowerStrata() const;
		const std::string& upperStrata() const;
		float correlation( const WeatherSettings& ws ) const;
		float avgDensity() const;
		Vector3 avgColourDim() const;
		float fog() const;
	private:
		std::string	lowerStrata_;
		std::string	upperStrata_;
		Vector3		position_;
		Vector3		light_;				// how much to dim the sunlight by
		float		fog_;
	};

	PhotonOccluder	*pPhotonOccluder_;	
	void			chooseBestMatch(const WeatherSettings& ws);
	void			doCloudsTransition( float dTime );

	float			avgCover_;	
	Vector3			precipitation_;		// rain, snow, hail	
	float			temperature_;

	//animations to affect the lighting / fogging
	float			lightingTransition_;
	float			lightingTransitionTime_;
	LinearAnimation< float	>	fogAnimation_;
	float			fog_;
	LinearAnimation< Vector3 >	lightDimAnimation_;
	Vector3			lightDim_;	

	SkyLightMap*	lightMap_;
	Moo::EffectMaterialPtr	material_;
	Moo::VisualPtr	visual_;

	//animations to affect the clouds
	float			cloudsTransition_;
	float			cloudsTransitionTime_;
	std::vector<Clouds::Rule*>	rules_;
	Clouds::Rule*	current_;	

	DataSectionPtr	settings_;
};

#ifdef CODE_INLINE
#include "clouds.ipp"
#endif

#endif	//CLOUDS_HPP
