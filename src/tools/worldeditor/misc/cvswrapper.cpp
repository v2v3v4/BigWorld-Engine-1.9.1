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
#include "worldeditor/misc/cvswrapper.hpp"
#include "appmgr/options.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/multi_file_system.hpp"
#include "common/string_utils.hpp"
#include "controls/message_box.hpp"
#include "resmgr/string_provider.hpp"
#include <fstream>
#include <sstream>


class FileNameListFile
{
	std::string fileName_;
public:
	template<typename T>
	FileNameListFile( const std::string& workingFolder, const T& filenames )
	{
		for( int i = 0;; ++i )
		{
			std::stringstream ss;
			ss << workingFolder << i << ".txt";
			fileName_ = ss.str();
			std::ofstream ofs( fileName_.c_str() );
			if( ofs )
			{
				for( T::const_iterator iter = filenames.begin(); iter != filenames.end(); ++iter )
				{
					ofs << *iter << std::endl;
				}
				break;
			}
		}
	}
	~FileNameListFile()
	{
		DeleteFile( fileName_.c_str() );
	}
	const std::string& filename() const
	{
		return fileName_;
	}
};

DECLARE_DEBUG_COMPONENT2( "CVSWrapper", 2 );

bool CVSWrapper::isFile( const std::string& pathName )
{
	IFileSystem::FileType fileType = BWResource::instance().fileSystem()->getFileType( pathName );

	return fileType == IFileSystem::FT_FILE;
}

bool CVSWrapper::isDirectory( const std::string& pathName )
{
	IFileSystem::FileType fileType = BWResource::instance().fileSystem()->getFileType( pathName );

	return fileType == IFileSystem::FT_DIRECTORY;
}

bool CVSWrapper::exists( const std::string& pathName )
{
	IFileSystem::FileType fileType = BWResource::instance().fileSystem()->getFileType( pathName );

	return fileType != IFileSystem::FT_NOT_FOUND;
}

static std::string getHKCRValue( const std::string& name )
{
	LONG size;
	if( RegQueryValue( HKEY_CLASSES_ROOT, name.c_str(), NULL, &size ) == ERROR_SUCCESS )
	{
		std::string result( (std::string::size_type)size - 1, ' ' );
		if( RegQueryValue( HKEY_CLASSES_ROOT, name.c_str(), &result[ 0 ], &size ) == ERROR_SUCCESS )
			return result;
	}
	return "";
}

std::string CVSWrapper::cvsPath_;
unsigned int CVSWrapper::batchLimit_;
bool CVSWrapper::directoryCommit_;
bool CVSWrapper::enabled_ = false;
std::string CVSWrapper::dirToIgnore_;

