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
#include "font.hpp"
#include "moo/texture_manager.hpp"
#include "moo/render_target.hpp"
#include "custom_mesh.hpp"
#include "cstdmf/debug.hpp"
#include "resmgr/multi_file_system.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/auto_config.hpp"
#include "time.h"

#ifdef _WIN32
#include "windows.h"
#include "Commdlg.h"
#endif


#ifndef CODE_INLINE
#include "font.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "Font",0 );

namespace { // anonymous
void logFontMetrics(const std::string fontName, const FontMetrics & fm)
{
#if FONT_DEBUG
	FILE *fp = fopen("font.log", "a");
	time_t t = time(NULL);	
	tm * timest = localtime(&t);
	fprintf(fp, "--==> %s : %s", fontName.c_str(), asctime(timest));
#endif

	std::vector<float>::const_iterator wiIt  = fm.widths_.begin();
	std::vector<Vector2>::const_iterator uvIt  = fm.uvs_.begin();
	std::vector<Vector2>::const_iterator uvEnd = fm.uvs_.end();
	while (uvIt != uvEnd)
	{

#if FONT_DEBUG
		fprintf(fp, "(%.2f,%.2f, %.2f), ", uvIt->x, uvIt->y, *wiIt);
#endif

		++wiIt;
		++uvIt;
	}

#if FONT_DEBUG
	fprintf(fp, "\n");
	fclose(fp);
#endif

}

} // namespace anonymous


//-----------------------------------------------------------------------------
//Section : FontMetrics
//-----------------------------------------------------------------------------
/**
 *	Font file format
 *
 *	<creation>
 *		<sourceFont>	arial black	</sourceFont>
 *		<sourceFontSize>	12	</sourceFontSize>
 *		<startChar>	32	</startChar>
 *		<endChar>	128		</endChar>
 *		<effectsMargin>	1	</effectsMargin>
 *		<textureMargin>	1	</textureMargin>
 *		<shadowAlpha>	255	</shadowAlpha>
 *		<spaceProxyChar> 105 </spaceProxyChar>
 *	</creation>
 *	<generated>
 *		<maxWidth	16	</maxWidth>
 *		<height>	16	</height>
 *		<positions>	00000b00.....ae23b023	</positions>
 *		<widths> 0a0f0a0a08.....04030a0a	</widths>
 *		<map>	someTextureName.dds	</map>
 *	</generated>
 */

/**
 *	Constructor.
 */
FontMetrics::FontMetrics()
:start_(0),
 end_(0),
 mapDimensions_(0,0),
 maxWidth_(0),
 height_(0)
{
}


/**
 *	This method loads the font metrics from an xml file.
 *
 *	@param	pSection	The data section from a font file.
 *	@return bool		True if the metrics were loaded successfully.
 */
bool FontMetrics::load( DataSectionPtr pSection )
{
	unsigned short i;

	start_ = (unsigned short)pSection->readInt( "creation/startChar", 0 );
	end_ = (unsigned short)pSection->readInt( "creation/endChar", 0 );
	mapDimensions_ = pSection->readVector2( "generated/mapDimensions", Vector2(1,1) );

	effectsMargin_.x = pSection->readFloat( "creation/effectsMargin", 0.f );
	textureMargin_ = pSection->readFloat( "creation/textureMargin", 0.f );
	effectsMargin_.y = effectsMargin_.x;
	effectsMargin_.x /= mapDimensions_.x;
	effectsMargin_.y /= mapDimensions_.y;
	textureMargin_ /= mapDimensions_.x;
	maxWidth_ = (float)pSection->readInt( "generated/maxWidth", 16 );
	maxWidth_ /= mapDimensions_.x;
	height_ = (float)pSection->readInt( "generated/height", 16 );
	height_ /= mapDimensions_.y;
	height_ += effectsMargin_.y;

	unsigned short numChars = end_ - start_ + 1;

	if ( numChars <= 0 )
	{
		ERROR_MSG( "FontMetrics::load failed because numChars was 0 or less\n" );
		return false;
	}

	std::string positions = pSection->readString( "generated/uvs", "" );
	std::string widths = pSection->readString( "generated/widths", "" );

	if ( positions.length() < (uint)(numChars * 8) )
	{
		ERROR_MSG( "FontMetrics::load failed because numChars did not match the position info string\n" );
		return false;
	}

	if ( widths.length() < (uint)(numChars * 4) )
	{
		ERROR_MSG( "FontMetrics::load failed because numChars did not match the width info string\n" );
		return false;
	}

	//The positions string is a long hex string containing position data in the bitmap.
	//There should be 4 hex characters per font character, being xxyy(xpos)(ypos)
	//The widths string is similar, but has only a 2-character hex code per string.
	uvs_.resize(numChars);
	widths_.resize(numChars);

	for ( i = 0; i < (uint)numChars; i++ )
	{
		int posIdx = i*8;	//posIdx goes up by 8, 
		int widIdx = i*4;	//widths go up by 4
		char temp[9];
		temp[0] = positions[posIdx];
		temp[1] = positions[posIdx+1];
		temp[2] = positions[posIdx+2];
		temp[3] = positions[posIdx+3];
		temp[4] = positions[posIdx+4];
		temp[5] = positions[posIdx+5];
		temp[6] = positions[posIdx+6];
		temp[7] = positions[posIdx+7];
		temp[8] = (char)0;

		Vector2 pos((float)((::fromHex(positions[posIdx])<<12) +
							(::fromHex(positions[posIdx+1])<<8) +
							(::fromHex(positions[posIdx+2])<<4) +
							::fromHex(positions[posIdx+3])),
					(float)((::fromHex(positions[posIdx+4])<<12) +
							(::fromHex(positions[posIdx+5])<<8) +
							(::fromHex(positions[posIdx+6])<<4) +
							::fromHex(positions[posIdx+7])) );		

		pos.x /= mapDimensions_.x;
		pos.y /= mapDimensions_.y;

		MF_ASSERT( pos.x >= 0.f );
		MF_ASSERT( pos.x < 1.f );
		MF_ASSERT( pos.y >= 0.f );
		MF_ASSERT( pos.y < 1.f );

		uvs_[i] = pos;
		widths_[i] = (float)((::fromHex(widths[widIdx])<<12) + 
							 (::fromHex(widths[widIdx+1])<<8) +
							 (::fromHex(widths[widIdx+2])<<4) +
							 ::fromHex(widths[widIdx+3]));

		MF_ASSERT( widths_[i] >= 0.f );
		MF_ASSERT( widths_[i] <= 1024.f );	//that's a big character.

		//DEBUG_MSG( "%c %0.2f\n", i + start_, widths_[i] );

		widths_[i] /= mapDimensions_.x;		//store width as normalised.
	}

	//Initialise the lookup table.
	for ( unsigned short i=start_; i<end_; i++ )
	{
		charToIdx_[i] = i-start_;
	}

	return true;
}


