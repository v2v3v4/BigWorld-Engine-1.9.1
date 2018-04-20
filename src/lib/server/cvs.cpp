/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "cvs.hpp"
#include "cstdmf/debug.hpp"
#include <vector>

#ifndef unix
#include "shlwapi.h"
#include <io.h>
#else
#include <sys/wait.h>
#include <sys/errno.h>
#endif

DECLARE_DEBUG_COMPONENT( 0 )

/**
 *	Helper method to execute a command and pipe the output to a file.
 */
static bool executeCommandAndPipeToFile( const std::string & cmd,
	const std::vector<std::string> & cmdline, FILE * pFile )
{
#ifdef unix
	std::vector<char *> fullline;
	fullline.push_back( const_cast<char *>( cmd.c_str() ) );
	for (uint i = 0; i < cmdline.size(); i++)
		fullline.push_back( const_cast<char *>( cmdline[i].c_str() ) );
	fullline.push_back( NULL );

	pid_t fres = fork();
	// not sure about wisdom of using vfork over fork ...
	// vfork is faster, but it blocks our other threads...
	// actually can't use vfork since we need to dup2 in the child
	if (fres == 0)
	{
		dup2( fileno( pFile ), STDOUT_FILENO );
		execvp( cmd.c_str(), &fullline[0] );
		exit( 1 );
	}
	else if (fres != -1)
	{
		int status = -1;
		while (waitpid( fres, &status, 0 ) == EINTR); // loop
		return WIFEXITED( status ) && WEXITSTATUS( status ) == 0;
	}
	else	// error
	{
		return false;
	}
#else
	// build the dodgy single-string command line
	std::string fullline;
	fullline += cmd;
	std::string space(" ");
	std::string quote("\"");
	for (uint i = 0; i < cmdline.size(); i++)
	{
		fullline += space;
		bool hasSpaces = cmdline[i].find_first_of(' ') < cmdline[i].length();
		if (hasSpaces)	fullline += quote;
		fullline += cmdline[i];
		if (hasSpaces)	fullline += quote;
	}

	// find the application
	char appName[MAX_PATH];
	strcpy( appName, (cmd+".exe").c_str() );
	if (!PathFindOnPath( appName, NULL )) return false;

	HANDLE sh = HANDLE(_get_osfhandle( _fileno( pFile ) ));
	HANDLE th = INVALID_HANDLE_VALUE;
	DuplicateHandle( GetCurrentProcess(), sh,
		GetCurrentProcess(), &th,
		0, /*bInheritHandle*/TRUE, DUPLICATE_SAME_ACCESS );

	STARTUPINFO si;
	memset( &si, 0, sizeof(si) );
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = GetStdHandle( STD_INPUT_HANDLE );
	si.hStdOutput = th;
	si.hStdError = GetStdHandle( STD_ERROR_HANDLE );

	PROCESS_INFORMATION pi;
	BOOL ok = CreateProcess( appName, (char*)fullline.c_str(), NULL, NULL,
		/*bInheritHandles*/TRUE, CREATE_NO_WINDOW,
		NULL, NULL, &si, &pi );

	CloseHandle( th );

	if (!ok) return false;

	WaitForSingleObject( pi.hProcess, INFINITE );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	// return result check ... ?

	return true;
#endif
}

/**
 *	Helper method to extract a file from CVS.
 *	The repository and relative path is determined from g_pRes/CVS.
 */
bool extractFileFromCVS(
	IFileSystem * pResFS, const std::string & resName, const std::string & cvsInfo,
	IFileSystem * pDstFS, const std::string & dstName )
{
	// find where res lives in cvs
	static std::string cvsRoot;
	static std::string cvsRepos;
	static bool cvsInitted = false;
	if (!cvsInitted)
	{
		cvsInitted = true;
		BinaryPtr pBRoot = pResFS->readFile( "CVS/Root" );
		BinaryPtr pBRepos = pResFS->readFile( "CVS/Repository" );
		if (!pBRoot || !pBRepos) return false;
		uint l;
		const char * d;
		d = (const char*)pBRoot->data();
		for (l = pBRoot->len(); l > 0; l--)
			if (d[l-1] != '\r' && d[l-1] != '\n') break;
		cvsRoot.assign( d, l );
		d = (const char*)pBRepos->data();
		for (l = pBRepos->len(); l > 0; l--)
			if (d[l-1] != '\r' && d[l-1] != '\n') break;
		cvsRepos.assign( d, l );
		INFO_MSG( "extractFileFromCVS: res lives at '%s' in %s\n",
			cvsRepos.c_str(), cvsRoot.c_str() );
		if (!cvsRepos.empty()) cvsRepos += "/";
	}
	if (cvsRoot.empty()) return false;

	// TODO: make this work with multiple res paths!
	// Cannot simply check them all since that version of the file
	// may exist in multiple res paths! Especially breaks if file removed
	// from specific path and added to the more general one.
	// Prolly append path index to cvs version info 'A'-'Z'...

	// find the version and/or date that we should extract
	char cvsInfoIndicator = *cvsInfo.begin();
	std::string cvsVersion = (!isalpha(cvsInfoIndicator)) ? cvsInfo :
		(cvsInfoIndicator == 'T') ? cvsInfo.substr( 1 ) :
		std::string();
	std::string cvsDate = (cvsInfoIndicator != 'D') ? std::string() :
		cvsInfo.substr( 1 );

	std::vector<std::string> cmdline;
	cmdline.push_back( "-d" );
	cmdline.push_back( cvsRoot );
	cmdline.push_back( "-Q" );
	cmdline.push_back( "co" );
	cmdline.push_back( "-p" );
	if (!cvsVersion.empty())
	{
		cmdline.push_back( "-r" );
		cmdline.push_back( cvsVersion );
	}
	if (!cvsDate.empty())
	{
		cmdline.push_back( "-D" );
		cmdline.push_back( cvsDate );
	}
	cmdline.push_back( cvsRepos + resName );

	std::string tempName = ".cvstemp";
	// TODO: make safe for multiple processes on multiple machines!
	// (but not multiple threads, that should be OK)

	FILE * pDst = pDstFS->posixFileOpen( tempName, "wb" );
	bool ok = executeCommandAndPipeToFile( "cvs", cmdline, pDst );
	fclose( pDst );

	IFileSystem::FileInfo fi;
	if (!ok ||
		pDstFS->getFileType( tempName, &fi ) != IFileSystem::FT_FILE ||
		fi.size == 0)
	{
		pDstFS->eraseFileOrDirectory( tempName );
		return false;
	}

	pDstFS->moveFileOrDirectory( tempName, dstName );
	// above might not succeed if another process already renamed it...
	// in which case we just leave the temp file around, which is not too bad

	return true;
}
