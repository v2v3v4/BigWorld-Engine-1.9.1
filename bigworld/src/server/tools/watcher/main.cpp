/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include <vector>

#define __USE_REENTRANT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32 // WIN32PORT
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pwd.h>
#include <ctype.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#else //ifndef _WIN32 // WIN32PORT
#endif //ndef _WIN32 // WIN32PORT

#include "network/portmap.hpp"
#include "network/endpoint.hpp"
#include "network/watcher_nub.hpp"
#include "network/machine_guard.hpp"
#include "network/misc.hpp"
#include "deem.hpp"

#include "main.hpp"

// The port the watcher daemon listens on
uint16 g_listenPort = PORT_WATCHER;


std::vector<ComponentRecord>		components;
pthread_mutex_t				componentsLock;

int main( int argc, char *argv[])
{
	int i;
	(void) argc;
	(void) argv;

	pthread_mutex_init(&componentsLock,NULL);

	// Catch manually specified listen ports
	for (i=1; i < argc; i++)
	{
		if (!strcmp( argv[i], "-p" ) || !strcmp( argv[i], "--port" ))
		{
			g_listenPort = (uint16)atoi( argv[++i] );
		}
	}

	// init deem
	fprintf(stderr,"watcher: initialising deem.\n");
	if(deemInit( g_listenPort ))
	{
		fprintf(stderr,"watcher: couldn't initialise daemon.\n");
		exit(1);
	}

	// init reg listener
	fprintf(stderr,"watcher: launching registration listener thread.\n");
	pthread_t	tid;
	pthread_create(&tid,NULL,registrationListenerThreadEntry,NULL);
	pthread_detach(tid);

	// gather any current registrations from every machined
	gatherExistingWatcherNubs();

	// run deem
	fprintf(stderr,"watcher: giving control to deem.\n");
	deemRun(deemHandlerEntry,0);
	fprintf(stderr,"watcher: exiting.\n");
	return 0;
}


void deemHandlerEntry(int ts, int /*serial*/, void * /*arg*/)
{
	FILE	*sock;

	fprintf(stderr,"watcher: got a thread from deem...\n");

	// first open a UDP port just for this interface
	int		threadSocket = socket(AF_INET,SOCK_DGRAM,0);
	if(threadSocket == -1)
	{
		perror("watcher:deemHandlerEntry: socket() failed");
		return;
	}

	sockaddr_in     sin;
	sin.sin_family = AF_INET;
	sin.sin_port = 0;
	sin.sin_addr.s_addr = 0;
	if(bind(threadSocket,(sockaddr*)&sin,sizeof(sin)))
	{
		perror("watcher:deemHandlerEntry: bind() failed");
		return;
	}

	// now find out who we're talking to
	char	accstr[256];
	char	*accptr = accstr;
	const char	*matchstr = "GET /";

	fd_set	fds;
	FD_ZERO(&fds);
	FD_SET(ts,&fds);

	// the timing is the only thing really separating
	// web and telnet ... not very good.
	timeval	tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	while(select(ts+1,&fds,NULL,NULL,&tv) > 0)
	{
		if(recv(ts,accptr++,1,0)!=1)
		{
			close(threadSocket);
			return;
		}
		if(toupper(accptr[-1]) != toupper(*(matchstr))) break;
		if(!*matchstr) break;
		matchstr++;
	}
	*accptr = 0;

	// open the nice file semantics
	sock = fdopen(ts,"a+");	// can't put 'b' in here

	if(!*matchstr)
		webInterface(sock,threadSocket,accstr);
	else
		telnetInterface(sock,threadSocket,accstr);

	close(threadSocket);

	fflush(sock);
	fclose(sock);	// not sure if this closes 'ts' or not (we want it
}					// to not close it!). Maybe I should dup b4 I fdopen.


static const char * telnetDelim = " \f\n\r\t\v=";	// isspace + '='

