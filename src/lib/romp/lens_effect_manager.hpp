/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef LENS_EFFECT_MANAGER_HPP
#define LENS_EFFECT_MANAGER_HPP

#include "lens_effect.hpp"
#include "moo/effect_material.hpp"
#include <set>

class PhotonOccluder;

/**
 *	This class manages and draws lens effects.
 *
 *	During the rendering pass, an object can
 *	register a lens effect with the LensEffectManager.
 *	If that lens effect is already alive, it will be
 *	refreshed.
 *	If this is a new lens effect, then it will fade in.
 *
 *	During LensEffectManager::tick, it will decay the
 *	age of current lens effects, and cull any lens
 *	effects that no longer exist.
 *
 *	During LensEffectManager::draw, it will perform
 *	line-of-sight checks on the scene, and ask
 *	individual lens effects to draw.
 *
 *	The lens effect manager uses plug-in photon occluder
 *	classes to perform line-of-sight checks.
 */
class LensEffectManager
{
public:
	LensEffectManager();
	~LensEffectManager();

	void finz();

	static LensEffectManager & instance();

	virtual void tick( float dTime );
	virtual void draw();

	//Lens Effects use this method, because there is currently
	//no global material manager.  The LensEffectManager
	//manages the lens effect materials

	//the returned pointer should not be held onto.
	Moo::EffectMaterialPtr getMaterial( const std::string& material );

	void	add( uint32 id,
				 const Vector3 & worldPosition,
				 const LensEffect & le );

	void	forget( uint32 id );
	void	clear( void );
	void	killFlares( const std::set<uint32> & ids );
	
	void	addPhotonOccluder( PhotonOccluder * occluder );
	void	delPhotonOccluder( PhotonOccluder * occluder );

	void	preload( const std::string& material );

	static bool s_partitcle_editor_;
	
	static uint32	s_drawCounter_;	

private:
	void	killOld( void );
	float	flareVisible( LensEffect & l );

	float	dTime_;

	//a vector of lens effects
	typedef std::vector<LensEffect>	LensEffects;
	LensEffects	lensEffects_;

	//lens flare materials are managed.
	class Materials : public std::map< std::string, Moo::EffectMaterialPtr >
	{
	public:
		~Materials();
		void clear();
		iterator get( const std::string & resourceID, bool reportError = false );
	};
	Materials materials_;

	//and a vector of photon occluders
	typedef std::vector< PhotonOccluder * >	PhotonOccluders;
	PhotonOccluders		photonOccluders_;

	LensEffectManager(const LensEffectManager&);
	LensEffectManager& operator=(const LensEffectManager&);
};

#ifdef CODE_INLINE
#include "lens_effect_manager.ipp"
#endif

#endif
/*lens_effect_manager.hpp*/
