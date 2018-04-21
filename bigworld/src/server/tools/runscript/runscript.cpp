/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


#include "cstdmf/debug.hpp"
#include "network/mercury.hpp"
#include "cellappmgr/cellappmgr_interface.hpp"
#include "baseappmgr/baseappmgr_interface.hpp"

DECLARE_DEBUG_COMPONENT(0)

int main(int argc, char* argv[])
{
	dprintf( "NOTICE: runscript is deprecated and should no longer be used.\n"
			"It will be removed in a future release.\n\n" );

	CellAppMgrInterface::shouldOffloadArgs args;
	Mercury::Nub nub;
	Mercury::Address addr;
	Mercury::Bundle b1, b2, b3;
	std::string script;
	int n, reason;
	char buf[256];
	FILE* pFile = stdin;

	bool onBase = false;
	int8 broadcast = 0;
	int space = 0;

	for ( int i=1; i<argc; i++ )
	{
		if (!strcmp( argv[i], "-h" ) ||
			!strcmp( argv[i], "--help" ))
		{
			dprintf(
	"\n"
	"Usage: runscript [options] [script-name]\n"
	"Runs the Python script specified by script-name on server components.\n"
	"If no script-name is specified, standard input is read.\n"
	"options:\n"
	" -h, --help    Print this message and exit.\n"
	" -base         Execute script on BaseApp. By default the least loaded\n"
	"                BaseApp is used.\n"
	" -cell         Execute script on CellApp. By default the least loaded\n"
	"                CellApp that has a space allocated to it is used.\n"
	" -all          Modifies the -base or -cell default to execute script\n"
	"                on all BaseApps and CellApps.\n"
	" -space SpaceID Modifies -cell to execute only in the specified space\n"
	"                on a cell\n");
			exit(0);
		}
		else if ( !strcmp( argv[i], "-base" ) )
		{
			onBase = true;
		}
		else if ( !strcmp( argv[i], "-cell" ) )
		{
			onBase = false;
		}
		else if ( !strcmp( argv[i], "-all" ) )
		{
			broadcast |= 1;
		}
		else if ( !strcmp( argv[i], "-backups" ) )
		{
			broadcast |= 2;
		}
		else if ( !strcmp( argv[i], "-space" ) )
		{
			if ( ++i == argc )
			{
				printf( "must specify a space-id when using -space\n" );
				return 1;
			}

			space = atoi( argv[i] );
		}
		else
		{
			if ( pFile != stdin )
			{
				printf( "can only run one file per run-script\n" );
				fclose( pFile );
				return 1;
			}
			pFile = fopen( argv[i], "r" );
			if ( !pFile )
			{
				printf( "Failed to open %s\n", argv[i] );
				return 1;
			}
		}
	}

	if ( onBase )
	{
		reason = nub.findInterface("BaseAppMgrInterface", 0, addr);
	}
	else
	{
		reason = nub.findInterface("CellAppMgrInterface", 0, addr);
	}

	if(reason != 0)
	{
		ERROR_MSG( "Failed to find cellappmgr, reason %d\n", reason );
		return 1;
	}

	// -backup is only valid for baseapp scripts
	if ((!onBase) && (broadcast & 2))
	{
		ERROR_MSG( "Option -backup can only be used on BaseApps. "
					"Try using -base.\n" );
		return 1;
	}

	if ( pFile == stdin )
	{
		dprintf( "\nAccepting input from stdin...\n" );
	}

	do
	{
		n = fread(buf, 1, sizeof(buf), pFile);

		if(n < 0)
		{
			perror("fread");
			return 1;
		}

		script.append(buf, n);
	}
	while(!feof(pFile));

	if(pFile != stdin)
	{
		fclose(pFile);
		pFile = NULL;
	}

	// Replace '\r' characters from Windows with nonharmful '\n',
	// otherwise server components report syntax error of scripts
	std::replace( script.begin(), script.end(), '\r', '\n' );

	// No need to lock if its a single cell script.

	if (onBase)
	{
		if (space)
		{
			printf( "warning: space=%d makes no sense for a base script\n",
					space );
		}

		printf("Executing script..\n");
		b2.startMessage( BaseAppMgrInterface::runScript );
		b2 << broadcast;
		b2 << script;
		nub.send(addr, b2);
#ifdef unix
		usleep(500000);
#else
		Sleep(500);
#endif
	}
	else
	{
		if(script.find("single-cell-only") != (unsigned int)-1)
		{
			if (broadcast)
			{
				printf( "can't broadcast a single-cell-only script" );
				return 1;
			}
		}

		if (broadcast)
		{
			// need to lock cells to ensure each entity is visited once
			// - we don't want entities in transit to be visited twice
			printf("Locking cells..\n");
			args.enable = false;
			b1 << args;
			nub.send(addr, b1);
#ifdef unix
			usleep(500000);
#else
			Sleep(500);
#endif
		}

		printf("Executing script..\n");
		b2.startMessage( CellAppMgrInterface::runScript );
		b2 << broadcast;
		b2 << space;
		b2 << script;
		nub.send(addr, b2);
#ifdef unix
		usleep(500000);
#else
		Sleep(500);
#endif

		if (broadcast)
		{
			printf("Unlocking cells..\n");
			args.enable = true;
			b3 << args;
			nub.send(addr, b3);
#ifdef unix
			usleep(500000);
#else
			Sleep(500);
#endif
		}
	}

	return 0;
}
