/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef EDITOR_CHUNK_FLARE_HPP
#define EDITOR_CHUNK_FLARE_HPP


#include "worldeditor/config.hpp"
#include "worldeditor/forward.hpp"
#include "worldeditor/world/items/editor_chunk_substance.hpp"
#include "chunk/chunk_flare.hpp"


/**
 *	This class is the editor version of a ChunkFlare
 */
class EditorChunkFlare : public EditorChunkSubstance<ChunkFlare>, public Aligned
{
	DECLARE_EDITOR_CHUNK_ITEM( EditorChunkFlare )
public:
	EditorChunkFlare();
	~EditorChunkFlare();

	bool load( DataSectionPtr pSection, Chunk* pChunk, std::string* errorString = NULL );

	virtual bool edSave( DataSectionPtr pSection );

	virtual const Matrix & edTransform();
	virtual bool edTransform( const Matrix & m, bool transient );

	virtual bool edEdit( class ChunkItemEditor & editor );

	#if UMBRA_ENABLE
	virtual void tick( float /*dTime*/ );
	#endif // UMBRA_ENABLE

	virtual void draw();

	Moo::Colour colour() const;
	void colour( const Moo::Colour & c );

	std::string flareResGet() const				{ return flareRes_; }
	void flareResSet( const std::string & res )	{ flareRes_ = res; }

	static void fini();
	
	virtual void syncInit();

	virtual bool edIsSnappable() { return false; }

private:
	EditorChunkFlare( const EditorChunkFlare& );
	EditorChunkFlare& operator=( const EditorChunkFlare& );

	virtual const char * sectName() const	{ return "flare"; }
	virtual const char * drawFlag() const	{ return "render/drawChunkFlares"; }
	virtual ModelPtr reprModel() const;

	std::string		flareRes_;

	Matrix			transform_;

	#if UMBRA_ENABLE
	// Pointer to whichever of the big small icons umbra should use.
	ModelPtr	currentUmbraModel_;
	#endif
};


typedef SmartPointer<EditorChunkFlare> EditorChunkFlarePtr;


#endif // EDITOR_CHUNK_FLARE_HPP
