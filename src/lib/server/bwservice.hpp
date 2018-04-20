/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BWSERVICE_HPP
#define BWSERVICE_HPP

#include "resmgr/bwresource.hpp"
#include "server/bwconfig.hpp"

void bwParseCommandLine( int argc, char **argv );

#ifdef _WIN32
#ifndef MF_CONFIG
#define MF_CONFIG Unknown
#endif
#endif

#define START_MSG( name )													\
	INFO_MSG( "---- %-10s "													\
			"Version: %s. "													\
			"Config: %s. "													\
			"Built: %s %s. "												\
			"UID: %d. "														\
			"PID: %d ----\n",												\
		name, BWVersion::versionString().c_str(),							\
		MF_CONFIG, __TIME__, __DATE__, 										\
		getUserId(), getpid() );											\
		{																	\
			int count = BWResource::getPathNum();							\
			for (int i = 0; i < count; ++i)									\
			{																\
				INFO_MSG( "Resource path (%d of %d): %s\n",					\
						i + 1, count,										\
						BWResource::getPath( i ).c_str() );					\
			}																\
		}

#ifndef _WIN32  // WIN32PORT

#define BIGWORLD_MAIN 										\
bwMain( int argc, char * argv[] );							\
int main( int argc, char * argv[] )							\
{															\
	BWResource bwresource;									\
	BWResource::init( argc, (const char **)argv );			\
	BWConfig::init( argc, argv );							\
	bwParseCommandLine( argc, argv );						\
	return bwMain( argc, argv );							\
}															\
int bwMain

#define BIGWORLD_MAIN_NO_RESMGR								\
bwMain( int argc, char * argv[] );							\
int main( int argc, char * argv[] )							\
{															\
	bwParseCommandLine( argc, argv );						\
	return bwMain( argc, argv );							\
}															\
int bwMain

#define BW_SERVICE_CHECK_POINT( MSECS )
#define BW_SERVICE_UPDATE_STATUS( STATE, WAIT_HINT, ERROR_CODE )

#else

#define BIGWORLD_MAIN										\
realBWMain( int argc, char * argv[] );						\
int bwMain( int argc, char * argv[] )						\
{															\
	BWResource bwresource;									\
	BWResource::init( argc, argv );							\
	BWConfig::init( argc, argv );							\
	bwParseCommandLine( argc, argv );						\
	return realBWMain( argc, argv );						\
}															\
int realBWMain

#define BIGWORLD_MAIN_NO_RESMGR								\
realBWMain( int argc, char * argv[] );						\
int bwMain( int argc, char * argv[] )						\
{															\
	bwParseCommandLine( argc, argv );						\
	return realBWMain( argc, argv );						\
}															\
int realBWMain

#define BW_SERVICE_CHECK_POINT( MSECS )	ServiceCheckpoint( MSECS );
#define BW_SERVICE_UPDATE_STATUS( STATE, WAIT_HINT, ERROR_CODE )	\
	ServiceUpdateStatus( STATE, WAIT_HINT, ERROR_CODE );

#include "service.hpp"

int bwMain( int argc, char * argv[] );
void bwStop();
extern char szServiceDependencies[];

class BigWorldService : public CService
{
	protected:

		HANDLE m_hStopEvent;
		HANDLE m_hThread;

		static DWORD WINAPI StopThreadProc( LPVOID pThis )
			{
				DWORD ret = ( (BigWorldService*)pThis )->StopThread();
				ExitThread(ret);
				return ret;
			}

		DWORD StopThread()
			{
				WaitForSingleObject( m_hStopEvent, INFINITE );
				onStop();
				return 0;
			}

	public:
		BigWorldService( LPTSTR pServiceName, LPTSTR pDisplayName ) :
					CService( pServiceName, pDisplayName ),
					m_hStopEvent( NULL ),
					m_hThread( NULL )
			{
				SetControlsAccepted(
					GetControlsAccepted() | SERVICE_ACCEPT_SHUTDOWN );
			}

	   ~BigWorldService()
			{
				SetEvent( m_hStopEvent );
				WaitForSingleObject( m_hThread, 1000 );
				CloseHandle( m_hThread );
				CloseHandle( m_hStopEvent );
			}


		virtual void Main()
			{
				Checkpoint(3000);

				TCHAR szEvent[32];
				wsprintf( szEvent, TEXT("machined-proc%d"),
						(unsigned short ) GetCurrentProcessId() );

				m_hStopEvent = CreateEvent( NULL, TRUE, FALSE, szEvent );
				DWORD tid;
				m_hThread = CreateThread( NULL, 4096,
					(LPTHREAD_START_ROUTINE) StopThreadProc,
					this, 0, &tid );

				bwMain( (int) dwArgc, lpArgv );
				UpdateStatus( SERVICE_STOP_PENDING, 3000 );
			}

