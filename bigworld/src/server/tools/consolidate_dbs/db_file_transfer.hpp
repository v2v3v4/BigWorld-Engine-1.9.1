/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef DB_FILE_TRANSFER_HPP
#define DB_FILE_TRANSFER_HPP

#include "msg_receiver.hpp"

#include "network/endpoint.hpp"
#include "network/interfaces.hpp"

#include <set>

// Forward declarations
class FileReceiverMgr;
class DBConsolidatorErrors;
typedef std::vector< std::string >	FileNames;

/**
 *	Stores information about the location of a secondary DB.
 */
struct SecondaryDBInfo
{
	uint32		hostIP;
	std::string location;

	SecondaryDBInfo() : hostIP( 0 ), location() {}
	SecondaryDBInfo( uint32 ip, const std::string& path ) :
		hostIP( ip ), location( path )
	{}
};
typedef std::vector< SecondaryDBInfo > SecondaryDBInfos;

/**
 * 	Receives a secondary database file.
 */
class FileReceiver : public Mercury::InputNotificationHandler
{
public:
	FileReceiver( int socket, uint32 ip, uint16 port, FileReceiverMgr& mgr );
	~FileReceiver();

	const Mercury::Address& srcAddr() const
	{
		return srcAddr_;
	}
	const std::string& destPath() const
	{
		return destPath_;
	}
	const std::string& srcPath() const
	{
		return srcPath_;
	}
	uint64 lastActivityTime() const { return lastActivityTime_; }

	bool deleteRemoteFile();
	bool deleteLocalFile();
	void abort();

	// Mercury::InputNotificationHandler override
	virtual int handleInputNotification( int fd );

private:
	size_t recvCommand();
	size_t recvSrcPathLen();
	size_t recvSrcPath();
	size_t recvFileLen();
	size_t recvFileContents();
	size_t recvErrorLen();
	size_t recvErrorStr();

	bool closeFile();

	typedef size_t (FileReceiver::*MessageProcessorFn)();

	Endpoint			endPoint_;
	FileReceiverMgr& 	mgr_;
	MsgReceiver			msgReceiver_;
	MessageProcessorFn	pMsgProcessor_;
	const char*			curActionDesc_;
	uint64				lastActivityTime_;
	Mercury::Address	srcAddr_;	// Cached for error info
	std::string			srcPath_;
	std::string			destPath_;
	uint32				expectedFileSize_;
	uint32				currentFileSize_;
	FILE*				destFile_;
};

/**
 *	Receives secondary database files.
 */
class FileReceiverMgr
{
public:
	FileReceiverMgr( Mercury::Nub& nub, const SecondaryDBInfos& secondaryDBs,
			const std::string& consolidationDir );
	~FileReceiverMgr();

	Mercury::Nub& nub()		{ return nub_; }

	const std::string& consolidationDir() const { return consolidationDir_; }

	bool finished() const;
	const FileNames& receivedFilePaths() const
	{
		return receivedFilePaths_;
	}
	bool cleanUpLocalFiles();
	bool cleanUpRemoteFiles( const DBConsolidatorErrors& errorDBs );

	// Called by TcpListener
	void onAcceptedConnection( int socket, uint32 ip, uint16 port )
	{
		startedReceivers_.insert( new FileReceiver( socket, ip, port,
				*this ) );
	}
	static void onFailedBind( uint32 ip, uint16 port );
	static void onFailedAccept( uint32 ip, uint16 port );

	// Called by FileReceiver
	void onFileReceiverStart( FileReceiver& receiver );
	void onFileReceived( FileReceiver& receiver );

	// Called to notify us of an error.
	void onFileReceiveError();

	// Map of file location to host IP address.
	bool hasUnstartedDBs() const
	{
		return ((unfinishedDBs_.size() - startedReceivers_.size()) > 0);
	}
	typedef std::map< std::string, uint32 >	SourceDBs;
	SourceDBs getUnstartedDBs() const;

	typedef std::set< FileReceiver* > ReceiverSet;
	const ReceiverSet& startedReceivers() const { return startedReceivers_; }

private:
	Mercury::Nub&				nub_;
	std::string					consolidationDir_;
	SourceDBs					unfinishedDBs_;
	ReceiverSet					startedReceivers_;
	typedef std::vector< FileReceiver* > Receivers;
	Receivers					completedReceivers_;
	FileNames					receivedFilePaths_;
};


#endif /*DB_FILE_TRANSFER_HPP*/
