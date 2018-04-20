LIB = CppUnitLite2

SRCS =						\
		ExceptionHandler	\
		Failure				\
		Test				\
		TestException		\
		TestRegistry		\
		TestResult			\
		TestResultStdErr	\

ifndef MF_ROOT
export MF_ROOT := $(subst /src/lib/third_party/$(LIB)/src,,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/makestuf/stdmake.mak

all::
