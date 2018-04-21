/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include <sstream>
#include <fstream>
#include <map>
#include <mysql/mysql.h>
#include <cstring>
#include <cstdlib>

typedef std::map< std::string, std::string > Section;
typedef std::map< std::string, Section > Configs;


/**
 *	This util executes privileged commands. The command arguments
 *	are read in from bigworld.conf which should be writable by
 *	root only.
 *
 *	To execute privileged commands the snapshot_helper binary needs
 *	to have it's setuid attribute set. This can be done by:
 *
 *	# chown root:root snapshot_helper
 *	# chmod 4511 snapshot_helper
 */
int main( int argc, char* argv[] )
{
	// Test if setuid attribute is set
	if (argc == 1)
	{
		return setuid( 0 );
	}

	// Read bigworld.conf
	Configs configs;

	std::ifstream file( "/etc/bigworld.conf" );
	std::string line;
	std::string sectionName;

	if (!file.is_open())
	{
		return -1;
	}

	while(!file.eof())
	{
		getline( file, line );

		size_t len = line.length();

		if (len == 0)
		{
			continue;
		}

		// Check for sections
		if (line[0] == '[' && line[len-1] == ']')
		{
			sectionName = line.substr( 1, len -2 );
			configs[sectionName] = Section();
			continue;
		}

		size_t pos = line.find( "=" );

		if (pos == std::string::npos)
		{
			continue;
		}

		std::string option = line.substr( 0, pos );
		std::string value = line.substr( pos + 1 );

		// Trim spaces
		size_t start = option.find_first_not_of( ' ' );
		size_t end = option.find_last_not_of( ' ' );
		option = option.substr( start, end + 1 );

		start = value.find_first_not_of( ' ' );
		end = value.find_last_not_of( ' ' );
		value = value.substr( start, end + 1);

		configs[sectionName][option] = value;
	}

	file.close();

	// Elevate privileges
	if (setuid( 0 ) != 0)
	{
		return -1;
	}

	// Execute commands
	if (std::strcmp( argv[1], "acquire-snapshot" ) == 0 && argc == 4)
	{
		const char * dbUser = argv[2];
		const char * dbPass = argv[3];
		const std::string dataDir = configs["snapshot"]["datadir"];
		const std::string lvGroup = configs["snapshot"]["lvgroup"];
		const std::string lvOrigin = configs["snapshot"]["lvorigin"];
		const std::string lvSnapshot = configs["snapshot"]["lvsnapshot"];
		const std::string lvSizeGB = configs["snapshot"]["lvsizegb"];

		std::string cmd;

		MYSQL sql;

		if (mysql_init( &sql ) == NULL)
		{
			return -1;
		}

		if (!mysql_real_connect( &sql, "localhost", dbUser, dbPass,
			NULL, 0, NULL, 0 ))
		{
			return -1;
		}

		cmd = "FLUSH TABLES WITH READ LOCK";
		if (mysql_real_query( &sql, cmd.c_str(), cmd.length() ) != 0)
		{
			return -1;
		}

		cmd = "lvcreate -L" + lvSizeGB + "G -s -n " + lvSnapshot +
			" /dev/" + lvGroup + "/" + lvOrigin;
		if (std::system( cmd.c_str() ) != 0)
		{
			mysql_close( &sql );
			return -1;
		}

		cmd = "UNLOCK TABLES";
		int status = mysql_real_query( &sql, cmd.c_str(), cmd.length() );
		mysql_close( &sql );
		if (status != 0)
		{
			return -1;
		}

		cmd = "mount /dev/" + lvGroup + "/" + lvSnapshot + " /mnt/" +
			lvSnapshot + "/";
		if (std::system( cmd.c_str() ) != 0)
		{
			return -1;
		}

		std::string snapshotFiles( "/mnt/" + lvSnapshot + "/" + dataDir );

		// Relax permissions so we can take ownership of the backup files,
		// this makes sending and consolidating easier on the snapshot machine
		cmd = "chmod -R 755 " + snapshotFiles;
		if (std::system( cmd.c_str() ) != 0)
		{
			return -1;
		}

		printf( "%s\n", snapshotFiles.c_str() );
	}
	else if (std::strcmp( argv[1], "release-snapshot" ) == 0 && argc == 2)
	{
		const std::string lvGroup = configs["snapshot"]["lvgroup"];
		const std::string lvSnapshot = configs["snapshot"]["lvsnapshot"];

		std::string cmd;
		bool isOK = true;

		cmd = "umount /mnt/" + lvSnapshot + "/";
		if (system( cmd.c_str() ) != 0)
		{
			isOK = false;
		}

		cmd = "lvremove -f /dev/" + lvGroup + "/" + lvSnapshot;
		if (system( cmd.c_str() ) != 0)
		{
			isOK = false;
		}

		return isOK ? 0 : -1;
	}
	else
	{
		return -1;
	}

	return 0;
}
