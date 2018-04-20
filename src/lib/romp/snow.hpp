/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SNOW_HPP
#define SNOW_HPP

#include "moo/moo_math.hpp"
#include "moo/node.hpp"

class PyMetaParticleSystem;
class SourcePSA;
class CameraMatrixLiaison;

struct WeatherSettings;

/**
 *	This class renders snow.
 */
class Snow
{
public:
	Snow();
	~Snow();

	void tick( float dTime );
	void draw();

	void amount( float a );
	float amount();

	void update( const struct WeatherSettings & ws, bool playerDead );

	void addAttachments( class PlayerAttachments & pa );

private:

	void createSnowFlakeSystem();
	void createColdBreathSystem();

	PyMetaParticleSystem*	pSnowFlakes_;

	float					flakesMaxRate_;
	float					flakesMaxAge_;

	Moo::NodePtr			cameraNode_;
	CameraMatrixLiaison *	cameraLiaison_;

	float					amount_;
	float					amountSmallFor_;

	Vector3					wind_;

	PyMetaParticleSystem*	pColdBreath_;
	SourcePSA *				pColdBreathSource_;
};



#endif // SNOW_HPP