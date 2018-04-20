/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef EDITOR_CHUNK_LIGHT_HPP
#define EDITOR_CHUNK_LIGHT_HPP


#include "worldeditor/config.hpp"
#include "worldeditor/forward.hpp"
#include "worldeditor/world/items/editor_chunk_substance.hpp"
#include "worldeditor/world/static_lighting.hpp"
#include "chunk/chunk_light.hpp"


/**
 * We extend Aligned here, as all the editor chunk lights have Matrix members
 */
template<class BaseLight>
class EditorChunkLight : public EditorChunkSubstance<BaseLight>, public Aligned
{
public:
	virtual void edPreDelete()
	{
		markInfluencedChunks();
		EditorChunkItem::edPreDelete();
	}

	virtual void edPostCreate()
	{
		markInfluencedChunks();
		this->syncInit();
	}

	virtual void markInfluencedChunks()
	{
		if (pChunk_)
			StaticLighting::markChunk( pChunk_ );
	}

	virtual bool load( DataSectionPtr pSection )
	{
		if (EditorChunkSubstance<BaseLight>::load( pSection ))
		{
			loadModel();
			return true;
		}
		else
		{
			return false;
		}
	}

	ModelPtr reprModel() const 
	{ 
		int renderLargeProxy = Options::getOptionInt( strLargeProxy_, 1 );
		
		if ( renderLargeProxy || (strLargeProxy_ == "") )
		{
			return model_; 
		}
		else
		{
			return modelSmall_; 
		}

		return NULL;
	}
	
	virtual void syncInit();

	virtual bool edIsSnappable() { return false; }

protected:
	ModelPtr model_;
	ModelPtr modelSmall_;
	std::string strLargeProxy_; //string of checkbox to query if the large version of the model should be shown
	virtual void loadModel() = 0;
	Matrix	transform_;
};


/**
 * A ChunkLight containing a light that moo knows about, ie everything but
 * ambient
 */
template<class BaseLight>
class EditorChunkMooLight : public EditorChunkLight<BaseLight>
{
	static uint32 s_settingsMark_;
public:
	EditorChunkMooLight() : staticLight_( true ) {}

	virtual void toss( Chunk * pChunk )
	{
		if (pChunk_ != NULL)
		{
			StaticLighting::StaticChunkLightCache & clc =
				StaticLighting::StaticChunkLightCache::instance( *pChunk_ );

			clc.lights()->removeLight( pLight_ );
		}

		this->EditorChunkSubstance<BaseLight>::toss( pChunk );

		if (pChunk_ != NULL)
		{
			StaticLighting::StaticChunkLightCache & clc =
				StaticLighting::StaticChunkLightCache::instance( *pChunk_ );

			if (staticLight())
				clc.lights()->addLight( pLight_ );
		}
	}

	virtual bool load( DataSectionPtr pSection )
	{
		staticLight_ = pSection->readBool( "static", true );
		return EditorChunkLight<BaseLight>::load( pSection );
	}

	virtual bool edShouldDraw()
	{
		if( !EditorChunkLight<BaseLight>::edShouldDraw() )
			return false;
		if (!Options::getOptionInt("render/proxys", 1) || !Options::getOptionInt("render/proxys/lightProxys", 1))
			return false;

		int drawStatic = Options::getOptionInt( "render/proxys/staticLightProxys", 1 );
		if (drawStatic && staticLight())
			return true;

		int drawDynamic = Options::getOptionInt( "render/proxys/dynamicLightProxys", 1 );
		if (drawDynamic && dynamicLight())
			return true;

		int drawSpecular = Options::getOptionInt( "render/proxys/specularLightProxys", 1 );
		if (drawSpecular && specularLight())
			return true;

		if( drawStatic && drawDynamic && drawSpecular && 
			!staticLight() && !dynamicLight() && !specularLight() )
			return true;

		return false;
	}

	void staticLight( const bool s )
	{
		if (s != staticLight_)
		{
			if (pChunk_)
			{
				StaticLighting::StaticChunkLightCache & clc =
					StaticLighting::StaticChunkLightCache::instance( *pChunk_ );

				if (s)
					clc.lights()->addLight( pLight_ );
				else
					clc.lights()->removeLight( pLight_ );

				markInfluencedChunks();
			}

			staticLight_ = s;
		}
	}

	bool staticLight() const	{ return staticLight_; }

	// Get and Set functions for AccessorDataProxy
	bool staticLightGet() const				{ return staticLight(); }
	bool staticLightSet( const bool& b )	{ staticLight( b ); loadModel(); return true; }

	bool dynamicLightGet() const			{ return dynamicLight(); }
	bool dynamicLightSet( const bool& b )	{ dynamicLight( b ); loadModel(); return true; }

	bool specularLightGet() const			{ return specularLight(); }
	bool specularLightSet( const bool& b )	{ specularLight( b ); loadModel(); return true; }
protected:
	bool	staticLight_;
};

template<class BaseLight> uint32 EditorChunkMooLight<BaseLight>::s_settingsMark_ = -16;

/**
 * A ChunkLight containing a light that moo knows about, and exists somewhere
 * in the world, ie, neither ambient nor directional
 */
