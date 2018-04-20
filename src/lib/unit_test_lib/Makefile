LIB =	unit_test_lib

SRCS =							\
	unit_test					\
	multi_proc_test_case		\
	TestResultBWOut				\

ifndef MF_ROOT
export MF_ROOT := $(subst /src/lib/$(LIB),,$(CURDIR))
endif

CPPFLAGS += -I $(MF_ROOT)/src/lib/third_party/CppUnitLite2/src

include $(MF_ROOT)/src/lib/makestuf/stdmake.mak