/**
 *	This function turns a vector2 vector into a hex string.
 *	@param vec		the vector of Vector2s
 *	@return string	string representing all the vector2 values.
 */
static const std::string& hexFromVector2s( const std::vector<Vector2>& vec )
{
	static std::string s_temp;

	s_temp.clear();

	std::vector<Vector2>::const_iterator it = vec.begin();
	std::vector<Vector2>::const_iterator end = vec.end();

	char buf[5];
	buf[4] = (char)0;

	while ( it != end )
	{
		::toHex( (uint)(*it).x, buf );
		s_temp += buf;
		::toHex( (uint)(*it++).y, buf );
		s_temp += buf;
	}

	s_temp += (char)0;

	return s_temp;
}


/**
 *	This function turns a float vector into a hex string.
 *	@param vec		the vector of floats
 *	@return string	string representing all the float values.
 */
static const std::string& hexFromFloats( const std::vector<float>& vec )
{
	static std::string s_temp;

	s_temp.clear();

	std::vector<float>::const_iterator it = vec.begin();
	std::vector<float>::const_iterator end = vec.end();

	char buf[5];
	buf[4] = (char)0;

	while ( it != end )
	{
		::toHex( (uint)(*it++), buf );
		s_temp += buf;
	}

	s_temp += (char)0;

	return s_temp;
}


/**
 *	This method saves a fontMetrics object to the supplied data section.
 *
 *	@param	pSection		The section to which we save.
 */
void FontMetrics::save( DataSectionPtr pSection )
{
	DataSectionPtr pSect = pSection->openSection( "generated" );
	pSect->writeVector2( "mapDimensions", this->mapDimensions_ );
	pSect->writeInt( "maxWidth", (int)(this->maxWidth_ * this->mapDimensions_.x) );
	pSect->writeInt( "height", (int)(this->height_*this->mapDimensions_.y) );

	//convert uvs from uv coords to map coords and write out.
	uint i;
	uint numChars = this->end_ - this->start_ + 1;

	for ( i=0; i<numChars; i++ )
	{
		uvs_[i] = uvs_[i] * mapDimensions_;
	}
	pSect->writeString( "uvs", ::hexFromVector2s( this->uvs_ ) );

	//convert widths from uv coords to map coords and write out.
	for ( i=0; i<numChars; i++ )
	{
		widths_[i] *= mapDimensions_.x;
	}
	pSect->writeString( "widths", ::hexFromFloats( this->widths_ ) );

	pSection->save();
}

/**
 *	This method breaks a string into a segments for display within a specified
 *	width and height.
 *
 *	This method accepts a long string and the max width, and returns the
 *	result width \& height ( in texels ) of the given string and the
 *	broken string lines.
 *
 *	@param	wstr						The string to break.
 *  @param	w							The desired max width.
 *  @param	h							The desired max height.
 *	@param	wordBreak					The chars that could be used as seperator of word.
 *  @param	punctuation					The chars work as punctuations.
 *  @param	minHyphenWidth				If the width of the rest of line is <= minHyphenWidth, we will use hyphen to break word
 *
 *	@return std::vector<std::string>	Broken string lines.
 */
std::vector<std::wstring> FontMetrics::breakString( std::wstring wstr, /* IN OUT */int& w, int& h,
	int minHyphenWidth, const std::wstring& wordBreak, const std::wstring& punctuation )
{
	std::vector<std::wstring> result;
	uint maxWidth = 0;
	h = 0;
	std::wstring::size_type offset1, offset2;
	while( wstr.find( '\r' ) != wstr.npos || wstr.find( '\n' ) != wstr.npos )
	{
		offset1 = wstr.find( '\r' );
		offset2 = wstr.find( '\n' );
		if( offset1 == wstr.npos || offset2 != wstr.npos && offset2 < offset1 )
			offset1 = offset2;
		int W = w, H;
		std::vector<std::wstring> temp = breakString( wstr.substr( 0, offset1 ), W, H, minHyphenWidth, wordBreak,
			punctuation );
		if( temp.empty() )
		{
			int width, height;
			stringDimensions( L" ", width, height );
			result.push_back( L"" );
			h += height;
		}
		else
		{
			result.insert( result.end(), temp.begin(), temp.end() );
			h += H;
		}
		if( (uint)W >= maxWidth )
			maxWidth = (uint)W;

		wstr.erase( wstr.begin(), wstr.begin() + offset1 );
		if( wstr.size() > 1 && wstr[ 0 ] != wstr[ 1 ] &&
			( wstr[ 1 ] == '\r' || wstr[ 1 ] == '\n' ) )
			wstr.erase( wstr.begin() );
		wstr.erase( wstr.begin() );
	}

	std::vector<std::wstring> words;
	while( !wstr.empty() )
	{
		std::wstring word;
		while( !wstr.empty() && wordBreak.find( wstr[ 0 ] ) != wordBreak.npos )
			wstr.erase( wstr.begin() );
		while( !wstr.empty() && wordBreak.find( wstr[ 0 ] ) == wordBreak.npos )
			word += wstr[ 0 ], wstr.erase( wstr.begin() );
		while( !wstr.empty() && ( punctuation.find( wstr[ 0 ] ) != punctuation.npos ||
			wordBreak.find( wstr[ 0 ] ) != wordBreak.npos ) )
			if( wordBreak.find( wstr[ 0 ] ) != wordBreak.npos )
				wstr.erase( wstr.begin() );
			else
				word += wstr[ 0 ], wstr.erase( wstr.begin() );
		words.push_back( word );
	}

	std::wstring line;
	while( !words.empty() )
	{
		std::wstring suffix = ( line.empty() || wordBreak.empty() ? L"" : wordBreak.substr( 0, 1 ) ) + words[ 0 ];
		if( stringWidth( line + suffix ) <= (uint)w )
		{
			line += suffix;
			words.erase( words.begin() );
		}
		else
		{
			if( stringWidth( line + ( line.empty() || wordBreak.empty() ? L"" : ( wordBreak[ 0 ] + L"-" ) ) )
				< (uint)minHyphenWidth || line.empty() )
			{
				if( !wordBreak.empty() && !line.empty() )
					line += wordBreak[ 0 ];
				while( !words[ 0 ].empty() && stringWidth( line + words[ 0 ][ 0 ] + L'-' ) < (uint)w )
					line += words[ 0 ][ 0 ], words[ 0 ].erase( words[ 0 ].begin() );
				if( !wordBreak.empty() )
					line += L'-';
			}
			int width, height;
			stringDimensions( line, width, height );
			if( (uint)width > maxWidth )
				maxWidth += width;
			h += height;
			result.push_back( line );
			line.clear();
		}
	}
	if( !line.empty() )
	{
		int width, height;
		stringDimensions( line, width, height );
		if( (uint)width > maxWidth )
			maxWidth += width;
		h += height;
		result.push_back( line );
	}
	w = (int)maxWidth;
	return result;
}

