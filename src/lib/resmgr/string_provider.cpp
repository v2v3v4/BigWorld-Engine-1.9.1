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
#include "string_provider.hpp"
#include <malloc.h>
#include <algorithm>

class DummyLanguage : public Language
{
public:
	void load( DataSectionPtr language, const std::string& root = "" ){};
	virtual const char* getLanguageName() const
	{
		return getLanguageEnglishName();
	}
	virtual const char* getLanguageEnglishName() const
	{
		return "English";
	}
	virtual const std::string& getIsoLangName() const
	{
		static std::string name = "en";
		return name;
	}
	virtual const std::string& getIsoCountryName() const
	{
		static std::string name = "us";
		return name;
	}
	virtual const char* getString( const StringID& id ) const
	{
		return NULL;
	}
};

StringID::StringID( const char* str )
: str_( isLocaliseToken( str ) ? str + 1: str )
{
	static unsigned char hash[ 256 ];
	static bool inithash = true;
	if( inithash )
	{
		inithash = false;
		for( int i = 0; i < 256; ++i )
			hash[ i ] = i;
		int k = 7;
		for( int j = 0; j < 4; ++j )
			for( int i = 0; i < 256; ++i )
			{
				unsigned char s = hash[ i ];
				k = ( k + s ) % 256;
				hash[ i ] = hash[ k ];
				hash[ k ] = s;
			}
	}
	key_ = ( 123 + str_.size() ) % 256;// 123 is just a hash magic, you can use any number
	for( unsigned int i = 0; i < str_.size(); ++i )
	{
		key_ = ( key_ + str_[ i ] ) % 256;
		key_ = hash[ key_ ];
	}
}

bool StringID::operator<( const StringID& that ) const
{
	if( key_ < that.key_ )
		return true;
	if( key_ > that.key_ )
		return false;
	return str_ < that.str_;
}

std::pair<std::string, std::string> Language::splitIsoLangCountryName( const std::string& isoLangCountryName )
{
	std::pair<std::string, std::string> result;
	if( isoLangCountryName.size() == (size_t)ISO_NAME_LENGTH )
		result = std::make_pair( isoLangCountryName, "" );
	else if( isoLangCountryName.size() == ISO_NAME_LENGTH * 2 + 1 &&
		isoLangCountryName[ ISO_NAME_LENGTH ] == '_' )
	{
		result = std::make_pair( isoLangCountryName.substr( 0, ISO_NAME_LENGTH ),
			isoLangCountryName.substr( ISO_NAME_LENGTH + 1, ISO_NAME_LENGTH ) );
	}
	std::transform( result.first.begin(), result.first.end(), result.first.begin(), tolower );
	std::transform( result.second.begin(), result.second.end(), result.second.begin(), tolower );
	return result;
}

class DSLanguage : public Language
{
public:
	DSLanguage( DataSectionPtr language );
	void load( DataSectionPtr language, const std::string& root = "" );
	virtual const char* getLanguageName() const;
	virtual const char* getLanguageEnglishName() const;
	virtual const std::string& getIsoLangName() const;
	virtual const std::string& getIsoCountryName() const;
	virtual const char* getString( const StringID& id ) const;
private:
	std::string isoLangName_;
	std::string isoCountryName_;
	std::map<StringID,unsigned int> strings_;
	std::vector<char> stringBuffer_;
};

DSLanguage::DSLanguage( DataSectionPtr language )
{
	std::pair<std::string, std::string> names = splitIsoLangCountryName( language->sectionName() );
	isoLangName_ = names.first;
	isoCountryName_ = names.second;
	if( !isoLangName_.empty() )
		load( language );
}

void DSLanguage::load( DataSectionPtr language, const std::string& root /*= ""*/ )
{
	for( int i = 0; i < language->countChildren(); ++i )
	{
		DataSectionPtr child = language->openChild( i );
		std::string name = child->sectionName();
		std::string value = child->asString();

		unsigned int offset = stringBuffer_.size();
		stringBuffer_.resize( stringBuffer_.size() + value.size() + 1 );
		std::strcpy( &stringBuffer_[ offset ], value.c_str() );
		strings_[ StringID( ( root + name ) .c_str() ) ] = offset;

		if( child->countChildren() )
			load( child, root + name + '/' );
	}
}

static StringID LanguageNameTag( LANGUAGE_NAME_TAG );
static StringID LanguageEnglishNameTag( ENGLISH_LANGUAGE_NAME_TAG );

