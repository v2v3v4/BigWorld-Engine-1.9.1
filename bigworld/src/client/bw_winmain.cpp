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
#include "bw_winmain.hpp"


//#include <crtdbg.h>
#include <mmsystem.h>

#include "moo/init.hpp"
#include "pyscript/script.hpp"

#include "app.hpp"


static const int MIN_WINDOW_WIDTH = 100;
static const int MIN_WINDOW_HEIGHT = 100;

int parseCommandLine( LPSTR str );

bool g_bActive = false;
bool g_bAppQuit = false;
static std::string configFilename = "";


/**
 *	This function is the BigWorld implementation of WinMain and contains the
 *	message pump. Before this function is called the application should already
 *	have registered a WNDCLASS with class name lpClassName. The registered
 *	WndProc function should also call BWWndProc.
 *
 *	@param hInstance	The instance handle passed to WinMain.
 *	@param lpCmdLine	The command line string passed to WinMain.
 *	@param nCmdShow		The initial window state passed to WinMain.
 *	@param lpClassName	The name of the WNDCLASS previously registered.
 *	@param lpWindowName	The window title for the program.
 *
 *	@return	The wParam value of the WM_QUIT that should be returned by WinMain.	
 */
int BWWinMain(	HINSTANCE hInstance,
				LPSTR lpCmdLine,
				int nCmdShow,
				LPCTSTR lpClassName,
				LPCTSTR lpWindowName )
{
	BW_GUARD;

	// Initiating here means that it is no longer destroyed at static
	// destruction time.
	BWResource bwresource;

	if (!parseCommandLine( lpCmdLine ))
	{
		return FALSE;
	}

	// Initialise Moo
	if ( !Moo::init() )
	{
		return FALSE;
	}

	//int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	//flags |= _CRTDBG_LEAK_CHECK_DF | _CRTDBG_DELAY_FREE_MEM_DF;
	//_CrtSetDbgFlag(flags);

	const int borderWidth = GetSystemMetrics(SM_CXFRAME);
	const int borderHeight = GetSystemMetrics(SM_CYFRAME);
	const int titleHeight = GetSystemMetrics(SM_CYCAPTION);

    // Create the main window
    HWND hWnd = CreateWindow(	lpClassName,
								lpWindowName,
								WS_OVERLAPPEDWINDOW,
								CW_USEDEFAULT, CW_USEDEFAULT,
								640 + borderWidth*2,
								480 + titleHeight,
								NULL,
								NULL,
								hInstance,
								0 );

    // Initialize and start a new game
    ShowWindow( hWnd, nCmdShow );
	UpdateWindow( hWnd );

	MSG msg;

	while( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
    {
		TranslateMessage( &msg );
		DispatchMessage( &msg );
    }

	timeBeginPeriod(1);

	// App scope
	try
	{
		// Initialize the application.
		App app( configFilename, compileTimeString );

		if( !app.init( hInstance, hWnd ) )
		{
			DestroyWindow( hWnd );
			return FALSE;
		}

		// Standard game loop.
		while( !g_bAppQuit )
		{
			MSG msg;

			if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
			{
				// Check for a quit message
				if( msg.message == WM_QUIT )
					break;

				TranslateMessage( &msg );
				DispatchMessage( &msg );
			}
			else
			{
				// Play the game (check user input 
				// and update the window)
				if (!app.updateFrame(g_bActive))
				{
					::PostQuitMessage(0);
				}
			}
		}
	}
	catch (const App::InitError &)
	{
		DestroyWindow( hWnd );
		return FALSE;
	}

	timeEndPeriod(1);

	Moo::fini();

#if defined(_DEBUG) && ENABLE_STACK_TRACKER
	DEBUG_MSG( "StackTracker: maximum stack depth achieved: %d.\n", StackTracker::getMaxStackPos() );
#endif
    return msg.wParam;
}



/**
 *	This function is the BigWorld implementation of WndProc which should be
 *	called by the WndProc registered when the application was started.
 *
 *	@param hWnd	The window handle assoaciated with the current windows message.
 *	@param msg		The message passed to WndProc
 *	@param wParam	Additional message parameter passed to WndProc
 *	@param lParam	Additional message parameter passed to WndProc
 *
 *	@return	DefWindowProc will be called if the message is not otherwise
 *			handled.
 */
LRESULT BWWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	BW_GUARD;
	static bool shuttingDown = false;
	switch (msg)
	{
	case WM_SETCURSOR:
		// From DirectX "ShowCursor" documentation, one should prevent Windows
		// from setting a Windows cursor for this window by doing this:
		if (Moo::rc().device())
		{
			SetCursor( NULL );		    
			Moo::rc().device()->ShowCursor( TRUE );
		}
		return TRUE;
		break;

	case WM_ACTIVATE:
		if (!shuttingDown)
		{
			if (wParam == WA_INACTIVE)
				App::handleSetFocus( false );
			else
				App::handleSetFocus( true );
		}

		break;

	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = MIN_WINDOW_WIDTH;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = MIN_WINDOW_HEIGHT;
		break;

	case WM_PAINT:
		if (!g_bAppStarted)
		{
			RECT rect;
			GetClientRect( hWnd, &rect );
			HDC hDeviceContext = GetDC( hWnd );
			FillRect( hDeviceContext, &rect, (HBRUSH)( COLOR_WINDOW+1) );
			ReleaseDC( hWnd, hDeviceContext );
		}
		break;

	case WM_MOVE:
		if (g_bActive && g_bAppStarted)
		{
			App::instance().moveWindow((int16)LOWORD(lParam), (int16)HIWORD(lParam));
		}
		break;

	case WM_SIZE:
		g_bActive = !( (SIZE_MAXHIDE == wParam) || (SIZE_MINIMIZED == wParam) || ( 0 == lParam ));

		if (g_bActive && g_bAppStarted)
		{
			App::instance().resizeWindow();
		}
		break;

	case WM_SYSCOMMAND:
		if ((wParam & 0xFFF0) == SC_CLOSE)
		{
			PostQuitMessage(0);
		}
		break;

	case WM_CLOSE:
		shuttingDown = true;
		break;
				
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc( hWnd, msg, wParam, lParam );
}


/**
 *	The function processes all outstanding windows messages and returns when
 *	there are non-remaining or a WM_QUIT is received.
 *
 *	@return Returns false if a WM_QUIT was encountered, otherwise true.
 */
bool BWProcessOutstandingMessages()
{
	BW_GUARD;
	MSG msg;

	while( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
	{
		if( msg.message == WM_QUIT )
		{
			g_bAppQuit = true;
			return false;
		}

		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}
	return true;
}


/**
 * Splits the given string in the style of Windows command line parsing (i.e. the
 * same style which is passed into a C main function under win32).
 *
 * - The string is split by delimeters (default: whitespace/newlines), which are 
 *   ignored when between matching binding characters (default: ' or "). 
 * - \" escapes the quote.
 * - \\" escapes the slash only when inside a quoted block.
 *
 *	@param str Raw string to be parsed
 *  @param out Output array of strings, where the result will be placed.
 *             Note that the array will be appended to, and not cleared.
 *  @param delim Possible argument delimeters. Defaults to whitespace characters.
 *  @param bind Binding characters used to quote arguments. Defaults to ".
 *	@param escape Character used to escape binding characters. Defaults to \.
 *
 *	@return Returns the number of arguments appended to the output list
 */
size_t SplitCommandArgs (const std::string& str, std::vector< std::string >& out, 
                         const std::string& delim = " \t\r\n", 
						 const std::string& bind = "\"",
						 char escape = '\\')
{
    const size_t prevSize = out.size();

    std::string buf;
    char binding = 0;

    for(size_t i = 0; i < str.size(); ++i)
    {
		// Process \" (escaped binding char)
		if(str[i] == escape && i < str.size()-1)
		{
			size_t pos = bind.find_first_of( str[i+1] );
			if (pos != std::string::npos)
			{
				buf.push_back( bind[pos] );
				++i;
				continue;
			}
		}

		// Two modes: bound, or unbound.
		// bound = we are currently inside a quote, so process until we hit a non-quote
		if(binding)
        {
			// Process \\" (escaped slash-with-a-quote)
			if(str[i] == escape && i < str.size()-2 && str[i+1] == escape)
			{
				size_t pos = bind.find_first_of( str[i+2] );
				if (pos != std::string::npos)
				{
					buf.push_back( escape );
					++i;
					continue;
				}
			}

            if(str[i] == binding)
            {
                // Hit matching binding char, add buffer to output (always - empty strings included)
                out.push_back(buf);
                buf.clear();

                // Set unbound
                binding = 0;
            }
            else
			{
                // Not special, add char to buffer
                buf.push_back(str[i]);
			}
        }
        else
        {
            if(delim.find_first_of(str[i]) != std::string::npos)
            {
                // Hit a delimeter
                if(!buf.empty())
                {
                    // Add buffer to output (if nonempty - ignore empty strings when unbound)
                    out.push_back(buf);
                    buf.clear();
                }
            }
            else if(bind.find_first_of(str[i]) != std::string::npos)
            {
                // Hit binding character
                if(!buf.empty())
                {
                    // Add buffer to output (if nonempty - ignore empty strings when unbound)
                    out.push_back(buf);
                    buf.clear();
                }

                // Set bounding character
                binding = str[i];
            }
            else
			{
                // Not special, add char to buffer
                buf.push_back(str[i]);
			}
        }
    }

	if(!buf.empty())
	{
        out.push_back(buf);
	}

    return out.size() - prevSize;
}

static std::vector< std::string > g_commandLine;

/**
 *	This function processes the command line.
 *
 *	@return True if successful, otherwise false.
 */
int parseCommandLine( LPSTR str )
{
	BW_GUARD;
	MF_ASSERT( g_commandLine.empty() && "parseCommandLine called twice!" );

#if !BWCLIENT_AS_PYTHON_MODULE
	// __argv is a macro exposed by the MS compiler (together with __argc).
	g_commandLine.push_back(__argv[0]);
#endif

	SplitCommandArgs(str, g_commandLine);

	if (g_commandLine.empty())
	{
		ERROR_MSG( "winmain::parseCommandLine: No path given\n" );
		return FALSE;
	}

	// Always copy argv[0] into python args
	Script::g_scriptArgv[ Script::g_scriptArgc++ ] = const_cast<char*>(g_commandLine[0].c_str());

	// Build a C style array of characters for functions that use this signature
	const size_t MAX_ARGS = 20;
	const char * argv[ MAX_ARGS ];
	size_t argc = 0;

	if (g_commandLine.size() >= MAX_ARGS)
	{
		ERROR_MSG( "winmain::parseCommandLine: Too many arguments!!\n" );
		return FALSE;
	}

	while(argc < g_commandLine.size())
	{
		std::string& arg = g_commandLine[argc];
		
		argv[argc] = arg.c_str();

		if( arg == "--res" || arg == "-r" ||
			arg == "--options" || 
			arg == "--config" || arg == "-c" ||
			arg == "--script-arg" || arg == "-sa" &&
			argc < g_commandLine.size())
		{
			std::string& arg2 = g_commandLine[++argc];
			argv[argc] = arg2.c_str();
			
			if(arg == "--config" || arg == "-c")
			{
				configFilename = arg2;
			}
			else if(arg == "--script-arg" || arg == "-sa")
			{
				Script::g_scriptArgv[ Script::g_scriptArgc++ ] = const_cast<char*>(arg2.c_str());
			}
		}

		++argc;
	}

#if BWCLIENT_AS_PYTHON_MODULE
	BWResource::overrideAppDirectory(Script::getMainScriptPath());
#endif // BWCLIENT_AS_PYTHON_MODULE

	return BWResource::init( (int)argc, (const char **)argv )	? TRUE : FALSE;
}



// bw_winmain.cpp