CVSWrapper::InitResult CVSWrapper::init()
{
	enabled_ = Options::getOptionBool( "bwlockd/use", true ) &&
		Options::getOptionBool( "CVS/enable", true );

	if( enabled_ )
	{
		std::string scriptPath =
			BWResource::resolveFilename(
				Options::getOptionString(
					"CVS/path", "resources/scripts/cvs_stub.py" ) );
					
		cvsPath_ = BWResource::removeExtension( scriptPath ) + ".exe";
		
		batchLimit_ = Options::getOptionInt( "CVS/batchLimit", 128 );
		if( !BWResource::fileExists( cvsPath_ ) )
		{
			MsgBox mb( L("WORLDEDITOR/WORLDEDITOR/BIGBANG/CVS_WRAPPER/CVS_WRAPPER_TITLE"),
					L("WORLDEDITOR/WORLDEDITOR/BIGBANG/CVS_WRAPPER/CVS_WRAPPER_CANNOT_FIND_STUB",cvsPath_),
					L("WORLDEDITOR/WORLDEDITOR/BIGBANG/CVS_WRAPPER/CVS_WRAPPER_EXIT"),
					L("WORLDEDITOR/WORLDEDITOR/BIGBANG/CVS_WRAPPER/CVS_WRAPPER_CONTINUE_WITHOUT") );
			if( mb.doModal() == 0 )
			{
				return FAILURE;
			}
			enabled_ = false;
		}
		else
		{
			if( cvsPath_.rfind( '.' ) != cvsPath_.npos )
			{
				std::string ext = cvsPath_.substr( cvsPath_.rfind( '.' ) );
				std::string type = getHKCRValue( ext );
				if( !type.empty() )
				{
					std::string openCommand = getHKCRValue( type + "\\shell\\open\\command" );
					if( !openCommand.empty() )
					{
						StringUtils::replace( openCommand, "%1", cvsPath_ );
						StringUtils::replace( openCommand, "%*", "" );
						cvsPath_ = openCommand;
					}
				}
			}
			else
				cvsPath_ = '\"' + cvsPath_ + '\"';

			int exitCode;
			std::string output;

			if( !exec( cvsPath_ + " check", ".", exitCode, output, NULL ) )
			{
				MsgBox mb( L("WORLDEDITOR/WORLDEDITOR/BIGBANG/CVS_WRAPPER/CVS_WRAPPER_TITLE"),
					L("WORLDEDITOR/WORLDEDITOR/BIGBANG/CVS_WRAPPER/CVS_WRAPPER_CANNOT_EXECUTE_STUB",cvsPath_),
					L("WORLDEDITOR/WORLDEDITOR/BIGBANG/CVS_WRAPPER/CVS_WRAPPER_EXIT"),
					L("WORLDEDITOR/WORLDEDITOR/BIGBANG/CVS_WRAPPER/CVS_WRAPPER_CONTINUE_WITHOUT") );
				if( mb.doModal() == 0 )
				{
					return FAILURE;
				}
				enabled_ = false;
			}
			else if( exitCode )//failure
			{
				MsgBox mb( L("WORLDEDITOR/WORLDEDITOR/BIGBANG/CVS_WRAPPER/CVS_WRAPPER_TITLE"),
					L("WORLDEDITOR/WORLDEDITOR/BIGBANG/CVS_WRAPPER/CVS_WRAPPER_CHECK_FAILED",output),
					L("WORLDEDITOR/WORLDEDITOR/BIGBANG/CVS_WRAPPER/CVS_WRAPPER_EXIT"),
					L("WORLDEDITOR/WORLDEDITOR/BIGBANG/CVS_WRAPPER/CVS_WRAPPER_CONTINUE_WITHOUT") );
				if( mb.doModal() == 0 )
				{
					return FAILURE;
				}
				enabled_ = false;
			}
			else
			{
				std::stringstream ss( output );

				std::getline( ss, dirToIgnore_ );
				std::getline( ss, output );

				directoryCommit_ = !output.empty() && output[0] != '0';

				while( !dirToIgnore_.empty() && isspace( *dirToIgnore_.rbegin() ) )
					dirToIgnore_.resize( dirToIgnore_.size() - 1 );
			}
		}
	}
	return enabled_ ? SUCCESS : DISABLED;
}

CVSWrapper::CVSWrapper( const std::string& workingPath, CVSLog* log /*= NULL*/ )
	: log_( log )
{
	workingPath_ = BWResource::resolveFilename( workingPath );
	if( *workingPath_.rbegin() != '/' )
		workingPath_ += '/';
}

void CVSWrapper::refreshFolder( const std::string& relativePathName )
{
	if( !enabled_ )
		return;

	std::string cmd = cvsPath_ + " refreshfolder ";
	cmd += '\"' + relativePathName + '\"';

	int exitCode;

	if( !exec( cmd, workingPath_, exitCode, output_, log_ ) )
		ERROR_MSG( "Couldn't exec %s\n", cmd.c_str() );
	else
		INFO_MSG( "refresh Done, cvs output:\n%s\n", output_.c_str() );
}

bool CVSWrapper::editFiles( std::vector<std::string> filesToEdit )
{
	if( !enabled_ )
		return true;
	bool result = true;
	while( !filesToEdit.empty() )
	{
		unsigned int limit = batchLimit_;
		std::string cmd = cvsPath_ + " editfile";
		while( !filesToEdit.empty() && limit != 0 )
		{
			--limit;
			cmd += " \"" + filesToEdit.front() + '\"';
			filesToEdit.erase( filesToEdit.begin() );
		}

		int exitCode;

		if( !exec( cmd, workingPath_, exitCode, output_, log_ ) )
		{
			ERROR_MSG( "Couldn't exec %s\n", cmd.c_str() );
			result = false;
		}
		else
			INFO_MSG( "Update Done, cvs output:\n%s\n", output_.c_str() );
		if( exitCode != 0 )
			result = false;
	}

	return result;
}

bool CVSWrapper::revertFiles( std::vector<std::string> filesToRevert )
{
	if( !enabled_ )
		return true;
	bool result = true;
	while( !filesToRevert.empty() )
	{
		unsigned int limit = batchLimit_;
		std::string cmd = cvsPath_ + " revertfile";
		while( !filesToRevert.empty() && limit != 0 )
		{
			--limit;
			cmd += " \"" + filesToRevert.front() + '\"';
			filesToRevert.erase( filesToRevert.begin() );
		}

		int exitCode;

		if( !exec( cmd, workingPath_, exitCode, output_, log_ ) )
		{
			ERROR_MSG( "Couldn't exec %s\n", cmd.c_str() );
			result = false;
		}
		else
			INFO_MSG( "Update Done, cvs output:\n%s\n", output_.c_str() );
		if( exitCode != 0 )
			result = false;
	}

	return result;
}