const char* DSLanguage::getLanguageName() const
{
	return	strings_.find( LanguageNameTag ) != strings_.end()					?
		&stringBuffer_[0] + strings_.find( LanguageNameTag )->second			:
		getLanguageEnglishName();
}

const char* DSLanguage::getLanguageEnglishName() const
{
	return	strings_.find( LanguageEnglishNameTag ) != strings_.end()			?
		&stringBuffer_[0] + strings_.find( LanguageEnglishNameTag )->second		:
		"(Invalid)";
}

const std::string& DSLanguage::getIsoLangName() const
{
	return isoLangName_;
}

const std::string& DSLanguage::getIsoCountryName() const
{
	return isoCountryName_;
}

const char* DSLanguage::getString( const StringID& id ) const
{
	return strings_.find( id ) != strings_.end()			?
		&stringBuffer_[0] + strings_.find( id )->second		:
		NULL;
}

LanguageNotifier::LanguageNotifier()
{
	StringProvider::instance().registerNotifier( this );
}

LanguageNotifier::~LanguageNotifier()
{
	StringProvider::instance().unregisterNotifier( this );
}

void StringProvider::load( DataSectionPtr file )
{
	if( file )
	{
		for( int i = 0; i < file->countChildren(); ++i )
		{
			DataSectionPtr child = file->openChild( i );
			if( child )
			{
				std::pair<std::string, std::string> names = Language::splitIsoLangCountryName( child->sectionName() );
				bool found = false;
				for( unsigned int j= 0; j < languageNum(); ++j )
				{
					LanguagePtr lang = getLanguage( j );
					if( lang->getIsoLangName() == names.first && lang->getIsoCountryName() == names.second )
					{
						lang->load( child );
						found = true;
						break;
					}
				}
				if( !found )
				{
					LanguagePtr l = new DSLanguage( child );
					if( !l->getIsoLangName().empty() )
						languages_.push_back( l );
				}
			}
		}
		for( unsigned int i = 0; i < languageNum(); ++i )
		{
			LanguagePtr lang = getLanguage( i );
			if( lang->getIsoLangName() == DEFAULT_LANGUAGE_NAME &&
				lang->getIsoCountryName() == DEFAULT_COUNTRY_NAME )
			{
				defaultLanguage_ = lang;
				break;
			}
			if( !defaultLanguage_ && lang->getIsoLangName() == DEFAULT_LANGUAGE_NAME )
				defaultLanguage_ = lang;
		}
	}
}

unsigned int StringProvider::languageNum() const
{
	return languages_.size();
}

LanguagePtr StringProvider::getLanguage( unsigned int index ) const
{
	return languages_.at( index );
}

void StringProvider::setLanguage()
{
#ifdef WIN32
	char country[16], lang[16];
	GetLocaleInfo( LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, country, sizeof( country ) );
	GetLocaleInfo( LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lang, sizeof( lang ) );
	_strlwr( country );
	_strlwr( lang );
	setLanguages( lang, country );
#else// TODO: need find a solution on linux to automatically config the locale
	setLanguages( "en", "us" );
#endif
}

void StringProvider::setLanguage( unsigned int language )
{
	setLanguages( getLanguage( language )->getIsoLangName(), getLanguage( language )->getIsoCountryName() );
}

void StringProvider::setLanguages( const std::string& langName, const std::string& countryName )
{
	currentLanguage_ = NULL;
	currentMainLanguage_ = NULL;

	for (unsigned int i = 0; i < languageNum(); ++i)
	{
		LanguagePtr lang = getLanguage( i );
		if (lang->getIsoLangName() == langName && lang->getIsoCountryName() == countryName)
			currentLanguage_ = lang;
		if (lang->getIsoLangName() == langName && lang->getIsoCountryName() == "")
			currentMainLanguage_ = lang;
	}

	if (currentLanguage_ == NULL && currentMainLanguage_ == NULL)
	{
		for (unsigned int i = 0; i < languageNum(); ++i)
		{
			LanguagePtr lang = getLanguage( i );
			if (lang->getIsoLangName() == "en" && lang->getIsoCountryName() == "us")
				currentLanguage_ = lang;
			if (lang->getIsoLangName() == "en" && lang->getIsoCountryName() == "")
				currentMainLanguage_ = lang;
		}
	}

	if (currentLanguage_ == NULL && currentMainLanguage_ == NULL)
	{
		currentLanguage_ = currentMainLanguage_ = new DummyLanguage;
	}
	notify();
}

