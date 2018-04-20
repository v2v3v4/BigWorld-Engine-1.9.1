LIB =	entitydef

SRCS =								\
		base_user_data_object_description	\
		data_description			\
		member_description			\
		data_types					\
		entity_description			\
		entity_description_debug	\
		entity_description_map		\
		mailbox_base				\
		method_description			\
		method_response				\
		user_data_object_description		\
		user_data_object_description_map	\

ifndef MF_ROOT
export MF_ROOT := $(subst /src/lib/$(LIB),,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/makestuf/stdmake.mak