void telnetInterface(FILE *stream, int threadSocket, char *command)
{
	char	cwd[256];		// always kept in good condition (trailing slash)
	char	path[256];		// temporary variable

	strcpy(cwd,"/");

	fprintf(stream,"Welcome to watcher.\r\n");

	bool firstTime = true;
	while(!deemGetDone())
	{
		char	*curWord, *nextWord;

		if(firstTime)
		{
			if(command)fgets(command+strlen(command),
				256-strlen(command),
				stream);
			firstTime = 0;
		}
		else
		{
			fgets(command,256,stream);
		}

		// find the first word
		curWord = my_strtok(nextWord = command,telnetDelim);
			// if it started with a space that doesn't count
		if(!*curWord) curWord = my_strtok(nextWord,telnetDelim);
			// if there was no command that doesn't count either
		if(!curWord) continue;

		// ok, save that word and go on to the next
		char * firstWord = curWord;
		curWord = my_strtok(nextWord,telnetDelim);

		// figure out what the command was
		if(firstWord[0]=='c')
		{
			combineDirs(path,cwd,curWord);
			strcpy(cwd,path);
			fprintf(stream,"The CWD is now %s\r\n",cwd);
		}
		else if(firstWord[0]=='l')
		{
			combineDirs(path,cwd,curWord);
			processGetOrSetCommand(stream,threadSocket,path,NULL);
		}
		else if(firstWord[0]=='g')
		{
			combineDirs(path,cwd,curWord);
			// this is a get not a list, so remove the trailing '/'
			char *lastPathChar = &path[strlen(path)-1];
			if(*lastPathChar=='/') *lastPathChar=0;

			processGetOrSetCommand(stream,threadSocket,path,NULL);
		}
		else if(firstWord[0]=='s')
		{
			combineDirs(path,cwd,curWord);

			// this is a set not a list, so remove the trailing '/'
			char *lastPathChar = &path[strlen(path)-1];
			if(*lastPathChar=='/') *lastPathChar = 0;

			if(!(curWord = my_strtok(nextWord,telnetDelim)))
			{
				fprintf(stream,"Missing value to set '%s' to\r\n",path);
				continue;
			}

			processGetOrSetCommand(stream,threadSocket,path,curWord);
		}
		else if(firstWord[0]=='q')
		{
			fprintf(stream,"Goodbye.\r\n");
			deemSetDone(1);
		}
		else
		{
			fprintf(stream,"Sorry, I don't understand '%s' at all\r\n",command);
		}
	}
	fprintf(stream,".rehctaw ot emocleW\r\n");
}


void combineDirs(char *dst, const char *cwd, char *arg)
{
	if(!arg)
	{
		strcpy(dst,cwd);
		return;
	}

	char *dnd = dst;
	if(arg[0]!='/')
	{
		strcpy(dst,cwd);
		dnd+=strlen(dnd);
	}
	else
	{
		strcpy(dst,"/");
		dnd++;
		arg++;
	}

	char	*curTok,	*nextTok;
	nextTok = arg;

	while((curTok = my_strtok(nextTok,"/")))
	{
		if(curTok[0]=='.' && curTok[1]=='.')
		{
			if(dnd-dst!=1)
			{
				*(dnd-1)=0;
				char *back = strrchr(dst,'/');
				if(back) {dnd=back+1; *dnd=0;}
			}
		}
		else
		{
			strcat(dnd,curTok);
			strcat(dnd,"/");
			dnd+=strlen(curTok)+1;
		}
	}
	// wow that was far harder than it ought to have been.
}

static const char * webDelim = " \f\n\r\t\v";

struct ReaderArgs
{
	int		source;
	char	* bigbuf;
	int		* buflen;
};