const char* StringProvider::str( const StringID& id ) const
{
	const char* result = NULL;
	if( currentLanguage_ ) result = currentLanguage_->getString( id );
	if( !result && currentMainLanguage_ ) result = currentMainLanguage_->getString( id );
	if( !result && defaultLanguage_ ) result = defaultLanguage_->getString( id );
	return result;
}

const char* StringProvider::str( const char* id, DefResult def /*= RETURN_PARAM_IF_NOT_EXISTING*/ ) const
{
	const char* realId = id;
	if ( isLocaliseToken( id ) )
		realId++;
	const char* result = str( StringID( realId ) );
	if( !result && def == RETURN_PARAM_IF_NOT_EXISTING )
		result = id;
	return result;
}

LanguagePtr StringProvider::currentLanguage() const
{
	return	currentLanguage_		?	currentLanguage_		:
			currentMainLanguage_	?	currentMainLanguage_	:
			defaultLanguage_;
}

StringProvider& StringProvider::instance()
{
	static StringProvider StringProvider;
	return StringProvider;
}

void StringProvider::registerNotifier( LanguageNotifier* notifier )
{
	notifiers_.insert( notifier );
}

void StringProvider::unregisterNotifier( LanguageNotifier* notifier )
{
	if( notifiers_.find( notifier ) != notifiers_.end() )
		notifiers_.erase( notifiers_.find( notifier ) );
}

void StringProvider::notify()
{
	std::set<LanguageNotifier*> notifiers = notifiers_;
	for( std::set<LanguageNotifier*>::iterator iter = notifiers.begin();
		iter != notifiers.end(); ++iter )
		(*iter)->changed();
}

#ifdef WIN32
template<typename STR>
bool localise( HWND hwnd, STR& str )
{
	const char*& s = *(const char**)&str;
	if( isLocaliseToken( s ) )
	{
		WindowTextNotifier::instance().set( hwnd, s );
		s = StringProvider::instance().str( s + 1 );
		return true;
	}
	return false;
}

WindowTextNotifier::WindowTextNotifier()
{
	callWinRetHook_ = SetWindowsHookEx( WH_CALLWNDPROCRET, CallWndRetProc,
		GetModuleHandle( NULL ), GetCurrentThreadId() );
	callWinHook_ = SetWindowsHookEx( WH_CALLWNDPROC, CallWndProc,
		GetModuleHandle( NULL ), GetCurrentThreadId() );
	comboWndProc_ = NULL;
	toolTipProc_ = NULL;
}

WindowTextNotifier::~WindowTextNotifier()
{
	UnhookWindowsHookEx( callWinRetHook_ );
	UnhookWindowsHookEx( callWinHook_ );
}

void WindowTextNotifier::set( HWND hwnd, const char* id )
{
	if( id )
	{
		windows_[ hwnd ] = StringID( id );
	}
	else
	{
		if( windows_.find( hwnd ) != windows_.end() )
			windows_.erase( windows_.find( hwnd ) );
	}
}

void WindowTextNotifier::set( HMENU menu )
{
	char text[ 1024 ];
	for( int i = 0; i < GetMenuItemCount( menu ); ++i )
	{
		if( GetMenuString( menu, i, text, sizeof( text ), MF_BYPOSITION ) &&
			isLocaliseToken( text ) )
		{
			menus_[ std::make_pair( menu, i ) ] = StringID( text );
			ModifyMenu( menu, i, MF_BYPOSITION,
				GetMenuItemID( menu, i ), L( text ) );
		}
		if( GetMenuItemID( menu, i ) == (UINT)-1 )
		{
			set( GetSubMenu( menu, i ) );
		}
	}
}

void WindowTextNotifier::addComboString( HWND hwnd, const char* id )
{
	combos_[ hwnd ].push_back( StringID( id ) );
}

void WindowTextNotifier::deleteComboString( HWND hwnd, std::vector<StringID>::size_type index )
{
	if( index < combos_[ hwnd ].size() )
	{
		combos_[ hwnd ].erase( combos_[ hwnd ].begin() + index );
		if( combos_[ hwnd ].empty() )
			combos_.erase( hwnd );
	}
}

void WindowTextNotifier::insertComboString( HWND hwnd, std::vector<StringID>::size_type index, const char* id )
{
	if( index == ( std::vector<StringID>::size_type )-1 || index >= combos_[ hwnd ].size() )
		addComboString( hwnd, id );
	else if( index < combos_[ hwnd ].size() )
		combos_[ hwnd ].insert( combos_[ hwnd ].begin() + index, StringID( id ) );
}

