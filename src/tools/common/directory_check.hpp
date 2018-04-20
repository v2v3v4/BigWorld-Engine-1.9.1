/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#pragma once

class DirectoryCheck
{
public:
	DirectoryCheck( const std::string& appName )
	{
		static const int MAX_PATH_SIZE = 8192;
		TCHAR curDir[ MAX_PATH_SIZE ];
		GetCurrentDirectory( MAX_PATH_SIZE, curDir );
		if( curDir[ _tcslen( curDir ) - 1 ] != '\\' )//curDir will not be empty
			_tcscat( curDir, TEXT( "\\resources" ) );
		else
			_tcscat( curDir, TEXT( "resources" ) );
		if( GetFileAttributes( curDir ) == INVALID_FILE_ATTRIBUTES )
		{// we are not running in the proper directory
			GetModuleFileName( NULL, curDir, MAX_PATH_SIZE );
			if( _tcsrchr( curDir, '\\' ) )
				*_tcsrchr( curDir, '\\' ) = 0;
			_tcscat( curDir, TEXT( "\\resources" ) );
			if( GetFileAttributes( curDir ) != INVALID_FILE_ATTRIBUTES )
			{
				_tcsrchr( curDir, '\\' )[ 1 ] = 0;
				SetCurrentDirectory( curDir );
			}
			else
			{
				GetCurrentDirectory( MAX_PATH_SIZE, curDir );
				MessageBox( NULL,
					L("COMMON/DIRECTORY_CHECK/DIR_CHECK" , appName, curDir, appName ),
					appName.c_str(), MB_OK | MB_ICONASTERISK );
			}
		}
	}
};