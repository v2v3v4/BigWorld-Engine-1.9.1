#
# Makefile for baseapp unit tests
#

BIN = dbmgr
BIN_PATH = /bigworld/src/server/$(BIN)/unit_test

SRCS = main									\
	test_run_config							\
	../db_interface_extras

MY_LIBS = server entitydef pyscript

USE_PYTHON = 1

ifndef MF_ROOT
export MF_ROOT := $(subst $(BIN_PATH),,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/unit_test_lib/unit_test.mak
