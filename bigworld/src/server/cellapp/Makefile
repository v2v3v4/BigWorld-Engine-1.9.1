BIN  = cellapp
SRCS =										\
	ack_cell_app_death_helper				\
	cell									\
	cellapp									\
	cellappmgr								\
	cellapp_death_listener					\
	cell_app_channel						\
	cell_chunk								\
	cell_range_list							\
	cell_viewer_connection					\
	cell_viewer_server						\
	controller								\
	entity									\
	entity_cache							\
	entity_extra							\
	entity_navigate							\
	entity_population						\
	entity_type								\
	entity_vision							\
	external_interfaces						\
	face_entity_controller					\
	history_event							\
	mailbox									\
	main									\
	move_controller							\
	navigation_controller					\
	base_acceleration_controller			\
	accelerate_to_point_controller			\
	accelerate_along_path_controller		\
	accelerate_to_entity_controller			\
	passenger_controller					\
	passenger_extra							\
	passengers								\
	portal_config_controller				\
	profile									\
	proximity_controller					\
	py_client								\
	py_entities								\
	range_list_node							\
	real_entity								\
	scan_vision_controller					\
	space									\
	timer_controller						\
	turn_controller							\
	visibility_controller					\
	vision_controller						\
	witness									\
	../../common/chunk_portal				\
	../../common/doc_watcher				\
	../../common/py_network					\
	../../common/py_physics2				\
	../../common/shared_data				\
	../dbmgr/db_interface_utils				\

ASMS =

MY_LIBS =		\
	pyscript	\
	entitydef	\
	server		\
	waypoint	\
	chunk		\
	terrain		\
	physics2	\
	moo			\
	speedtree	\
	png

BUILD_TIME_FILE = main
USE_PYTHON = 1
#HAS_PCH = 1

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/$(BIN),,$(CURDIR))
endif

include $(MF_ROOT)/bigworld/src/server/common/common.mak

all::

ifeq ($(wildcard cell_range_list.cp*),cell_range_list.cpp)
# this file doesn't want to be optimised ... for some reason
$(MF_CONFIG)/cell_range_list.o: cell_range_list.cpp
ifdef QUIET_BUILD
	test -e $(MSG_FILE) && cat $(MSG_FILE); rm -f $(MSG_FILE)
	@echo $<
endif
	$(COMPILE.cc) -O0 $< $(OUTPUT_OPTION)
	@if test -e cell_range_list.d; then 						\
		echo -n $(MF_CONFIG)/ > $(MF_CONFIG)/cell_range_list.d;	\
		cat cell_range_list.d >> $(MF_CONFIG)/cell_range_list.d;\
		rm cell_range_list.d;									\
	fi
endif

