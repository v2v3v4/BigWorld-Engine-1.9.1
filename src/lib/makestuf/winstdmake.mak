
DEFAULT_STACKSIZE=8192

# standard directories (can be overridden by environment variables)

!ifndef MFLIBDIR
MFLIBDIR=\mf\lib
!endif
!ifndef MFINCDIR
MFINCDIR=\mf\include
!endif
!ifndef MFBINDIR
MFBINDIR=\mf\bin
!endif
!ifndef LIB
LIB=d:\fg\lib;c:\mf\lib
!endif
!ifndef MFSERVERBINDIR
MFSERVERBINDIR=\\zoo\mf\bin
!endif

!ifdef PROFILE_VTUNE
CODEVIEW_DEBUG_SYMS=true
!endif

!ifdef SOFTICE
CODEVIEW_DEBUG_SYMS=true
!endif

!ifdef BOUNDSCHECK
CODEVIEW_DEBUG_SYMS=true
!endif

# create the name the name of the lib based on standard prefixes

!ifdef DEBUG
!if "$(COMPILER)" == "msc700"
LIBNAME=dm$(LIBBASENAME)
!else
!ifdef WIN
LIBNAME=dn$(LIBBASENAME)
!else
LIBNAME=dw$(LIBBASENAME)
!endif
!endif

!else

!if "$(COMPILER)" == "msc700"
LIBNAME=m$(LIBBASENAME)
!else
!ifdef WIN
LIBNAME=n$(LIBBASENAME)
!else
LIBNAME=w$(LIBBASENAME)
!endif
!endif
!endif



# standard programs


# C compiler
!if "$(COMPILER)" == "msc700" || "$(COMPILER)" == "vc400"
CC=cl
CPP=cl
!else
CC=wcc386
CPP=wpp386
!endif

# linker
!if "$(COMPILER)" == "msc700" || "$(COMPILER)" == "vc400"
LINK=link
!else
LINK=wlink
!endif

# assembler
AS=masm

# library maintainer
!if "$(COMPILER)" == "msc700" || "$(COMPILER)" == "vc400"
AR=lib
AREXTRA=;
!else
AR=wlib -q
AREXTRA=
!endif

# make file program
MAKE=nmake

# remove file utility
RM=rm -q

# copy file utility
CP=cp -qF
CP_READONLY=cp -qFR

# change directory utility
CD=cd

# .Wav file converter
CVWAV=convert

# Touch
TOUCH=touch

# standard debug flags

!ifdef DEBUG

!ifndef CCDEBUGLEVEL
CCDEBUGLEVEL=1
!endif

!if "$(COMPILER)" == "msc700" || "$(COMPILER)" == "vc400"
CCDEBUG=-Zi -Od -d_DEBUG
LINKDEBUG=/co/map
!else
!ifdef CODEVIEW_DEBUG_SYMS
CCDEBUG=-d$(CCDEBUGLEVEL) -hc -d_DEBUG
LINKDEBUG=DEBUG codeview OPTION cvp
!else
CCDEBUG=-d$(CCDEBUGLEVEL) -hw -d_DEBUG
LINKDEBUG=DEBUG all
!endif
!endif
ASDEBUG=-zi
!endif #DEBUG


!ifdef PROFILE_VTUNE
CCPROFILE=-d$(CCDEBUGLEVEL) -hc
!endif

# standard compile/link flags

!if "$(STACKSIZE)"==""
STACKSIZE=$(DEFAULT_STACKSIZE)
!endif

!if "$(COMPILER)" == "msc700" || "$(COMPILER)" == "vc400"
CFLAGS=-c -AL -W4 -nologo $(CCDEBUG) $(CFLAGS_EXTRA)
LFLAGS=/stack:$(STACKSIZE)/nol$(LINKDEBUG)$(LFLAGS_EXTRA)
AFLAGS=-mx $(ASDEBUG)
ARFLAGS=/nol

!else

WARNING_LEVEL=99
# -5r means use pentium register based function calling (use -5s for stack)
BASIC_CFLAGS=-zq -5r -zp2 -ei -w$(WARNING_LEVEL)
!ifdef _DEBUG
!ifndef PROFILE_VTUNE
MAP=map,symFile
!endif
!endif


!ifdef WIN
!ifdef CONSOLE
NORMAL_C_FLAGS=$(BASIC_CFLAGS) $(CCPROFILE) -oneatx -bm -bt=nt
!else
!ifdef BOUNDSCHECK
NORMAL_C_FLAGS=$(BASIC_CFLAGS) $(CCPROFILE) -od -bm -bt=nt -d_WINDOWS
!else
NORMAL_C_FLAGS=$(BASIC_CFLAGS) $(CCPROFILE) -oneatx -bm -bt=nt -d_WINDOWS
!endif
!endif
!ifdef DEBUG
CFLAGS=$(NORMAL_C_FLAGS) $(CCDEBUG) $(CFLAGS_EXTRA)
LFLAGS=SYSTEM nt_win $(LINKDEBUG) OPTION $(MAP)dosseg,maxErrors=500,quiet,stack=$(STACKSIZE)$(LFLAGS_EXTRA)
!else
CFLAGS=$(NORMAL_C_FLAGS) -DNODEBUGMSG $(CFLAGS_EXTRA)
LFLAGS=SYSTEM nt_win OPTION $(MAP)dosseg,maxErrors=500,quiet,stack=$(STACKSIZE)$(LFLAGS_EXTRA)
!endif
CPPFLAGS=$(CFLAGS) -xs -zo 
!ifdef CONSOLE
LFLAGS=SYSTEM nt $(LINKDEBUG) OPTION $(MAP)dosseg,maxErrors=500,quiet,stack=$(STACKSIZE)$(LFLAGS_EXTRA)
!else
LFLAGS=SYSTEM nt_win $(LINKDEBUG) OPTION $(MAP)dosseg,maxErrors=500,quiet,stack=$(STACKSIZE)$(LFLAGS_EXTRA)
!endif
!else
CFLAGS=$(BASIC_CFLAGS) -mf -bt=dos $(CCDEBUG) $(CFLAGS_EXTRA)
CPPFLAGS=$(CFLAGS) -xs -zo 
LFLAGS=SYSTEM dos4g $(LINKDEBUG) OPTION $(MAP)maxErrors=500,quiet,stack=$(STACKSIZE)$(LFLAGS_EXTRA)
!endif

!endif

# other macros

STD_DEPENDENCIES = $(STD_HEADERS) makefile


# standard inference rules

.SUFFIXES : .lib

.c.obj:
	$(CC) $(CFLAGS) $*.c

.cpp.obj:
	$(CPP) $(CPPFLAGS) $*.cpp

.obj.exe:
	$(LINK) $(LFLAGS) $** $(EXTRA_OBJ),$*.exe,$*.map,$(LIBRARIES);

.asm.obj:
	$(AS) $(AFLAGS) $*.asm

.obj.lib:
	$(AR) $(ARFLAGS) $@ -+$?;





# utility targets


all::


clean::
	$(RM) *.obj *.err state.rst


headers::


#end winstdmake.mak#
