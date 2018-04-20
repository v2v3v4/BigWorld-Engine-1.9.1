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
#include "resmgr/bwresource.hpp"
#include "controls/file_system_helper.hpp"
#include <vector>
#include "dir_dialog.hpp"
#include "shlobj.h"

#include < shlwapi.h >				// Required for the StrRet and Path functions


CString DirDialog::s_basePath_ = "";
std::vector<CString> DirDialog::s_paths_;

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

MyFolderFilter::MyFolderFilter()
			   : m_ulRef( 0 )
			   , m_pszFilter( NULL )
{
}

STDMETHODIMP MyFolderFilter::QueryInterface( IN     REFIID  riid, 
											  IN OUT LPVOID* ppvObj )
{
	ATLASSERT( ppvObj != NULL );
	if( !ppvObj )
		return ( E_FAIL );

	HRESULT hResult = ( E_NOINTERFACE );
	*ppvObj = NULL;

	if( IsEqualIID( riid, IID_IUnknown ) )
		*ppvObj =  static_cast< IUnknown* >( this );
	else if( IsEqualIID( riid, IID_IFolderFilter ) )
		*ppvObj =  static_cast< IFolderFilter* >( this );
	
	if( *ppvObj )
	{
		reinterpret_cast< IUnknown* >( *ppvObj )->AddRef();
		hResult = ( S_OK );
	}

	return ( hResult );
}

STDMETHODIMP_( ULONG ) MyFolderFilter::AddRef( VOID )
{
	return ::InterlockedIncrement( (PLONG)&m_ulRef );
}

STDMETHODIMP_( ULONG ) MyFolderFilter::Release( VOID )
{
	ULONG ulNewRef = ::InterlockedDecrement( (PLONG)&m_ulRef );
	if( ulNewRef <= 0 )
	{
		delete this;
		return ( 0 );
	}

	return ( ulNewRef );
}

STDMETHODIMP MyFolderFilter::ShouldShow( IN IShellFolder* pIShellFolder, 
										  IN LPCITEMIDLIST /*pidlFolder*/, 
										  IN LPCITEMIDLIST pidlItem )
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

	for( unsigned i = 0; i < DirDialog::paths().size(); ++i )
	{
		std::string s = DirDialog::paths()[i];
		pathToWindows( s );

		if( _strnicmp( dir.c_str(), s.c_str(), s.size() ) == 0 ||
			_strnicmp( dir.c_str(), s.c_str(), dir.size() ) == 0 )
			return S_OK;
	}

	//The folder is neither in the paths nor a parent, don't allow it
	return ( S_FALSE );
}

STDMETHODIMP MyFolderFilter::GetEnumFlags( IN  IShellFolder*	/*pIShellFolder*/, 
											IN  LPCITEMIDLIST	/*pidlFolder*/, 
											IN  HWND*			/*phWnd*/,
											OUT LPDWORD			pdwFlags )
{
	ASSERT( pdwFlags != NULL );

	*pdwFlags = (DWORD)( SHCONTF_FOLDERS | SHCONTF_NONFOLDERS );
	return ( S_OK );
}

/*static*/ bool DirDialog::isPathOk( const char* path )
{
	char tmp[MAX_PATH];
	strcpy( tmp, path );
	_strlwr( tmp );
	std::string spath = tmp;
	std::replace( spath.begin(), spath.end(), '\\', '/' );
	return BWResolver::dissolveFilename( spath ) != spath;
}

// Callback function called by SHBrowseForFolder's browse control after initialisation and when selection changes
/*static*/ int __stdcall DirDialog::browseCtrlCallback(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	DirDialog* pDirDialogObj = (DirDialog*)lpData;
	if (uMsg == BFFM_INITIALIZED )
	{
		if( ! pDirDialogObj->startDirectory_.IsEmpty() )
		{
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)(LPCTSTR)(pDirDialogObj->startDirectory_));
			if ( isPathOk( (LPCSTR)pDirDialogObj->startDirectory_ ) )
				SendMessage(hwnd, BFFM_ENABLEOK, 0, TRUE);
			else
				SendMessage(hwnd, BFFM_ENABLEOK, 0, FALSE);
		}
		if( ! pDirDialogObj->windowTitle_.IsEmpty() )
			SetWindowText(hwnd, (LPCTSTR) pDirDialogObj->windowTitle_);
	}
	else if( uMsg == BFFM_SELCHANGED )
	{
		LPITEMIDLIST pidl = (LPITEMIDLIST) lParam;
		char selection[MAX_PATH];
		if( ! ::SHGetPathFromIDList(pidl, selection) )
			selection[0] = '\0';

		if( isPathOk( selection ) )
			SendMessage(hwnd, BFFM_ENABLEOK, 0, TRUE);
		else
			SendMessage(hwnd, BFFM_ENABLEOK, 0, FALSE);
	}
	else if ( uMsg == BFFM_IUNKNOWN)
	{
		IUnknown* inter = (IUnknown*) lParam;
		if (inter != NULL)
        {
		    CComQIPtr<IFolderFilterSite> folderFilterSite;
		    if (SUCCEEDED(inter->QueryInterface(
			    IID_IFolderFilterSite, (void**)&folderFilterSite)))
		    {
			    folderFilterSite->SetFilter((IFolderFilter*)pDirDialogObj->folderFilter_);
		    }
        }
	}
	
	return 0;
}


