BIN  = message_logger
SRCS =							\
	logging_string_handler		\
	main						\
	bwlog						\
	logger

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/tools/$(BIN),,$(CURDIR))
endif

INSTALL_DIR = $(MF_ROOT)/bigworld/tools/server/message_logger

MY_LIBS = server pyscript entitydef

USE_PYTHON = 1

include $(MF_ROOT)/bigworld/src/server/common/common.mak

all:: bwlog.so

bwlog.so: $(MF_CONFIG)/bwlog.o $(MF_CONFIG)/logging_string_handler.o \
$(MF_CONFIG)/bw_extension_hack.o
	$(CXX) $(LDFLAGS) $(ARCHFLAGS) -g -shared -o $(OUTPUTDIR)/$@ $^ \
		-lentitydef -lnetwork -lpyscript \
		-lserver -lresmgr -lzip -lmath  -lcstdmf

$(MF_CONFIG)/bw_extension_hack.o: ../../../../../src/lib/python/bw_extension_hack.c
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ -c $^

clean::
	@rm $(RM_FLAGS) $(OUTPUTDIR)/bwlog.so
