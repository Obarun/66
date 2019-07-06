#
# This Makefile requires GNU make.
#
# Do not make changes here.
# Use the included .mak files.
#

it: all

make_need := 3.81
ifeq "" "$(strip $(filter $(make_need), $(firstword $(sort $(make_need) $(MAKE_VERSION)))))"
fail := $(error Your make ($(MAKE_VERSION)) is too old. You need $(make_need) or newer)
endif

CC = $(error Please use ./configure first)

STATIC_LIBS :=
SHARED_LIBS :=
INTERNAL_LIBS :=
EXTRA_TARGETS :=
LIB_DEFS :=

define library_definition
LIB$(firstword $(subst =, ,$(1))) := lib$(lastword $(subst =, ,$(1))).$(if $(DO_ALLSTATIC),a,so).xyzzy
ifdef DO_SHARED
SHARED_LIBS += lib$(lastword $(subst =, ,$(1))).so.xyzzy
endif
ifdef DO_STATIC
STATIC_LIBS += lib$(lastword $(subst =, ,$(1))).a.xyzzy
endif
endef

-include config.mak
include package/targets.mak

$(foreach var,$(LIB_DEFS),$(eval $(call library_definition,$(var))))

include package/deps.mak

version_m := $(basename $(version))
version_M := $(basename $(version_m))
version_l := $(basename $(version_M))
CPPFLAGS_ALL := $(CPPFLAGS_AUTO) $(CPPFLAGS)
CFLAGS_ALL := $(CFLAGS_AUTO) $(CFLAGS)
ifeq ($(strip $(STATIC_LIBS_ARE_PIC)),)
CFLAGS_SHARED := -fPIC
else
CFLAGS_SHARED :=
endif
LDFLAGS_ALL := $(LDFLAGS_AUTO) $(LDFLAGS)
REALCC = $(CROSS_COMPILE)$(CC)
AR := $(CROSS_COMPILE)ar
RANLIB := $(CROSS_COMPILE)ranlib
STRIP := $(CROSS_COMPILE)strip
INSTALL := ./tools/install.sh

