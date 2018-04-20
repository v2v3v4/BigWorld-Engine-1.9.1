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

#pragma warning (disable:4355)	// 'this' : used in base member initialiser list
#pragma warning (disable:4503)	// 'identifier' : decorated name length exceeded, name was truncated
#pragma warning (disable:4786)	// 'identifier' : identifier was truncated to 'number' characters in the debug information

#include "sink_psa.hpp"
#include "particle/particle_system.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk.hpp"

DECLARE_DEBUG_COMPONENT2( "Particle", 0 )

#ifndef CODE_INLINE
#include "sink_psa.ipp"
#endif


ParticleSystemActionPtr SinkPSA::clone() const
{
    return ParticleSystemAction::clonePSA(*this);
}

// -----------------------------------------------------------------------------
// Section: Overrides to the Particle System Action Interface.
// -----------------------------------------------------------------------------

PROFILER_DECLARE( SinkPSA_execute, "PSA Sink Execute" );

/**
 *	This method executes the action for the given frame of time. The dTime
 *	parameter is the time elapsed since the last call.
 *
 *	@param particleSystem	The particle system on which to operate.
 *	@param dTime			Elapsed time in seconds.
 */
void SinkPSA::execute( ParticleSystem &particleSystem, float dTime )
{
	BW_GUARD_PROFILER( SinkPSA_execute );

	// Do nothing if the dTime passed was zero or if the particle system
	// is not quite old enough to be active.
	if ( ( age_ < delay() ) || ( dTime <= 0.0f ) )
	{
		age_ += dTime;
		return;
	}

	float sz = 0.01f;
	if (particleSystem.pRenderer() && particleSystem.pRenderer()->knowParticleRadius())
		sz = particleSystem.pRenderer()->particleRadius();

	if ( (maximumAge() >= 0.0f) || (minimumSpeed() >= 0.0f) || outsideOnly_ )
	{
		bool remove = false;
		float minSpdSqrd = (minimumSpeed() >= 0 ) ? minimumSpeed() * minimumSpeed() : -1.f;
		//note : can't save the end() during iteration because particles
		//may be erased, thus moving the end iterator. ( i.e. this is ok )
		Particles::iterator current = particleSystem.begin();
		while ( current != particleSystem.end() )
		{
			Particle &particle = *current;
			remove = false;

			if ( particle.isAlive() )
			{
				Vector3 velocity;
				particle.getVelocity( velocity );
				
				if ((particle.age() > maximumAge()) ||
					(velocity.lengthSquared() < minSpdSqrd))
				{
					remove = true;
				}
				else if (outsideOnly_)
				{
					//note - particles get moved after the actions, so find out
					//where the particle will be before checking if its inside.
					Vector3 newPos;
					particleSystem.predictPosition( particle, dTime, newPos );

					if (particleSystem.isLocal())
					{
						newPos = particleSystem.worldTransform().applyPoint( newPos );
					}

					if ( !particleSystem.pRenderer()->isMeshStyle() )
					{						
						//'size' of sprite particles is its radius, however since
						//they can rotate, we need to get the length of the
						//hypotenuse.  They're square and so times by root 2.
						const float root2 = 1.415f;
						float radius = particle.size() * root2;
						remove = this->isIndoors( newPos, radius );
					}
					else
					{	
						//'size' of mesh particles is calculated by the renderer,
						//and has already been adjusted to be the length of the
						//hypotenuse of the bounding box.
						//(see base mesh particle renderer)
						remove = this->isIndoors( newPos, sz );
					}					
				}			
			}
				
			if (remove)
			{
				if ( !particleSystem.forcingSave() )
					particleSystem.removeFlareID( ParticleSystem::getUniqueParticleID( particle, particleSystem ) );
				current = particleSystem.removeParticle( current );				
			}
			else
			{
				current++;
			}
		}
	}
}

/**
 *	This is the serialiser for SinkPSA properties
 */
void SinkPSA::serialiseInternal(DataSectionPtr pSect, bool load)
{
	BW_GUARD;
	SERIALISE(pSect, maximumAge_, Float, load);
	SERIALISE(pSect, minimumSpeed_, Float, load);
	SERIALISE(pSect, outsideOnly_, Bool, load );
}