/**
 *	This method returns the width, in texels, of the given string.
 *	@param	str		The string to calculate the width.
 *	@return uint	The width of the string, in texels.
 */
uint FontMetrics::stringWidth( const std::string& str )
{
	std::wstring wstr( str.size(), ' ' );
	bw_snwprintf( &wstr[0], wstr.size(), L"%S\0", str.c_str() );
	return stringWidth( wstr );
}


/**
 *	This method returns the width, in texels, of the given string.
 *	@param	str		The string to calculate the width.
 *	@return uint	The width of the string, in texels.
 */
uint FontMetrics::stringWidth( const std::wstring& str )
{
	uint w = 0;
	uint s = str.size();

	//Note about this calculation.
	//
	//Firstly, the charWidth includes the effects margin ( e.g. drop shadow )
	//However, when drawing many characters into a string, the effects margin
	//is overwritten by the next character.
	//
	//We probably should the very last character's effects margin back on,
	//so that the width is exactly right for the string.
	//
	//However, there are two probable ways of using string width.
	//1) for single words in the gui.  in this case, the calculated width will
	//be one pixel too small, meaning any centering calculations will still be ok.
	//2) for single words in the gui, if wrapping the word in a border, then one
	//would expect a margin to be applied anyway, in which case the small
	//inaccuracy won't matter.
	//3) *most importantly* the string width is used to lay out text fields, and
	//is used incrementally ( i.e. calculate the incremental width of a sentence )
	//in this case, we don't want to add the effects margin on at all, because if
	//we did the accumulated addition of effects margins ( that won't be drawn )
	//would end up reported a far greater width than is correct.
	//
	//Therefore, we choose the lowest probable error and don't add on the final
	//margin.
	//
	//TODO : maybe we could just have two functions, one for incremental calcs, and
	//one for single use calcs.
	for ( uint i = 0; i < s; i++ )
	{
		float width = charWidth(str[i]);
		w += (uint)( (width-effectsMargin_.x) * mapDimensions_.x + 0.5f );
	}

	return w;
}

//-----------------------------------------------------------------------------
//Section : FontIndices
//-----------------------------------------------------------------------------
/*FontIndices Font::s_fontIndices;

FontIndices::FontIndices()
{
	s_indices = new uint16[1536];

	int idx=0;

	for ( int i=0; i<1536; i+=6 )	//allow for 256 characters at once.
	{
		s_indices[i] = idx*4;
		s_indices[i+1] = s_indices[i]+1;
		s_indices[i+2] = s_indices[i]+3;

		s_indices[i+3] = s_indices[i]+2;
		s_indices[i+4] = s_indices[i]+3;
		s_indices[i+5] = s_indices[i]+1;
		idx++;
	}
}


FontIndices::~FontIndices()
{
	delete[] s_indices;
}*/

//-----------------------------------------------------------------------------
//Section : Font
//-----------------------------------------------------------------------------

/**
 *	Constructor.
 */
Font::Font( Moo::BaseTexturePtr t, FontMetrics& fm )
:scale_( 1.f, 1.f ),
 texture_( t ),
 metrics_( fm ),
 fitToScreen_( false ),
 numCharsXY_( 120, 40 ),
 colour_( 0xffffffff )
{
}


/**
 *	This method draws the given string into a mesh.  The mesh contains
 *	vertices in clip coordinates, and is anchored at the top left.
 *
 *	Note the text is added to the mesh, so make sure you clear the mesh
 *	first, if that is what you require.
 *
 *  @param	mesh	The mesh to draw into.
 *	@param	str		The string to draw.
 *	@param	clipX	The starting X coordinate in clip space.
 *	@param	clipY	The starting Y coordinate in clip space.
 *	@param	retW	The final X position (NOT width).
 *	@param	retH	The height of the string.
 *
 *	@return	float	The width of the string.
 */
