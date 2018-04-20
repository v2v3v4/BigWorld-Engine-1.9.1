/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef EDITOR_CHUNK_PARTICLE_SYSTEM_HPP
#define EDITOR_CHUNK_PARTICLE_SYSTEM_HPP


#include "worldeditor/config.hpp"
#include "worldeditor/forward.hpp"
#include "particle/chunk_particles.hpp"
#include "editor_chunk_substance.hpp"
#include <map>
#include <set>
#include <string>

/**
 *	This class is the editor version of a ChunkParticleSystem
 */
class EditorChunkParticleSystem : public EditorChunkSubstance<ChunkParticles>
{
	DECLARE_EDITOR_CHUNK_ITEM( EditorParticleSystem )

	static std::map<std::string, std::set<EditorChunkParticleSystem*> > editorChunkParticleSystem_;
	static void add( EditorChunkParticleSystem*, const std::string& resourceName );
	static void remove( EditorChunkParticleSystem* );
	using ChunkParticles::load;
	static uint32 s_settingsMark_;
public:
	static void reload( const std::string& filename );

	EditorChunkParticleSystem();
	~EditorChunkParticleSystem();

	bool load( DataSectionPtr pSection, Chunk* chunk, std::string* errorString = NULL );

	virtual bool edShouldDraw();
	virtual void draw();

	virtual bool edSave( DataSectionPtr pSection );

	virtual const Matrix & edTransform();
	virtual bool edTransform( const Matrix & m, bool transient );

	virtual bool edIsSnappable() { return false; }

	virtual bool edEdit( class ChunkItemEditor & editor );

	virtual std::vector<std::string> edCommand( const std::string& path ) const;
	virtual bool edExecuteCommand( const std::string& path, std::vector<std::string>::size_type index );

	virtual void drawBoundingBoxes( const BoundingBox &bb, const BoundingBox &vbb, const Matrix &spaceTrans ) const;

	static void fini();

private:
	EditorChunkParticleSystem( const EditorChunkParticleSystem& );
	EditorChunkParticleSystem& operator=( const EditorChunkParticleSystem& );

	virtual const char * sectName() const	{ return "particles"; }
	virtual const char * drawFlag() const	{ return "render/drawParticleSystems"; }
	virtual ModelPtr reprModel() const;

	std::string		resourceName_;
	DataSectionPtr  pOriginalSect_;
};


typedef SmartPointer<EditorChunkParticleSystem> EditorChunkParticleSystemPtr;



#endif // EDITOR_CHUNK_PARTICLE_SYSTEM_HPP
