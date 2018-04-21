/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "db_file_transfer.hpp"

#include "db_consolidator_error.hpp"

#include "network/nub.hpp"
#include "resmgr/file_system.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

// -----------------------------------------------------------------------------
// Section: FileReceiver
// -----------------------------------------------------------------------------
/**
 *	Constructor.
 */
FileReceiver::FileReceiver( int socket, uint32 ip, uint16 port,
		FileReceiverMgr& mgr ) :
	mgr_( mgr ),
	msgReceiver_( 512 ),
	pMsgProcessor_( &FileReceiver::recvCommand ),
	curActionDesc_( "receive command" ),
	lastActivityTime_( timestamp() ),
	srcAddr_( ip, port ),
	destPath_( mgr.consolidationDir() ),
	expectedFileSize_( 0 ),
	currentFileSize_( 0 ),
	destFile_( NULL )
{
//	char	addrStr[15+1+5+1];	// nnn.nnn.nnn.nnn:nnnnn
//	srcAddr_.writeToString( addrStr, sizeof( addrStr ) );
//	DEBUG_MSG( "FileReceiver::FileReceiver: Accepted incoming connection from "
//			"%s\n", addrStr );

	endPoint_.setFileDescriptor( socket );
	endPoint_.setnonblocking( true );

	msgReceiver_.setMsgSize( sizeof( char ) );	 // To match recvCommand()

	mgr_.nub().registerFileDescriptor( endPoint_, this );
}

/**
 *	Destructor
 */
FileReceiver::~FileReceiver()
{
	mgr_.nub().deregisterFileDescriptor( endPoint_ );
}

/**
 * 	Mercury::InputNotificationHandler override
 */
int FileReceiver::handleInputNotification( int fd )
{
	lastActivityTime_ = timestamp();

	if (pMsgProcessor_ == NULL)
	{
		ERROR_MSG( "FileReceiverMgr::handleInputNotification: Receiving "
				"data but we are currently in %s mode\n", curActionDesc_ );
		return 0;
	}

	bool isMore;
	do
	{
		isMore = false;
		bool isOK = msgReceiver_.recvMsg( endPoint_ );
		if (isOK)
		{
			if (msgReceiver_.isDone())
			{
				size_t nextMsgSize = (this->*pMsgProcessor_)();
				if (pMsgProcessor_ != NULL)
				{
					bool wasAlwaysDone = msgReceiver_.isAlwaysDone();

					msgReceiver_.setMsgSize( nextMsgSize );

					// Prevent infinite loop when nextMsgSize is 0. In that
					// case, msgReceiver_.isDone() is always true.
					// NOTE: This is handling a few cases:
					// Case 1: Transition from not-always-done to always-done:
					//			- Do another loop.
					// Case 2: Remaining on always-done:
					//			- Don't do another loop.
					// Case 3: Transition from always-done to not-always-done:
					//			- Do another loop.
					// Case 4: Remaining on not-always-done:
					//			- Do another loop.
					isMore = !(msgReceiver_.isAlwaysDone() && wasAlwaysDone);
				}
			}
		}
		else
		{
			char	addrStr[15+1+5+1];	// nnn.nnn.nnn.nnn:nnnnn
			srcAddr_.writeToString( addrStr, sizeof( addrStr ) );
			ERROR_MSG( "FileReceiver::handleInputNotification: "
					"Error communicating with file transfer process on %s. "
					"Failed to %s.\n", addrStr, curActionDesc_ );

			pMsgProcessor_ = NULL;
			curActionDesc_ = "wait for termination after error";
			mgr_.onFileReceiveError();
		}
	} while (isMore);

	return 0;
}

/**
 * 	Processes a "command" from the remote end.
 */