/**
 *	This method is a private helper method. It finds whether
 *	the box centred on pos, with radius radius, intersects an
 *	indoor area.  Currently it does 8 checks, on the corner of
 *	the box.  This will break with large radii.
 */
bool SinkPSA::isIndoors( const Vector3& pos, float radius ) const
{
	BW_GUARD;
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (!pSpace)
	{
		return false;
	}

	//First check the middle point
	Chunk* pChunk = pSpace->findChunkFromPoint( pos );
	if (pChunk && !pChunk->isOutsideChunk())
	{
		return true;
	}

	if (radius <= 0.001f)
	{
		return false;
	}

	//Then check the AABB corners.
	Vector3 corners[2];
	Vector3 curr;
	corners[0] = pos - Vector3(radius,radius,radius);
	corners[1] = pos + Vector3(radius,radius,radius);

	for (uint32 i=0; i<2; i++)
	{
		for (uint32 j=0; j<2; j++)
		{			
			for (uint32 k=0; k<2; k++)
			{	
				curr.x = corners[i].x;
				curr.y = corners[j].y;
				curr.z = corners[k].z;

				Chunk* pChunk = pSpace->findChunkFromPoint( curr );
				if (pChunk && !pChunk->isOutsideChunk())
				{
					return true;
				}				
			}			
		}		
	}

	return false;
}

// -----------------------------------------------------------------------------
// Section: Python Interface to the PySinkPSA.
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( PySinkPSA )


/*~ function Pixie.SinkPSA
 *	Factory function to create and return a new PySinkPSA object. SinkPSA is a
 *	ParticleSystemAction that destroys particles within a ParticleSystem. 
 *	@return A new PySinkPSA object.
 */
PY_FACTORY_NAMED( PySinkPSA, "SinkPSA", Pixie )

PY_BEGIN_METHODS( PySinkPSA )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( PySinkPSA )
	/*~ attribute PySinkPSA.maximumAge
	 *	Particles whose time in existence is greater then maximumAge are
	 *	removed from the ParticleSystem. Default -1.0. This attribute is ignored
	 *	if value is < 0.
	 *	@type Float.
	 */
	PY_ATTRIBUTE( maximumAge )
	/*~ attribute PySinkPSA.minimumSpeed
	 *	Particles with a speed of less than minimumSpeed are removed from the
	 *	ParticleSystem. Default -1.0. This attribute is ignored if value is < 0.
	 *	@type Float.
	 */
	PY_ATTRIBUTE( minimumSpeed )
PY_END_ATTRIBUTES()

/**
 *	This is an automatically declared method for any derived classes of
 *	PyObjectPlus. It connects the readable attributes to their corresponding
 *	C++ components and searches the parent class' attributes if not found.
 *
 *	@param attr		The attribute being searched for.
 */
PyObject *PySinkPSA::pyGetAttribute( const char *attr )
{
	BW_GUARD;
	PY_GETATTR_STD();

	return PyParticleSystemAction::pyGetAttribute( attr );
}

/**
 *	This is an automatically declared method for any derived classes of
 *	PyObjectPlus. It connects the writable attributes to their corresponding
 *	C++ components and searches the parent class' attributes if not found.
 *
 *	@param attr		The attribute being searched for.
 *	@param value	The value to assign to the attribute.
 */
int PySinkPSA::pySetAttribute( const char *attr, PyObject *value )
{
	BW_GUARD;
	PY_SETATTR_STD();

	return PyParticleSystemAction::pySetAttribute( attr, value );
}

/**
 *	This is a static Python factory method. This is declared through the
 *	factory declaration in the class definition.
 *
 *	@param args		The list of parameters passed from Python. This should
 *					be two floats - maximumAge and minimumSpeed.
 */
PyObject *PySinkPSA::pyNew( PyObject *args )
{
	BW_GUARD;
	float maximumAge = -1.0f;
	float minimumSpeed = -1.0f;

	if ( PyArg_ParseTuple( args, "|ff", &maximumAge, &minimumSpeed ) )
	{
		SinkPSAPtr pAction = new SinkPSA( maximumAge, minimumSpeed );
		return new PySinkPSA(pAction);
	}
	else
	{
		PyErr_SetString( PyExc_TypeError, "SinkPSA:"
			"Expected two numbers." );
		return NULL;
	}
}


PY_SCRIPT_CONVERTERS( PySinkPSA )
