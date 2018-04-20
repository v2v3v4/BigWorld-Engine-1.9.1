/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *	StringUtils: String utility methods.
 */

#ifndef STRING_UTILS_HPP
#define STRING_UTILS_HPP

class StringUtils
{
public:
	static std::string vectorToString( const std::vector<std::string>& vec, char separator = ';' );
	static void vectorFromString( const std::string str, std::vector<std::string>& vec, char* separators = ",;" );
	static bool matchExtension( const std::string& fname, const std::vector<std::string>& extensions );
	static bool matchSpecCaseSensitive(const char* fname, const char* spec);
	static bool matchSpec( const std::string& fname, const std::vector<std::string>& specs );
	static bool findInVector( const std::string& str, const std::vector<std::string>& vec );
	static void filterSpecVector( std::vector<std::string>& vec, const std::vector<std::string>& exclude );
	static void toLowerCase( std::string& str );
	static void toUpperCase( std::string& str );
	static void toMixedCase( std::string& str );
	static const std::string lowerCase( const std::string& str );
	static const std::string upperCase( const std::string& str );
	static void replace( std::string& str, char ch, char rep );
	static void replace( std::string& str, const std::string& from, const std::string& to );
    enum IncrementStyle { IS_EXPLORER, IS_END };
    static void increment( std::string& str, IncrementStyle incrementStyle );
    static bool copyToClipboard( const std::string & str );
    static bool canCopyFromClipboard();
    static bool copyFromClipboard( std::string & str );
    static bool makeValidFilename( std::string & str, char replaceChar = '_', bool allowSpaces = true );
	static char * retrieveCmdToken( char * & cmd );
};

#endif // STRING_UTILS_HPP