float Font::drawIntoMesh(
	VectorNoDestructor<Moo::VertexXYZDUV>& mesh,
	const std::wstring& str,
	float clipX,
	float clipY,
	float* retW,
	float* retH )
{
	float initialX = clipX;	

	if ( str.size() == 0 )
	{
		return 0.f;
	}

	int base = mesh.size();
	int n = str.size();
	mesh.resize( base + n*6);
	Vector2 pos( clipX, clipY );

	float halfx = Moo::rc().halfScreenWidth();
	float halfy = Moo::rc().halfScreenHeight();
	Vector2 texToClip( metrics_.mapWidth() / halfx, metrics_.mapHeight() / halfy );
	float effectsWidthInClip = metrics_.effectsMargin_.x * texToClip.x;

	for ( int i=0; i<n; i++ )
	{
		pos.x += this->makeCharacter( &mesh[base+i*6], str[i], pos );
		pos.x -= effectsWidthInClip * scale_.x;
	}

	pos.x += effectsWidthInClip;

	if ( retW )
		*retW = pos.x;
	if ( retH )
		*retH = metrics_.height() * texToClip.y;

	return pos.x - initialX;	//assumes no multi-line as yet.
}


/**
 *	This method draws the given string into a mesh.  The mesh contains
 *	vertices in clip coordinates, and is anchored at the top left.  The
 *	required dimensions are passed in, and the string is resized to
 *	exactly fit within the area.
 *
 *  @param	mesh	The mesh to draw into.
 *	@param	str		The string to draw.
 *	@param	clipX	The starting X coordinate in clip space.
 *	@param	clipY	The starting Y coordinate in clip space.
 *	@param	w		The desired width.
 *	@param	h		The desired height.
 *	@param	retW	The width being used is returned in this variable (if provided).
 *	@param	retH	The height being used is returned in this variable (if provided).
 */
void Font::drawIntoMesh(
	VectorNoDestructor<Moo::VertexXYZDUV>& mesh,
	const std::wstring& str,
	float clipX,
	float clipY,
	float w,
	float h,
	float* retW,
	float* retH )
{
	float width,height;
	int base = mesh.size();
	this->drawIntoMesh(mesh,str,clipX,clipY,&width,&height);

	//now resize the vertices to fit the given box.
	if (w!=0.f || h!=0.f)
	{		
		//if w or h is 0, we keep the other dimension explicit,
		//and set the width/height such that the correct aspect
		//ratio is maintained.
		if (w == 0.f && h != 0.f)
			w = h * (width/height);	
		else if (h == 0.f && w != 0.f)
			h = w * (height/width);	

		Vector2 scale( w/width, h/height );
		for (uint32 i=base; i<mesh.size(); i++)
		{
			mesh[i].pos_.x *= scale.x;
			mesh[i].pos_.y *= scale.y;
		}	
	}
	else
	{
		//if passed in (desired) width, height are 0,
		//then we leave the mesh at the optimal size
		w = width;
		h = height;
	}

	if (retW) *retW = w;
	if (retH) *retH = h;
}

/**
 * This method returns the fonts character size which can be used
 * to determine the offsets for each character to be printed on the
 * screen.
 */
Vector2 Font::screenCharacterSize()
{
	int charSizePx = int(metrics_.maxWidth_ * metrics_.mapDimensions_.x);
	int charSizePy = int(metrics_.height_ * metrics_.mapDimensions_.y );
	int effectsWidthInPixels = int(metrics_.effectsMargin_.x * metrics_.mapDimensions_.x);

	Vector2 size( float(charSizePx - effectsWidthInPixels), float(charSizePy) );

	if ( fitToScreen_ )
	{
		//calculate the difference between desired num chars and how big the font is on the screen
		float desiredPx = numCharsXY_.x * (float)charSizePx;
		float actualPx = Moo::rc().screenWidth();
		float desiredPy = numCharsXY_.y * (float)charSizePy;
		float actualPy = Moo::rc().screenHeight();
		size.x *= actualPx / desiredPx;
		size.y *= actualPy / desiredPy;		
	}

	return size;
}

/**
 *	This method draws the string directly onto the screen, using the
 *	current material settings.
 *
 *	@param	str		The string to draw.
 *	@param	x		The x coordinate, in character blocks.
 *	@param	y		The y coordinate, in character blocks.
 */
void Font::drawConsoleString( const std::string& str, int col, int row, int x, int y )
{
	int charSizePx = int(metrics_.maxWidth_ * metrics_.mapDimensions_.x);
	int charSizePy = int(metrics_.height_ * metrics_.mapDimensions_.y );
	int effectsWidthInPixels = int(metrics_.effectsMargin_.x * metrics_.mapDimensions_.x);
	int px = col * (charSizePx - effectsWidthInPixels) + x;
	int py = row * charSizePy + y;
	Vector2 savedScale = scale_;

	if ( fitToScreen_ )
	{
		//calculate the difference between desired num chars and how big the font is on the screen
		float desiredPx = numCharsXY_.x * (float)charSizePx;
		float actualPx = Moo::rc().screenWidth();
		float desiredPy = numCharsXY_.y * (float)charSizePy;
		float actualPy = Moo::rc().screenHeight();
		scale_.x = actualPx / desiredPx;
		scale_.y = actualPy / desiredPy;
	}

	this->drawString( str, px, py );

	scale_ = savedScale;
}


/**
 *	This method draws the string directly onto the screen, using the
 *	current material settings.
 *
 *	@param	str		The string to draw.
 *	@param	x		The x coordinate, in pixels.
 *	@param	y		The y coordinate, in pixels.
 */
void Font::drawString( const std::string& str, int x, int y )
{
	//convert string to unicode
	wchar_t buf[256];
	MF_ASSERT( str.size() < 256 );
	bw_snwprintf( buf, sizeof(buf)/sizeof(wchar_t), L"%S\0", str.c_str() );
	this->drawWString( buf, x, y );
}


/**
 *	This method draws the string directly onto the screen, using the
 *	current material settings.
 *
 *	@param	str		The string to draw.
 *	@param	x		The x coordinate, in pixels.
 *	@param	y		The y coordinate, in pixels.
 */
void Font::drawWString( const std::wstring& str, int x, int y )
{
	if ( !str.size() )
		return;

	//convert pixels to clip
	Vector3 pos(0, 0, 0);
	float halfx = Moo::rc().halfScreenWidth();
	float halfy = Moo::rc().halfScreenHeight();
	pos.x = ((float)x - halfx) / halfx;
	pos.y = (halfy - (float)y) / halfy;

	this->drawStringInClip(str, pos);
}