		virtual void onStop(void)
			{
				UpdateStatus( SERVICE_STOP_PENDING, 3000 );
				bwStop();
			}
		virtual void onShutdown(void) { onStop(); }

};

void usage()
{
	 printf( "\narguments:\n" );
	 printf( "  [serviceName]						to run the service\n");
	 printf( "  -install [serviceName] [DisplayName] to install the service\n");
	 printf( "  -remove  [serviceName]			   to remove the service\n" );
	 printf( "\n" );
}

int __cdecl main( int argc, char * argv[] )
{
	// parse basic command line here.
	char* pServiceName = NULL;
	char* pDisplayName = NULL;

	bool bRun = false;
	bool fromMachineD = false;

	if ( argc > 1 )
	{
		if ( _stricmp( "-machined", argv[1] ) == 0 )
		{
			fromMachineD = true;
			bRun = true;
		}
		else if ( _stricmp( "-install", argv[1] ) == 0 )
		{
			if ( argc > 2 )
			{
				pServiceName = argv[2];
			}
			else
			{
				// pServiceName = argv[0];
				pServiceName = "BigWorld";
			}

			if ( argc > 3 )
			{
				pDisplayName = argv[3];
			}
			else
			{
				pDisplayName = pServiceName;
			}

			// create the servce with the appropriate names
			BigWorldService svc( pServiceName, pDisplayName );
			svc.Install(  szServiceDependencies,
						  SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS
						);


			DWORD disp;
			HKEY hkey = NULL;
			char szBuf[MAX_PATH];
			wsprintf( szBuf, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s", pServiceName );
			if ( ERROR_SUCCESS == RegCreateKeyEx( HKEY_LOCAL_MACHINE,
												szBuf,
												0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL,
												&hkey, &disp ) &&
					hkey )
			{

				char szPath[512];
				DWORD length = GetModuleFileName( NULL, szPath, 512 );
				if ( length > 0 )
				{
					DWORD dwType = 7;
					if ( RegSetValueEx( hkey, "EventMessageFile", 0, REG_EXPAND_SZ, (const BYTE*)szPath, length ) ||
							RegSetValueEx( hkey, "TypesSupported", 0, REG_DWORD, (LPBYTE)&dwType, sizeof(dwType) ) )
					{
						printf( "Service was not able to add eventviewer registry entries\n" );
					}
				}

				RegCloseKey( hkey );
			}

		}
		else if ( _stricmp( "-remove", argv[1] ) == 0 )
		{
			if ( argc > 2 )
			{
				pServiceName = argv[2];
			}
			else
			{
				// pServiceName = argv[0];
				pServiceName = "BigWorld";
			}

			// don't worry about a bogus display name
			BigWorldService svc( pServiceName, pServiceName );
			svc.Remove();

			char szBuf[MAX_PATH];
			wsprintfA( szBuf, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s", pServiceName );
			RegDeleteKey( HKEY_LOCAL_MACHINE, szBuf );

		}
		else if ( ( _stricmp( "-?", argv[1] ) == 0 ) ||  ( _stricmp( "-help", argv[1] ) == 0 )  ||  ( _stricmp( "--help", argv[1] ) == 0 ) )
		{
			usage();
		}
		else
		{
			bRun = true;
		}
	}
	else
	{
		bRun = true;
	}

	if ( bRun )
	{
		pServiceName = argv[0];

		char szBuf[MAX_PATH];
		wsprintf( szBuf, "SYSTEM\\CurrentControlSet\\Services\\%s", pServiceName );
		HKEY hkey = NULL;
		if ( ( ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, szBuf,
												0, KEY_READ, &hkey ) ) && hkey )
		{
			DWORD type;
			DWORD size = sizeof(szBuf);
			if ( ERROR_SUCCESS == RegQueryValueEx(hkey, "DisplayName", NULL, &type, (LPBYTE)szBuf, &size) && (type == REG_SZ) )
			{
				pDisplayName = szBuf;
			}
			else
			{
				pDisplayName = pServiceName;
			}

			RegCloseKey( hkey );
		}

		{
			char szPath[MAX_PATH];
			DWORD len = GetModuleFileNameA( NULL, szPath, sizeof( szPath ) );
			if ( len )
			{
				while( len )
				{
					len--;
					if ( szPath[len] == '\\' )
					{
						szPath[len] = '\0';
						SetCurrentDirectory(szPath);
						break;
					}
				}
			}
		}


		BigWorldService svc( pServiceName, pDisplayName );
		if ( fromMachineD )
			svc.NeverService();
		svc.Start(argc, argv);

	}

	ExitProcess(0);
}

#endif // _WIN32

#endif // BWSERVICE_HPP
