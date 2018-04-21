# MF_CONFIG=Debug
# CPPFLAGS += -DWORKERTHREAD_SELFTEST

# Enables the -syncDefsToTables command-line option and the 
# dbmgr/syncDefsToTables configuration option. Disable this in production
# servers for added safety.
ENABLE_TABLE_SCHEMA_ALTERATIONS=1

USE_MYSQL=0
USE_XML=1
BUILD_TIME_FILE = main
SELF_TEST = 0

BIN = dbmgr
SRCS =	main 								\
		custom								\
		database							\
		db_config							\
		db_entitydefs						\
		db_status							\
		entity_recoverer					\
		worker_thread						\
		../baseappmgr/baseappmgr_interface	\
		../baseapp/baseapp_int_interface	\
		../../common/doc_watcher			\
		../../common/login_interface		\
		../../../../src/lib/chunk/station_graph \
		../../../../src/lib/chunk/user_data_object \
		../../../../src/lib/chunk/user_data_object_type \
		../../../../src/lib/chunk/user_data_object_link_data_type

ifeq ($(ENABLE_TABLE_SCHEMA_ALTERATIONS), 1)
CPPFLAGS += -DENABLE_TABLE_SCHEMA_ALTERATIONS
endif

ifeq ($(USE_MYSQL), 1)

HAS_MYSQL := $(shell ../common/test_mysql.sh && echo $$?)
ifneq ($(HAS_MYSQL),0)
$(warning MySQL development libraries not installed. Disabling USE_MYSQL.)
USE_MYSQL=0
else
SRCS += mysql_wrapper mysql_prepared mysql_notprepared mysql_thread \
	mysql_typemapping mysql_database db_interface_utils \
	mysql_table mysql_named_lock
endif

endif

ifeq ($(USE_XML), 1)
SRCS += xml_database
endif

ifeq ($(SELF_TEST), 1)
CPPFLAGS += -DDBMGR_SELFTEST
endif

MY_LIBS = server entitydef pyscript

USE_PYTHON = 1

USE_OPENSSL = 1

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/$(BIN),,$(CURDIR))
endif

include $(MF_ROOT)/bigworld/src/server/common/common.mak

ifeq ($(USE_MYSQL), 1)
USE_MYSQL_PREPARED_STATEMENTS=1
LDLIBS += `mysql_config --libs_r`
CPPFLAGS += -DUSE_MYSQL
ifeq ($(USE_MYSQL_PREPARED_STATEMENTS), 1)
	CPPFLAGS += -DUSE_MYSQL_PREPARED_STATEMENTS
endif
CPPFLAGS += `mysql_config --cflags`
endif

ifeq ($(USE_XML), 1)
CPPFLAGS += -DUSE_XML
endif
