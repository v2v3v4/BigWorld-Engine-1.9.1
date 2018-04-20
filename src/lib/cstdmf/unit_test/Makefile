LIB_NAME = cstdmf
LIB_PATH = /src/lib/$(LIB_NAME)/unit_test

SRCS =										\
	main									\
	test_asserts							\
	test_dogwatch							\
	test_static_array						\
	test_time_queue							\

MY_LIBS = cstdmf
LDFLAGS += -lpthread -rdynamic

ifndef MF_ROOT
export MF_ROOT := $(subst $(LIB_PATH),,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/unit_test_lib/unit_test.mak
