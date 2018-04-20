#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>

extern "C"
{

/**
 * This includes implementations of functions that are in the BigWorld modified
 * Python interpreter that aren't in stock standard interpreters (e.g. those used
 * by mod_python and stock standard distribution Python interpreters).
 *
 */

int PyOS_statTypeDefault( const char * filename )
{
	struct stat statbuf;
	if (stat( filename, &statbuf ) != 0) return -1;
	if (S_ISREG( statbuf.st_mode )) return 0;
	if (S_ISDIR( statbuf.st_mode )) return 1;
	return 2;
}

void * PyOS_dlopenDefault( const char * pathname, int flags )
{
	return dlopen( pathname, flags );
}

}