bool CVSWrapper::updateFolder( const std::string& relativePathName )
{
	if( !enabled_ )
		return true;

	std::string cmd = cvsPath_ + " updatefolder \"" + relativePathName + '\"';

	int exitCode;

	if( !exec( cmd, workingPath_, exitCode, output_, log_ ) )
	{
		ERROR_MSG( "Couldn't exec %s\n", cmd.c_str() );
		return false;
	}
	else
		INFO_MSG( "Update Done, cvs output:\n%s\n", output_.c_str() );

	return exitCode == 0;
}

bool CVSWrapper::commitFiles( const std::set<std::string>& filesToCommit,
		const std::set<std::string>& foldersToCommit, const std::string& commitMsg )
{
	if( !enabled_ )
		return true;

	std::set<std::string> toCommit;
	if( directoryCommit_ )
	{
		toCommit = filesToCommit;
		toCommit.insert( foldersToCommit.begin(), foldersToCommit.end() );
	}
	FileNameListFile fnlf( workingPath_, directoryCommit_ ? toCommit : filesToCommit );

	bool result = true;

	int exitCode;
	std::string cmd = cvsPath_ + " commitfile \"" + commitMsg + "\"" + " \"" + fnlf.filename() + '\"';

	if ( !exec( cmd, workingPath_, exitCode, output_, log_ ) )
	{
		ERROR_MSG( "Couldn't exec %s\n", cmd.c_str() );
		result = false;
	}
	else
		INFO_MSG( "Commit Done, cvs output:\n%s\n", output_.c_str() );

	return result && exitCode == 0;
}

bool CVSWrapper::isInCVS( const std::string& relativePathName )
{
	if( !enabled_ )
		return false;

	int exitCode;
	std::string cmd = cvsPath_ + " managed \"" + relativePathName + '\"';
	if ( !exec( cmd, workingPath_, exitCode, output_, log_ ) )
	{
		ERROR_MSG( "Couldn't exec %s\n", cmd.c_str() );
		return false;
	}
	INFO_MSG( "%s %s under version control\n", relativePathName.c_str(), exitCode == 0 ? "is" : "isn't" );
	return exitCode == 0;
}

void CVSWrapper::removeFile( const std::string& relativePathName )
{
	if( !enabled_ )
		return;

	int exitCode;
	std::string cmd = cvsPath_ + " removefile \"" + relativePathName + '\"';
	if ( !exec( cmd, workingPath_, exitCode, output_, log_ ) )
		ERROR_MSG( "Couldn't exec %s\n", cmd.c_str() );
}

std::set<std::string> CVSWrapper::addFolder( std::string relativePathName, const std::string& commitMsg,
											bool checkParent )
{
	std::set<std::string> error;
	std::set<std::string> result;

	if( !enabled_ )
		return error;

	if( !isDirectory( workingPath_ + relativePathName ) )
		return error;

	int exitCode = 0;

	if( !relativePathName.empty() && relativePathName[0] == '/' )
		relativePathName.erase( relativePathName.begin() );

	if( checkParent )
	{
		std::string prefix, suffix = relativePathName;

		while( !suffix.empty() )
		{
			if( !prefix.empty() )
				prefix += '/';
			if( suffix.find( '/' ) != suffix.npos )
			{
				prefix += suffix.substr( 0, suffix.find( '/' ) );
				suffix = suffix.substr( suffix.find( '/' ) + 1 );
			}
			else
			{
				prefix += suffix;
				suffix.clear();
			}

			if( !isInCVS( prefix ) )
			{
				std::string cmd = cvsPath_ + " addfolder \"" + commitMsg + "\" ";
				cmd += '\"' + prefix + '\"';

				if( !exec( cmd, workingPath_, exitCode, output_, log_ ) || exitCode != 0 )
				{
					ERROR_MSG( "Couldn't exec %s:\n%s\n", cmd.c_str(), output_.c_str() );
					return error;
				}
				result.insert( prefix );
			}
		}
	}
	else
	{
		std::string cmd = cvsPath_ + " addfolder \"" + commitMsg + "\" ";
		cmd += '\"' + relativePathName + '\"';

		if( !exec( cmd, workingPath_, exitCode, output_, log_ ) || exitCode != 0 )
		{
			ERROR_MSG( "Couldn't exec %s:\n%s\n", cmd.c_str(), output_.c_str() );
			return error;
		}
		result.insert( relativePathName );
	}
	WIN32_FIND_DATA findData;
	HANDLE find = FindFirstFile( ( workingPath_ + relativePathName + "/*.*" ).c_str(), &findData );
	if( find != INVALID_HANDLE_VALUE )
	{
		do
		{
			if( !( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) ||
				strcmp( findData.cFileName, "." ) == 0 ||
				strcmp( findData.cFileName, ".." ) == 0 ||
				stricmp( findData.cFileName, dirToIgnore_.c_str() ) == 0 )
				continue;
			std::set<std::string> files = addFolder( relativePathName + '/' + findData.cFileName, commitMsg, false );
			if( files.empty() )
				return error;
			result.insert( files.begin(), files.end() );
		}
		while( FindNextFile( find, &findData ) );
		FindClose( find );
	}
	return result;
}

