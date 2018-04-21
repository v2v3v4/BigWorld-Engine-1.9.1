# Check that MySQL development libraries have been installed.
HAS_MYSQL := $(shell ../../common/test_mysql.sh && echo $$?)
ifneq ($(HAS_MYSQL),0)

$(warning MySQL development libraries not installed. Skipping consolidate_dbs build.)

else

IS_COMMAND = 1
BIN = consolidate_dbs
SRCS = db_consolidator 				\
	db_file_transfer 				\
	msg_receiver					\
	main 							\
	../../dbmgr/db_config			\
	../../dbmgr/db_entitydefs		\
	../../dbmgr/mysql_wrapper		\
	../../dbmgr/mysql_notprepared	\
	../../dbmgr/mysql_named_lock	\
	../../dbmgr/mysql_prepared		\
	../../dbmgr/mysql_table			\
	../../dbmgr/mysql_typemapping	\
	../../../../../src/lib/chunk/station_graph \
	../../../../../src/lib/chunk/user_data_object \
	../../../../../src/lib/chunk/user_data_object_type \
	../../../../../src/lib/chunk/user_data_object_link_data_type

endif
# endif MySQL development libraries installed. 

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/tools/consolidate_dbs,,$(CURDIR))
endif

USE_PYTHON = 1
USE_MYSQL_PREPARED_STATEMENTS=1

ifeq ($(USE_MYSQL_PREPARED_STATEMENTS), 1)
	CPPFLAGS += -DUSE_MYSQL_PREPARED_STATEMENTS
endif

MY_LIBS = server entitydef pyscript sqlite 

LDLIBS += `mysql_config --libs_r`

include $(MF_ROOT)/bigworld/src/server/common/common.mak