void webInterface(FILE *stream, int threadSocket, char *str)
{
	fgets(str+strlen(str),256-strlen(str),stream);

	char * reqOperation = my_strtok(str,webDelim);
	char * reqPath = my_strtok(str,webDelim);
	char * reqFormat = my_strtok(str,webDelim);

	// make sure it's a GET from a web browser... (stuff POSTs)
	if(	!reqOperation || strcasecmp(reqOperation,"GET") ||
		!reqPath || reqPath[0]!='/' ||
		!reqFormat || (strcasecmp(reqFormat,"HTTP/1.0") &&
			strcasecmp(reqFormat,"HTTP/1.1")) )
		{
		// whoops! Very unlikely this.
		telnetInterface(stream,threadSocket,str);
		return;
		}

	// ok, now process the request
	char * value;
	if((value = strchr(reqPath,'?')))
	{
		*(value++) = 0;
		if(value[0]!='v' || value[1]!='=')
			value = NULL;
		else
		{
			value += 2;
			decodeHTTPString(value,value);
		}
	}

	char path[256];
	decodeHTTPString(reqPath,reqPath);
	combineDirs(path,"/",reqPath);	// vanilla-flavoured paths here thanks
	char *lastPathChar = &path[strlen(path)-1];
	if(*lastPathChar=='/') *lastPathChar = 0;


	int two[2];
	pipe(two);
	FILE *fakeFile = fdopen(two[1],"a");
	fprintf(fakeFile,
		"<HTML><HEAD><TITLE>Watcher Output for '%s'</TITLE></HEAD>\r\n",
		path);
	fprintf(fakeFile,"<BODY>\r\n");

	// Start the parallel reader. We have to read in parallel or else
	// the pipe will block when it gets full! This bug took ages to find.
	char	bigbuf[65536];
	int		buflen = 0;
	ReaderArgs	rargs = { two[0], bigbuf, &buflen };
	pthread_t	parallelReaderTid;
	pthread_create( &parallelReaderTid, NULL, parallelReader, &rargs );

	processGetOrSetCommand(fakeFile,-threadSocket,path,value);

	fprintf(fakeFile,"</BODY></HTML>\r\n");
	fclose(fakeFile);
	close(two[1]);

	// Wait for the parallel reader to finish
	pthread_join( parallelReaderTid, NULL );

	close(two[0]);

	fprintf(stream,"HTTP/1.0 200 OK\r\n");
	fprintf(stream,"Server: Watcher Debug Server (Giles)\r\n");
	fprintf(stream,"Pragma: No-cache\r\n");
	fprintf(stream,"Expires: -1\r\n");
	fprintf(stream,"Content-type: text/html\r\n");
	fprintf(stream,"Content-length: %d\r\n\r\n",buflen);
	fflush(stream);
	fwrite(bigbuf,1,buflen,stream);
}

void * parallelReader( void * arg )
{
	ReaderArgs * ra = (ReaderArgs*)arg;

	int		readone;
	while ((readone = read(ra->source,ra->bigbuf+(*ra->buflen),256)) > 0)
		(*ra->buflen) += readone;

	return NULL;
}


