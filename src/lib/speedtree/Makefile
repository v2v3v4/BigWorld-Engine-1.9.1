LIB = speedtree

SRCS =						\
	speedtree_collision		\
	speedtree_renderer		\
	speedtree_renderer_off	\

ifndef MF_ROOT
export MF_ROOT := $(subst /src/lib/$(LIB),,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/makestuf/stdmake.mak
