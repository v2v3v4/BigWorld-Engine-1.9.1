/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef STRING_PROVIDER_HPP__
#define STRING_PROVIDER_HPP__

#include "datasection.hpp"
#include "cstdmf/smartpointer.hpp"
#include <string>
#include <cstring>
#include <stdio.h>
#include <sstream>
#include <vector>
#include <set>
#include <map>

class Formatter
{
	std::string str_;
public:
	Formatter(){}
	Formatter( const std::string& str )	: str_( str ){}
	Formatter( const char* str )	: str_( str ){}
	Formatter( float f, const char* format = "%g" )
	{
		char s[1024];
		bw_snprintf( s, sizeof( s ), format, f );
		s[ sizeof( s ) - 1 ] = 0;
		str_ = s;
	}
	Formatter( double d, const char* format = "%g" )
	{
		char s[1024];
		bw_snprintf( s, sizeof( s ), format, d );
		s[ sizeof( s ) - 1 ] = 0;
		str_ = s;
	}
	Formatter( int i, const char* format = "%d" )
	{
		char s[1024];
		bw_snprintf( s, sizeof( s ), format, i );
		s[ sizeof( s ) - 1 ] = 0;
		str_ = s;
	}
	Formatter( unsigned int ui, const char* format = "%u" )
	{
		char s[1024];
		bw_snprintf( s, sizeof( s ), format, ui );
		s[ sizeof( s ) - 1 ] = 0;
		str_ = s;
	}
	Formatter( unsigned long ul, const char* format = "%u" )
	{
		char s[1024];
		bw_snprintf( s, sizeof( s ), format, ul );
		s[ sizeof( s ) - 1 ] = 0;
		str_ = s;
	}
	Formatter( char ch, const char* format = "%c" )
	{
		char s[1024];
		bw_snprintf( s, sizeof( s ), format, ch );
		s[ sizeof( s ) - 1 ] = 0;
		str_ = s;
	}
	Formatter( unsigned char ch, const char* format = "%c" )
	{
		char s[1024];
		bw_snprintf( s, sizeof( s ), format, ch );
		s[ sizeof( s ) - 1 ] = 0;
		str_ = s;
	}
	Formatter( void* p, const char* format = "%p" )
	{
		char s[1024];
		bw_snprintf( s, sizeof( s ), format, p );
		s[ sizeof( s ) - 1 ] = 0;
		str_ = s;
	}
	const std::string& str() const {	return str_;	}
};


// for index out of the range, set it to empty string
inline const char* formatString( const char* format, const Formatter& f1 = Formatter(), const Formatter& f2 = Formatter(),
						 const Formatter& f3 = Formatter(), const Formatter& f4 = Formatter(),
						 const Formatter& f5 = Formatter(), const Formatter& f6 = Formatter(),
						 const Formatter& f7 = Formatter(), const Formatter& f8 = Formatter() )
{
	static const int MAX_LOCALISED_STRING_LENGTH = 10240;
#ifdef WIN32
	__declspec(thread) static char sz[ MAX_LOCALISED_STRING_LENGTH + 1 ];
#else//WIN32
	static char sz[ MAX_LOCALISED_STRING_LENGTH + 1 ];
#endif//WIN32

	std::string strs[ 8 ];
	strs[ 0 ] = f1.str();	strs[ 1 ] = f2.str();	strs[ 2 ] = f3.str();	strs[ 3 ] = f4.str();
	strs[ 4 ] = f5.str();	strs[ 5 ] = f6.str();	strs[ 6 ] = f7.str();	strs[ 7 ] = f8.str();
	std::string result;
	while( const char* escape = std::strchr( format, '%' ) )
	{
		result += std::string( format, escape - format );
		format = escape;
		if( format[1] == 0 )
		{
			++format;
			break;
		}
		if( format[1] == '%' )
		{
			result += '%';
			++format;
		}
		else if( format[1] >= '0' && format[1] <= '7' )
		{
			result += strs[ format[1] - '0' ];
			++format;
		}
		/*
		else
			0;// wrong escape ==> empty strings
		*/
		++format;
	}
	result += format;

	std::strncpy( sz, result.c_str(), sizeof( sz ) );

	sz[MAX_LOCALISED_STRING_LENGTH] = 0;
	return sz;
}

#define LANGUAGE_NAME_TAG ( "LanguageName" )
#define ENGLISH_LANGUAGE_NAME_TAG ( "EnglishLanguageName" )
#define DEFAULT_LANGUAGE_NAME ( "en" )
#define DEFAULT_COUNTRY_NAME ( "us" )

class StringID
{
public:
	StringID() : key_(0) {}
	explicit StringID( const char* str );
	unsigned int key() const	{	return key_;	}
	const std::string& str() const	{	return str_;	}
	bool operator<( const StringID& that ) const;// should be a free function, but ...
private:
	std::string str_;
	unsigned int key_;
};

class Language : public SafeReferenceCount
{
public:
	virtual void load( DataSectionPtr language, const std::string& root = "" ) = 0;
	virtual const char* getLanguageName() const = 0;
	virtual const char* getLanguageEnglishName() const = 0;
	virtual const std::string& getIsoLangName() const = 0;
	virtual const std::string& getIsoCountryName() const = 0;
	virtual const char* getString( const StringID& id ) const = 0;
	static std::pair<std::string, std::string> splitIsoLangCountryName( const std::string& isoLangCountryName );
	static const int ISO_NAME_LENGTH = 2;
};