template<class BaseLight>
class EditorChunkPhysicalMooLight : public EditorChunkMooLight<BaseLight>
{
public:
	virtual void markInfluencedChunks()
	{
		if (pChunk_)
			StaticLighting::markChunks( pChunk_, pLight_ );
	}
};


/**
 *	This class is the editor version of a chunk directional light
 */
class EditorChunkDirectionalLight :
	public EditorChunkMooLight<ChunkDirectionalLight>
{
	DECLARE_EDITOR_CHUNK_ITEM( EditorChunkDirectionalLight )
public:

	virtual bool load( DataSectionPtr pSection );
	virtual bool edSave( DataSectionPtr pSection );

	virtual bool edEdit( class ChunkItemEditor & editor );

	virtual const Matrix & edTransform();
	virtual bool edTransform( const Matrix & m, bool transient );

	virtual const char * sectName() const { return "directionalLight"; }
	virtual const char * drawFlag() const { return "render/drawChunkLights"; }

	Moo::DirectionalLightPtr	pLight()	{ return pLight_; }

	float getMultiplier() const { return pLight_->multiplier(); }
	bool setMultiplier( const float& m ) { pLight_->multiplier(m); markInfluencedChunks(); return true; }

protected:
	virtual void loadModel();

};


/**
 *	This class is the editor version of a chunk omni light
 */
class EditorChunkOmniLight : public EditorChunkPhysicalMooLight<ChunkOmniLight>
{
	DECLARE_EDITOR_CHUNK_ITEM( EditorChunkOmniLight )
public:

	virtual bool load( DataSectionPtr pSection );
	virtual bool edSave( DataSectionPtr pSection );

	virtual bool edEdit( class ChunkItemEditor & editor );

	virtual const Matrix & edTransform();
	virtual bool edTransform( const Matrix & m, bool transient );

	virtual const char * sectName() const { return "omniLight"; }
	virtual const char * drawFlag() const { return "render/drawChunkLights"; }

	Moo::OmniLightPtr	pLight()	{ return pLight_; }

	float getMultiplier() const { return pLight_->multiplier(); }
	bool setMultiplier( const float& m ) { pLight_->multiplier(m); markInfluencedChunks(); return true; }

protected:
	virtual void loadModel();

};


/**
 *	This class is the editor version of a chunk spot light
 */
class EditorChunkSpotLight : public EditorChunkPhysicalMooLight<ChunkSpotLight>
{
	DECLARE_EDITOR_CHUNK_ITEM( EditorChunkSpotLight )
public:

	virtual bool load( DataSectionPtr pSection );
	virtual bool edSave( DataSectionPtr pSection );

	virtual bool edEdit( class ChunkItemEditor & editor );

	virtual const Matrix & edTransform();
	virtual bool edTransform( const Matrix & m, bool transient );

	virtual const char * sectName() const { return "spotLight"; }
	virtual const char * drawFlag() const { return "render/drawChunkLights"; }

	Moo::SpotLightPtr	pLight()	{ return pLight_; }

	float getMultiplier() const { return pLight_->multiplier(); }
	bool setMultiplier( const float& m ) { pLight_->multiplier(m); markInfluencedChunks(); return true; }

	virtual bool edShouldDraw();

protected:
	virtual void loadModel();

};


/**
 *	This class is the editor version of a chunk omni light
 */
class EditorChunkPulseLight : public EditorChunkPhysicalMooLight<ChunkPulseLight>
{
	DECLARE_EDITOR_CHUNK_ITEM( EditorChunkPulseLight )
public:

	virtual bool load( DataSectionPtr pSection );
	virtual bool edSave( DataSectionPtr pSection );

	virtual bool edEdit( class ChunkItemEditor & editor );

	virtual const Matrix & edTransform();
	virtual bool edTransform( const Matrix & m, bool transient );

	virtual const char * sectName() const { return "pulseLight"; }
	virtual const char * drawFlag() const { return "render/drawChunkLights"; }

	Moo::OmniLightPtr	pLight()	{ return pLight_; }

	float getMultiplier() const { return pLight_->multiplier(); }
	bool setMultiplier( const float& m ) { pLight_->multiplier(m); markInfluencedChunks(); return true; }

	virtual bool edShouldDraw();

protected:
	virtual void loadModel();
};


/**
 *	This class is the editor version of a chunk ambient light
 */

class EditorChunkAmbientLight :
	public EditorChunkLight<ChunkAmbientLight>
{
	DECLARE_EDITOR_CHUNK_ITEM( EditorChunkAmbientLight )
public:

	bool load( DataSectionPtr pSection );
	virtual bool edSave( DataSectionPtr pSection );

	virtual bool edEdit( class ChunkItemEditor & editor );

	virtual const Matrix & edTransform();
	virtual bool edTransform( const Matrix & m, bool transient );

	virtual const char * sectName() const { return "ambientLight"; }
	virtual const char * drawFlag() const { return "render/drawChunkLights"; }

	EditorChunkAmbientLight *	pLight()	{ return this; }

	const Moo::Colour & colour() const			{ return colour_; }
	void colour( const Moo::Colour & c );

	virtual void toss( Chunk * pChunk );

	float getMultiplier() const { return multiplier(); }
	bool setMultiplier( const float& m );

	virtual bool edShouldDraw();

protected:
	virtual void loadModel();

};


#endif // EDITOR_CHUNK_LIGHT_HPP
