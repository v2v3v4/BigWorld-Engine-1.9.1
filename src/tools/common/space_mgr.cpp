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
#include "space_mgr.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/string_provider.hpp"
#include "controls/file_system_helper.hpp"
#include "chunk/chunk_space.hpp"
#include <shobjidl.h>
#include <shlobj.h>
#include <sstream>
#include <algorithm>

// helper function to ensure that paths are understood by windows even if
// EDITOR_ENABLED is not defined (i.e. NavGen)
static void pathToWindows( std::string& path )
{
	// make sure it has a drive letter.
	std::replace( path.begin(), path.end(), '/', '\\' );
	char fullPath[MAX_PATH];
	char* filePart;
	if ( GetFullPathName( path.c_str(), sizeof( fullPath ), fullPath, &filePart ) )
		path = fullPath;
}

class FolderFilter : public IFolderFilter
{
public:
	FolderFilter()
	{}

public: // IUnknown implementation
	STDMETHOD( QueryInterface )( IN REFIID riid, IN OUT LPVOID* ppvObj )
	{
		*ppvObj = NULL;

		if( IsEqualIID( riid, IID_IUnknown ) )
			*ppvObj =  static_cast< IUnknown* >( this );
		else if( IsEqualIID( riid, IID_IFolderFilter ) )
			*ppvObj =  static_cast< IFolderFilter* >( this );

		return *ppvObj ? S_OK : E_NOINTERFACE;
	}
	STDMETHOD_( ULONG, AddRef )( VOID )
	{
		return 1;
	}
	STDMETHOD_( ULONG, Release )( VOID )
	{
		return 1;
	}

public: // IFolderFilter implementation
	STDMETHOD( ShouldShow )( IN IShellFolder* pIShellFolder, IN LPCITEMIDLIST pidlFolder, IN LPCITEMIDLIST pidlItem )
	{
		MF_ASSERT( pIShellFolder != NULL );	
		MF_ASSERT( pidlItem	  != NULL );
	
		// If an item is a folder, then accept it
		LPCITEMIDLIST pidl[ 1 ] = { pidlItem };
		SFGAOF ulAttr = SFGAO_FOLDER;
		pIShellFolder->GetAttributesOf( 1, pidl, &ulAttr );
	
		//Ignore all non-folders
		if ( ( ulAttr & SFGAO_FOLDER ) !=  SFGAO_FOLDER )
			return ( S_FALSE );
		
		STRRET name;

		pIShellFolder->GetDisplayNameOf( pidlItem, SHGDN_FORPARSING, &name );

		std::string dir;

		switch (name.uType)
		{
		case STRRET_WSTR:
			{
				std::wstring wstr = name.pOleStr;
				dir.assign( wstr.begin(), wstr.end() );
			}
			break;

		case STRRET_CSTR:
			dir = name.cStr;
			break;

		case STRRET_OFFSET:
			dir = (char*)pidlItem + name.uOffset;
			break;
		}

		//Add some cosmetics for string matching
		_strlwr( &dir[ 0 ] );
		if( *dir.rbegin() != '\\' )
			dir += '\\';

		for( int i = 0; i < BWResource::getPathNum(); ++i )
		{
			std::string s = BWResource::getPath( i );
			pathToWindows( s );

			if( _strnicmp( dir.c_str(), s.c_str(), s.size() ) == 0 ||
				_strnicmp( dir.c_str(), s.c_str(), dir.size() ) == 0 )
				return S_OK;
		}

		//The folder is neither in the paths nor a parent, don't allow it
		return ( S_FALSE );
	}
	STDMETHOD( GetEnumFlags )( IN IShellFolder* /*pIShellFolder*/, IN LPCITEMIDLIST /*pidlFolder*/, IN HWND* /*phWnd*/, OUT LPDWORD pdwFlags )
	{
		*pdwFlags = SHCONTF_FOLDERS;
		return S_OK;
	}
};

static FolderFilter folderFilter;

SpaceManager::SpaceManager( MRUProvider& mruProvider, std::vector<std::string>::size_type maxMRUEntries /*= 10*/ ):
	mruProvider_( mruProvider ), maxMRUEntries_( maxMRUEntries )
{
	for( std::vector<std::string>::size_type i = 0; i < maxMRUEntries_; ++i )
	{
		std::stringstream ss;
		ss << "space/mru" << i;
		std::string spacePath = mruProvider_.get( ss.str() );
		if( spacePath.size() )
			recentSpaces_.push_back( spacePath );
	}
}

void SpaceManager::addSpaceIntoRecent( const std::string& space )
{
	for( std::vector<std::string>::iterator iter = recentSpaces_.begin();
		iter != recentSpaces_.end(); ++iter )
		if( *iter == space )
		{
			recentSpaces_.erase( iter );
			break;
		}
	if( recentSpaces_.size() >= maxMRUEntries_ )
		recentSpaces_.pop_back();
	recentSpaces_.insert( recentSpaces_.begin(), space );

	for( std::vector<std::string>::size_type i = 0; i < maxMRUEntries_; ++i )
	{
		std::stringstream ss;
		ss << "space/mru" << i;
		mruProvider_.set( ss.str(), "" );
	}

	for( std::vector<std::string>::size_type i = 0; i < num(); ++i )
	{
		std::stringstream ss;
		ss << "space/mru" << i;
		mruProvider_.set( ss.str(), entry( i ) );
	}
}