bool CVSWrapper::addFile( std::string relativePathName, bool isBinary, bool recursive )
{
	if( !enabled_ )
		return true;

	int exitCode = 0;

	std::string cmd = cvsPath_;
	if( isBinary )
		cmd += " addbinaryfile ";
	else
		cmd += " addfile ";
	cmd += '\"' + relativePathName + '\"';

	if ( !exec( cmd, workingPath_, exitCode, output_, log_ ) || exitCode != 0 )
	{
		ERROR_MSG( "Couldn't exec %s:\n%s\n", cmd.c_str(), output_.c_str() );
		return false;
	}

	if( relativePathName.find( '*' ) != relativePathName.npos && recursive )
	{
		WIN32_FIND_DATA findData;
		HANDLE find = FindFirstFile( ( workingPath_ + "*.*" ).c_str(), &findData );
		if( find != INVALID_HANDLE_VALUE )
		{
			do
			{
				if( !( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) ||
					strcmp( findData.cFileName, "." ) == 0 ||
					strcmp( findData.cFileName, ".." ) == 0 ||
					stricmp( findData.cFileName, dirToIgnore_.c_str() ) == 0 )
					continue;
				if( !CVSWrapper( workingPath_ + findData.cFileName ).addFile( relativePathName, isBinary, recursive ) )
					return false;
			}
			while( FindNextFile( find, &findData ) );
			FindClose( find );
		}
	}

	return exitCode == 0;
}

const std::string& CVSWrapper::output() const
{
	return output_;
}

bool CVSWrapper::exec( std::string cmd, std::string workingDir, int& exitCode, std::string& output, CVSLog* log )
{
	output.clear();

	if( !enabled_ )
		return true;

	CWaitCursor wait;
	INFO_MSG( "executing %s in %s\n", cmd.c_str(), workingDir.c_str() );

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
	SECURITY_ATTRIBUTES saAttr; 

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
	saAttr.bInheritHandle = TRUE; 
	saAttr.lpSecurityDescriptor = NULL; 

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

	si.dwFlags = STARTF_USESTDHANDLES;

	HANDLE stdErrRead, stdErrWrite;

	if (!CreatePipe( &stdErrRead, &stdErrWrite, &saAttr, 0 )) 
	{
		ERROR_MSG( "Couldn't create pipe\n" );
		return false;
	}

    si.hStdError = stdErrWrite;
	si.hStdOutput = stdErrWrite;

	if (!CreateProcess( 0, const_cast<char*>( cmd.c_str() ), 0, 0, true, CREATE_NO_WINDOW, 0, workingDir.c_str(), &si, &pi ))
	{
		ERROR_MSG( "Unable to create process %s, last error is %u\n", cmd.c_str(), GetLastError() );
		return false;
	}

	// stdErrWrite is used by the new process now, we don't need it
	CloseHandle( stdErrWrite );

	// Read all the output
	char buffer[1024];
	DWORD amntRead;

	while ( ReadFile( stdErrRead, buffer, 1023, &amntRead, 0 ) != 0 )
	{
		MF_ASSERT( amntRead < 1024 );

		buffer[amntRead] = '\0';
		if( log )
			log->add( buffer );
		output += buffer;
	}

	CloseHandle( stdErrRead );

    // Wait until child process exits
	if (WaitForSingleObject( pi.hProcess, INFINITE ) == WAIT_FAILED)
	{
		ERROR_MSG( "WaitForSingleObject failed\n" );
		return false;
	}

	// Get the exit code
	if (!GetExitCodeProcess( pi.hProcess, (LPDWORD) &exitCode ))
	{
		ERROR_MSG( "Unable to get exit code\n" );
		return false;
	}

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );

	return true;
}
