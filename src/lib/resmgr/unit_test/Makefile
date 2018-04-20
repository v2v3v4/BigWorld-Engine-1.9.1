LIB_NAME = resmgr
LIB_PATH = /src/lib/$(LIB_NAME)/unit_test

SRCS =								\
	main							\
	stdafx							\
	test_file_system				\
	test_packed_section				\
	test_zip_section				

MY_LIBS = resmgr cstdmf math zip
LDFLAGS += -lpthread -rdynamic

ifndef MF_ROOT
export MF_ROOT := $(subst $(LIB_PATH),,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/unit_test_lib/unit_test.mak
