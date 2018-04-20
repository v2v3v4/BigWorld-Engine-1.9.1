/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// clouds.ipp

#ifdef CODE_INLINE
#define INLINE inline
#else
#define INLINE
#endif


INLINE class SkyLightMap& Clouds::lightMap()
{
	return *lightMap_;
}

INLINE PhotonOccluder & Clouds::photonOccluder()
{
	return *pPhotonOccluder_;
}

INLINE float Clouds::avgCover() const
{
	return avgCover_;
}

INLINE float Clouds::avgDensity() const
{
	return 1.f - lightDim_.length();
}

INLINE const Vector3 & Clouds::avgColourDim() const
{
	return lightDim_;
}

INLINE const Vector3 &	Clouds::precipitation() const
{
	return precipitation_;
}

INLINE float Clouds::avgFogMultiplier() const
{
	return fog_;
}

INLINE float Clouds::windSpeed( int stratum ) const
{
	return 0;
	//TODO : put this back in
	//return cloudStrata[stratum % STRATA_COUNT].windSpeed.y;
}

/**
 *	This method returns the texture name for the lower cloud strata.
 */
INLINE const std::string& Clouds::Rule::lowerStrata() const
{
	return lowerStrata_;
}


/**
 *	This method returns the texture name for the upper cloud strata.
 */
INLINE const std::string& Clouds::Rule::upperStrata() const
{
	return upperStrata_;
}


/**
 *	This method returns the average density of the cloud layer.  It is
 *	currently determined by how much the light is dimmed by these clouds.
 */
INLINE float Clouds::Rule::avgDensity() const
{
	return 1.f - light_.length();
}


/**
 *	This method returns how much the light should be dimmed by these clouds.
 */
INLINE Vector3 Clouds::Rule::avgColourDim() const
{
	return light_;
}


/** 
 *	This method returns how much the fog should be brought in by these clouds.
 */
INLINE float Clouds::Rule::fog() const
{
	return fog_;
}


// clouds.ipp