void WindowTextNotifier::resetContent( HWND hwnd )
{
	combos_[ hwnd ].clear();
}

void WindowTextNotifier::changed()
{
	std::set<HWND> destroyed;
	for( std::map<HWND, StringID>::iterator iter = windows_.begin(); iter != windows_.end(); ++iter )
	{
		if( IsWindow( iter->first ) )
			SetWindowText( iter->first, L( iter->second.str().c_str() ) );
		else
			destroyed.insert( iter->first );
	}
	for( std::map<HWND, std::vector<StringID> >::iterator iter = combos_.begin();
		iter != combos_.end(); ++iter )
	{
		if( IsWindow( iter->first ) )
		{
			std::vector<StringID> ids = iter->second;
			int curSel = SendMessage( iter->first, CB_GETCURSEL, 0, 0 );
			SendMessage( iter->first, CB_RESETCONTENT, 0, 0 );
			for( std::vector<StringID>::iterator viter = ids.begin(); viter != ids.end(); ++viter )
				SendMessage( iter->first, CB_ADDSTRING, 0, (LPARAM)viter->str().c_str() );
			SendMessage( iter->first, CB_SETCURSEL, curSel, 0 );
		}
		else
			destroyed.insert( iter->first );
	}
	for( std::set<HWND>::iterator iter = destroyed.begin(); iter != destroyed.end(); ++iter )
		set( *iter, NULL );

	for( std::map<std::pair<HMENU, UINT>, StringID>::iterator iter = menus_.begin();
		iter != menus_.end(); ++iter )
	{
		if( IsMenu( iter->first.first ) )
		{
			ModifyMenu( iter->first.first, iter->first.second, MF_BYPOSITION,
				GetMenuItemID( iter->first.first, iter->first.second ),
				StringProvider::instance().str( iter->second.str().c_str() ) );
		}
	}
}

namespace
{
	static __declspec(thread) WindowTextNotifier* s_instance = NULL;
}

WindowTextNotifier& WindowTextNotifier::instance()
{
	if( s_instance == NULL )
		s_instance = new WindowTextNotifier;
	return *s_instance;
}

void WindowTextNotifier::fini()
{
	delete s_instance; s_instance = NULL;
}

LRESULT CALLBACK WindowTextNotifier::CallWndRetProc( int nCode, WPARAM wParam, LPARAM lParam )
{
	CWPRETSTRUCT* rs = (CWPRETSTRUCT*)lParam;
	if( rs->message == WM_DESTROY )
		WindowTextNotifier::instance().set( rs->hwnd, NULL );
	else if( rs->message == WM_SETTEXT )
	{
		if( localise( rs->hwnd, rs->lParam ) )
			SetWindowText( rs->hwnd, (const char*)rs->lParam );
	}
	return CallNextHookEx( WindowTextNotifier::instance().callWinRetHook_, nCode, wParam, lParam );
}

LRESULT CALLBACK WindowTextNotifier::CallWndProc( int nCode, WPARAM wParam, LPARAM lParam )
{
	CWPSTRUCT* rs = (CWPSTRUCT*)lParam;

	// handle ComboBox first
	char className[1024];
	GetClassName( rs->hwnd, className, 1024 );
	className[ sizeof(className) - 1 ] = 0;
	if( strcmp( className, "ComboBox" ) == 0 )
	{
		if( GetClassLong( rs->hwnd, GCL_WNDPROC ) != (LONG)ComboProc )
		{
			if( !WindowTextNotifier::instance().comboWndProc_ )
				WindowTextNotifier::instance().comboWndProc_ = (WNDPROC)GetClassLong( rs->hwnd, GCL_WNDPROC );
			SetClassLong( rs->hwnd, GCL_WNDPROC, (LONG)ComboProc );
		}
	}
	else if( strcmp( className, TOOLTIPS_CLASS ) == 0 )
	{
		if( GetClassLong( rs->hwnd, GCL_WNDPROC ) != (LONG)ToolTipProc )
		{
			if( !WindowTextNotifier::instance().toolTipProc_ )
				WindowTextNotifier::instance().toolTipProc_ = (WNDPROC)GetClassLong( rs->hwnd, GCL_WNDPROC );
			SetClassLong( rs->hwnd, GCL_WNDPROC, (LONG)ToolTipProc );
		}
	}
	else if( rs->message == WM_CREATE )
	{
		CREATESTRUCT* cs = (CREATESTRUCT*)rs->lParam;
		if( localise( rs->hwnd, cs->lpszName ) )
			SetWindowText( rs->hwnd, cs->lpszName );
	}
	return CallNextHookEx( WindowTextNotifier::instance().callWinHook_, nCode, wParam, lParam );
}

