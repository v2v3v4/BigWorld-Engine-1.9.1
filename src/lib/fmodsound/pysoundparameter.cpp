/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pysoundparameter.hpp"

DECLARE_DEBUG_COMPONENT2( "PySoundParameter", 0 );

#if FMOD_SUPPORT
# include <fmod_errors.h>
#endif

/*~ class NoModule.PySoundParameter
 *
 *  A PySoundParameter provides access to sound event parameters and is
 *  basically a partial interface to FMOD::EventParameter.  For more information
 *  about event parameters and how they are used, please see the FMOD Designer API
 *  documentation and the FMOD Designer User Manual, both available from
 *  www.fmod.org.
 */
PY_TYPEOBJECT( PySoundParameter );

PY_BEGIN_METHODS( PySoundParameter )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( PySoundParameter )

	/*~ attribute PySoundParameter.min
	 *
	 *  The minimum legal value for this parameter.
	 *
	 *  @type	Float
	 */
	PY_ATTRIBUTE( min )

	/*~ attribute PySoundParameter.max
	 *
	 *  The maximum legal value for this parameter.
	 *
	 *  @type	Float
	 */
	PY_ATTRIBUTE( max )

	/*~ attribute PySoundParameter.value
	 *
	 *  The current value of this parameter.
	 *
	 *  @type	Float
	 */
	PY_ATTRIBUTE( value )

	/*~ attribute PySoundParameter.velocity
	 *
	 *  The current velocity of this parameter.  Please see the documentation
	 *  for FMOD::EventParameter::setVelocity() for details on the mechanics and
	 *  usage of EventParameter velocities.
	 *
	 *  @type	Float
	 */
	PY_ATTRIBUTE( velocity )

	/*~ attribute PySoundParameter.name
	 *
	 *  The name of this parameter.
	 */
	PY_ATTRIBUTE( name )

PY_END_ATTRIBUTES()

PY_SCRIPT_CONVERTERS_DECLARE( PySoundParameter )
PY_SCRIPT_CONVERTERS( PySoundParameter )

PySoundParameter::PySoundParameter( SoundManager::EventParameter * pParam,
	PySound * pSound, PyTypePlus * pType ) :
	PyObjectPlus( pType ),
	pParam_( pParam ),
	minimum_( 0 ),
	maximum_( 0 ),
	pSound_( pSound ),
	pEvent_( pSound->pEvent() ),
	index_( 0 )
{
	BW_GUARD;	
#if FMOD_SUPPORT
	FMOD_RESULT result;

	result = pParam_->getRange( &minimum_, &maximum_ );
	if (result != FMOD_OK)
	{
		ERROR_MSG( "PySoundParameter::PySoundParameter: "
			"Couldn't get min/max for %s: %s\n",
			this->name(), FMOD_ErrorString( result ) );
	}

	result = pParam_->getInfo( &index_, NULL );

	if (result != FMOD_OK)
	{
		ERROR_MSG( "PySoundParameter::PySoundParameter: "
			"Couldn't get index: %s\n",
			FMOD_ErrorString( result ) );
	}
#endif
}


PySoundParameter::~PySoundParameter()
{
}

#if FMOD_SUPPORT

/**
 *  Getter for parameter value.
 */
float PySoundParameter::value()
{
	BW_GUARD;
	if (!this->refresh())
		return -1;

	float value;
	FMOD_RESULT result = pParam_->getValue( &value );

	if (result == FMOD_OK)
	{
		return value;
	}
	else
	{
		ERROR_MSG( "PySoundParameter::value( %s ): %s\n",
			this->name(), FMOD_ErrorString( result ) );
		return -1;
	}
}


/**
 *  Setter for parameter value.
 */
void PySoundParameter::value( float value )
{
	BW_GUARD;
	if (!this->refresh())
		return;

	if (value < minimum_ || value > maximum_)
	{
		ERROR_MSG( "PySoundParameter::value( %s ): "
			"Value %f is outside valid range [%f,%f]\n",
			this->name(), value, minimum_, maximum_ );
		return;
	}

	FMOD_RESULT result = pParam_->setValue( value );

	if (result != FMOD_OK)
	{
		ERROR_MSG( "PySoundParameter::value( %s ): %s\n",
			this->name(), FMOD_ErrorString( result ) );
	}
}


/**
 *  Getter for parameter velocity.
 */
float PySoundParameter::velocity()
{
	BW_GUARD;
	if (!this->refresh())
		return -1;

	float velocity;
	FMOD_RESULT result = pParam_->getVelocity( &velocity );
	if (result == FMOD_OK)
	{
		return velocity;
	}
	else
	{
		ERROR_MSG( "PySoundParameter::velocity( %s ): %s\n",
			this->name(), FMOD_ErrorString( result ) );
		return -1;
	}
}

/**
 *  Setter for parameter velocity.
 */
void PySoundParameter::velocity( float velocity )
{
	BW_GUARD;
	if (!this->refresh())
		return;

	FMOD_RESULT result = pParam_->setVelocity( velocity );
	if (result != FMOD_OK)
	{
		ERROR_MSG( "PySoundParameter::velocity( %s ): %s\n",
			this->name(), FMOD_ErrorString( result ) );
	}
}


/**
 *  Get the name of this parameter.  Uses memory managed by FMOD so don't expect
 *  it to be around for long.
 */
const char *PySoundParameter::name() const
{
	BW_GUARD;
	static const char *err = "<error>";
	char *name;

	FMOD_RESULT result = pParam_->getInfo( NULL, &name );
	if (result == FMOD_OK)
		return name;
	else
	{
		ERROR_MSG( "PySoundParameter::name: %s\n", FMOD_ErrorString( result ) );
		return err;
	}
}


/**
 *  Ensure that the FMOD::EventParameter* handle in this object actually
 *  corresponds to the FMOD::Event* stored in the PySound associated with this
 *  object.
 */
bool PySoundParameter::refresh()
{
	BW_GUARD;
	// Make sure the sound is up-to-date
	if (!pSound_->refresh( PySoundParameter::REFRESH_MASK ))
		return false;

	// If the Event* hasn't changed, we can break now
	if (pSound_->pEvent() == pEvent_)
		return true;

	// If we haven't returned yet, then we need to get a new reference.
	pEvent_ = pSound_->pEvent();
	FMOD_RESULT result = pEvent_->getParameterByIndex( index_, &pParam_ );

	if (result == FMOD_OK)
	{
		return true;
	}
	else
	{
		ERROR_MSG( "PySoundParameter::refresh: "
			"Couldn't re-acquire parameter handle for %s: %s\n",
			this->name(), FMOD_ErrorString( result ) );
		return false;
	}
}

#else // FMOD_SUPPORT

float PySoundParameter::value()
{
	return -1;
}

void PySoundParameter::value( float val )
{
}

float PySoundParameter::velocity()
{
	return -1;
}

void PySoundParameter::velocity( float val )
{
}

const char *PySoundParameter::name() const
{
	return "<FMOD support disabled, all sounds calls will fail>";
}

#endif // FMOD_SUPPORT


PyObject* PySoundParameter::pyGetAttribute( const char* attr )
{
	BW_GUARD;
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}


int PySoundParameter::pySetAttribute( const char* attr, PyObject* value )
{
	BW_GUARD;
	PY_SETATTR_STD();

	return PyObjectPlus::pySetAttribute( attr, value );
}

// pysoundparameter.cpp