void SpaceManager::removeSpaceFromRecent( const std::string& space )
{
	for( std::vector<std::string>::iterator iter = recentSpaces_.begin();
		iter != recentSpaces_.end(); ++iter )
		if( *iter == space )
		{
			recentSpaces_.erase( iter );
			break;
		}

	for( std::vector<std::string>::size_type i = 0; i < maxMRUEntries_; ++i )
	{
		std::stringstream ss;
		ss << "space/mru" << i;
		mruProvider_.set( ss.str(), "" );
	}

	for( std::vector<std::string>::size_type i = 0; i < num(); ++i )
	{
		std::stringstream ss;
		ss << "space/mru" << i;
		mruProvider_.set( ss.str(), entry( i ) );
	}
}

std::vector<std::string>::size_type SpaceManager::num() const
{
	return recentSpaces_.size();
}

std::string SpaceManager::entry( std::vector<std::string>::size_type index ) const
{
	return recentSpaces_.at( index );
}

std::string SpaceManager::browseForSpaces( HWND parent ) const
{
	CoInitialize( NULL );
	char path[ MAX_PATH ];
	std::string result;
	BROWSEINFO browseInfo = { 0 };
    browseInfo.hwndOwner = parent;
    browseInfo.pidlRoot = commonRoot();
    browseInfo.pszDisplayName = path;
    browseInfo.lpszTitle = L("COMMON/SPACE_MGR/SELECT_FOLDER");
    browseInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;
    browseInfo.lpfn = BrowseCallbackProc;
    browseInfo.lParam = (LPARAM)this;
	LPITEMIDLIST pidl = SHBrowseForFolder( &browseInfo );
	if( pidl || browseInfo.pidlRoot )
	{
		if( pidl )
			result = getFolderByPidl( pidl );
		LPMALLOC malloc;
		if( SUCCEEDED( SHGetMalloc( &malloc ) ) )
		{
			if( pidl )
				malloc->Free( pidl );
			if( browseInfo.pidlRoot )
				malloc->Free( (void*)browseInfo.pidlRoot );
		}
	}
	CoUninitialize();
	return result;
}

int CALLBACK SpaceManager::BrowseCallbackProc( HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData )
{
	SpaceManager* mgr = (SpaceManager*)lpData;
	std::string path;
	switch( uMsg )
	{
	case BFFM_INITIALIZED:
		SendMessage( hwnd, BFFM_SETOKTEXT, 0, (LPARAM) L"&Open Space");
		if( mgr->num() )
		{
			path = BWResolver::resolveFilename( mgr->entry( 0 ) );
			pathToWindows( path );
			SendMessage( hwnd, BFFM_SETSELECTIONA, TRUE, (LPARAM) path.c_str() );
		}
		break;
	case BFFM_SELCHANGED:
		path = getFolderByPidl( (LPITEMIDLIST)lParam );
		if( !path.empty() )
		{
			if( *path.rbegin() != '\\' )
				path += '\\';
			pathToWindows( path );
			SendMessage( hwnd, BFFM_ENABLEOK, 0,
				GetFileAttributes( ( path + SPACE_SETTING_FILE_NAME ).c_str() )
				!= INVALID_FILE_ATTRIBUTES );
		}
		break;
	case BFFM_IUNKNOWN:
		if( lParam )
		{
			IUnknown* unk = (IUnknown*)lParam;
			IFolderFilterSite* site;
			if( SUCCEEDED( unk->QueryInterface( IID_IFolderFilterSite, (LPVOID*)&site ) ) )
			{
				site->SetFilter( &folderFilter );
				site->Release();
			}
		}
		break;
	}
	return 0;
}

std::string SpaceManager::getFolderByPidl( LPITEMIDLIST pidl )
{
	char path[ MAX_PATH ];
	if( SHGetPathFromIDList( pidl, path ) )
		return path;
	return "";
}

LPITEMIDLIST SpaceManager::commonRoot()
{
	if( BWResource::getPathNum() == 0 )
		return NULL;
	std::string root = BWResource::getPath( 0 );
	_strlwr( &root[0] );
	for( int i = 1; i < BWResource::getPathNum(); ++i )
	{
		std::string s = BWResource::getPath( i );
		_strlwr( &s[0] );
		if( s.size() > root.size() )
			s.resize( root.size() );
		else
			root.resize( s.size() );
		while( s != root )
			s.resize( s.size() - 1 ), root.resize( root.size() - 1 );
	}

	if( !root.empty() )
	{
		root = FileSystemHelper::fixCommonRootPath( root );

		pathToWindows( root );
		
		SFGAOF f;
		LPITEMIDLIST pidl;
		std::wstring wstr( root.begin(), root.end() );
		if( SUCCEEDED( SHParseDisplayName( wstr.c_str(), NULL, &pidl, 0, &f ) ) )
			return pidl;
	}

	//If the base root was not on the same drive then use the "Drives" virtual folder.
	LPITEMIDLIST pidlDrives = NULL;
	SHGetFolderLocation( NULL, CSIDL_DRIVES, 0, 0, &pidlDrives );
    return pidlDrives;


}
