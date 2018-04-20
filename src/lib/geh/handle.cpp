/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "handle.hpp"
#include <dbgHelp.h>
#include <stdio.h>
#include "geh.hpp"
#include "ftpwrite.hpp"
#include <strsafe.h>
#pragma comment( lib, "strsafe.lib" )
#pragma comment( lib, "winmm.lib" )
#pragma comment( lib, "shlwapi.lib" )

typedef BOOL (WINAPI *MiniDumpWriteDumpFunc)( HANDLE hProcess, DWORD ProcessId, HANDLE hFile, MINIDUMP_TYPE DumpType,
	PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	PMINIDUMP_CALLBACK_INFORMATION CallbackParam );

static MiniDumpWriteDumpFunc GEHMiniDumpWriteDump = NULL;
static MINIDUMP_TYPE minidumpType = MiniDumpNormal;
static bool s_enabled = true;
static bool s_localOnly = false;

LONG WINAPI GEHExceptionFilter( _EXCEPTION_POINTERS* ExceptionInfo )
{
	MINIDUMP_EXCEPTION_INFORMATION exceptionInfo =
	{
		GetCurrentThreadId(), ExceptionInfo, TRUE
	};
	if( writeDebugFiles( &exceptionInfo, true ) &&
		isThreadImportant() )
	{
		TerminateProcess( GetCurrentProcess(), 3 );// non-zero indicates an error
	}
	Sleep( INFINITE );
	return EXCEPTION_CONTINUE_SEARCH;
}

void setupHandlers()
{
	HMODULE dbghelp = LoadLibrary( "dbghelp.dll" );
	if( dbghelp )
	{
		GEHMiniDumpWriteDump = (MiniDumpWriteDumpFunc)GetProcAddress( dbghelp,
			"MiniDumpWriteDump" );
	}

	s_localOnly = strstr( GetCommandLine(), "localdump" ) != NULL;

	if( !IsDebuggerPresent() )
	{
		DWORD oldProtect;
		VirtualProtect( UnhandledExceptionFilter, 5, PAGE_EXECUTE_READWRITE, &oldProtect );
		*(char*)UnhandledExceptionFilter = '\xe9';// far jmp
		*(unsigned int*)( (char*)UnhandledExceptionFilter + 1 ) = (unsigned int)GEHExceptionFilter -
			( (unsigned int)UnhandledExceptionFilter + 5 );
		VirtualProtect( UnhandledExceptionFilter, 5, oldProtect, &oldProtect );

		SetUnhandledExceptionFilter( GEHExceptionFilter );
	}

	initGEH();
}

static DWORD tlsImportant = 0;
static char dmpFileName[ 128 ];
static char sysInfoFileName[ 128 ];
static char logFileName[ 128 ];
static char remoteLogFileName[ 128 ];
static char computername[ 128 ];
static char sysInfo[ 65536 ];
static int dotOffset;

static char* findLastChar( char* str, char ch )
{
	char* ptr = NULL;
	while( *str != 0 )
	{
		if( *str == ch )
			ptr = str;
		++str;
	}
	return ptr;
}

void initGEH()
{
	tlsImportant = TlsAlloc();
	unsigned int tickCount = timeGetTime();
	DWORD size = sizeof( computername ) / sizeof( *computername );
	GetComputerName( computername, &size );
	StringCchPrintf( dmpFileName, sizeof( dmpFileName ), "BW%s%d.dmp", computername, tickCount );
	StringCchPrintf( sysInfoFileName, sizeof( sysInfoFileName ), "BW%s%d.txt", computername, tickCount );

	SYSTEM_INFO system_info;
	GetSystemInfo( &system_info );

	GetModuleFileName( NULL, logFileName, sizeof( logFileName ) / sizeof( *logFileName ) );
	*findLastChar( logFileName, '.' ) = 0;
	StringCbCat( logFileName, sizeof( logFileName ), ".log" );

	StringCchPrintf( remoteLogFileName, sizeof( remoteLogFileName ), "BW%s%d.log", computername, tickCount );
	dotOffset = findLastChar( remoteLogFileName, '.' ) - remoteLogFileName - 1;

	StringCchPrintf( sysInfo, sizeof( sysInfo ), "COMPUTERNAME = %s\nAPPLICATION = %s\n"
		"%d PROCESSOR(S) = %x - %x %x\n", computername, GetCommandLine(),
		system_info.dwNumberOfProcessors, system_info.wProcessorArchitecture,
		system_info.wProcessorLevel, system_info.wProcessorRevision );

	for( DWORD dev = 0; ; ++dev )
	{
		static DISPLAY_DEVICE device = { sizeof( device ) };
		if( EnumDisplayDevices( NULL, dev, &device, 0 ) )
		{
			StringCchPrintf( sysInfo + strlen( sysInfo ), sizeof( sysInfo ) - strlen( sysInfo ), "DISPLAYDEVICE %d = %s, %s, %s\n",
				dev, device.DeviceName, device.DeviceString, device.DeviceID );
		}
		else
			break;
	}
}

