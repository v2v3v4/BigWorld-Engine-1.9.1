/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


// what is it with unix and structures?
typedef struct timeval timeval;
typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;

// prototype for deem connection handlers.
// connection handlers run in their own thread, and they should
// NOT attempt to close the socket they are given (shutdown is ok)
typedef void (*DeemHandlerProc)(int socket, int serial, void *arg);

// --- Functions called by the main program ---

// do initialisations. returns -1 on error
int deemInit(unsigned short port);

// run it. returns when it gets a signal.
void deemRun(DeemHandlerProc handler, void *arg);

// --- Functions called by connection handlers ---

// see if your connection is over
int deemGetDone(void);

// make your connection over.
void deemSetDone(int done);
