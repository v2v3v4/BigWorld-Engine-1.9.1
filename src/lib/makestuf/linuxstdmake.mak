# standard directories (can be overridden by environment variables)


ifndef MFLIBDIR
MFLIBDIR=$(HOME)/mf/lib
endif

ifndef MFINCDIR
MFINCDIR=$(HOME)/mf/include
endif

ifndef MFBINDIR
MFBINDIR=$(HOME)/mf/bin
endif

ifndef LIB
LIB=$(HOME)/mf/lib
endif

ifndef MFSERVERBINDIR
MFSERVERBINDIR=//zoo/mf/bin
endif

ifdef PROFILE_VTUNE
CODEVIEW_DEBUG_SYMS=true
endif

ifdef SOFTICE
CODEVIEW_DEBUG_SYMS=true
endif

ifdef BOUNDSCHECK
CODEVIEW_DEBUG_SYMS=true
endif



# create the name the name of the lib based on standard prefixes

ifdef DEBUG
LIBNAME=libd$(LIBBASENAME).a
else
LIBNAME=lib$(LIBBASENAME).a
endif


# standard programs

# C compiler
CC=gcc
CPP=g++

# linker
LINK=ld

# assembler
AS=as

# library maintainer
AR=ar
AREXTRA=

# make file program
MAKE=make

# remove file utility
RM=rm

# copy file utility
CP=cp
CP_READONLY=cp
#### TBD: read only version of cp

# change directory utility
CD=cd

# Touch
TOUCH=touch

# standard debug flags

ifdef DEBUG
    CCDEBUG=
    LINKDEBUG=

    ASDEBUG=-zi

    ifndef CCDEBUGLEVEL
         CCDEBUGLEVEL=1
    endif

endif #DEBUG


# standard compile/link flags

ifeq "$(STACKSIZE)" ""
STACKSIZE=$(DEFAULT_STACKSIZE)
endif

AFLAGS=$(ASDEBUG)
ARFLAGS=rsuv

WARNING_LEVEL=99
BASIC_C_FLAGS=-c -DMF_SERVER $(CCPROFILE) -I$(INCLUDE)
BASIC_CPP_FLAGS=-c -DMF_SERVER $(CCPROFILE) -I$(INCLUDE)

ifndef DEBUG
BASIC_CPP_FLAGS += -DCODE_INLINE
endif

ifdef DEBUG
  CFLAGS=$(BASIC_C_FLAGS) $(CCDEBUG) $(CFLAGS_EXTRA)
  CPPFLAGS=$(BASIC_CPP_FLAGS) $(CPPDEBUG) $(CPPFLAGS_EXTRA)
  LFLAGS=
else
  CFLAGS=$(BASIC_C_FLAGS) -DNODEBUGMSG $(CFLAGS_EXTRA)
  CPPFLAGS=$(BASIC_CPP_FLAGS) -DNODEBUGMSG $(CPPDEBUG) $(CPPFLAGS_EXTRA)
  LFLAGS=
endif

# other macros

STD_DEPENDENCIES = $(STD_HEADERS) makefile


# standard inference rules

.SUFFIXES : .a

.c.o:
	$(CC) $(CFLAGS) $*.c

.cpp.o:
	$(CPP) $(CPPFLAGS) $*.cpp

.o.exe:
	$(LINK) $(LFLAGS) $** $(EXTRA_OBJ),$*.exe,$*.map,$(LIBRARIES);

.asm.o:
	$(AS) $(AFLAGS) $*.asm

.o.a:
	$(AR) $(ARFLAGS) $@ $?





# utility targets


all::


clean::
	-$(RM) *.o


headers::


#end linuxstdmake.mak#
