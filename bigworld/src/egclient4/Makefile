
BIN  = egclient4
SRCS =										\
	main									\
	entity									\
	entity_type								\
	py_server								\
	py_entities								\
	../common/login_interface				\
	../common/servconn						\
	../common/simple_client_entity			\

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/$(BIN),,$(CURDIR))
endif

INSTALL_DIR = $(MF_ROOT)/bigworld/src/egclient4

ASMS =

MY_LIBS = entitydef pyscript chunk

USE_PYTHON = 1

USE_OPENSSL = 1

include $(MF_ROOT)/bigworld/src/server/common/common.mak

all::