/**
 *	This method draws the string directly onto the screen with width and height limit, using the
 *	current material settings.
 *
 *	@param	wstr			The string to draw.
 *	@param	x				The x coordinate, in pixels.
 *	@param	y				The y coordinate, in pixels.
 *	@param	w				The max width, in pixels.
 *	@param	h				The max height, in pixels.
 *	@param	minHyphenWidth	If width is smaller than it, we will break the next word by hyphen.
 *	@param	wordBreak		Characters act as word breaks.
 *	@param	punctuation		Characters act as word punctuations.
 *	@return	int				The height of the real string output in pixels
 */
int Font::drawString( std::wstring wstr, int x, int y, int w, int h, int minHyphenWidth,
	const std::wstring& wordBreak, const std::wstring& punctuation )
{
	int H;
	int yOff = 0;
	std::vector<std::wstring> wstrs = metrics().breakString( wstr, w, H, minHyphenWidth, wordBreak, punctuation );
	for( std::vector<std::wstring>::iterator iter = wstrs.begin(); iter != wstrs.end(); ++iter )
	{
		int linew, lineh;
		metrics().stringDimensions( *iter, linew, lineh );
		if( yOff + lineh > h )
			break;
		drawWString( *iter, x, y + yOff );
		yOff += lineh;
	}
	return yOff;
}

void Font::draw3DString( const std::string& str, const Vector3 & position )
{
	//convert string to unicode
	wchar_t buf[256];
	MF_ASSERT( str.size() < 256 );
	bw_snwprintf( buf, sizeof(buf)/sizeof(wchar_t), L"%S\0", str.c_str() );
	this->draw3DWString( buf, position );
}


void Font::draw3DWString( const std::wstring& wstr, const Vector3 & position )
{
	Matrix viewProj = Moo::rc().view();
	viewProj.postMultiply( Moo::rc().projection() );
	Vector3 projectedPos = viewProj.applyPoint(position);
	if ( projectedPos.z <= 1.f )
		this->drawStringInClip(wstr, projectedPos);
}


void Font::drawStringInClip( const std::wstring& wstr, const Vector3 & position )
{
	static CustomMesh<Moo::VertexXYZDUV> mesh;
	mesh.clear();

	//then draw
	this->drawIntoMesh( mesh, wstr.c_str(), position.x, position.y );

	if ( !mesh.size() )
		return;

	Moo::rc().setRenderState( D3DRS_LIGHTING, FALSE );
	Moo::rc().setVertexShader( NULL );
	Moo::rc().setPixelShader( NULL );
	
	if ( SUCCEEDED( Moo::rc().setFVF( Moo::VertexXYZDUV::fvf() ) ) )
	{
		Moo::rc().device()->SetTransform( D3DTS_WORLD, &Matrix::identity );
		Moo::rc().device()->SetTransform( D3DTS_VIEW, &Matrix::identity );
		Moo::rc().device()->SetTransform( D3DTS_PROJECTION, &Matrix::identity );

		// Now rendering with DynamicVertexBuffer....
		Moo::DynamicVertexBufferBase2<Moo::VertexXYZDUV>& vb = Moo::DynamicVertexBufferBase2<Moo::VertexXYZDUV>::instance();
		uint32 lockIndex = 0;
		if ( vb.lockAndLoad( &mesh.front(), mesh.size(), lockIndex ) &&
			 SUCCEEDED(vb.set( 0 )) )
		{
			Moo::rc().drawPrimitive( D3DPT_TRIANGLELIST, lockIndex, mesh.size() / 3);
			vb.unset( 0 );
		}
	}

	Moo::rc().setRenderState( D3DRS_LIGHTING, TRUE );
}

/**
 *	This method puts a character into the vertex mesh.
 *	Six vertices will be made, so make sure that there is room enough.
 *	Vertices are generated ready for render with drawPrimitiveUP.
 *
 *	@param	vert		Pointer to six vertices to be filled.
 *	@param	c			The character to draw into the vertex array.
 *	@param	pos			The clip position to make the character at.
 *
 *	@return	width		The width of the character, in clip coordinates
 *						This includes the effects margin.
 */
float Font::makeCharacter( Moo::VertexXYZDUV* vert, wchar_t c, const Vector2& pos )
{
	float uvWidth = this->metrics_.charWidth(c);
	float uvHeight = this->metrics_.height();
	float halfx = Moo::rc().halfScreenWidth();
	float halfy = Moo::rc().halfScreenHeight();
	Vector2 texToClip( metrics_.mapWidth() / halfx, metrics_.mapHeight() / halfy );
	float clipWidth = uvWidth*texToClip.x*scale_.x;
	float clipHeight = uvHeight*texToClip.y*scale_.y;

	static float s_offsetAmount = -1000.f;
	if ( s_offsetAmount < 0.f )
	{
		s_offsetAmount = 0.5f;

		MF_WATCH(	"Render/Font Offset", s_offsetAmount, Watcher::WT_READ_WRITE, 
					"Offset added to the characters in the font texture so "
					"that each texel get mapped to a pixel on the screen"	);
	}
	float clipOffsetX = -s_offsetAmount / halfx;
	float clipOffsetY = -s_offsetAmount / halfy;

	// setup a four-vertex list with the char's quad coords
	Moo::VertexXYZDUV tmpvert[4];
	tmpvert[0].pos_.x	= pos.x;
	tmpvert[0].pos_.y	= pos.y;
	tmpvert[0].pos_.z	= 0.0f;
	tmpvert[0].uv_		= this->metrics_.charPosition(c);
	tmpvert[0].uv_.y	= tmpvert[0].uv_.y;
	tmpvert[0].colour_	= colour_;

	tmpvert[1].pos_.x	=  pos.x+clipWidth;
	tmpvert[1].pos_.y	= pos.y;
	tmpvert[1].pos_.z	= 0.0f;
	tmpvert[1].uv_.x	= tmpvert[0].uv_.x + uvWidth;
	tmpvert[1].uv_.y	= tmpvert[0].uv_.y;
	tmpvert[1].colour_	= colour_;

	tmpvert[2].pos_.x	= pos.x+clipWidth;
	tmpvert[2].pos_.y	= pos.y-clipHeight;
	tmpvert[2].pos_.z	= 0.0f;
	tmpvert[2].uv_.x	= tmpvert[1].uv_.x;
	tmpvert[2].uv_.y	= tmpvert[1].uv_.y + uvHeight;
	tmpvert[2].colour_	= colour_;

	tmpvert[3].pos_.x	= pos.x;
	tmpvert[3].pos_.y	= pos.y-clipHeight;
	tmpvert[3].pos_.z	= 0.0f;
	tmpvert[3].uv_.x	= tmpvert[0].uv_.x;
	tmpvert[3].uv_.y	= tmpvert[2].uv_.y;
	tmpvert[3].colour_	= colour_;

	//adjust so everything looks nice and crisp
	for ( int i=0; i<4; i++ )
	{
		tmpvert[i].pos_.x += clipOffsetX;
		tmpvert[i].pos_.y += clipOffsetY;
	}

	// Return the tmp vertices as six, ordered, ready-to-render vertices
	vert[0] = tmpvert[ 0 ];
	vert[1] = tmpvert[ 1 ];
	vert[2] = tmpvert[ 3 ];
	vert[3] = tmpvert[ 2 ];
	vert[4] = tmpvert[ 3 ];
	vert[5] = tmpvert[ 1 ];

	return clipWidth;
}