void processGetOrSetCommand(FILE *stream, int socket, char *path, char *value)
{
	// ASSERT(path[0]=='/' && path[1]!='/') // ok for path[1] to be 0 'tho.
	path++;	// skip initial '/'

	char * look = my_strtok( path, "/" );
	char * addrStr = my_strtok( path, "/" );

	if (!addrStr)	// see if we're at the root
	{
		if(!value)
			processComponentListCommand( stream, socket, look );
		else
			fprintf(stream,"Can't set the root directory to anything.\r\n%s",
				socket<0?"<p>":"");
		return;
	}

	uint dip[4];
	int port;
	sscanf( addrStr, "%d.%d.%d.%d:%d",
			&dip[0],
			&dip[1],
			&dip[2],
			&dip[3],
			&port );
	uint32 ip = (dip[0] << 24)|(dip[1] << 16)|(dip[2] << 8)|dip[3];
	ip = ntohl( ip );
	port = ntohs( port );

	// see if we can find the desired object
	ComponentRecord	found;
	int 			i;

	pthread_mutex_lock(&componentsLock);
	for(i=0;i<(int)components.size();i++)
	{
		if(!strcasecmp(components[i].wrm.abrv,look) &&
				(components[i].ip == ip) &&
				(components[i].port == port))
		{
			found = components[i];
			break;
		}
	}
	pthread_mutex_unlock(&componentsLock);
	if(i>=(int)components.size())
	{	// no such component
		fprintf(stream,
			"Component '%s' is not registered here.\r\n%s",
			look,
			socket<0?"<p>":"");
		return;
	}

	// ask this component
	sendPacket(	socket < 0 ? -socket : socket,
				&found,
				value ? WATCHER_MSG_SET : WATCHER_MSG_GET_WITH_DESC,
				path,
				value );
	fprintf(stream,
		"Sent transmission to object '%s' on component '%s'.\r\n%s",
		path?path:"",
		look,
		socket<0?"<br>":"");

	// wait for the reply
	char	*bigbuf = new char[65536];
	int		count;
	count = recvPacket(	socket < 0 ? -socket : socket,
						&found,
						WATCHER_MSG_TELL,
						(WatcherDataMsg*)bigbuf,
						65536);

	// if it was bad then say so
	if(count < 0)
	{
		fprintf(stream,
			"Request got a packet error (%s). Deregistering component.\r\n%s",
			count==-1?"general":"timeout",socket<0?"<p>":"");

		// try to remove it
		pthread_mutex_lock(&componentsLock);

		for(i=0;i<(int)components.size();i++)
		{
			if(!strcasecmp(components[i].wrm.abrv,look)) break;
		}
		if(i<(int)components.size()) components.erase(components.begin() + i);
		else
		{
			fprintf(stream,
				"Whoops - component already gone! "
					"(that explains the error then).\r\n%s",
				socket<0?"<p>":"");
		}
		pthread_mutex_unlock(&componentsLock);
	}
	else
	{
		// output the reply since it's good
		fprintf(stream,"Received reply:\r\n%s",
			socket<0?"<p>":"");

		if(socket<0)
		{
			char	lpath[256],	lpathenc[300];

			if(!path || !*path)
				strcpy(lpath,"");
			else
			{
				bw_snprintf( lpath, sizeof(lpath),
					"%s/%s/%s", look, addrStr, path );
				char *lpp;
				if((lpp=strrchr(lpath,'/'))) lpp[1]=0;
			}

			encodeHTTPString(lpathenc,lpath);
			fprintf(stream,"<hr><p><h3><a href=/%s>Back</a></h3><p>"
				"<table border=0 cellpadding=3>\r\n", lpathenc);
		}

		char	*astr = ((WatcherDataMsg*)bigbuf)->string;
		if (count)
		{
			fprintf( stream, "<tr><th>Type</th><th>Name</th>"
						"<th>Value</th><th>Description</th></tr>\n" );
		}
		for(i=0;i<count;i++)
		{
			char *bstr = astr + strlen(astr)+1;
			char *descstr = bstr + strlen(bstr)+1;

			if(socket>=0)
			{
				fprintf(stream,"'%s' = '%s'\r\n",astr,bstr);
			}
			else
			{
				char aencval[256], bencval[256], dencval[256];
				char *aend;

				// get rid of trailing slash if any
				if (astr[0] && astr[strlen(astr)-1] == '/')
					astr[strlen(astr)-1] = 0;

				// only interested in the file name
				//  (for display, and we use relative links)
				if ( (aend = strrchr(astr,'/')) )
					aend++;
				else
					aend = astr;

				// do some encoding
				encodeHTTPString(aencval,aend);
				encodeHTMLString(bencval,bstr);
				encodeHTMLString(dencval,descstr);

				if(!strcmp(bstr,"<DIR>"))
				{
					fprintf(stream,
							"<tr>"
							"<td>[Dir]</td>"
							"<td><a href=%s/>%s</a></td><td>%s</td></tr>",
							aencval,aend,dencval);
				}
				else
				{
					// print up a form if there's only one element
					if(count == 1)
					{
						fprintf(stream,"<tr>"
							"<td>[edit]</td>"
							"<td>%s</td>"
							"<td><form action=\"%s\" method=\"get\">"
							"<input name=v size=50 value=\"%s\">"
							"</form></td><td>%s</td></tr>",
						aend,aencval,bencval,dencval);
					}
					else
					{
						fprintf(stream,"<tr>"
								"<td>[value]</td>"
								"<td><a href=%s>%s</a></td><td>%s</td><td>%s</td></tr>",
								aencval,aend,bencval,dencval);
					}
				}
			}

			astr = descstr + strlen(descstr)+1;
		}

		if(socket<0)
		{

			fprintf(stream,
			"</table><p>\r\n");
		}
	}

	delete [] bigbuf;
}