size_t FileReceiver::recvCommand()
{
	MF_ASSERT( msgReceiver_.msgLen() == sizeof(char) );

	const char* pCommand =
			reinterpret_cast< const char * >( msgReceiver_.msg() );

	size_t nextMsgSize;
	switch (*pCommand)
	{
		case 'n':
			nextMsgSize = sizeof( uint16 );
			pMsgProcessor_ = &FileReceiver::recvSrcPathLen;
			curActionDesc_ = "receive source file path length";
			break;
		case 'e':
			nextMsgSize = sizeof( uint16 );
			pMsgProcessor_ = &FileReceiver::recvErrorLen;
			curActionDesc_ = "receive error string length";
			break;
		default:
			ERROR_MSG( "FileReceiver::recvCommand: Received "
					"invalid command %c\n", *pCommand );
			nextMsgSize = 0;
			pMsgProcessor_ = NULL;
			curActionDesc_ = "wait for termination after error";
			mgr_.onFileReceiveError();
			break;
	}

	return nextMsgSize;
}

/**
 * 	Receive the length of the source file's path.
 */
size_t FileReceiver::recvSrcPathLen()
{
	MF_ASSERT( msgReceiver_.msgLen() == sizeof( uint16 ) );

	uint16 	pathLen;
	memcpy( &pathLen, msgReceiver_.msg(), sizeof( pathLen ) );

	pMsgProcessor_ = &FileReceiver::recvSrcPath;
	curActionDesc_ = "receive source file path";

	return pathLen;
}

/**
 * 	Receives the source file's path.
 */
size_t FileReceiver::recvSrcPath()
{
	srcPath_.assign( reinterpret_cast< const char * >( msgReceiver_.msg() ),
			msgReceiver_.msgLen() );

	// Extract file name and add to destPath_
	std::string::size_type pos = srcPath_.find_last_of( '/' );
	if (pos == std::string::npos)
		pos = 0;
	destPath_.insert( destPath_.end(), srcPath_.begin() + pos + 1,
			srcPath_.end() );

	// Check that file doesn't already exist and append suffix if it does.
	std::string origDestPath = destPath_;
	int			suffix = 1;
	while (NativeFileSystem::getAbsoluteFileType( destPath_ ) !=
		IFileSystem::FT_NOT_FOUND)
	{
		++suffix;

		std::stringstream ss;
		ss << origDestPath << '-' << suffix;
		destPath_ = ss.str();
	}
	if (destPath_ != origDestPath)
	{
		WARNING_MSG( "FileReceiver::recvSrcPath: Default destination file '%s' "
				"already exists. Saving to new destination '%s'\n",
				origDestPath.c_str(), destPath_.c_str() );
	}

	MF_ASSERT( destFile_ == NULL );

	// Open file
	size_t nextMsgSize = sizeof( expectedFileSize_ );
	destFile_ = fopen( destPath_.c_str(), "wb" );
	if (destFile_ != NULL)
	{
		pMsgProcessor_ = &FileReceiver::recvFileLen;
		curActionDesc_ = "receive file length";
	}
	else
	{
		ERROR_MSG( "FileReceiverMgr::recvSrcFileInfo: Failed to create local "
				"file '%s'\n", destPath_.c_str() );
		pMsgProcessor_ = NULL;
		curActionDesc_ = "wait for termination after error";
		nextMsgSize = 0;
		mgr_.onFileReceiveError();
	}

	return nextMsgSize;
}

/**
 *	Receives the file length from the socket.
 */
size_t FileReceiver::recvFileLen()
{
	MF_ASSERT( msgReceiver_.msgLen() == sizeof( expectedFileSize_ ) );

	memcpy( &expectedFileSize_, msgReceiver_.msg(), sizeof(expectedFileSize_) );

	if (expectedFileSize_ > 0)
	{
		char	addrStr[15+1+5+1];	// nnn.nnn.nnn.nnn:nnnnn
		srcAddr_.writeToString( addrStr, sizeof( addrStr ) );
		TRACE_MSG( "FileReceiver::recvFileLen: Receiving data for "
				"file '%s' of size %u from '%s' on %s\n", destPath_.c_str(),
				expectedFileSize_, srcPath_.c_str(), addrStr );
		pMsgProcessor_ = &FileReceiver::recvFileContents;
		curActionDesc_ = "receive file contents";
	}
	else
	{
		ERROR_MSG( "FileReceiverMgr::recvSrcFileInfo: Received empty file "
				"'%s'\n", destPath_.c_str() );
		pMsgProcessor_ = NULL;
		curActionDesc_ = "wait for termination after error";
		mgr_.onFileReceiveError();
	}

	return 0;
}

