/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 * 	This program uses the EntityLoader class to load all entities from a scene
 * 	graph into a running server.
 */

#ifdef _WIN32
#pragma warning ( disable: 4786 )
#endif

#include "Python.h"		// See http://docs.python.org/api/includes.html

#include <string>

#include "network/nub.hpp"
#include "resmgr/datasection.hpp"
#include "resmgr/bwresource.hpp"
#include "entity_loader.hpp"
#include "entitydef/constants.hpp"

DECLARE_DEBUG_COMPONENT(0)

static char USAGE[] =
"Usage: eload [options] <project>\n"
"  where <project> is an XML 'chunk' file containing entities.\n"
"  See fantasydemo/res/server/projects for examples of such files\n"
"\n"
"Options:\n"
" -u|--uid <n>       Override the uid of the server to connect to\n"
" --sleep <millis>   Delay between loading each entity (default: 10)\n"
" --cell             Create entities on the cell instead of the base\n"
" --res <res-path>   Specify the BW_RES_PATH to use\n"
" --space <id>       Specify the space ID to load the entities on\n"
"\n"
"NOTE: Since BigWorld 1.7, BigBang/WorldEditor has supported the placement of\n"
"      entities in chunks, which can then be loaded by the server when the space\n"
"      is created.  That was the purpose of this tool, and such, it is now\n"
"      deprecated.  The suggested method for interactively loading entities on\n"
"      a running server is using `runscript` or calling a loading function\n"
"      on the server using a python telnet console (see the 'pyconsole'\n"
"      section of `control_cluster.py --help` for more info)\n";

void printUsage()
{
	printf( USAGE );
}

extern bool g_shouldWriteToConsole;

extern int PyPatrolPath_token;
static int s_moduleTokens = PyPatrolPath_token;
extern int PyUserDataObject_token;
extern int UserDataObjectDescriptionMap_Token;
static int s_udoTokens = PyUserDataObject_token | UserDataObjectDescriptionMap_Token;


int main( int argc, char * argv[] )
{
	g_shouldWriteToConsole = true;

	BWResource::init( argc, (const char **)argv );
	char * projectName = NULL;

	if(argc < 2)
	{
		printUsage();
		return 1;
	}

	EntityLoader::Component component = EntityLoader::ON_BASE;
	SpaceID spaceID = 0;
	int sleepTime = 10;

	for (int i = 1; i < argc; i++)
	{
		if (strcmp( argv[i], "-h" ) == 0 || strcmp( argv[i], "--help" ) == 0)
		{
			printUsage();
			return 0;
		}

		else if (!strcmp( argv[i], "-UID" ) || !strcmp( argv[i], "-u" )
			|| !strcmp( argv[i], "--uid" ))
		{
			if (++i < argc)
			{
				size_t buflen = strlen( argv[i-1] ) + strlen( argv[i] ) + 2;
				char * buf = new char[ buflen ];

				printf( "Setting %s to %s\n", argv[ i - 1] + 1, argv[i] );
				bw_snprintf( buf, buflen, "%s=%s", argv[i - 1] + 1, argv[i] );
				putenv( buf );
			}
			else
			{
				printUsage();
				return 1;
			}
		}
		else if (!strcmp( argv[i], "-sleep" ) || !strcmp( argv[i], "--sleep" ))
		{
			if (++i < argc)
			{
				sleepTime = atoi(argv[i]);
			}
			else
			{
				printUsage();
				return 1;
			}
		}
		else if (!strcmp( argv[i], "-cell" ) || !strcmp( argv[i], "--cell" ))
		{
			component = EntityLoader::ON_CELL;
		}
		else if (!strcmp( argv[i], "-space" ) || !strcmp( argv[i], "--space" ))
		{
			if (++i < argc)
			{
				spaceID = atoi(argv[i]);
			}
			else
			{
				printUsage();
				return 1;
			}
		}
		else if (strcmp( argv[i], "--res" )==0)
		{
			i++;
			continue;
		}
		else if (projectName == NULL)
		{
			projectName = argv[i];
		}
		else
		{
			printUsage();
			return 1;
		}
	}

	if (projectName == NULL)
	{
		printUsage();
		return 1;
	}

	EntityLoader entityLoader( component, spaceID, sleepTime );
	DataSectionPtr pSection;

	Script::init( EntityDef::Constants::databasePath(),
				"database" );
	if (!entityLoader.startup())
		return false;

	pSection = BWResource::openSection( projectName );

	if(!pSection)
	{
		ERROR_MSG("Can't open scene graph '%s'\n", projectName );
		return 0;
	}

	entityLoader.loadScene( pSection );

	Script::fini();
	return 0;
}
