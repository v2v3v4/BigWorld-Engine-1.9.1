/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _GEH_HPP__
#define _GEH_HPP__

#include <windows.h>
#include <dbgHelp.h>

/*****************************************************************************************
General Exception Handler
--------------------------------------------
What is it?
--------------------------------------------
It handles any unhandled exception ( either SEH or C++ exception ) and then writes debug dumps
and sends them back to Bigworld

How to use it?
--------------------------------------------
In Project Property->Linker->Advanced, set Entry Point to GEHWinMainCRTStartup if your application
is a Win32 non-Unicode one, or GEHwWinMainCRTStartup for Win32 Unicode, GEHmainCRTStartup for console
non-Unicode, GENwmainCRTStartup for consolde Unicode.
Then link your application to this library.
*****************************************************************************************/
void setThreadImportance( bool important );
void setMinidumpType( MINIDUMP_TYPE type );
bool writeDebugFiles( MINIDUMP_EXCEPTION_INFORMATION* exceptionInfo, bool writeMinidump );
void enableFeedBack( bool enable, bool localOnly );

#endif//_GEH_HPP__