LRESULT CALLBACK WindowTextNotifier::ComboProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	if( msg == CB_ADDSTRING )
	{
		WindowTextNotifier::instance().addComboString( hwnd, (char*)lParam );
		localise( hwnd, lParam );
	}
	else if( msg == CB_DELETESTRING )
	{
		WindowTextNotifier::instance().deleteComboString( hwnd, wParam );
	}
	else if( msg == CB_FINDSTRING )
	{
		localise( hwnd, lParam );
	}
	else if( msg == CB_FINDSTRINGEXACT )
	{
		localise( hwnd, lParam );
	}
	else if( msg == CB_INSERTSTRING )
	{
		WindowTextNotifier::instance().insertComboString( hwnd, wParam, (char*)lParam );
	}
	else if( msg == CB_SELECTSTRING )
	{
		localise( hwnd, lParam );
	}
	else if( msg == CB_RESETCONTENT )
	{
		WindowTextNotifier::instance().resetContent( hwnd );
	}
	return CallWindowProc( WindowTextNotifier::instance().comboWndProc_,
		hwnd, msg, wParam, lParam );
}

LRESULT CALLBACK WindowTextNotifier::ToolTipProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	if( msg == TTM_ADDTOOL || msg == TTM_SETTOOLINFO || msg == TTM_UPDATETIPTEXT )
	{
		TOOLINFO ti = *(LPTOOLINFO)lParam;
		if( ti.lpszText && ti.lpszText != LPSTR_TEXTCALLBACK )
		{
			char s[10240];
			if( ti.hinst != NULL )
			{
				if( LoadString( ti.hinst, (UINT)ti.lpszText, s, sizeof(s) - 1 ) )
				{
					ti.hinst = NULL;
					ti.lpszText = s;
				}
			}
			if( ti.hinst == NULL )
			{
				localise( hwnd, ti.lpszText );
				lParam = (LPARAM)&ti;
			}
		}
		else if( ti.lpszText == LPSTR_TEXTCALLBACK )
		{
			std::map<HWND, WNDPROC>& subClassMap = WindowTextNotifier::instance().subClassMap_;
			if( subClassMap.find( ti.hwnd ) == subClassMap.end() )
			{
				subClassMap[ ti.hwnd ] = (WNDPROC)GetWindowLong( ti.hwnd, GWL_WNDPROC );
				SetWindowLong( ti.hwnd, GWL_WNDPROC, (LONG)ToolTipParentProc );
			}
		}
	}
	return CallWindowProc( WindowTextNotifier::instance().toolTipProc_,
		hwnd, msg, wParam, lParam );
}

LRESULT CALLBACK WindowTextNotifier::ToolTipParentProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	std::map<HWND, WNDPROC>& subClassMap = WindowTextNotifier::instance().subClassMap_;

	LRESULT result = CallWindowProc( subClassMap[hwnd], hwnd, msg, wParam, lParam );

	if( msg == WM_NCDESTROY )
	{
		SetWindowLong( hwnd, GWL_WNDPROC, (LONG)subClassMap[hwnd] );
		subClassMap.erase( hwnd );
	}
	else if( msg == WM_NOTIFY )
	{
		NMTTDISPINFO* dispInfo = (NMTTDISPINFO*)lParam;
		if( dispInfo->hdr.code == TTN_NEEDTEXTA )
		{
			char* str = dispInfo->lpszText;
			if( isLocaliseToken( str ) )
			{
				std::strcpy( str, L( str ) );
			}
		}
		else if( dispInfo->hdr.code == TTN_NEEDTEXTW )
		{
			wchar_t* str = (wchar_t*)dispInfo->lpszText;
			if( isLocaliseToken( str ) )
			{
				char buffer[1024];
				BOOL defaultCharUsed = FALSE;
				if( WideCharToMultiByte( CP_OEMCP, 0, str, -1, buffer, sizeof( buffer ), "?", &defaultCharUsed ) &&
					!defaultCharUsed )
				{
					std::strcpy( buffer, L( buffer ) );
					MultiByteToWideChar( CP_OEMCP, 0, buffer, -1, str, sizeof( dispInfo->szText ) );
				}
			}
		}
	}

	return result;
}

#endif//WIN32
