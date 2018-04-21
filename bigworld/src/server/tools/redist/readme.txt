Compiling Python eggs from source files
===============================================================================

To recompile and create egg files from these packages, run the following
script:

	./create_egg.sh <package_file_name>

Example:

	./create_egg.sh MySQL-python-1.2.2.tar.gz

The output will be generated in "mf/bigworld/tools/server/pycommon/redist".

****************************************
NOTE: Before running this script make sure you remove the corresponding egg if
it already exists, in "mf/bigworld/tools/server/pycommon/redist".

Otherwise the easy_install script will abort without generating a new package.

For example, if you need to recompile the MySQL-python package, make sure you
remove all .egg files in "mf/bigworld/tools/server/pycommon/redist" which
start with "MySQL-python".
*****************************************

The script "create_egg.sh" really just forwards to
mf/bigworld/tools/server/pycommon/redist/setuptools-0.6c5/easy_install.py, so
you could use that script directly too if you wish.

EXTRA NOTES
===============================================================================

Binaries compiled on FC6 aren't backwards compatible with FC5 or Debian Sarge.
To get around this, the script appends the following option to CFLAGS after
checking whether gcc supports it:

-Wl,--hash-style=both

This shouldn't cause any problems, but if you wish to disable this
behaviour then comment out the appropriate line in create_egg.sh.

(This doesn't affect the environment variables of the shell which executed
this script, only the environment with which easy_install.py is called
inside the script).
