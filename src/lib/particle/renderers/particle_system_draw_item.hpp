/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PARTICLE_SYSTEM_DRAW_ITEM_HPP
#define PARTICLE_SYSTEM_DRAW_ITEM_HPP


#include "moo/visual_channels.hpp"
#include "particle/particle.hpp"
#include "particle/particles.hpp"
#include "math/matrix.hpp"


class   SpriteParticleRenderer;


/**
 * TODO: to be documented.
 */
class ParticleSystemDrawItem : public Moo::ChannelDrawItem, public Aligned
{
public:
	ParticleSystemDrawItem
    ( 
        SpriteParticleRenderer      *renderer, 
        const Matrix&               worldTransform, 
        Particles::iterator         beg, 
        Particles::iterator         end, 
        float                       distance 
    );

	~ParticleSystemDrawItem();

	void draw();

	void fini();

private:
	SpriteParticleRenderer          *renderer_;
	Matrix	                        worldTransform_;
	Particles::iterator             beg_;
	Particles::iterator             end_;
};


#endif // PARTICLE_SYSTEM_DRAW_ITEM_HPP
