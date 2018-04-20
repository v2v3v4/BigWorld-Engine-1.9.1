LIB =	cstdmf

SRCS =					\
	base64				\
	bgtask_manager		\
	binary_stream		\
	bwrandom			\
	bwversion			\
	concurrency			\
	cstdmf				\
	debug				\
	diary				\
	dogwatch			\
	dprintf				\
	intrusive_object	\
	md5					\
	memory_tracker		\
	profile				\
	profiler			\
	timestamp			\
	unique_id			\
	watcher				\
	watcher_path_request\
	string_utils		\

ifndef MF_ROOT
export MF_ROOT := $(subst /src/lib/$(LIB),,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/makestuf/stdmake.mak

