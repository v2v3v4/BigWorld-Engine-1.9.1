LIB =	pyscript

SRCS =					\
	frozen_modules		\
	personality			\
	pickler				\
	py_data_section		\
	py_output_writer	\
	py_patrolpath		\
	py_to_stl			\
	pyobject_plus		\
	pywatcher			\
	res_mgr_import		\
	res_mgr_script		\
	resource_table		\
	script				\
	script_math			\
	stl_to_py			\
	py_traceback		\

ifndef MF_ROOT
export MF_ROOT := $(subst /src/lib/$(LIB),,$(CURDIR))
endif

include $(MF_ROOT)/src/lib/makestuf/stdmake.mak
