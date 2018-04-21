
ifndef MF_ROOT
export MF_ROOT := $(subst /bigworld/src/server/tools,,$(CURDIR))
endif

all all_config clean realclean install::
	@cd bwmachined && $(MAKE) $@
	@cd bots && $(MAKE) $@
	@cd eload && $(MAKE) $@
	@cd message_logger && $(MAKE) $@
	@cd runscript && $(MAKE) $@
	@cd watcher && $(MAKE) $@
	@cd mls && $(MAKE) $@
	@cd consolidate_dbs && $(MAKE) $@
	@cd snapshot_helper && $(MAKE) $@

ifdef BUILD_BSDIFF
all::
	@cd $(MF_ROOT)/src/tools/offline_patcher/bsdiff && python setup.py build &&	cp `find -name _bsdiff.so | head -n1` $(MF_ROOT)/bigworld/tools/misc/offline_patcher/patcherlib/

clean::
	@cd $(MF_ROOT)/src/tools/offline_patcher/bsdiff && python setup.py clean
	rm -f $(MF_ROOT)/bigworld/tools/misc/offline_patcher/patcherlib/_bsdiff.so
endif

#   Don't build these for now
#	@cd calcmd5 && $(MAKE) $@
#	@cd bdiff && $(MAKE) $@
#	@cd bundiff && $(MAKE) $@

#end
