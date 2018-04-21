
BIN  = egclient2
SRCS =										\
	main									\
	entity									\
	entity_type								\
	main_loop_action						\
	../common/login_interface				\
	../common/servconn						\

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/$(BIN),,$(CURDIR))
endif

INSTALL_DIR = $(MF_ROOT)/bigworld/src/egclient2

ASMS =

MY_LIBS = pyscript entitydef server chunk

USE_PYTHON = 1

USE_OPENSSL = 1

include $(MF_ROOT)/bigworld/src/server/common/common.mak

all::

