#
# Makefile for baseapp
#

BIN  = baseapp
SRCS = main								\
	base								\
	baseapp								\
	bwtracer							\
	entity_type							\
	external_interfaces					\
	global_bases						\
	loading_thread						\
	mailbox								\
	proxy								\
	py_bases							\
	py_cell_data						\
	rate_limit_message_filter			\
	sqlite_database						\
	worker_thread						\
	../../common/shared_data			\
	../../common/doc_watcher			\
	../../common/py_network				\
	../dbmgr/db_interface				\
	../dbmgr/db_interface_utils			\
	../../../../src/lib/chunk/station_graph \
	../../../../src/lib/chunk/user_data_object \
	../../../../src/lib/chunk/user_data_object_type \
	../../../../src/lib/chunk/user_data_object_link_data_type

MY_LIBS = server entitydef pyscript sqlite

USE_PYTHON = 1

ifndef USE_OPENSSL
	USE_OPENSSL = 1
endif

BUILD_TIME_FILE = main

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/$(BIN),,$(CURDIR))
endif

include $(MF_ROOT)/bigworld/src/server/common/common.mak