/**
 *	Receives the file contents and write it to the output file.
 */
size_t FileReceiver::recvFileContents()
{
	size_t numToWrite = msgReceiver_.msgLen();
	if (numToWrite == 0)
	{
		return 0;
	}

	int success = fwrite( msgReceiver_.msg(), numToWrite, 1, destFile_ );
	if (success == 0)
	{
		ERROR_MSG( "FileReceiverMgr::recvFileContents: Failed to write to "
				"file '%s'\n", destPath_.c_str() );
		pMsgProcessor_ = NULL;
		curActionDesc_ = "wait for termination after error";
		mgr_.onFileReceiveError();
		return 0;
	}

	currentFileSize_ += numToWrite;

	if (currentFileSize_ >= expectedFileSize_)
	{
		// TODO: Make sure we don't read more than necessary.
		MF_ASSERT( currentFileSize_ == expectedFileSize_ );

		this->closeFile();
		pMsgProcessor_ = NULL;
		curActionDesc_ = "wait for termination after finished";

		mgr_.onFileReceived( *this );
	}

	return 0;
}

/**
 *	Receives error string length from the socket.
 */
size_t FileReceiver::recvErrorLen()
{
	uint16 errorLen;
	memcpy( &errorLen, msgReceiver_.msg(), sizeof( errorLen ) );

	pMsgProcessor_ = &FileReceiver::recvErrorStr;
	curActionDesc_ = "receive error string";

	return errorLen;
}

/**
 *	Reads the error string from the socket.
 */
size_t FileReceiver::recvErrorStr()
{
	std::string	errorStr( reinterpret_cast< const char * >(msgReceiver_.msg()),
			msgReceiver_.msgLen() );
	char	addrStr[15+1+5+1];	// nnn.nnn.nnn.nnn:nnnnn
	srcAddr_.writeToString( addrStr, sizeof( addrStr ) );
	ERROR_MSG( "FileReceiver::recvErrorStr: "
			"Database transfer utility on %s reported error: %s\n",
			addrStr, errorStr.c_str() );

	pMsgProcessor_ = NULL;
	curActionDesc_ = "wait for termination after error";

	mgr_.onFileReceiveError();

	return 0;
}

/**
 *	Closes the local file that we're writing to.
 */
bool FileReceiver::closeFile()
{
	bool isOK = true;
	if (fclose( destFile_ ) != 0)
	{
		char errnoBuf[1024];
		ERROR_MSG( "FileReceiverMgr::closeFile: Failed to close file '%s': %s",
				destPath_.c_str(),
				strerror_r( errno, errnoBuf, sizeof( errnoBuf ) ) );
		isOK = false;
	}
	destFile_ = NULL;

	return isOK;
}

/**
 *	Deletes the remote file.
 */
bool FileReceiver::deleteRemoteFile()
{
	bool isOK = true;

	// Tell remote end to delete their file.
	// TODO: Wait for acknowledgement?
	if (endPoint_.send( "d", 1 ) != 1)
	{
		char errnoBuf[1024];
		char addrStr[15+1+5+1];	// nnn.nnn.nnn.nnn:nnnnn
		srcAddr_.writeToString( addrStr, sizeof( addrStr ) );

		ERROR_MSG( "FileReceiver::deleteRemoteFile: Failed to delete file "
				"'%s' on %s: %s\n", srcPath_.c_str(), addrStr,
				strerror_r( errno, errnoBuf, sizeof( errnoBuf ) ) );

		isOK = false;
	}

	return isOK;
}

/**
 *	Deletes the local (transferred) file.
 */
bool FileReceiver::deleteLocalFile()
{
	if (destPath_ == mgr_.consolidationDir())
	{
		return true;	// Nothing to delete.
	}

	bool isOK = true;
	if (remove( destPath_.c_str() ) != 0)
	{
		char errnoBuf[1024];

		ERROR_MSG( "FileReceiver::deleteLocalFile: Failed to delete local file "
				"'%s': %s\n", destPath_.c_str(),
				strerror_r( errno, errnoBuf, sizeof( errnoBuf ) ) );

		isOK = false;
	}

	return isOK;
}