void processComponentListCommand( FILE *stream, int socket, char * filter )
{
	struct passwd *pw = getpwuid(getUserId());

	fprintf(stream,
		"%sListing of root directory for %s (uid %d)%s\r\n",
		socket<0?"<h2>":"",
		pw?pw->pw_name:"[unknown]",
		getUserId(),
		socket<0?"</h2><p><hr><p>":":\r\n"
		);

	pthread_mutex_lock(&componentsLock);

	if(!components.size())
	{
		fprintf(stream,"No components registered.\r\n%s",
			socket<0?"<p>":"");
	}
	else if(socket>=0)
	{
		for(int i=0;i<(int)components.size();i++)
		{
			if (!filter || (strcmp( components[i].wrm.abrv, filter ) == 0))
			{
				char	regstr[256],	*regend;
				strcpy(regstr,ctime(&components[i].regat));
				if ( (regend = strchr(regstr,'\n')) != NULL )
					*regend = 0;
				fprintf(stream,"%s\t%s, since %s\r\n",
					components[i].wrm.abrv,
					components[i].wrm.name,
					regstr);	// ctime ok as we're in componentsLock
				char	ipbuf[32];
				fprintf(stream,"\t(ID %d, Address %s:%d)\r\n",
					components[i].wrm.id,
					dottedIPAddressString(ipbuf,components[i].ip),
					(int)htons(components[i].port)
					);
			}
		}
	}
	else
	{
		fprintf(stream,"<table border=0 cellpadding=5>");
		fprintf(stream,"<tr><th>Type</th><th>Abbreviation</th><th>Name</th>"
			"<th>ID</th><th>Address</th><th>Registration Date</th></tr>\r\n");
		for(int i=0;i<(int)components.size();i++)
		{
			if (!filter || (strcmp( components[i].wrm.abrv, filter ) == 0))
			{
				char	ipbuf[32];
				fprintf(stream,"<tr>");
				fprintf(stream,
					"<td>[Dir]</td>"
						"<td><a href=%s/%s:%d/>%s</a></td><td>%s</td>",
					components[i].wrm.abrv,
					dottedIPAddressString( ipbuf, components[i].ip),
					htons( components[i].port ),
					components[i].wrm.abrv,
					components[i].wrm.name);
				fprintf(stream,"<td>%d</td><td>%s:%d</td>",
					components[i].wrm.id,
					dottedIPAddressString(ipbuf,components[i].ip),
					(int)htons(components[i].port)
					);
				fprintf(stream,"<td>%s</td>",ctime(&components[i].regat));
				fprintf(stream,"</tr>\r\n");
			}
		}
		fprintf(stream,"</table>\r\n");
	}
	pthread_mutex_unlock(&componentsLock);

}





void sendPacket(int sd,
				ComponentRecord const *cr,
				int message,
				char *astr,
				char *bstr)
{
	char	*pakBuf = new char[4096];			// watch out compiler:
	WatcherDataMsg *wdm = (WatcherDataMsg*)pakBuf;	// aliasing here

	wdm->message = message;
	wdm->count = 1;
	char * pile = wdm->string;

	strcpy(pile,astr?astr:"");
	pile+=strlen(pile)+1;

	strcpy(pile,bstr?bstr:"");
	pile+=strlen(pile)+1;

	int pakLen = pile-pakBuf;

	sockaddr_in	sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = cr->ip;
	sin.sin_port = cr->port;
	int res = sendto(sd,pakBuf,pakLen,0,(sockaddr*)&sin,sizeof(sin));
	if(res <= 0)
		perror("watcher:sendPacket: error from sendto");

	delete [] pakBuf;
}


int recvPacket( int sd,
				ComponentRecord const *cr,
				int message,
				WatcherDataMsg *wdm,
				int len)
{
	// wait for up to 30s to receive the packet
	fd_set		fds;
	FD_ZERO(&fds);
	FD_SET(sd,&fds);

	timeval	tv;
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	int selectResult = select(sd+1,&fds,NULL,NULL,&tv);
	if(selectResult < 0)
	{
		perror("watcher::recvPacket: error from select");
		return -1;
	}
	else if(selectResult == 0)
	{
		fprintf(stderr,"watcher::recvPacket: timeout from select\n");
		return -2;
	}

	// receive a packet
	sockaddr_in	sad;
	socklen_t	sadLen = sizeof(sad);
	int res = recvfrom(sd,wdm,len,0,(sockaddr*)&sad,&sadLen);
	if(res <= 0)
	{
		perror("watcher::recvPacket: error from recvfrom");
		return -1;
	}

	// make sure it fits the bill
	if(sad.sin_addr.s_addr != cr->ip) return -1;
	if(sad.sin_port != cr->port) return -1;
	if(res < (int)sizeof(WatcherDataMsg)) return -1;
	if(wdm->message != message) return -1;

	// and return the count field
	return wdm->count;
}


