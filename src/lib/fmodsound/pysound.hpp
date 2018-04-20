/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PY_SOUND_HPP
#define PY_SOUND_HPP

#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"
#include "pyscript/script_math.hpp"
#include "pyscript/stl_to_py.hpp"

#include "soundmanager.hpp"
#include "duplo/pymodel.hpp"

/**
 *  A PySound is a wrapper for an FMOD::Event.  It can be used to trigger and
 *  re-trigger a sound event, and provides an interface for inspecting various
 *  attributes of a sound event.
 */
class PySound : public PyObjectPlus
{
	Py_Header( PySound, PyObjectPlus )

public:
	PySound( SoundManager::Event * pEvent, const std::string &path,
		PyTypePlus* pType = &PySound::s_type_ );

private:
	~PySound();

public:
	// PyObjectPlus overrides
	PyObject*			pyGetAttribute( const char * attr );
	int					pySetAttribute( const char * attr, PyObject * value );

	PyObject* param( const std::string& name );
	PY_AUTO_METHOD_DECLARE( RETOWN, param, ARG( std::string, END ) );

	bool play();
	PY_AUTO_METHOD_DECLARE( RETDATA, play, END );

	bool stop();
	PY_AUTO_METHOD_DECLARE( RETDATA, stop, END );

	float volume();
	void volume( float newValue );
	PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( float, volume, volume );

	float duration();
	PY_RO_ATTRIBUTE_DECLARE( duration(), duration );

	const char *name() const;
	PY_RO_ATTRIBUTE_DECLARE( name(), name );

	std::string state();
	PY_RO_ATTRIBUTE_DECLARE( state(), state );

	PyObject * pyGet_position();
	int pySet_position( PyObject * position );

	SoundManager::Event * pEvent() { return pEvent_; }
	void setModel( PyModel * pModel ) { pModel_ = pModel; }

	bool refresh( SoundManager::EventState okMask = FMOD_EVENT_STATE_READY );
	
	void reset();

protected:
	SoundManager::Event * pEvent_;

	// The group this sound belongs to
	SoundManager::EventGroup * pGroup_;

	// The absolute path to this sound, only set when unloading is allowed
	std::string path_;

	// The index this sound resides at in its EventGroup
	int index_;

	// For a 3D sound attached to a model, this is the PyModel that this sound
	// is attached to.
	PyModel * pModel_;

	// The position of a 3D sound not attached to a PyModel.
	Vector3 * pPosition_;

	// Has this sound been played yet?  This is a small optimisation to avoid a
	// duplicate call to PyModel::attachSound() in the call to refresh() during
	// the first playback of this sound
	bool played_;

	// This flag will be true if the the Event needs to be reset. This could
	// be because the SoundBank which it came from has been unloaded, and now
	// the sound needs to be re-loaded from another source.
	bool reset_;
};

typedef SmartPointer< PySound > PySoundPtr;

#endif // PY_SOUND_HPP
