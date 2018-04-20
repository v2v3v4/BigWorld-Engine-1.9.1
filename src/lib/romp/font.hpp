/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef FONT_HPP
#define FONT_HPP

#include "cstdmf/stdmf.hpp"
#include "cstdmf/stringmap.hpp"
#include "math/vector2.hpp"
#include "moo/render_context.hpp"
#include "moo/material.hpp"
#include "moo/base_texture.hpp"
#include "moo/vertex_formats.hpp"
#include "resmgr/datasection.hpp"

/**
 *	This class can be queried for font based information, including
 *	character width, string width and height.
 *
 *	All values are returned in texels, unless otherwise specified.
 */
class FontMetrics : public ReferenceCount
{
public:
	FontMetrics();

	bool load( DataSectionPtr pSection );
	void save( DataSectionPtr pSection );

	std::vector<std::wstring> breakString( std::wstring wstr, /* IN OUT */int& w, int& h,
		int minHyphenWidth = 0, const std::wstring& wordBreak = L" ",
		const std::wstring& punctuation = L",<.>/?;:'\"[{]}\\|`~!@#$%^&*()-_=+" );

	void stringDimensions( const std::string& str, int& w, int& h );
	uint stringWidth( const std::string& str );
	void stringDimensions( const std::wstring& str, int& w, int& h );
	uint stringWidth( const std::wstring& str );
	float charWidth( wchar_t c ) const;
	float clipWidth( wchar_t c ) const;
	float clipHeight() const;
	float height() const;							//in uv coords
	const Vector2& charPosition( wchar_t c ) const;	//in uv coords
	float mapWidth() const;
	float mapHeight() const;
//private:
	uint start_;
	uint end_;
	Vector2 mapDimensions_;
	typedef std::map<wchar_t, uint>	FontLUT;
	FontLUT	charToIdx_;
	std::vector<Vector2> uvs_;	//in texture coordinates (0..1)
	std::vector<float> widths_;	//in texture coordinates (0..1)
	Vector2 effectsMargin_;		//in texture coordinates (0..1)
	float textureMargin_;		//in texture coordinates (0..1)
	float maxWidth_;			//in texture coordinates (0..1)
	float height_;				//in texture coordinates (0..1)
};

typedef SmartPointer<FontMetrics>	FontMetricsPtr;


/**
 *	This static class exists to merely allow procedural creation
 *	of a list of indices, and automatic construction/destruction.
 */
// This class is no longer used since text now renders without
// indices, because drawIndexedPrimitiveUP was causing an unexplained
// flickering on nVidia cards.
// TODO: use indices again if the real cause for the flickering problem
// is found and fixed, or remove it once and for all
/*
class FontIndices
{
public:
	FontIndices();
	~FontIndices();

	uint16* s_indices;
};
*/

/**
 *	This class represents an instance of a font that can draw
 *	immediately to the screen, or into a mesh.
 *	The font class includes all font metrics, and can be drawn
 *	proportionally or fixed.
 *	The only states a Font has is scale, and colour. ( and perhaps
 *	indirectly a font metrics cache )
 */
class Font : public ReferenceCount
{
public:
	//creates a mesh in clip coordinates, ignoring the font scale
	//when using font meshes, simple prepend a matrix transformation.
	virtual float drawIntoMesh(
		VectorNoDestructor<Moo::VertexXYZDUV>& mesh,
		const std::wstring& str,
		float clipX = 0.f,
		float clipY = 0.f,
		float* w = NULL,
		float* h = NULL );
	//creates a mesh in clip coordinates, exactly within the box
	//specified
	virtual void drawIntoMesh(
		VectorNoDestructor<Moo::VertexXYZDUV>& mesh,
		const std::wstring& str,
		float clipX,
		float clipY,
		float w,
		float h,
		float* retW = NULL,
		float* retH = NULL);
	//draws immediately to the screen at the griven character block
	void drawConsoleString( const std::string& str, int col, int row, int x = 0, int y = 0 );
	//draws immediately to the screen at the given pixel position
	virtual void drawString( const std::string& str, int x, int y );
	//draws immediately to the screen at the given pixel position
	virtual void drawWString( const std::wstring& wstr, int x, int y );
	//draws immediately to the screen at the given pixel position with maxWidth
	virtual int drawString( std::wstring wstr, int x, int y, int w, int h,
		int minHyphenWidth = 0, const std::wstring& wordBreak = L" ",
		const std::wstring& punctuation = L",<.>/?;:'\"[{]}\\|`~!@#$%^&*()-_=+"	);
	//draws immediately to the screen at the given world position
	virtual void draw3DString( const std::string& str, const Vector3 & position );
	//draws immediately to the screen at the given world position
	virtual void draw3DWString( const std::wstring& str, const Vector3 & position );

	Moo::BaseTexturePtr pTexture() const	{ return texture_; }

	void colour( uint32 col )	{ colour_ = col; }
	uint32 colour() const			{ return colour_; }
	void scale( const Vector2& s )	{ scale_ = s; }
	const Vector2& scale() const	{ return scale_; }
	//const FontIndices& indices() const	{ return s_fontIndices; }

	void fitToScreen( bool state, const Vector2& numCharsXY );
	bool fitToScreen() const;

	Vector2 screenCharacterSize();

	FontMetrics& metrics();
protected:
	Font( Moo::BaseTexturePtr t, FontMetrics& fm );
	float	makeCharacter( Moo::VertexXYZDUV* vert, wchar_t c, const Vector2& pos );

	Moo::BaseTexturePtr	texture_;
	FontMetrics& metrics_;
	Vector2 scale_;
	uint32	colour_;
	//static FontIndices s_fontIndices;

	//this should go in ConsoleFont
	bool	fitToScreen_;
	Vector2	numCharsXY_;

	//friend used so constructor may be private, ensuring
	//fonts are only created via the FontManager.
	friend class FontManager;
	
private:
	void drawStringInClip( const std::wstring& wstr, const Vector3 & position );

};

typedef SmartPointer<Font>	FontPtr;


/**
 *	This class looks like Font, but it caches a number of meshes of
 *	frequently used strings.
 *
 *	Currently the cache is ever-growing, so use wisely.
 */
class CachedFont : public Font
{
public:
private:
	CachedFont( Moo::BaseTexturePtr t, FontMetrics& fm );

	//friend used so constructor may be private, ensuring
	//fonts are only created via the FontManager.
	friend class FontManager;
};


/**
 *	This font class can interpret simple HTML strings and change the colour, size and font on the fly.
 */
class HTMLFont: public Font
{
};


/**
 *	This class manages font resources.	FontPtrs are handed out freely, and may
 *	be used with wild abandon.  The manager manages textures and fontMetrics classes.
 */
class FontManager
{
public:
	static FontManager& instance();
	
	FontPtr	get( const std::string& resourceName, bool htmlSupport = false, bool cachedMetrics = false );
	void	setMaterialActive( FontPtr pFont );
	const std::string& findFontName( FontPtr pFont );

	void	preCreateAllFonts();

private:
	//This method returns false if GDI is not available.
	FontManager();
	bool	createFont( DataSectionPtr pSection );
	std::string checkFontGenerated( DataSectionPtr fontDataSection );

	struct Resource
	{
		Resource()
		{
			metrics_ = new FontMetrics;
		}
		Moo::BaseTexturePtr	texture_;
		FontMetricsPtr		metrics_;
	};
	StringHashMap<Resource>	fonts_;
	Moo::Material			material_;
};

#ifdef CODE_INLINE
#include "font.ipp"
#endif

#endif