void * registrationListenerThreadEntry( void * /*args*/ )
{
	int		udpSocket = socket(AF_INET,SOCK_DGRAM,0);
	if(udpSocket == -1)
	{
		perror("watcher:registrationListener: socket() failed");
		return NULL;
	}

	int	val = 1;	// turn on the reuseaddr flag
	int res = setsockopt(udpSocket,SOL_SOCKET,SO_REUSEADDR,&val,sizeof(val));
	if(res) perror("watcher:registrationListener: setsockopt(SO_REUSEADDR) failed");

	val = 1;		// turn on the broadcast flag
	res = setsockopt(udpSocket,SOL_SOCKET,SO_BROADCAST,&val,sizeof(val));
	if(res) perror("watcher:registrationListener: setsockopt(SO_BROADCAST) failed");

	sockaddr_in     sin;
	sin.sin_family = AF_INET;
    sin.sin_port = htons( g_listenPort );
	sin.sin_addr.s_addr = 0;
	if(bind(udpSocket,(sockaddr*)&sin,sizeof(sin)))
	{
		perror("watcher:registrationListener: bind() failed");
		return NULL;
	}

	// OK! We're ready to receive broadcast udp packets! woohoo!
	bool hell_not_frozen_over = true;
	while(hell_not_frozen_over)
	{
		WatcherRegistrationMsg	wrm;
		sockaddr_in				sad;
		socklen_t				sadLen = sizeof(sad);

		res = recvfrom(udpSocket,&wrm,sizeof(wrm),0,
			(sockaddr*)&sad,&sadLen);

		// make sure we got the packet alright
		if(res==-1)
		{
			perror("watcher:registrationListener: problems with recvfrom");
			continue;
		}

		// make sure it's a valid packet
		if(wrm.version!=0)
		{
			fprintf(stderr,
				"watcher:registrationListener: received bad version id %d\n",
				wrm.version);
			continue;
		}

		// make sure this packet is from the system being run by our user id
		if(wrm.uid!=(int)getUserId() && wrm.uid != 0)
		{	// clients use uid 0 = root
			continue;
		}

		int i;
		// find out what kind of message it is
		switch(wrm.message)
		{
		case WATCHER_MSG_REGISTER:
			// ok, let's add another componentrecord then
			ComponentRecord	cr;
			cr.ip = sad.sin_addr.s_addr;
			cr.port = sad.sin_port;
			cr.filler = 0;
			cr.regat = time(NULL);
			cr.wrm = wrm;

			addComponent(cr);

			fprintf(stderr,"watcher: Registered component '%s'\n",
				cr.wrm.abrv);
			break;
		case WATCHER_MSG_DEREGISTER:
			// if there's one there already get rid of it
			pthread_mutex_lock(&componentsLock);

			for(i=0;i<(int)components.size();i++)
			{
				if(components[i].wrm.id == wrm.id &&
					!strcasecmp(components[i].wrm.abrv,wrm.abrv)) break;
			}
			if(i<(int)components.size()) components.erase(components.begin() + i);

			pthread_mutex_unlock(&componentsLock);

			fprintf(stderr,"watcher: Deregistered component '%s'\n",
				wrm.abrv);
			break;
		case WATCHER_MSG_FLUSHCOMPONENTS:
			// this one's easy
			pthread_mutex_lock(&componentsLock);
			components.clear();
			pthread_mutex_unlock(&componentsLock);

			char ipbuf[32];
			fprintf(stderr,
				"watcher: Deregistered all components on orders of %s\n",
				dottedIPAddressString(ipbuf,sad.sin_addr.s_addr));
			break;
		default:
			fprintf(stderr,
				"watcher:registrationListener: received unknown message id %d\n",
				wrm.message);
			break;
		}
	// now go and receive the next one...
	}

	return NULL;
}


void addComponent(ComponentRecord & cr)
{
	int	i;

	// we either replace an existing one or add it at the end
	pthread_mutex_lock(&componentsLock);

	for (i=0; i<(int)components.size(); i++)
	{
		if (components[i].wrm.id == cr.wrm.id &&
			!strcasecmp(components[i].wrm.abrv,cr.wrm.abrv)) break;
	}

	// now add it
	if (i < (int)components.size())
		components[i] = cr;
	else
		components.push_back(cr);

	pthread_mutex_unlock(&componentsLock);
}

class NubHandler : public MachineGuardMessage::ReplyHandler
{
	virtual bool onProcessStatsMessage( ProcessStatsMessage &psm, uint32 addr )
	{
		// now add it to our vector
		ComponentRecord	cr;
		cr.ip = addr;
		cr.port = psm.port_;
		cr.filler = 0;
		cr.regat = time( NULL );
		cr.wrm.version = 0;
		cr.wrm.uid = getUserId();
		cr.wrm.message = WATCHER_MSG_REGISTER;
		cr.wrm.id = psm.id_;
		strcpy( cr.wrm.abrv, psm.name_.c_str() );
		strcpy( cr.wrm.name, psm.name_.c_str() );

		// call common function
		addComponent( cr );

		fprintf( stderr, "watcher: Gathered component '%s'\n",
			cr.wrm.abrv);

		return true;
	}
};

void gatherExistingWatcherNubs(void)
{
	ProcessStatsMessage psm;
	psm.param_ = psm.PARAM_USE_CATEGORY | psm.PARAM_USE_UID;
	psm.category_ = psm.WATCHER_NUB;
	psm.uid_ = getUserId();

	NubHandler handler;
	int reason;
	if ((reason = psm.sendAndRecv( 0, BROADCAST, &handler )) !=
		Mercury::REASON_SUCCESS)
	{
		fprintf( stderr, "gatherExistingWatcherNubs: "
			"psm.sendAndRecv failed (%s)\n",
			Mercury::reasonToString( (Mercury::Reason&)reason ) );
	}
}



// See the big comment on this in the header file
char * my_strtok(char *&nextTok, const char *delim)
{
	// have we run out of tokens?
	if(!nextTok || !*nextTok) return NULL;

	// find the next token
	char * curTok = nextTok;
	nextTok = strpbrk(nextTok,delim);

	// if we found one, skip all separator chars
	if(nextTok)
	{
		*(nextTok++) = 0;
		while(*nextTok && strchr(delim,*nextTok)) nextTok++;
	}

	// return the current token regardless
	return curTok;
}


char * dottedIPAddressString(char *buf, unsigned long networkIP)
{
    ulong ip = ntohl(networkIP);
    sprintf(buf,"%d.%d.%d.%d",
        (int)(unsigned char)(ip>>24),
        (int)(unsigned char)(ip>>16),
        (int)(unsigned char)(ip>>8),
        (int)(unsigned char)(ip));

    return buf;
}


char	hexDigits[]="0123456789ABCDEF";

char * decodeHTTPString(char * dst, char * src)
{
	char	*origDst = dst;

	for(;*src;src++)
	{
		if(src[0]=='%')
		{
			int val = 0;
			char *place;
			if(( place = strchr(hexDigits,toupper(*(++src))) ))
				val += place-hexDigits;
			val <<= 4;
			if(( place = strchr(hexDigits,toupper(*(++src))) ))
				val += place-hexDigits;

			*(unsigned char*)(dst++) = (unsigned char)val;
		}
		else if(src[0]=='+')
			*(dst++) = ' ';
		else
			*(dst++) = src[0];
	}
	*dst = *src;	// i.e. null-terminate

	return origDst;
}

char * encodeHTTPString(char * dst, char *src)
{
	char *origDst = dst;

	for(;*src;src++)
	{
		if(isalnum(src[0]) || src[0]=='/') *(dst++) = src[0];
		else
		{
			*(dst++) = '%';
			*(dst++) = hexDigits[ (((unsigned char*)src)[0] >> 4) ];
			*(dst++) = hexDigits[ (((unsigned char*)src)[0] & 15) ];
		}
	}
	*dst = *src;

	return origDst;
}

typedef struct encodeEquiv
{
	char	c;
	char	str[15];
} encodeEquiv;

encodeEquiv encodeEquivs[] = {
	{'"',"&quot;"},
	{'<',"&lt;"},
	{'>',"&gt;"},
	{'\n',"<br>"},
	{0,""}
};

char * encodeHTMLString(char * dst, char *src)
{
	char *origDst = dst;

	for(;*src;src++)
	{
		encodeEquiv *ee;

		for(ee = encodeEquivs;ee->c;ee++)	// this is NOT efficient I know
		{
			if(src[0] == ee->c) break;
		}

		if(ee->c)
		{
			strcpy(dst,ee->str);
			dst+=strlen(ee->str);
		}
		else
			*(dst++) = src[0];
	}
	*dst = 0;

	return origDst;
}
