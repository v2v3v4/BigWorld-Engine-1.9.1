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

#include "pch.hpp"
#include <windows.h>
#include "Shlwapi.h"
#include <algorithm>
#include <string>
#include <vector>
#include "string_utils.hpp"
#include "cstdmf/debug.hpp"
#include "resmgr/string_provider.hpp"

DECLARE_DEBUG_COMPONENT( 0 );


std::string StringUtils::vectorToString( const std::vector<std::string>& vec, char separator /* = ';' */ )
{
	std::string ret;
	for( std::vector<std::string>::const_iterator i = vec.begin(); i != vec.end(); ++i )
	{
		if ( ret.length() > 0 )
			ret += separator;
		ret += (*i);
	}

	return ret;
}

void StringUtils::vectorFromString( const std::string str, std::vector<std::string>& vec, char* separators /* = ",;" */ )
{
	char* resToken;
	int curPos = 0;

	resToken = strtok( (char*)str.c_str(), separators );
	while ( resToken != 0 )
	{
		std::string subStr = resToken;
		vec.push_back( subStr );

		resToken = strtok( 0, separators );
	}
}

bool StringUtils::matchExtension( const std::string& fname, const std::vector<std::string>& extensions )
{
	if ( extensions.empty() )
		return true;

	int dot = (int)fname.find_last_of( '.' ) + 1;
	if ( dot <= 0 )
		return false;

	std::string ext = fname.substr( dot );
	toLowerCase( ext );

	for( std::vector<std::string>::const_iterator i = extensions.begin(); i != extensions.end(); ++i )
		if ( (*i) == ext )
			return true;

	return false;
}


/**
 *	This function tests to see if a string 'fname' matches the DOS-style
 *	wildcard string 'spec' similar to PathMatchSpec, but case-sensitive.
 */
bool StringUtils::matchSpecCaseSensitive(const char* fname, const char* spec)
{
	if ( fname == NULL || spec == NULL )
		return false;
	
	while ( *fname && *spec )
	{
		// find spec substring between *
		if ( *spec == '*' )
		{
			// skip stars
			while (*spec && *spec == '*')
				spec++;

			if ( !*spec )
				return true;

			const char *specSubstr = spec;

			// find end of substring
			unsigned int specSubstrLen = 0;
			while (*spec && *spec != '*')
			{
				spec++;
				specSubstrLen++;
			}

			const char *fnameSubstr = fname;
			bool match = false;
			while ( *fnameSubstr )
			{
				// if the spec substring is shorter than the fname, no match!
				if ( strlen( fnameSubstr ) < specSubstrLen )
					return false;

				// compare the spec's substring with fname.
				match = true;
				for ( unsigned int i = 0; i < specSubstrLen; ++i )
				{
					if ( specSubstr[i] != fnameSubstr[i] && specSubstr[i] != '?' )
					{
						match = false;
						break;
					}
				}

				if ( match )
					break;

				fnameSubstr++;
			}

			if ( !match )
				return false;

			// If everything went well, update the string pointers
			fname = fnameSubstr + specSubstrLen;
			continue;
		}
		if ( *spec != *fname && *spec != '?' )
			return false;

		spec++;
		fname++;
	}

	// if spec is not at the end, try to skip any trailing stars.
	while (*spec && *spec == '*')
		spec++;

	// They match if both the fname and the spec where exhausted.
	return !*fname && !*spec;
}


/**
 *	This function tests to see if a string 'fname' matches at least one of the
 *	DOS-style wildcard strings in the 'specs' vector. In addition, if a spec
 *	starts with '!', the test for it is done case-sensitive.
 */
bool StringUtils::matchSpec( const std::string& fname, const std::vector<std::string>& specs )
{
	if ( specs.empty() )
		return true;

	const char* pfname = fname.c_str();

	for( std::vector<std::string>::const_iterator i = specs.begin(); i != specs.end(); ++i )
	{
		const char* spec = (*i).c_str();
		bool caseSensitive = false;
		if ( spec[0] == '!' )
		{
			spec++;
			caseSensitive = true;
		}
		if ( caseSensitive )
		{
			if ( matchSpecCaseSensitive( pfname, spec ) )
				return true;
		}
		else
		{
			if ( PathMatchSpec( pfname, spec ) )
				return true;
		}
	}

	return false;
}

bool StringUtils::findInVector( const std::string& str, const std::vector<std::string>& vec )
{
	if ( vec.empty() )
		return true;

	const char* pstr = str.c_str();

	for( std::vector<std::string>::const_iterator i = vec.begin(); i != vec.end(); ++i )
		if ( _stricmp( (*i).c_str(), pstr ) == 0 )
			return true;

	return false;
}

