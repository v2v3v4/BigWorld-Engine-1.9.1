/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


typedef struct ComponentRecord
{
	unsigned long	ip;
	unsigned short	port;
	short			filler;
	time_t			regat;
	WatcherRegistrationMsg	wrm;
} ComponentRecord;





int main(int argc, char *argv[]);

void deemHandlerEntry(int ts, int serial, void *arg);

void telnetInterface(FILE *stream, int threadSocket, char *str);
void webInterface(FILE *stream, int threadSocket, char *str);
void * parallelReader( void * arg );

void processGetOrSetCommand(FILE *stream, int socket, char *path, char *value);
void processComponentListCommand( FILE *stream, int socket, char * filter );

void combineDirs(char *dst, const char *cwd, char *arg);

void sendPacket(int sd,
				ComponentRecord const *cr,
				int message,
				char *astr,
				char *bstr);

int recvPacket(	int sd,
				ComponentRecord const *cr,
				int message,
				WatcherDataMsg *wdm,
				int len);


void * registrationListenerThreadEntry(void *args);

void addComponent(ComponentRecord & cr);

void gatherExistingWatcherNubs(void);

/**
 *	This function works like strtok, but doesn't use globals,
 *	so it's thread-safe. Warning: This function modifies both
 *	the pointer passed to it and the underlying string. If the
 *	first char of the string passed in is in delim, then the
 *	first token is zero-length.
 *
 *	@param	nextTok	Points to the rest of the string. Make
 *					this the whole string for the first call.
 *	@param	delim	The token separator characters.
 *	@return			Pointer to the next token if there's any
 *					left, otherwise NULL.
 */
char * my_strtok(char *&nextTok, const char *delim);

char * dottedIPAddressString(char *buf, unsigned long networkIP);

// src and dst can be the same
char * decodeHTTPString(char * dst, char * src);

// src and dst shouldn't be the same
char * encodeHTTPString(char * dst, char *src);

// src and dst shouldn't be the same
char * encodeHTMLString(char * dst, char *src);