ALL_BINS := $(LIBEXEC_TARGETS) $(BIN_TARGETS)
ALL_LIBS := $(SHARED_LIBS) $(STATIC_LIBS) $(INTERNAL_LIBS)
ALL_INCLUDES := $(wildcard src/include/$(package)/*.h)
ALL_DATA := $(wildcard skel/*)
ALL_MAN := $(wildcard doc/man/*.[1-8].scd)
INSTALL_MAN := $(wildcard doc/man/*.[1-8])

all: $(ALL_LIBS) $(ALL_BINS) $(ALL_INCLUDES) $(ALL_DATA)

clean:
	@exec rm -f $(ALL_LIBS) $(ALL_BINS) $(wildcard src/*/*.o src/*/*.lo) \
	$(INSTALL_MAN) $(EXTRA_TARGETS)

distclean: clean
	@exec rm -f config.mak src/include/$(package)/config.h

tgz: distclean
	@. package/info && \
	rm -rf /tmp/$$package-$$version && \
	cp -a . /tmp/$$package-$$version && \
	cd /tmp && \
	tar -zpcv --owner=0 --group=0 --numeric-owner --exclude=.git* -f /tmp/$$package-$$version.tar.gz $$package-$$version && \
	exec rm -rf /tmp/$$package-$$version

strip: $(ALL_LIBS) $(ALL_BINS)
ifneq ($(strip $(STATIC_LIBS)),)
	exec $(STRIP) -x -R .note -R .comment -R .note.GNU-stack $(STATIC_LIBS)
endif
ifneq ($(strip $(ALL_BINS)$(SHARED_LIBS)),)
	exec $(STRIP) -R .note -R .comment -R .note.GNU-stack $(ALL_BINS) $(SHARED_LIBS)
endif

install: install-dynlib install-libexec install-bin install-lib install-include install-data
install-dynlib: $(SHARED_LIBS:lib%.so.xyzzy=$(DESTDIR)$(dynlibdir)/lib%.so)
install-libexec: $(LIBEXEC_TARGETS:%=$(DESTDIR)$(libexecdir)/%)
install-bin: $(BIN_TARGETS:%=$(DESTDIR)$(bindir)/%)
install-lib: $(STATIC_LIBS:lib%.a.xyzzy=$(DESTDIR)$(libdir)/lib%.a)
install-include: $(ALL_INCLUDES:src/include/$(package)/%.h=$(DESTDIR)$(includedir)/$(package)/%.h)
install-data: $(ALL_DATA:skel/%=$(DESTDIR)$(sysconfdir)/%)

ifneq ($(exthome),)

$(DESTDIR)$(exthome): $(DESTDIR)$(home)
	exec $(INSTALL) -l $(notdir $(home)) $(DESTDIR)$(exthome)

update: $(DESTDIR)$(exthome)

global-links: $(DESTDIR)$(exthome) $(SHARED_LIBS:lib%.so.xyzzy=$(DESTDIR)$(sproot)/library.so/lib%.so.$(version_M)) $(BIN_TARGETS:%=$(DESTDIR)$(sproot)/command/%)

$(DESTDIR)$(sproot)/command/%: $(DESTDIR)$(home)/command/%
	exec $(INSTALL) -D -l ..$(subst $(sproot),,$(exthome))/command/$(<F) $@

$(DESTDIR)$(sproot)/library.so/lib%.so.$(version_M): $(DESTDIR)$(dynlibdir)/lib%.so.$(version_M)
	exec $(INSTALL) -D -l ..$(subst $(sproot),,$(exthome))/library.so/$(<F) $@

.PHONY: update global-links

endif

$(DESTDIR)$(sysconfdir)/%: skel/% 
	exec $(INSTALL) -D -m 644 $< $@ 
	grep -- ^$(@F) < package/modes | { read name mode owner && \
	if [ x$$owner != x ] ; then chown -- $$owner $@ ; fi && \
	chmod $$mode $@ ; } && \
	exec sed -e "s/@BINDIR@/$(subst /,\/,$(bindir))/g" \
			-e "s/@EXECLINE_SHEBANGPREFIX@/$(subst /,\/,$(shebangdir))/g" \
			-e "s/@LIVEDIR@/$(subst /,\/,$(livedir))/g" $< > $@
	
$(DESTDIR)$(system_log)/% $(DESTDIR)$(service_packager)/% $(DESTDIR)$(service_sys)/% $(DESTDIR)$(service_sysconf)/% : 
	exec $(INSTALL) -D -m 0755 $< $@

$(DESTDIR)$(dynlibdir)/lib%.so: lib%.so.xyzzy
	$(INSTALL) -D -m 755 $< $@.$(version) && \
	$(INSTALL) -l $(@F).$(version) $@.$(version_m) && \
	$(INSTALL) -l $(@F).$(version_m) $@.$(version_M) && \
	exec $(INSTALL) -l $(@F).$(version_M) $@

$(DESTDIR)$(libexecdir)/% $(DESTDIR)$(bindir)/%: % package/modes
	exec $(INSTALL) -D -m 600 $< $@
	grep -- ^$(@F) < package/modes | { read name mode owner && \
	if [ x$$owner != x ] ; then chown -- $$owner $@ ; fi && \
	chmod $$mode $@ ; }

$(DESTDIR)$(libdir)/lib%.a: lib%.a.xyzzy
	exec $(INSTALL) -D -m 644 $< $@

$(DESTDIR)$(includedir)/$(package)/%.h: src/include/$(package)/%.h
	exec $(INSTALL) -D -m 644 $< $@

$(DESTDIR)$(mandir)/man1/%.1: man/%.1
	exec $(INSTALL) -D -m 644 $< $@

%.o: %.c
	exec $(REALCC) $(CPPFLAGS_ALL) $(CFLAGS_ALL) -c -o $@ $<

%.lo: %.c
	exec $(REALCC) $(CPPFLAGS_ALL) $(CFLAGS_ALL) $(CFLAGS_SHARED) -c -o $@ $<

$(ALL_BINS):
	exec $(REALCC) -o $@ $(CFLAGS_ALL) $(LDFLAGS_ALL) $(LDFLAGS_NOSHARED) $^ $(EXTRA_LIBS) $(LDLIBS)

lib%.a.xyzzy:
	exec $(AR) rc $@ $^
	exec $(RANLIB) $@

lib%.so.xyzzy:
	exec $(REALCC) -o $@ $(CFLAGS_ALL) $(CFLAGS_SHARED) $(LDFLAGS_ALL) $(LDFLAGS_SHARED) -Wl,-soname,$(patsubst lib%.so.xyzzy,lib%.so.$(version_M),$@) $^ $(EXTRA_LIBS) $(LDLIBS)

man: $(ALL_MAN:%.scd=%)

%: %.scd
	sed -e 's,%%livedir%%,$(livedir),' \
		-e 's,%%system_dir%%,$(system_dir),' \
		-e 's,%%user_dir%%,$(user_dir),' \
		-e 's,%%service_sysconf%%,$(service_sysconf),' \
		-e 's,%%service_userconf%%,$(service_userconf),' \
		-e 's,%%service_packager%%,$(service_packager),g' \
		-e 's,%%user_log%%,$(user_log),' \
		-e 's,%%service_sys%%,$(service_sys),' \
		-e 's,%%system_log%%,$(system_log),' \
		-e 's,%%sysconfdir%%,$(sysconfdir),' \
		-e 's,%%service_user%%,$(service_user),' $@.scd | scdoc > $@
	
install-man:
	for i in 1 5 8 ; do \
		install -m755 -d $(DESTDIR)$(mandir)/man$$i; \
		install -m644 man/*.$$i $(DESTDIR)$(mandir)/man$$i/ ; \
	done

.PHONY: it all clean distclean tgz strip install install-dynlib install-bin install-lib install-include man install-man

.DELETE_ON_ERROR:
