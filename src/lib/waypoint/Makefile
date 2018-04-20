LIB = waypoint

SRCS =						\
	adjacent_chunk_set		\
	chunk_nav_poly_set		\
	chunk_waypoint_set		\
	navigator				\
	waypoint				\
	waypoint_chunk			\
	waypoint_set			\
	waypoint_set_state		\
	waypoint_state			\

# Windows only files
# waypoint_generator waypoint_flood

ifndef MF_ROOT
export MF_ROOT := $(subst /src/lib/$(LIB),,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/makestuf/stdmake.mak
