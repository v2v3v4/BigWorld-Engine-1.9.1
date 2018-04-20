LIB = physics2

SRCS =				\
	bsp				\
	hulltree		\
	material_kinds	\
	quad_tree		\
	worldpoly		\
	worldtri

LDFLAGS += -rdynamic

ifndef MF_ROOT
export MF_ROOT := $(subst /src/lib/$(LIB),,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/makestuf/stdmake.mak
