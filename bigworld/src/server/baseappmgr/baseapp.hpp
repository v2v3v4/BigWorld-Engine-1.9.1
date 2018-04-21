/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BASE_APP_HPP
#define BASE_APP_HPP

#include "network/channel.hpp"
#include "server/backup_hash.hpp"

class BackupBaseApp;

class BaseApp: public Mercury::ChannelOwner
{
public:
	BaseApp( const Mercury::Address & intAddr,
			const Mercury::Address & extAddr,
			int id );

	float load() const { return load_; }

	void updateLoad( float load, int numBases, int numProxies )
	{
		load_ = load;
		numBases_ = numBases;
		numProxies_ = numProxies;
	}

	bool hasTimedOut( uint64 currTime, uint64 timeoutPeriod,
		   uint64 timeSinceHeardAny ) const;

	const Mercury::Address & externalAddr() const { return externalAddr_; }

	int numBases() const	{ return numBases_; }
	int numProxies() const	{ return numProxies_; }

	BaseAppID id() const	{ return id_; }
	void id( int id )		{ id_ = id; }

	void setBackup( BackupBaseApp * pBackup )	{ pBackup_ = pBackup; }
	BackupBaseApp * getBackup() const			{ return pBackup_; }

	void addEntity();

	static Watcher * makeWatcher();

	const BackupHash & backupHash() const { return backupHash_; }
	BackupHash & backupHash() { return backupHash_; }

	const BackupHash & newBackupHash() const { return newBackupHash_; }
	BackupHash & newBackupHash() { return newBackupHash_; }

private:
	Mercury::Address		externalAddr_;
	BaseAppID				id_;

	float					load_;
	int						numBases_;
	int						numProxies_;

	BackupBaseApp *			pBackup_;

	BackupHash backupHash_;
	BackupHash newBackupHash_;
};


/**
 *
 */
class BackupBaseApp : public Mercury::ChannelOwner
{
public:
	typedef std::set< BaseApp * > BackedUpSet;

	BackupBaseApp( const Mercury::Address & addr, int id );

	bool backup( BaseApp & cache );
	bool stopBackup( BaseApp & cache, bool tellBackupBaseApp );

	BaseAppID id() const					{ return id_; }
	float load() const						{ return load_; }

	void updateLoad( float load )
	{
		load_ = load;
	}

private:
	BaseAppID			id_;
	float				load_;
public:
	BackedUpSet			backedUp_;
};

#endif // BASE_APP_HPP
