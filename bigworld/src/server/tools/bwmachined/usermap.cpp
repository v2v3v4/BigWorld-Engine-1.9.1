/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "usermap.hpp"
#include "common_machine_guard.hpp"
#include <syslog.h>

UserMap::UserMap()
{
	this->queryUserConfs();

	// Set up the failed reply
	notfound_.uid_ = notfound_.UID_NOT_FOUND;
	notfound_.outgoing( true );
}

void UserMap::queryUserConfs()
{
	struct passwd *pEnt;

	while ((pEnt = getpwent()) != NULL)
	{
		UserMessage um;
		um.outgoing( true );
		um.init( *pEnt );

		// Initially are only interested in users with a valid ~/.bwmachined.conf
		if (this->getEnv( um, pEnt ))
			this->add( um );
	}
	endpwent();
}

void UserMap::add( const UserMessage &um )
{
	map_.insert( Map::value_type( um.uid_, um ) );
}

UserMessage* UserMap::add( struct passwd *ent )
{
	UserMessage newguy;
	newguy.init( *ent );
	newguy.outgoing( true );

	this->getEnv( newguy );
	this->add( newguy );

	return this->fetch( newguy.uid_ );
}

UserMessage* UserMap::fetch( uint16 uid )
{
	Map::iterator it;
	if ((it = map_.find( uid )) != map_.end())
		return &it->second;
	else
		return NULL;
}

// Anonymous namespace
namespace
{

/**
 * Helper function for determining whether a line from .bwmachined.conf is
 * empty or not.
 */
bool isEmpty( const char *buf )
{
	while (*buf)
	{
		if (!isspace( *buf ))
			return false;
		++buf;
	}

	return true;
}

}


bool UserMap::getEnv( UserMessage &um, struct passwd *pEnt )
{
	char buf[ 1024 ], mfroot[ 256 ], bwrespath[ 1024 ];
	const char *filename = um.getConfFilename();
	bool done = false;

	// If this uid doesn't exist on this system, fail now
	if (!pEnt && getpwuid( um.uid_ ) == NULL)
	{
		syslog( LOG_ERR, "Uid %d doesn't exist on this system!", um.uid_ );
		return false;
	}

	// first look in the user's home directory (under linux)
	FILE * file;
	if ((file = fopen( filename, "r" )) != NULL)
	{
		while (fgets( buf, sizeof(buf)-1, file ) != NULL)
		{
			if (buf[0] != '#' && buf[0] != 0)
			{
				if (sscanf( buf, "%[^;];%s", mfroot, bwrespath ) == 2)
				{
					um.mfroot_ = mfroot;
					um.bwrespath_ = bwrespath;
					done = true;
					break;
				}
				else if (!::isEmpty( buf ))
				{
					syslog( LOG_ERR, "%s has invalid line '%s'\n",
						filename, buf );
				}
			}
		}
	}

	if (file != NULL) fclose( file );
	if (done) return true;

	// Now consult the global file in /etc/ (or C:program files/bigworld/).
	// Don't warn on missing /etc/bigworld.conf since this isn't strictly
	// required.
	if ((file = fopen( machinedConfFile, "r" )) == NULL)
	{
		return false;
	}

	while (fgets( buf, sizeof(buf)-1, file ) != NULL)
	{
		if(buf[0] == '#' || buf[0] == 0)
			continue;

		if (buf[0] == '[')
		{
			// Reached the tags section. Break out and fail.
			break;
		}

		int file_uid;
		if (sscanf( buf, "%d;%[^;];%s", &file_uid, mfroot, bwrespath ) == 3 &&
			file_uid == um.uid_)
		{
			um.mfroot_ = mfroot;
			um.bwrespath_ = bwrespath;
			done = true;
			break;
		}
	}

	fclose( file );

	if (done)
		return true;
	else
		return false;
}

bool UserMap::setEnv( const UserMessage &um )
{
	// TODO: Get a non-blocking file-lock (fcntl(2)) here first (do tools too)

	// Truncate the user's config file
	FILE *file = fopen( um.getConfFilename(), "w" );
	if (file == NULL)
		return false;

	fprintf( file, "%s;%s\n", um.mfroot_.c_str(), um.bwrespath_.c_str() );
	fchown( fileno( file ), um.uid_, (gid_t)-1 );
	fclose( file );
	return true;
}

void UserMap::flush()
{
	map_.clear();
	this->queryUserConfs();
}
