/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _HANDLE_HPP__
#define _HANDLE_HPP__

#include <windows.h>

void initGEH();
LONG WINAPI GEHExceptionFilter( _EXCEPTION_POINTERS* ExceptionInfo );
void setupHandlers();
bool isThreadImportant();

#endif//_HANDLE_HPP__