CachedFont::CachedFont( Moo::BaseTexturePtr t, FontMetrics& fm )
:Font( t, fm )
{
}


//-----------------------------------------------------------------------------
//Section : FontManager
//-----------------------------------------------------------------------------


static AutoConfigString s_fontRoot( "system/fontRoot" );

FontManager& FontManager::instance()
{
	static FontManager s_instance;
	return s_instance;
}


FontManager::FontManager()
{
	Moo::TextureStage ts;
	ts.useMipMapping( false );
	ts.minFilter( Moo::TextureStage::POINT );
	ts.magFilter( Moo::TextureStage::POINT );
	ts.colourOperation( Moo::TextureStage::MODULATE );
	ts.alphaOperation( Moo::TextureStage::MODULATE );
	material_.addTextureStage( ts );

	Moo::TextureStage ts2;
	ts2.colourOperation( Moo::TextureStage::DISABLE );
	ts2.alphaOperation( Moo::TextureStage::DISABLE );
	material_.addTextureStage( ts2 );

	material_.srcBlend( Moo::Material::SRC_ALPHA );
	material_.destBlend( Moo::Material::INV_SRC_ALPHA );
	material_.alphaBlended( true );
	material_.sorted( false );
	material_.doubleSided( false );
	material_.fogged(false);
	material_.zBufferRead(false);
	material_.zBufferWrite(false);
}


/**
 *	This method retrieves a new font pointer.
 *
 *	@param	resourceName	The name of the font resource.
 *	@param	htmlSupport		Whether the font supports drawing html strings.
 *	@param	cached			Whether the font caches string width info.  Use this
 *	only if you will be using the font to draw similar strings repeatedly.
 *
 *	@return	FontPtr				A reference counted font object.
 */
FontPtr	FontManager::get( const std::string& resourceName, bool htmlSupport, bool cached )
{
	Resource* res = NULL;

	//Check the cache
	StringHashMap<Resource>::iterator it = fonts_.find( resourceName );
	if ( it != fonts_.end() )
	{
		res = &it->second;
	}
	else
	{
		DataSectionPtr pSection = BWResource::instance().openSection(
			s_fontRoot.value() + resourceName );
		if ( pSection.hasObject() )
		{
			std::string mapName = this->checkFontGenerated( pSection );

			if (mapName.empty())
			{
				return NULL;
			}

			Moo::BaseTexturePtr pTex = Moo::TextureManager::instance()->get(
						mapName, false, false, true, "texture/font" );

			Resource r;
			r.metrics_->load( pSection );
			r.texture_ = pTex;
			fonts_[resourceName] = r;
			res = &fonts_.find( resourceName )->second;
		}
		else
		{
			ERROR_MSG( "Font resource %s does not exist\n",
						resourceName.c_str() );
			return NULL;
		}
	}

	if ( res != NULL )
	{
		return ( cached ? new CachedFont( res->texture_, *res->metrics_ ):
						  new Font( res->texture_, *res->metrics_ ) );	
	}

	return NULL;
}


/**
 *	This function ensures that a given font file has had its texture file
 *	generated.
 *
 *	@param	fontDataSection	The data section of the font that should be
 *							generated.
 *
 *	@return	The name of the generated texture file or empty on failure.
 */
std::string FontManager::checkFontGenerated( DataSectionPtr fontDataSection )
{
	if( fontDataSection.exists() == false )
		return "";

	std::string mapName = fontDataSection->readString( "generated/map", "" );

	if (!BWResource::fileExists( mapName ))
	{
		if (createFont( fontDataSection ))
		{
			mapName = fontDataSection->readString( "generated/map" );
		}
		else
		{
			ERROR_MSG( "Font resource '%s' could not be created\n",
						fontDataSection->sectionName().c_str() );
			return std::string();
		}
	}

	return mapName;
}


/**
 *	This method finds the font name for a given font pointer.
 *	Do not use this method all the time, cause it performs a map find.
 *
 *	@param	pFont	smart pointer to the font to retrieve the name for.
 */
const std::string& FontManager::findFontName( FontPtr pFont )
{
	static std::string s_fontName = "font not found.";

	if ( !pFont )
		return s_fontName;

	StringHashMap<Resource>::iterator it = fonts_.begin();
	StringHashMap<Resource>::iterator end = fonts_.end();

	while ( it != end )
	{
		Resource& r = it->second;
		if ( r.texture_ == pFont->pTexture() )
			if ( r.metrics_ == &pFont->metrics() )
			{
				s_fontName = it->first;
				return s_fontName;
			}
		it++;
	}

	return s_fontName;
}


