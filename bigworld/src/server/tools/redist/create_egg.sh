#!/bin/sh

# Remember to update this package path if the setuptools
# directory changes!
EASY_INSTALL=`which easy_install`
if [ $? != 0 ]; then
	echo "ERROR: Unable to locate easy_install, please check it is installed."
	exit 1
fi

OUTPUT_PATH="../../../../tools/server/pycommon/redist"

if [ ! -n "$1" ]
then
	echo "Usage: `basename $0` <package_filename>"
	echo
	echo "For example, to install the package in MySQL-python-1.2.2.tar.gz:"
	echo 
	echo "./create_egg.sh MySQL-python-1.2.2.tar.gz"
	echo
	echo "You will need to remove the existing egg from $OUTPUT_PATH if it already"
	echo "exists, however, otherwise easy_install will just quit without creating"
	echo "the new egg package."
	exit $E_BADARGS
fi

echo "Checking for --hash-style availability"

HAS_HASH_STYLE=`gcc -dumpspecs | grep -c "\-\-hash-style"`

if [ "$HAS_HASH_STYLE" -eq "1" ]
then
	export CFLAGS=" $CFLAGS -Wl,--hash-style=both "
	echo "hash-style found, setting CFLAGS env to: \"$CFLAGS\"..."
else
	echo "No hash-style found, continuing..."
fi

echo "Running easy_install..."
$EASY_INSTALL -f . -zmaxd $OUTPUT_PATH $*