/**
 *	Abort file transfer.
 */
void FileReceiver::abort()
{
	if (pMsgProcessor_ != NULL)
	{
		pMsgProcessor_ = NULL;
		curActionDesc_ = "abort file transfer";
	}

	if (destFile_)
	{
		this->closeFile();
	}
}


// -----------------------------------------------------------------------------
// Section: FileReceiverMgr
// -----------------------------------------------------------------------------
/**
 *	Constructor.
 */
FileReceiverMgr::FileReceiverMgr( Mercury::Nub& nub,
		const SecondaryDBInfos& secondaryDBs,
		const std::string& consolidationDir ) :
	nub_( nub ),
	consolidationDir_( consolidationDir )
{
	for (SecondaryDBInfos::const_iterator i = secondaryDBs.begin();
			i != secondaryDBs.end(); ++i)
	{
		// All paths should be unique.
		MF_VERIFY( unfinishedDBs_.insert(
				SourceDBs::value_type( i->location, i->hostIP ) ).second );
	}
}

/**
 * 	Destructor.
 */
FileReceiverMgr::~FileReceiverMgr()
{
	// Destroy still running receivers.
	for (ReceiverSet::iterator i = startedReceivers_.begin();
			i != startedReceivers_.end(); ++i)
	{
		delete *i;
	}

	// Destroy completed receivers
	for ( Receivers::iterator i = completedReceivers_.begin();
			i != completedReceivers_.end(); ++i )
	{
		delete *i;
	}
}

/**
 *	Called by TcpListener if listener bind failed.
 */
void FileReceiverMgr::onFailedBind( uint32 ip, uint16 port )
{
	MF_ASSERT( (ip = INADDR_ANY) && (port = 0) );
	ERROR_MSG( "FileReceiverMgr::onFailedBind: Failed to bind to any port.\n" );
}

/**
 *	Called by TcpListener if we failed to accept incoming connection.
 */
void FileReceiverMgr::onFailedAccept( uint32 ip, uint16 port )
{
	Mercury::Address addr( ip, port );
	char	addrStr[15+1+5+1];	// nnn.nnn.nnn.nnn:nnnnn
	addr.writeToString( addrStr, sizeof( addrStr ) );
	ERROR_MSG( "FileReceiverMgr::onFailedAccept: Failed to accept incoming "
			"connection from %s\n", addrStr );
}

/**
 * 	Called by FileReceiver when they finished receiving a file.
 */
void FileReceiverMgr::onFileReceived( FileReceiver& receiver )
{
	SourceDBs::iterator pSourceDB = unfinishedDBs_.find( receiver.srcPath() );
	if (pSourceDB != unfinishedDBs_.end())
	{
		unfinishedDBs_.erase( pSourceDB );
	}
	else
	{
		char	addrStr[15+1+5+1];	// nnn.nnn.nnn.nnn:nnnnn
		receiver.srcAddr().writeToString( addrStr, sizeof( addrStr ) );
		ERROR_MSG( "DBConsolidator: Received unknown file '%s' from %s\n",
				receiver.srcPath().c_str(), addrStr );
	}

	MF_VERIFY( startedReceivers_.erase( &receiver ) == 1 );
	completedReceivers_.push_back( &receiver );
	receivedFilePaths_.push_back( receiver.destPath() );

	if (unfinishedDBs_.empty())
	{
		// Break processing. This will be picked up by DBConsolidator.
		nub_.breakProcessing();
	}
}

/**
 * 	Called by to notify us of an error in file transfer.
 */
void FileReceiverMgr::onFileReceiveError()
{
	ERROR_MSG( "FileReceiverMgr:onFileReceiveError: Aborting file "
			"transfer!\n" );

	// Break processing. This will be picked up by DBConsolidator.
	nub_.breakProcessing();
}

/**
 * 	Returns true if we've finished receiving all our files.
 */
bool FileReceiverMgr::finished() const
{
	return unfinishedDBs_.empty();
}

