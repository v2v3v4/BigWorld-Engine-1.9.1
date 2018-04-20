/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifdef CODE_INLINE
#define INLINE inline
#else
#define INLINE
#endif

INLINE FontMetrics& Font::metrics()
{
	return metrics_;
}


INLINE void Font::fitToScreen( bool state, const Vector2& numCharsXY )
{
	fitToScreen_ = state;
	numCharsXY_ = numCharsXY;
}


INLINE bool Font::fitToScreen() const
{
	return fitToScreen_;
}


/**
 *	This method returns the height of the font.  It is assumed
 *	all characters in the font set have the same height.
 *
 *	@return	float		The height of the font.
 */
INLINE float FontMetrics::height() const
{
	return height_;
}


/**
 *	This method returns the height of the font, in clip coordinates.
 *	It is calculated each time, because texel->clip can change.
 *
 *	@return	float		The height of the font, in clip coordinates.
 */
INLINE float FontMetrics::clipHeight() const
{
	return height_ * mapDimensions_.y / (Moo::rc().halfScreenHeight());
}


/**
 *	This function turns a hex character into an integer.
 *	@param c		the hex character
 *	@return uint	integer value represented by c
 */
INLINE uint fromHex( char c )
{
	if ( c >= '0' && c <= '9' )
		return ( c-'0' );
	else if ( c >= 'a' && c <= 'f' )
		return ( 10 + ( c-'a') );
	else if ( c >= 'A' && c <= 'F' )
		return ( 10 + ( c-'A') );
	else
		return 0;
}


/**
 *	This function turns an integer into a two hex character string.
 *	@param u		the integer to convert
 *	@param ret		character array filled by this function.
 */
INLINE void toHex( uint u, char ret[4] )
{
	if ( u > 65535 )
	{
		ret[0] = 'x';
		ret[1] = 'x';
		ret[2] = 'x';
		ret[3] = 'x';
	}
	else
	{
		uint a = (u>>12)&0xf;
		uint b = (u>>8)&0xf;
		uint c = (u>>4)&0xf;
		uint d = u&0xf;
		
		ret[0] = a>=10 ? (a-10) + 'a' : a + '0';
		ret[1] = b>=10 ? (b-10) + 'a' : b + '0';
		ret[2] = c>=10 ? (c-10) + 'a' : c + '0';
		ret[3] = d>=10 ? (d-10) + 'a' : d + '0';
	}
}


/**
 *	This method returns the uv width of a single character.
 *
 *	@param	c		The character for the uv width calculation.
 *	@return uint	The width of the character, in uv units.
 */
INLINE float FontMetrics::charWidth( wchar_t c ) const
{
	uint idx = 0;
	FontLUT::const_iterator it = charToIdx_.find( c );
	if ( it != charToIdx_.end() )
	{
		idx = it->second;
	}
	
	MF_ASSERT( idx < widths_.size() );
	return widths_[idx];
}


/**
 *	This method returns the width of a character, in clip coordinates.
 *	It is calculated each time, because texel->clip can change.
 *
 *	@param	c			The character for the uv width calculation.
 *	@return	float		The width of c, in clip coordinates.
 */
INLINE float FontMetrics::clipWidth( wchar_t c ) const
{
	return this->charWidth( c ) * mapDimensions_.x / (Moo::rc().halfScreenWidth());
}


/**
 *	This method returns the normalised position (uv coords)
 *	of the given character in the bitmap.
 */
INLINE const Vector2& FontMetrics::charPosition( wchar_t c ) const
{
	uint idx = 0;
	FontLUT::const_iterator it = charToIdx_.find( c );
	if ( it != charToIdx_.end() )
	{
		idx = it->second;
	}
	
	MF_ASSERT( idx < uvs_.size() );
	return uvs_[idx];
}


/**
 *	This method simply returns the width of the font's texture map.
 */
INLINE float FontMetrics::mapWidth() const
{
	return mapDimensions_.x;
}


/**
 *	This method simply returns the height of the font's texture map.
 */
INLINE float FontMetrics::mapHeight() const
{
	return mapDimensions_.y;
}

/**
 *	This method returns the width and height of a string, in texels.
 *
 *	@param	str		The string for dimension calculation.
 *	@param	w		The returned width
 *	@param	h		The returned height
 */
INLINE void FontMetrics::stringDimensions( const std::string& str, int& w, int& h )
{
	w = this->stringWidth( str );
	h = (int)( this->height() * mapDimensions_.y + 0.5f );
}

/**
 *	This method returns the width and height of a string, in texels.
 *
 *	@param	str		The string for dimension calculation.
 *	@param	w		The returned width
 *	@param	h		The returned height
 */
INLINE void FontMetrics::stringDimensions( const std::wstring& str, int& w, int& h )
{
	w = this->stringWidth( str );
	h = (int)( this->height() * mapDimensions_.y + 0.5f );
}