/**
 *	This method sets up a material for the given font, just in case
 *	the font user doesn't want to use it's own materials.  ( Useful
 *	for the console and generic font drawing routines )
 */
void FontManager::setMaterialActive( FontPtr pFont )
{
	Moo::TextureStage& ts = material_.textureStage(0);

	if (pFont->fitToScreen())
	{	
		ts.useMipMapping( true );
		ts.minFilter( Moo::TextureStage::LINEAR );
		ts.magFilter( Moo::TextureStage::LINEAR );
	}
	else
	{
		ts.useMipMapping( false );
		ts.minFilter( Moo::TextureStage::POINT );
		ts.magFilter( Moo::TextureStage::POINT );
	}
	ts.pTexture( pFont->pTexture() );
	material_.set();
}





/**
 *	This method pre-creates all fonts in root font directory.
 *	Note: This method also clears and presents the render device multiple times
 *	during generation and so should not be called inside beginScene() and
 *	endScene().
 */
void FontManager::preCreateAllFonts()
{
	DataSectionPtr fontDir = BWResource::instance().openSection( s_fontRoot );

	if (!fontDir)
		return;

	const uint32 clearFlags = D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER |
					(Moo::rc().stencilAvailable() ? D3DCLEAR_STENCIL : 0);

	Moo::rc().device()->Clear( 0, NULL, clearFlags, 0x00000000, 1.0f, 0 );
	Moo::rc().device()->Present( NULL, NULL, NULL, NULL);

	for (int iFont=0; iFont < fontDir->countChildren(); ++iFont)
	{
		const std::string name = fontDir->childSectionName( iFont );
		if (BWResource::getExtension( name ) == "font")
		{
			this->checkFontGenerated( fontDir->openChild( iFont ) );

			Moo::rc().device()->Clear( 0, NULL, clearFlags, 0x00000000, 1.0f, 0 );
			Moo::rc().device()->Present( NULL, NULL, NULL, NULL);
		}
	}
}



float nextPowerOfTwo( float f )
{
	float lValue = logf(f)/logf(2.f);
	lValue += 1.f;
	float power = floorf(lValue);
	return powf(2,power);
}