typedef SmartPointer<Language> LanguagePtr;

struct LanguageNotifier
{
	LanguageNotifier();
	virtual ~LanguageNotifier();
	virtual void changed() = 0;
};

/**
	Localised String Provider, after setting to the appropriate language/country
	You can call localised string provider with an id to get back a string
 */

class StringProvider
{
	std::set<LanguageNotifier*> notifiers_;
	std::vector<LanguagePtr> languages_;
	LanguagePtr currentLanguage_;
	LanguagePtr currentMainLanguage_;
	LanguagePtr defaultLanguage_;

	const char* str( const StringID& id ) const;
	const char* formatString( const StringID& formatID, const Formatter& f1 = Formatter(), const Formatter& f2 = Formatter(),
						 const Formatter& f3 = Formatter(), const Formatter& f4 = Formatter(),
						 const Formatter& f5 = Formatter(), const Formatter& f6 = Formatter(),
						 const Formatter& f7 = Formatter(), const Formatter& f8 = Formatter() )
	{
		return ::formatString( str( formatID ), f1, f2, f3, f4, f5, f6, f7, f8 );
	}
public:
	enum DefResult
	{
		RETURN_NULL_IF_NOT_EXISTING,
		RETURN_PARAM_IF_NOT_EXISTING
	};


	void load( DataSectionPtr file );
	unsigned int languageNum() const;
	LanguagePtr getLanguage( unsigned int index ) const;

	void setLanguage();
	void setLanguage( unsigned int language );
	void setLanguages( const std::string& langName, const std::string& countryName );

	const char* str( const char* id, DefResult def = RETURN_PARAM_IF_NOT_EXISTING ) const;

	const char* formatString( const char* formatID, const Formatter& f1 = Formatter(), const Formatter& f2 = Formatter(),
						 const Formatter& f3 = Formatter(), const Formatter& f4 = Formatter(),
						 const Formatter& f5 = Formatter(), const Formatter& f6 = Formatter(),
						 const Formatter& f7 = Formatter(), const Formatter& f8 = Formatter() )
	{
		return ::formatString( str( formatID ), f1, f2, f3, f4, f5, f6, f7, f8 );
	}


	LanguagePtr currentLanguage() const;

	static StringProvider& instance();

	void registerNotifier( LanguageNotifier* notifier );
	void unregisterNotifier( LanguageNotifier* notifier );
	void notify();
};

inline const char* formatLocalisedString( const char* format, const Formatter& f1, const Formatter& f2 = Formatter(),
						 const Formatter& f3 = Formatter(), const Formatter& f4 = Formatter(),
						 const Formatter& f5 = Formatter(), const Formatter& f6 = Formatter(),
						 const Formatter& f7 = Formatter(), const Formatter& f8 = Formatter() )
{
	return StringProvider::instance().formatString( format, f1, f2, f3, f4, f5, f6, f7, f8 );
}

inline const char* L( const char* format, const Formatter& f1, const Formatter& f2 = Formatter(),
						 const Formatter& f3 = Formatter(), const Formatter& f4 = Formatter(),
						 const Formatter& f5 = Formatter(), const Formatter& f6 = Formatter(),
						 const Formatter& f7 = Formatter(), const Formatter& f8 = Formatter() )
{
	return formatLocalisedString( format, f1, f2, f3, f4, f5, f6, f7, f8 );
}

inline const char* L( const char* key, StringProvider::DefResult def = StringProvider::RETURN_PARAM_IF_NOT_EXISTING )
{
	return StringProvider::instance().str( key, def );
}

inline bool isLocaliseToken( const char* s )
{
	return s && s[0] == '`';
}

inline bool isLocaliseToken( const wchar_t* s )
{
	return s && s[0] == '`';
}

#ifdef WIN32
#include <commctrl.h>
class WindowTextNotifier : LanguageNotifier
{
	std::map<HWND, StringID> windows_;
	std::map<HWND, std::vector<StringID> > combos_;
	std::map<std::pair<HMENU, UINT>, StringID> menus_;
	std::map<HWND, WNDPROC> subClassMap_;
	HHOOK callWinRetHook_;
	HHOOK callWinHook_;
	WNDPROC comboWndProc_;
	WNDPROC toolTipProc_;
public:
	WindowTextNotifier();
	~WindowTextNotifier();

	void set( HWND hwnd, const char* id );

	void set( HMENU menu );

	void addComboString( HWND hwnd, const char* id );
	void deleteComboString( HWND hwnd, std::vector<StringID>::size_type index );
	void insertComboString( HWND hwnd, std::vector<StringID>::size_type index, const char* id );
	void resetContent( HWND hwnd );

	virtual void changed();

	static WindowTextNotifier& instance();
	static void fini();
	static LRESULT CALLBACK CallWndRetProc( int nCode, WPARAM wParam, LPARAM lParam );
	static LRESULT CALLBACK CallWndProc( int nCode, WPARAM wParam, LPARAM lParam );
	static LRESULT CALLBACK ComboProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
	static LRESULT CALLBACK ToolTipProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
	static LRESULT CALLBACK ToolTipParentProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
};

inline void localiseWindowText( HWND hwnd, const char* id )
{
	WindowTextNotifier::instance().set( hwnd, id );
}

inline void L( HWND hwnd, const char* id )
{
	localiseWindowText( hwnd, id );
}

#endif//WIN32

#endif//STRING_PROVIDER_HPP__