/**
 *	This function takes a mighty good guess as to which databases still have
 *	not started their transfer.
 */
FileReceiverMgr::SourceDBs FileReceiverMgr::getUnstartedDBs() const
{
	SourceDBs unstartedDBs = unfinishedDBs_;
	Receivers unstartedReceivers;

	// Remove databases that have started transferring.
	for ( ReceiverSet::const_iterator ppReceiver = startedReceivers_.begin();
			ppReceiver != startedReceivers_.end(); ++ppReceiver)
	{
		if (!(*ppReceiver)->srcPath().empty())
		{
			SourceDBs::iterator pFound =
					unstartedDBs.find( (*ppReceiver)->srcPath() );

			if (pFound != unstartedDBs.end())
			{
				unstartedDBs.erase(pFound);
			}
			else
			{
				ERROR_MSG( "FileReceiverMgr::getUnstartedDBs: Cannot find %s\n",
						(*ppReceiver)->srcPath().c_str() );
			}
		}
		else
		{
			// Connected but haven't yet transferred their source path.
			unstartedReceivers.push_back( *ppReceiver );
		}
	}

	if (!unstartedReceivers.empty())
	{
		// Have to work out which entry to remove for those that have
		// connected but not sent their source path.

		// Make IP address to unstartedDBs item map.
		typedef std::multimap< uint32, SourceDBs::iterator > IPToSrcDB;
		IPToSrcDB	ipToSrcDB;
		for ( SourceDBs::iterator i = unstartedDBs.begin();
				i != unstartedDBs.end(); ++i )
		{
			ipToSrcDB.insert( IPToSrcDB::value_type( i->second, i ) );
		}

		// Now remove secondary DBs that have connected but haven't started
		// their transfer.
		for (Receivers::const_iterator ppReceiver = unstartedReceivers.begin();
				ppReceiver != unstartedReceivers.end(); ++ppReceiver)
		{
			// __kyl__(6/8/2008) We are removing some random entry in
			// unstartedDBs that matches the IP address. It could be the wrong
			// entry but doesn't matter much at the moment since we're only
			// using this to print out error messages.
			IPToSrcDB::iterator i = ipToSrcDB.find( (*ppReceiver)->srcAddr().ip );
			MF_ASSERT( i != ipToSrcDB.end() );
			unstartedDBs.erase( i->second );
		}
	}

	return unstartedDBs;
}

/**
 * 	Deletes the local copies of the secondary DB files.
 */
bool FileReceiverMgr::cleanUpLocalFiles()
{
	bool isOK = true;
	for ( Receivers::iterator i = completedReceivers_.begin();
			i != completedReceivers_.end(); ++i )
	{
		isOK = (*i)->deleteLocalFile() && isOK;
	}

	for ( ReceiverSet::iterator i = startedReceivers_.begin();
			i != startedReceivers_.end(); ++i )
	{
		(*i)->abort();
		isOK = (*i)->deleteLocalFile() && isOK;
	}
	return isOK;
}

/**
 * 	Sends a message to delete the remote secondary database files, except
 * 	for those secondary databases that had errors when we tried to
 * 	consolidate them (i.e. those in errorDBs).
 */
bool FileReceiverMgr::cleanUpRemoteFiles( const DBConsolidatorErrors& errorDBs )
{
	MF_ASSERT( finished() );

	bool isOK = true;

	for ( Receivers::iterator i = completedReceivers_.begin();
			i != completedReceivers_.end(); ++i )
	{
		if (!errorDBs.secondaryDBHasError( (*i)->destPath() ))
		{
			isOK = (*i)->deleteRemoteFile() && isOK;
		}
		else
		{
			char	addrStr[15+1+5+1];	// nnn.nnn.nnn.nnn:nnnnn
			(*i)->srcAddr().writeToString( addrStr, sizeof( addrStr ) );

			WARNING_MSG( "FileReceiverMgr::cleanUpRemoteFiles: "
					"Skipped deletion of secondary database file %s on %s "
					"because there were errors during consolidation\n",
					(*i)->destPath().c_str(), addrStr );
		}
	}

	return isOK;
}