DirDialog::DirDialog()
{
	folderFilter_ = new MyFolderFilter;
	
	if (s_basePath_.Compare("") == 0) // If the base path has not yet been set, set it...
	{

		// get application's directory
		char buffer[1024];
		GetModuleFileName( NULL, buffer, sizeof( buffer ) );
		CString appPath = buffer;
		appPath.Replace("\\","/");
		appPath.Delete( appPath.ReverseFind('/'), appPath.GetLength() );
				
		int pathNum = 0;
		CString defaultPath = BWResource::getPath( pathNum++ ).c_str();

		//Save this on the path list for later folder filtering
		s_paths_.push_back( defaultPath );
		s_paths_[pathNum-1] = s_paths_[pathNum-1] + "/";
		s_paths_[pathNum-1].Replace("/","\\");
		s_paths_[pathNum-1].MakeLower();

		CString test = BWResource::getPath( pathNum++ ).c_str();

		while (test.Compare( "" ) != 0) // If there is a test string to comapre against
		{
			if (test.CompareNoCase( appPath ) != 0) // Make sure we don't use the application path
			{
				//Save this on the path list for later folder filtering
				s_paths_.push_back( test );
				s_paths_[pathNum-1] = s_paths_[pathNum-1] + "/";
				s_paths_[pathNum-1].Replace("/","\\");
				s_paths_[pathNum-1].MakeLower();
				
				//Get the length of the common string
				int i = 0;
				while (defaultPath[i] && test[i] && (toupper(defaultPath[i]) == toupper(test[i])))
				{
					i++;
				}
				
				//Strip path down to the minimum common path
				defaultPath.Delete(i, defaultPath.GetLength());

			}
			
			//Get the next path to check
			test = BWResource::getPath(pathNum++).c_str();
		}

		defaultPath =
			CString(
				FileSystemHelper::fixCommonRootPath(
					std::string( defaultPath ) ).c_str() );

		s_basePath_ = defaultPath; // Save it away.
	}
}

DirDialog::~DirDialog()
{
	// This won't let me delete this for some reason without causing seg faults later (I blame Windows).
	// Yes, this is a very (very) slow leak.
	//delete folderFilter_;
}

BOOL DirDialog::doBrowse(CWnd *pwndParent)
{
	if( ! startDirectory_.IsEmpty() )
	{
		// correct the format
		startDirectory_.Replace('/', '\\');
		
		startDirectory_.TrimRight();
		if (startDirectory_.Right(1) == "\\")
			startDirectory_ = startDirectory_.Left(startDirectory_.GetLength() - 1);
	}
	
	LPMALLOC pMalloc;
	if (SHGetMalloc(&pMalloc) != NOERROR)
	{
		return FALSE;
	}
	
	BROWSEINFO bInfo;
	LPITEMIDLIST pidl;
	ZeroMemory((PVOID)(&bInfo), sizeof(BROWSEINFO));
	
	if (!fakeRootDirectory_.IsEmpty())
	{
		OLECHAR olePath[MAX_PATH];
		HRESULT hr;
		LPSHELLFOLDER pDesktopFolder;
		
		// the desktop's IShellFolder interface.
		if (SUCCEEDED(SHGetDesktopFolder(&pDesktopFolder)))
		{
			// correct the format
			fakeRootDirectory_.Replace('/', '\\');
			
			// need unicode
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, fakeRootDirectory_.GetBuffer(MAX_PATH), -1, olePath, MAX_PATH);
			
			// Convert the path to an ITEMIDLIST.
			hr = pDesktopFolder->ParseDisplayName(NULL, NULL, olePath, NULL, &pidl, NULL);
			
			if (FAILED(hr))
			{
				pMalloc->Free(pidl);
				pMalloc->Release();
				return FALSE;
			}
			bInfo.pidlRoot = pidl;
		}
	}
	else
	{
		//If the base root was not on the same drive then use the "Drives" virtual folder.
		LPITEMIDLIST pidlRoot = NULL;
		SHGetFolderLocation( NULL, CSIDL_DRIVES, 0, 0, &pidlRoot );
		bInfo.pidlRoot = pidlRoot;
	}
	
	bInfo.hwndOwner = pwndParent == NULL ? NULL : pwndParent->GetSafeHwnd();
	bInfo.pszDisplayName = userSelectedDirectory_.GetBuffer(MAX_PATH);
	bInfo.lpszTitle = (promptText_.IsEmpty()) ? "Open" : promptText_;
	bInfo.ulFlags = BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bInfo.lpfn = browseCtrlCallback;
	bInfo.lParam = (LPARAM)this;

	// open the dialog!
	if ((pidl = SHBrowseForFolder(&bInfo)) == NULL)
	{
		return FALSE;
	}
    
	// get the selected directory
	userSelectedDirectory_.ReleaseBuffer();
	if (SHGetPathFromIDList(pidl, userSelectedDirectory_.GetBuffer(MAX_PATH)) == FALSE)
	{
		pMalloc->Free(pidl);
		pMalloc->Release();
		return FALSE;
	}

	userSelectedDirectory_.ReleaseBuffer();
	userSelectedDirectory_.Replace('\\', '/');

	pMalloc->Free(pidl);
	pMalloc->Release();
	return TRUE;
}
