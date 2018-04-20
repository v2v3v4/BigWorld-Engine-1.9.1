/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef FILE_DIALOG_HPP
#define FILE_DIALOG_HPP

#include "afxdlgs.h"

class FolderSetter
{
	static const int MAX_PATH_SIZE = 8192;
	char envFolder_[ MAX_PATH_SIZE ];
	char curFolder_[ MAX_PATH_SIZE ];
public:
	FolderSetter()
	{
		GetCurrentDirectory( MAX_PATH_SIZE, envFolder_ );
		GetCurrentDirectory( MAX_PATH_SIZE, curFolder_ );
	}
	void enter()
	{
		GetCurrentDirectory( MAX_PATH_SIZE, envFolder_ );
		SetCurrentDirectory( curFolder_ );
	}
	void leave()
	{
		GetCurrentDirectory( MAX_PATH_SIZE, curFolder_ );
		SetCurrentDirectory( envFolder_ );
	}
};


class FolderGuard
{
	FolderSetter setter_;
public:
	FolderGuard()
	{
		setter_.enter();
	}
	~FolderGuard()
	{
		setter_.leave();
	}
};

/**
 *	This class works exactly the same as CFileDialog except that it
 *	preserves the current working folder.
 */
class BWFileDialog : public CFileDialog
{
	FolderGuard folderGuard_;
public:
	explicit BWFileDialog( BOOL bOpenFileDialog, LPCTSTR lpszDefExt = NULL,
		LPCTSTR lpszFileName = NULL, DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		LPCTSTR lpszFilter = NULL, CWnd* pParentWnd = NULL, DWORD dwSize = 0 )
			: CFileDialog( bOpenFileDialog, lpszDefExt, lpszFileName, dwFlags,
				lpszFilter, pParentWnd, dwSize )
	{
	}
};

#endif // FILE_DIALOG_HPP
