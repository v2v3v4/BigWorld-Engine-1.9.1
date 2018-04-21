
COMMON=../../../common

BIN  = eload
SRCS = eload entity_loader 													\
	../../cellappmgr/cellappmgr_interface 									\
	../../baseappmgr/baseappmgr_interface 									\

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/tools/$(BIN),,$(CURDIR))
endif

INSTALL_DIR = $(MF_ROOT)/bigworld/tools/server

ASMS =

MY_LIBS = entitydef pyscript server chunk

USE_PYTHON = 1

include $(MF_ROOT)/bigworld/src/server/common/common.mak

