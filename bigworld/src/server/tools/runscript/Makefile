
COMMON=../../../common

BIN  = runscript
SRCS = runscript							\
	../../cellappmgr/cellappmgr_interface	\
	../../baseappmgr/baseappmgr_interface

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/tools/$(BIN),,$(CURDIR))
endif

INSTALL_DIR = $(MF_ROOT)/bigworld/tools/server

MY_LIBS = server

include $(MF_ROOT)/bigworld/src/server/common/common.mak

