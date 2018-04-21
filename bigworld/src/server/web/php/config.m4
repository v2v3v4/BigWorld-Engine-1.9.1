#
# This is the macro file for the BigWorld PHP extension. 
# Use phpize to generate configure scripts.
#

PHP_ARG_ENABLE(bigworld_php, whether to enable bigworld support,
[  --enable-bigworld_php           Enable BigWorld support])

if test "$PHP_bigworld_php" != "no"; then
  
  
  AC_MSG_CHECKING(for MF_CONFIG environment variable)
  if test -z "$MF_CONFIG"; then
	MF_CONFIG="Hybrid"
	AC_MSG_RESULT([none found, setting to Hybrid])
  else
    AC_MSG_RESULT($MF_CONFIG)
  fi
  
  PHP_REQUIRE_CXX()
# Put the path to your own python2.5 include path here
  PHP_ADD_INCLUDE(/usr/local/python2.5/include/python2.5)
  PHP_SUBST(BIGWORLD_PHP_SHARED_LIBADD)

# Put the path to your own python2.5 static library path here
  PHP_ADD_LIBRARY_WITH_PATH(python2.5, /usr/local/python2.5/lib/python2.5/config, BIGWORLD_PHP_SHARED_LIBADD) 
  
  PHP_ADD_LIBRARY(stdc++, 1, BIGWORLD_PHP_SHARED_LIBADD)
  PHP_ADD_LIBRARY(util, 1, BIGWORLD_PHP_SHARED_LIBADD)

  PHP_NEW_EXTENSION(bigworld_php, module-init.cpp functions.cpp, $ext_shared)
fi

