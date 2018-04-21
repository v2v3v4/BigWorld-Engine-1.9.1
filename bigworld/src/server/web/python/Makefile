# 
# Makefile for Web Integration Module.
#

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/web/python,,$(CURDIR))
endif

ifeq (,$(MF_CONFIG))
MF_CONFIG = Hybrid
	ifeq ($(shell uname -m),x86_64)
		 MF_CONFIG=Hybrid_AMD64
	endif
endif

MF_REAL_CONFIG := $(MF_CONFIG)

ifneq (,$(findstring $(MF_CONFIG), SingleThreaded) )
MF_CONFIG := $(MF_CONFIG)_SingleThreaded
endif

export MF_CONFIG

SO = BigWorld
OUTPUTDIR = $(MF_ROOT)/bigworld/bin/web/$(MF_REAL_CONFIG)

BUILD_TIME_FILE = module_init
SRCS =	functions									\
		mailbox										\
		web_integration								\
		../../baseapp/baseapp_int_interface			\
		../../dbmgr/db_interface					\
		../../../common/login_interface				\

CSRCS =	extra-python								\



MY_LIBS = chunk server entitydef pyscript resmgr network zip math cstdmf
MAKE_LIBS=1

include $(MF_ROOT)/bigworld/src/server/common/common.mak	


