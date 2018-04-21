
BIN  = egclient
SRCS =										\
	main									\
	../common/login_interface				\
	../common/servconn						\

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/$(BIN),,$(CURDIR))
endif

INSTALL_DIR = $(MF_ROOT)/bigworld/src/egclient

ASMS =

MY_LIBS =

USE_PYTHON =

USE_OPENSSL = 1

include $(MF_ROOT)/bigworld/src/server/common/common.mak

all::

