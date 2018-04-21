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
#include <stdlib.h>

#ifndef _WIN32  // WIN32PORT

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <pthread.h>

#else // ifndef _WIN32  // WIN32PORT

#include <windows.h>

#endif //ndef _WIN32  // WIN32PORT


// public definitions
#include "deem.hpp"

// private definitions
void * deemHandleConnectionThreadEntry(void *args);
void deemSignal(int sig);


int			gDeemSockme;	// listening socket
int			gDeemAllover;	// global dead flag
pthread_t	gDeemDone;		// local dead flag

// implementations

int deemInit(unsigned short port)
{
	sockaddr_in	sin;
	int			val;

	gDeemSockme = socket(AF_INET,SOCK_STREAM,0);
	if(gDeemSockme==-1)
	{
		perror("deem: socket() failed");
		return -1;
	}

	val = 1;
	setsockopt(gDeemSockme,SOL_SOCKET,SO_REUSEADDR,&val,sizeof(int));

	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = 0;
	if(bind(gDeemSockme,(sockaddr*)&sin,sizeof(sin)))
	{
		perror("deem: bind() failed");
		return -1;
	}

	if(listen(gDeemSockme,1))
	{
		perror("deem: listen() failed");
		return -1;
	}

	return 0;
}


// so we don't have to pass it to each thread...
DeemHandlerProc gDeemHandler;
void			*gDeemUarg;

void deemRun(DeemHandlerProc handler, void *arg)
{
	fd_set		sele;
	int			serial = 0;

	gDeemHandler = handler;
	gDeemUarg = arg;

	signal(SIGPIPE,deemSignal);
	signal(SIGINT,deemSignal);
	signal(SIGTERM,deemSignal);
	signal(SIGHUP,NULL);	// we can live without a terminal

	gDeemDone = (pthread_t)-1;
	gDeemAllover = 0;
	while(!gDeemAllover)
	{
		pthread_t	tid;
		sockaddr	you;
		int			ts;
		socklen_t	len;
		int			*args;

		fprintf(stderr,"deem: Waiting for connection.\n");

		FD_ZERO(&sele);
		FD_SET(gDeemSockme,&sele);
		if(select(gDeemSockme+1,&sele,NULL,NULL,NULL)<1)
		{
			if(gDeemAllover) break;
			perror("deem: select() failed");
			return;
		}

		fprintf(stderr,"deem: Got a connection.\n");

		len = sizeof(sockaddr);
		ts = accept(gDeemSockme,&you,&len);
		if(ts==-1)
		{
			perror("deem: accept() failed");
			return;
		}

		args = (int *)malloc(sizeof(int)*2);
		args[0] = ts;
		args[1] = serial++;
		pthread_create(&tid,NULL,deemHandleConnectionThreadEntry,args);
		pthread_detach(tid);	// not interested in you anymore
	}
	close(gDeemSockme);
}


void * deemHandleConnectionThreadEntry(void *args)
{
//	void	*uarg;
	int		ts,		serial;

	ts 		= ((int*)args)[0];
	serial  = ((int*)args)[1];
	free(args);

	deemSetDone(0);

	(*gDeemHandler)(ts,serial,gDeemUarg);
	
	close(ts);
	return 0;
}



// yes, this isn't a particularly robust way of doing 'done'.
// hopefully it'll work for our purposes 'tho.
int deemGetDone(void)
{
	if(gDeemAllover)
		return 1;
	else
		return gDeemDone == pthread_self();
}

void deemSetDone(int done)
{
	if(done)
	{
		gDeemDone = pthread_self();
	}
	else
	{
		if(gDeemDone == pthread_self())
		{
			gDeemDone = (pthread_t)-1;
		}
	}
}

void deemSignal(int sig)
{
	deemSetDone(1);
	if(sig!=SIGPIPE) gDeemAllover = 1;
}