void StringUtils::filterSpecVector( std::vector<std::string>& vec, const std::vector<std::string>& exclude )
{
	if ( vec.empty() || exclude.empty() )
		return;

	for( std::vector<std::string>::iterator i = vec.begin(); i != vec.end(); )
	{
		if ( matchSpec( (*i), exclude ) )
			i = vec.erase( i );
		else
			++i;
	}
}

void StringUtils::toLowerCase( std::string& str )
{
	std::transform( str.begin(), str.end(), str.begin(), tolower );
}

void StringUtils::toUpperCase( std::string& str )
{
	std::transform( str.begin(), str.end(), str.begin(), toupper );
}

void StringUtils::toMixedCase( std::string& str )
{
	bool lastSpace = true;
	
	std::string::iterator it = str.begin();
	std::string::iterator end = str.end();

	for(; it != end; ++it)
	{
		if (lastSpace)
			*it = toupper( *it );
		else
			*it = tolower( *it );
		lastSpace = ( *it == ' ' );
	}
}

const std::string StringUtils::lowerCase( const std::string& str )
{
	std::string temp = str;
	toLowerCase( temp );
	return temp;
}

const std::string StringUtils::upperCase( const std::string& str )
{
	std::string temp = str;
	toUpperCase( temp );
	return temp;
}

void StringUtils::replace( std::string& str, char ch, char rep )
{
    std::replace(str.begin(), str.end(), ch, rep );
}

void StringUtils::replace( std::string& str, const std::string& from, const std::string& to )
{
	if( !from.empty() )
	{
		std::string newStr;
		while( const char* p = strstr( str.c_str(), from.c_str() ) )
		{
			newStr.insert( newStr.end(), str.c_str(), p );
			newStr += to;
			str.erase( str.begin(), str.begin() + ( p - str.c_str() ) + from.size() );
		}
		str = newStr + str;
	}
}

bool StringUtils::copyToClipboard( const std::string & str )
{
    bool ok = false;
    if (::OpenClipboard(NULL))
    {
        HGLOBAL data = 
            ::GlobalAlloc
            (
                GMEM_MOVEABLE, 
                (str.length() + 1)*sizeof(char)
            );
        if (data != NULL && ::EmptyClipboard() != FALSE)
        {
            LPTSTR cstr = (LPTSTR)::GlobalLock(data);
            memcpy(cstr, str.c_str(), str.length()*sizeof(char));
            cstr[str.length()] = '\0';
            ::GlobalUnlock(data);
            ::SetClipboardData(CF_TEXT, data);
            ok = true;
        }
        ::CloseClipboard();
    }
    return ok;
}

bool StringUtils::canCopyFromClipboard()
{
    bool ok = false;
    if (::OpenClipboard(NULL))
    {
        ok = ::IsClipboardFormatAvailable(CF_TEXT) != FALSE;
        ::CloseClipboard();
    }
    return ok;
}

bool StringUtils::copyFromClipboard( std::string & str )
{
    bool ok = false;
    str.clear();
    if (::OpenClipboard(NULL))
    {
        HGLOBAL data = ::GetClipboardData(CF_TEXT);
        if (data != NULL)
        {
            LPTSTR cstr = (LPTSTR)::GlobalLock(data);
            str = cstr;
            ::GlobalUnlock(data);
            ok = true;
        }
        ::CloseClipboard();
    }
    return ok;
}

