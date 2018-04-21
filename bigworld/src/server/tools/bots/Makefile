
BIN  = bots

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/tools/$(BIN),,$(CURDIR))
endif

SRCS =														\
	client_app												\
	main													\
	main_app												\
	movement_controller										\
	patrol_graph											\
	py_bots													\
	py_server												\
	py_entities												\
	entity													\
	entity_type												\
	space_data_manager										\
	zigzag_patrol_graph										\
	beeline_controller										\
	$(MF_ROOT)/bigworld/src/common/servconn					\
	$(MF_ROOT)/bigworld/src/common/simple_client_entity		\
	$(MF_ROOT)/bigworld/src/common/login_interface			\

ASMS =

MY_LIBS = entitydef pyscript server chunk

USE_PYTHON = 1

USE_OPENSSL = 1

include $(MF_ROOT)/bigworld/src/server/common/common.mak

all::
