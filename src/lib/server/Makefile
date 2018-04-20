LIB = server

SRCS =									\
	add_to_manager_helper				\
	anonymous_channel_client			\
	backup_hash							\
	bwconfig							\
	bwservice							\
	cvs									\
	event_history_stats					\
	id_client							\
	plugin_library						\
	python_server						\
	reviver_subject						\
	time_keeper							\
	watcher_forwarding					\
	watcher_forwarding_collector		\
	watcher_protocol

ifndef MF_ROOT
export MF_ROOT := $(subst /src/lib/$(LIB),,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/makestuf/stdmake.mak