void StringUtils::increment( std::string& str, IncrementStyle incrementStyle )
{
    //
    // For IS_EXPLORER the incrementation goes like:
    //
    //      original string
    //      Copy of original string
    //      Copy (2) of original string
    //      Copy (3) of original string
    //      ...
    //
    if (incrementStyle == IS_EXPLORER)
    {
        // Handle the degenerate case:
        if (str.empty())
            return;

        // See if the string starts with "Copy of ".  If it does then the result
        // will be "Copy (2) of" remainder.
		std::string prefix = L("COMMON/STRING_UTILS/COPY_OF");
		if (str.substr(0, prefix.size()) == prefix)
        {
            std::string remainder = str.substr(prefix.size(), std::string::npos);
            str = L("COMMON/STRING_UTILS/COPY_OF_N", 2, remainder );
            return;
        }
        // See if the string starts with "Copy (n) of " where n is a number.  If it
        // does then the result will be "Copy (n + 1) of " remainder.
		prefix = L("COMMON/STRING_UTILS/COPY");
        if (str.substr(0, prefix.size()) == prefix)
        {
            size_t       pos = 6;
            unsigned int num = 0;
            while (pos < str.length() && ::isdigit(str[pos]))
            {
                num *= 10;
                num += str[pos] - '0';
                ++pos;
            }
            ++num;
            if (pos < str.length())
            {
				std::string suffix = L("COMMON/STRING_UTILS/OF");
                if (str.substr(pos, suffix.size()) == suffix)
                {
                    std::string remainder = str.substr(pos + suffix.size(), std::string::npos);
					str = L("COMMON/STRING_UTILS/COPY_OF_N", num, remainder );
                    return;
                }
            }
        }

        // The result must be "Copy of " str.
		str = L("COMMON/STRING_UTILS/COPY_OF")+std::string(" ")+ str;
        return;
    }
    //
    // For IS_END the incrementation goes like:
    //
    //      original string
    //      original string 2
    //      original string 3
    //      original string 4
    //      ...
    //
    // or, if the orignal string is "original string(0)":
    //
    //      original string(0)
    //      original string(1)
    //      original string(2)
    //      ...
    //
    else if (incrementStyle == IS_END)
    {
        if (str.empty())
            return;

        char lastChar    = str[str.length() - 1];
        bool hasLastChar = ::isdigit(lastChar) == 0;

        // Find the position of the ending number and where it begins:
        int pos = (int)str.length() - 1;
        if (hasLastChar)
            --pos;
        unsigned int count = 0;
        unsigned int power = 1;
        bool hasDigits = false;
        for (;pos >= 0 && ::isdigit(str[pos]) != 0; --pos)
        {
            count += power*(str[pos] - '0'); 
            power *= 10;
            hasDigits = true;
        }

        // Case where there was no number:
        if (!hasDigits)
        {
            count = 1;
            ++pos;
            hasLastChar = false;
        }

        // Increment the count:
        ++count;    

        // Construct the result:
        std::stringstream stream;
        std::string prefix = str.substr(0, pos + 1);
        stream << prefix.c_str();
        if (!hasDigits)
            stream << ' ';
        stream << count;
        if (hasLastChar)
            stream << lastChar;
        str = stream.str();
        return;
    }
    else
    {
        return;
    }
}

bool 
StringUtils::makeValidFilename
(
    std::string     &str, 
    char            replaceChar /*= '_'*/,
    bool            allowSpaces /*= true*/
)
{
    static char const *NOT_ALLOWED         = "/<>?\\|*:";
    static char const *NOT_ALLOWED_NOSPACE = "/<>?\\|*: ";

    bool changed = false; // Were any changes made?

    // Remove leading whitespace:
    while (!str.empty() && ::isspace(str[0]))
    {
        changed = true;
        str.erase(str.begin() + 0);
    }

    // Remove trailing whitespace:
    while (!str.empty() && ::isspace(str[str.length() - 1]))
    {
        changed = true;
        str.erase(str.begin() + str.length() - 1);
    }

    // Handle the degenerate case:
    if (str.empty())
    {
        str = replaceChar;
        return false;
    }
    else
    {
        // Look for and replace characters that are not allowed:        
        size_t pos = std::string::npos;
        do
        {
            if (allowSpaces)
                pos = str.find_first_of(NOT_ALLOWED);
            else
                pos = str.find_first_of(NOT_ALLOWED_NOSPACE);
            if (pos != std::string::npos)
            {
                str[pos] = replaceChar;
                changed = true;
            }
        }
        while (pos != std::string::npos);
        return !changed;
    }
}


/**
 * The function retrieve a token substring seperated by ' ','\t' from a string. characters quoted in '""'
 * is treated as one token, the return string excludes '"' character.
 * Note: source string would be modified after calling.
 *
 * @param			a pointer of start position of string, on return it points to next-time start position
 *
 * @returns			token string (NULL terminated)
 */
char * StringUtils::retrieveCmdToken( char * & cmd )
{
	if (!cmd) return NULL;

	char * result = *cmd && *cmd != ' ' && *cmd != '\t' ? cmd : NULL;
	
	while (*cmd)
	{
		switch (*cmd)
		{
		case ' ':
		case '\t':
			while (*cmd && (*cmd == ' ' || *cmd == '\t'))
			{
				*cmd++ = '\0';
			}
			if ( *cmd && !result)
			{
				result = cmd;
				break;
			}
			return result;
		case '"':
			result = ++cmd;
			while (*cmd && *cmd != '"')
			{
				++cmd;
			}
			if (*cmd)
			{
				*cmd++ = '\0';
			}
			return result;
		default:
			++cmd;
		}
	}
	return result;
}