//This method returns false if GDI is not available.
bool FontManager::createFont( DataSectionPtr pSection )
{
#ifdef _WIN32
	bool success = false;

	TRACE_MSG( "Generating font '%s'\n", pSection->sectionName().c_str() );

	DataSectionPtr pSect = pSection->openSection( "creation" );

	if ( pSect )
	{
		std::string fontName = pSect->readString( "sourceFont" );

		int pointSize = pSect->readInt( "sourceFontSize" );
		int startChar = pSect->readInt( "startChar", 32 );
		int endChar = pSect->readInt( "endChar", 132 );
		int fixedWidth = pSect->readInt( "fixedWidth", 0 );
		int effectsMargin = (int)pSect->readFloat( "effectsMargin", 0.f );
		int textureMargin = (int)pSect->readFloat( "textureMargin", 0.f );
		int spaceProxyChar = pSect->readInt( "spaceProxyChar", 105 ); // i
		uint MAX_WIDTH = (uint)pSect->readInt( "maxTextureWidth", 1024 );
		bool dropShadow = pSect->readBool( "dropShadow", false );
		int shadowAlpha = pSect->readInt( "shadowAlpha", 255 );
		bool antialias = pSect->readBool( "antialias", true );
		bool bold = pSect->readBool( "bold", false );
		bool proportional = (fixedWidth <= 0);		
		char buf[256];
		bw_snprintf( buf, sizeof(buf), "%s%s_%d.dds\0", s_fontRoot.value().c_str(),
			fontName.c_str(), pointSize<0?-pointSize:pointSize );
		std::string mapName( buf );
		pSection->deleteSection( "generated" );
		pSection->newSection( "generated" );
		pSection->writeString( "generated/map", mapName );

		//create DX font		
		ID3DXFont* pFont;

		/*	This commented out section shows the "choose font" dialog
		CHOOSEFONT cf; 
		// Initialise members of the CHOOSEFONT structure. 

		cf.lStructSize = sizeof(CHOOSEFONT); 
		cf.hwndOwner = (HWND)NULL; 
		cf.hDC = (HDC)NULL; 
		cf.lpLogFont = &lf; 
		cf.iPointSize = 0; 
		cf.Flags = CF_SCREENFONTS; 
		cf.rgbColors = RGB(0,0,0); 
		cf.lCustData = 0L; 
		cf.lpfnHook = (LPCFHOOKPROC)NULL; 
		cf.lpTemplateName = (LPSTR)NULL; 
		cf.hInstance = (HINSTANCE) NULL; 
		cf.lpszStyle = (LPSTR)NULL; 
		cf.nFontType = SCREEN_FONTTYPE; 
		cf.nSizeMin = 0; 
		cf.nSizeMax = 0; 

		// Display the CHOOSEFONT common-dialog box. 

		ChooseFont(&cf);

		// Create a logical font based on the user's 
		// selection and return a handle identifying 
		// that font. 

		DEBUG_MSG( "Font name is %s", cf.lpLogFont->lfFaceName );
		HRESULT hr = D3DXCreateFontIndirect( Moo::rc().device(), cf.lpLogFont, &pFont );*/

		D3DXFONT_DESC d3dfd;
		ZeroMemory( &d3dfd, sizeof( D3DXFONT_DESC ) );
		d3dfd.Height=pointSize;
		d3dfd.Width=0;
		d3dfd.Weight=bold ? FW_BOLD : FW_NORMAL;
		d3dfd.MipLevels=1;
		d3dfd.Italic=FALSE;
		d3dfd.CharSet=ANSI_CHARSET;
		d3dfd.Quality= antialias ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY;
		d3dfd.PitchAndFamily=DEFAULT_PITCH;
		strncpy( d3dfd.FaceName, fontName.c_str(), LF_FACESIZE );

		HRESULT hr = D3DXCreateFontIndirect( Moo::rc().device(), &d3dfd, &pFont );

		if ( SUCCEEDED( hr ) )
		{
			//create metrics.
			FontMetrics fm;

			fm.start_ = startChar;
			fm.end_ = endChar;
			RECT rect= {0,0,0,0};
			unsigned short i;
			fm.maxWidth_ = 0;
			uint maxWidth = 0;
			uint currentU = 0;
			uint currentV = 0;
			uint yStep = 0;

//			hr = pFont->Begin();
			for ( i=fm.start_; i<=fm.end_; i++ )
			{
				//calculate how many pixels (rectangle) needed for this character
				rect.right=0;
				rect.bottom=0;

				// for some unknown reason, DX9 returns width=0 
				// whenever calculating the width of a space. That's 
				// why we need to use another character here (the proxy).
				int character = i == 32 ? spaceProxyChar : i;
				pFont->DrawText( 
						NULL, (LPCSTR)&character, 1, &rect, 
						DT_LEFT | DT_TOP | DT_CALCRECT, 0xffffffff );

				if ( !proportional )
				{
					rect.right = fixedWidth;
				}

				//factor in the effects margin, when we actually come to draw the
				//font, we will be adding "effectsMargin" number of pixels.
				rect.right += effectsMargin;

				//store widths in pixels for now.
				//when we later calculate how large the bitmap should be, then
				//we can adjust all values to uv coordinates before saving.
				if ( currentU + rect.right >= MAX_WIDTH )
				{
					currentU = 0;
					currentV += yStep;
					fm.uvs_.push_back( Vector2( (float)currentU, (float)currentV ) );
				}
				else
				{
					fm.uvs_.push_back( Vector2( (float)currentU, (float)currentV ) );
				}

				fm.widths_.push_back( (float)rect.right );
				currentU += rect.right;
				if ( rect.right > (int)fm.maxWidth_ )
					fm.maxWidth_ = (float)rect.right;
				if ( currentU > maxWidth )
					maxWidth = currentU;
				currentU += textureMargin;
				if ( i==fm.start_ )
				{
					fm.height_ = (float)rect.bottom;
					yStep = (uint)nextPowerOfTwo( fm.height_ );
				}
			}

			logFontMetrics(fontName, fm);

//			hr = pFont->End();

			//find next power of two for the total width
			float fWidth = (float)(maxWidth);
			float totalWidth = nextPowerOfTwo( fWidth );
			//find next power of two for the total height
			float totalHeight = (float)(currentV + yStep);
			float fHeight = nextPowerOfTwo( totalHeight );

			//Don't allow font bitmaps to be wider than MAX_WIDTH pixels
			MF_ASSERT( totalWidth <= MAX_WIDTH );

			fm.mapDimensions_.x = (float)totalWidth;
			fm.mapDimensions_.y = fHeight;

			//create bitmap with enough space for font
			Moo::RenderTarget rt( "temporaryForFontCreation" );
			rt.create( (uint)fm.mapDimensions_.x, (uint)fm.mapDimensions_.y );
			rt.push();

			Moo::rc().beginScene();			
			Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, 0x00FFFFFF, 1, 0 );
			//Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, 0x80808080, 1, 0 );
			//Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, 0xFF600000, 1, 0 );
			Moo::rc().fogEnabled( false );
			Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE,
				D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );

			//write text into bitmap
//			hr = pFont->Begin();
			const D3DCOLOR shadowColor = D3DCOLOR_ARGB( shadowAlpha, 0, 0, 0 );
			rect.left=0;
			rect.right=0;
			rect.top=0;
			rect.bottom=(uint)fm.mapDimensions_.y;
			int idx=0;
			for ( i=fm.start_; i<=fm.end_; i++ )
			{		
				rect.left = (uint)fm.uvs_[idx].x;
				rect.top = (uint)fm.uvs_[idx].y;

				// use actual font metrics instead of the rect.right
				// because this may have been forced to fixed width
				RECT charMetrics = {0, 0, 0, 0};
				int character = i;
				pFont->DrawText( 
						NULL, (LPCSTR)&character, 1, &charMetrics, 
						DT_LEFT | DT_TOP | DT_CALCRECT, 0xffffffff );

				rect.right = rect.left + charMetrics.right;
				rect.bottom = rect.top + charMetrics.bottom;

				if ( dropShadow )
				{
					::OffsetRect(&rect,1,1);
					pFont->DrawText( NULL, (LPCSTR)&i, 1, &rect, DT_LEFT | DT_TOP, shadowColor );
					::OffsetRect(&rect,-1,-1);
				}
				pFont->DrawText( NULL, (LPCSTR)&i, 1, &rect, DT_LEFT | DT_TOP, 0xffffffff );
				idx++;
			}
			Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE,
				D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN );
//			pFont->End();
			Moo::rc().endScene();

			//save bitmap
			rt.pop();

			// write out the texture for later
			Moo::TextureManager::writeDDS( rt.pTexture(), mapName, D3DFMT_DXT3 );

			//release DX font
			pFont->Release();

			//adjust all font metrics info to be correct
			uint numChars = fm.end_ - fm.start_ + 1;
			for ( i=0; i<numChars; i++ )
			{
				fm.uvs_[i].x /= fm.mapDimensions_.x;
				fm.uvs_[i].y /= fm.mapDimensions_.y;
				fm.widths_[i] /= fm.mapDimensions_.x;
			}
			fm.maxWidth_ /= fm.mapDimensions_.x;
			fm.height_ /= fm.mapDimensions_.y;

			//save the metrics information into the data section.
			fm.save( pSection );
		}
		else
		{
			ERROR_MSG( "FontManager::createFont - D3DXCreateFontIndirect failed, error code %lx\n", hr );
			return NULL;
		}

		success = true;
	}
	else
	{
		ERROR_MSG( "FontManager::createFont - creation section does not exist\n" );
	}
#endif	//yes to win32

	return success;
}
