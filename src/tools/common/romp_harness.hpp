/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

//---------------------------------------------------------------------------

#ifndef romp_test_harnessH
#define romp_test_harnessH

#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"
#include "resmgr/datasection.hpp"

class EnviroMinder;
class TimeOfDay;

class RompHarness : public PyObjectPlus
{
	Py_Header( RompHarness, PyObjectPlus )
public:
	RompHarness( PyTypePlus * pType = &s_type_ );
	~RompHarness();

    bool	init();
	void	changeSpace();
    void	initWater( DataSectionPtr pProject );

    void	setTime( float t );
	void	setSecondsPerHour( float sph );
	void	fogEnable( bool state );
	bool	fogEnable() const;
    void	setRainAmount( float r );
    void	propensity( const std::string& weatherSystemName, float amount );

    void	update( float dTime, bool globalWeather );
    void	drawPreSceneStuff( bool sparkleCheck = false, bool renderEnvironment = true );
	void	drawDelayedSceneStuff( bool renderEnvironment = true );
    void	drawPostSceneStuff( bool showWeather = true, bool showFlora = true, bool showFloraShadowing = false );

	bool	useShimmer() const	{ return useShimmer_; }

    TimeOfDay*	timeOfDay() const;
	EnviroMinder& enviroMinder() const;

	//-------------------------------------------------
	//Python Interface
	//-------------------------------------------------
	PyObject *	pyGetAttribute( const char * attr );
	int			pySetAttribute( const char * attr, PyObject * value );

	//methods
	PY_METHOD_DECLARE( py_setTime )
	PY_METHOD_DECLARE( py_setSecondsPerHour )
	PY_METHOD_DECLARE( py_setRainAmount )
	PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( bool, fogEnable, fogEnable )
    
private:
	void		disturbWater();
	float		dTime_;
	class Bloom* bloom_;
	bool		useBloom_;
	bool		canBloom_;
	class HeatShimmer* shimmer_;
	class Distortion* distortion_;
	bool		useShimmer_;
	bool		canShimmer_;
    bool		inited_;
    Vector3		waterMovement_[2];
};
//---------------------------------------------------------------------------
#endif