void setThreadImportance( bool important )
{
	TlsSetValue( tlsImportant, (LPVOID)( important ? 0 : 1 ) );
}

bool isThreadImportant()
{
	return TlsGetValue( tlsImportant ) == 0;
}

void setMinidumpType( MINIDUMP_TYPE type )
{
	minidumpType = type;
}

#define FTP_SERVER "crashdump.bigworldtech.com"
#define USER_NAME "bwcrashdump"
#define PASSWORD "jo6iFish"
#define FOLDER "/dumps-1.9.1"

bool writeDebugFiles( MINIDUMP_EXCEPTION_INFORMATION* exceptionInfo, bool writeMinidump )
{
	static HANDLE file;
	static DWORD size;
	static volatile bool entered;

	if( entered )// it is a critical function, we accept only one entrance per instance
		return false;
	entered = true;
	if( s_enabled )
	{
		if( !s_localOnly )
		{
			static char exeName[1024] = "\"";
			GetModuleFileName( NULL, exeName + 1, sizeof(exeName) - 1 );
			StringCbCat( exeName, sizeof( exeName ), "\" -crashdump" );
			WinExec( exeName, SW_NORMAL );
		}

		Ftp::InitFtp( FTP_SERVER, USER_NAME, PASSWORD );
		Ftp::createDirectory( FOLDER );
		Ftp::setCurrentDirectory( FOLDER );

		if( Ftp::openFile( sysInfoFileName ) )
		{
			size = 0;
			while( sysInfo[ size ] != 0 )
				++size;
			Ftp::writeFile( sysInfo, size );
			Ftp::closeFile();
		}

		static char buffer[ 65536 ];
		file = CreateFile( logFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
		if( file != INVALID_HANDLE_VALUE )
		{
			if( GetFileSize( file, NULL ) > 65536 )
				SetFilePointer( file, -65536, 0, SEEK_END );
			if( ReadFile( file, buffer, sizeof( buffer ), &size, NULL ) )
			{
				CloseHandle( file );
				if( Ftp::openFile( remoteLogFileName ) )
				{
					Ftp::writeFile( buffer, size );
					Ftp::closeFile();
				}
			}
			else
				CloseHandle( file );
		}

		if( GEHMiniDumpWriteDump && writeMinidump )
		{
			file = CreateFile( dmpFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
				CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL );
			if( file != INVALID_HANDLE_VALUE )
			{
				GEHMiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), file, minidumpType,
					exceptionInfo, NULL, NULL );
				CloseHandle( file );
				if( !s_localOnly && Ftp::putFile( dmpFileName, dmpFileName ) )
					DeleteFile( dmpFileName );
			}
		}

		Ftp::DeInitFtp();

		++dmpFileName[ dotOffset ];
		++sysInfoFileName[ dotOffset ];
		++remoteLogFileName[ dotOffset ];
	}
	else if( GEHMiniDumpWriteDump && writeMinidump )
	{
		file = CreateFile( dmpFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL );
		if( file != INVALID_HANDLE_VALUE )
		{
			GEHMiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), file, minidumpType,
				exceptionInfo, NULL, NULL );
			CloseHandle( file );
		}
	}
	return true;
}

void enableFeedBack( bool enable, bool localOnly )
{
	s_enabled = enable;
	s_localOnly = localOnly;
}
