ifneq ($(wildcard cellappmgr/Makefile),cellappmgr/Makefile)
	LICENSE_SERVER=1
endif

ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server,,$(CURDIR))
endif

all clean realclean all_config install::

# Check with and without the trailing slash
ifneq ($(CURDIR),$(MF_ROOT)/bigworld/src/server)
ifneq ($(CURDIR),$(MF_ROOT)bigworld/src/server)
	@echo 'Error: MF_ROOT=$(MF_ROOT)'
	@echo '       is not the root of $(CURDIR)'
	@echo 'Remove the MF_ROOT environment variable.'
	@false
endif
endif

	$(MAKE) -C ../../../src/lib $@
	$(MAKE) -C baseapp $@
	$(MAKE) -C baseappmgr $@
	$(MAKE) -C cellapp $@
ifndef LICENSE_SERVER
	$(MAKE) -C cellappmgr $@
endif
	$(MAKE) -C dbmgr $@
	$(MAKE) -C loginapp $@
	$(MAKE) -C reviver $@
	$(MAKE) -C web/python $@
	$(MAKE) -C tools $@

ifdef MF_CONFIG
	@echo completed $@ \(MF_CONFIG = $(MF_CONFIG)\)
else
	@echo completed $@
endif
	$(MAKE) done

examples:
	@$(MAKE) -C egextra all

clean realclean::
# Explicitly clean out Python library
	$(MAKE) -C ../../../src/lib/python $@
# and the Python web integration module
	$(MAKE) -C web/python $@
done:
ifdef DO_NOT_BELL
else
	echo -n 
endif

server:
	$(MAKE) all

# standard all build, run the unit tests, and build documentation.
package: all unit_tests_build doc capi pyapi

# for Unit Test
unit_tests unit_tests_build unit_tests_run:
	$(MAKE) -C ../../../src/lib $@

# build documentation.
doc:
	$(MAKE) -C ../../doc

# C++ API documentation.
capi:
	@cd ../../doxygen && ./gen_all

# Python API documentation.
pyapi:
	@cd ../../../licensing/procedures/doc_gen/python && ./generate.py
	@echo "For generating CHM documents, please running below command under Windows:"
	@echo "licensing/procedures/doc_gen/python/make_chm.bat"

rpm:
	$(MAKE) -C ../../tools/server/rpm
#end
