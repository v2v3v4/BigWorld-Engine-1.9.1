/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "network/portmap.hpp"
#include "network/endpoint.hpp"
#include "network/machine_guard.hpp"
#include "network/mercury.hpp"


char * addr2str(u_int32_t nhost);
int usage();

struct MachineInfo
{
	std::string	wholeMachine;
	std::string serverComponents;
};
typedef std::map<u_int32_t,MachineInfo> MachineInfos;

typedef std::vector<std::string> StrVec;
StrVec getMachinesForComponent( const MachineInfos &mis, const char *tag );
void showAllComponents( const MachineInfos &mis, const char *tag,
						const char *procname );

bool verbose = false;
int uid = 0;
MachineInfos mis;

class WMMHandler : public MachineGuardMessage::ReplyHandler
{
	virtual bool onWholeMachineMessage( WholeMachineMessage &wmm, uint32 addr )
	{
		char linebuf[256];
		char * line = linebuf;
		line += sprintf( line, "We have %-8s at %-11s",
			wmm.hostname_.c_str(), addr2str( addr ) );
		if (verbose)
		{
			line += sprintf( line, " using %d%% cpu %d%% mem",
				(wmm.cpuLoads_[0]*100)/256, (wmm.mem_*100)/256 );
		}
		if (wmm.nCpus_ > 1)
		{
			line += sprintf( line,": cpu at %d x %d\n",
				wmm.cpuSpeed_, wmm.nCpus_ );
		}
		else if (wmm.nCpus_ != 0)
		{
			line += sprintf( line,": cpu at %d\n", wmm.cpuSpeed_ );
		}
		else
		{
			line += sprintf( line,".\n" );
		}

		if (!verbose)
			printf( "%s", linebuf );
		else
			mis[ addr ].wholeMachine = linebuf;

		return true;
	}
};

class PSMHandler : public MachineGuardMessage::ReplyHandler
{
	virtual bool onProcessStatsMessage( ProcessStatsMessage &psm, uint32 addr )
	{
		if ((uid == 0 || psm.uid_ == uid) && psm.pid_ != 0)
		{
			char linebuf[256];
			bw_snprintf( linebuf, sizeof( linebuf ),
				"\tRunning %-24s under uid %3d "
				"using %d%% cpu %d%% mem\n",
				psm.name_.c_str(), psm.uid_,
				psm.cpu_*100/256, psm.mem_*100/256 );
			mis[ addr ].serverComponents += linebuf;
		}

		return true;
	}
};

int main(int argc, char *argv[])
{
	bool byComponent = false;

	if (argc > 1)
	{
		if (!strcmp( argv[1], "-v" ))
		{
			verbose = true;
		}
		else if (!strcmp( argv[1], "-u"))
		{
			if (argc > 2)
			{
				// TODO: Should do better arg checking.
				uid = atoi( argv[2] );
			}
			else
			{
				uid = getUserId();
			}

			verbose = true;
		}

		// Dirty little hack to permit output sorted into component categories
		else if (!strcmp( argv[1], "-c" ))
		{
			verbose = byComponent = true;
			uid = getUserId();
		}

		else
			return usage();
	}

	// Get machines
	WholeMachineMessage wmm;
	WMMHandler wmmHandler;
	wmm.sendAndRecv( 0, BROADCAST, &wmmHandler );

	if (verbose)
	{
		ProcessStatsMessage psm;
		psm.param_ = psm.PARAM_USE_CATEGORY;
		psm.category_ = psm.SERVER_COMPONENT;

		PSMHandler psmHandler;
		psm.sendAndRecv( 0, BROADCAST, &psmHandler );
	}

	if (byComponent)
	{
		// Dirty hack to prevent execution of loop below
		verbose = false;

		// Find world server
		StrVec worlds = getMachinesForComponent( mis, "LoginInterface" );
		if (worlds.size() == 1)
			printf( "world server is %s\n", worlds[0].c_str() );
		else if (worlds.size() == 0)
			printf( "no world server found!\n" );
		else
			printf( "WARNING: multiple world servers found!\n" );

		// Cells, bases, and bots
		showAllComponents( mis, "CellAppInterface", "cellapp" );
		showAllComponents( mis, "BaseAppIntInterface", "baseapp" );
		showAllComponents( mis, "ClientInterface", "bot" );
	}

	if (verbose)
	{
		MachineInfos::iterator it;
		for (it = mis.begin(); it != mis.end(); it++)
		{
			if (it->second.wholeMachine.empty())
				printf( "Unknown machine.\n" );

			if ((uid == 0) || !it->second.serverComponents.empty())
			{
				printf( "%s%s", it->second.wholeMachine.c_str(),
					it->second.serverComponents.c_str() );
			}
		}
	}

	return 0;
}

StrVec getMachinesForComponent( const MachineInfos &mis, const char *tag )
{
	StrVec ret;
	char buf[256];

	MachineInfos::const_iterator it;
	for (it = mis.begin(); it != mis.end(); it++)
		if ((int)it->second.serverComponents.find( tag ) >= 0 &&
			sscanf( it->second.wholeMachine.c_str(), "We have %s", buf ))
			ret.push_back( std::string( buf ) );

	return ret;
}

void showAllComponents( const MachineInfos &mis, const char *tag,
						const char *procname )
{
	StrVec machines = getMachinesForComponent( mis, tag );
	if (machines.empty())
		printf( "no %ss found!\n", procname );
	else
	{
		printf( "%ss (%zu) on ", procname, machines.size() );
		for (uint i=0; i < machines.size(); i++)
			printf( "%s ", machines[i].c_str() );
		printf( "\n" );
	}
}


typedef unsigned char uint8;

char * addr2str(u_int32_t nhost)
{
	static char	gaddr[64];

	u_int32_t host = ntohl(nhost);
	bw_snprintf( gaddr, sizeof(gaddr),
		"%d.%d.%d.%d",
		(int)(uint8)(host>>24),
		(int)(uint8)(host>>16),
		(int)(uint8)(host>>8),
		(int)(uint8)(host));
	return gaddr;
}

static char USAGE[] =
"Usage: mls [-v] [-u <UID>]\n"
"\n"
"List machines on network running machined\n"
"\n"
"Options:\n"
" -v         verbose listing (include components)\n"
" -u         verbose listing limited to the current users UID\n"
" -u <UID>   verbose listing limited to the UID specified\n"
"\n"
"NOTE: Since BigWorld 1.7, the functionality of `mls` (and a whole lot more)\n"
"      has been provided by bigworld/tools/server/control_cluster.py.  This\n"
"      utility now exists primarily as an example of how to use the C++ side\n"
"      of the MachineGuardMessage API in src/lib/network/machine_guard.[ch]pp\n"
"      for talking to bwmachined.  It is no longer being actively developed\n"
"      and should be considered deprecated.\n";

int usage()
{
	puts( USAGE );
	return 1;
}
