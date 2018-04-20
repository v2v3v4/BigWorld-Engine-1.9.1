LIB_NAME = network
LIB_PATH = /src/lib/$(LIB_NAME)/unit_test

SRCS =								\
	main							\
	test_config						\
	test_channel					\
	test_flood						\
	test_mangle						\
	test_nub						\
	test_fragment					\
	test_threadsafety				\
	test_compresslength				\

MY_LIBS = network cstdmf
LDLIBS += -lpthread -rdynamic

ifndef MF_ROOT
export MF_ROOT := $(subst $(LIB_PATH),,$(PWD))
endif

include $(MF_ROOT)/src/lib/unit_test_lib/unit_test.mak
