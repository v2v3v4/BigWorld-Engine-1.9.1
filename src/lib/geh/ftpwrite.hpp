/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _FTPWRITE_HPP__
#define _FTPWRITE_HPP__

#include <wininet.h>
#pragma comment(lib,"wininet.lib")

#define AGENT_NAME "BWTech"

// this is a ftp write only class, should only be used in GEH
namespace Ftp
{
	static HINTERNET inet;
	static HINTERNET ftp;
	static HINTERNET file;

	inline void InitFtp( const char* server, const char* username, const char* password )
	{
		inet = InternetOpen( AGENT_NAME, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0 );
		if( inet )
		{
			ftp = InternetConnect( inet, server, INTERNET_DEFAULT_FTP_PORT,
				username, password, INTERNET_SERVICE_FTP, 0, 0 );
			if( !ftp )
			{
				InternetCloseHandle( inet );
				inet = NULL;
			}
		}
	}
	inline void createDirectory( const char* dirname )
	{
		if( ftp )
			FtpCreateDirectory( ftp, dirname );
	}
	inline void setCurrentDirectory( const char* dirname )
	{
		if( ftp )
			FtpSetCurrentDirectory( ftp, dirname );
	}
	inline BOOL putFile( const char* localname, const char* remotename )
	{
		if( ftp )
			return FtpPutFile( ftp, localname, remotename, FTP_TRANSFER_TYPE_BINARY, 0 );
		return FALSE;
	}
	inline BOOL openFile( const char* remotename )
	{
		if( ftp )
		{
			file = FtpOpenFile( ftp, remotename, GENERIC_WRITE, FTP_TRANSFER_TYPE_BINARY, 0 );
			return file != NULL;
		}
		return FALSE;
	}
	inline void closeFile()
	{
		if( file )
		{
			InternetCloseHandle( file );
			file = NULL;
		}
	}
	inline void writeFile( const void* buffer, DWORD& size )
	{
		if( file )
			InternetWriteFile( file, buffer, size, &size );
	}
	inline void DeInitFtp()
	{
		if( file )
			InternetCloseHandle( file );
		if( ftp )
			InternetCloseHandle( ftp );
		if( inet )
			InternetCloseHandle( inet );
		inet = ftp = file = NULL;
	}
};

#endif//_FTPWRITE_HPP__
