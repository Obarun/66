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
AR := $(CROSS_COMPILE)ar
RANLIB := $(CROSS_COMPILE)ranlib
STRIP := $(CROSS_COMPILE)strip
INSTALL := ./tools/install.sh

ALL_BINS := $(LIBEXEC_TARGETS) $(BIN_TARGETS)
ALL_LIBS := $(SHARED_LIBS) $(STATIC_LIBS) $(INTERNAL_LIBS)
ALL_INCLUDES := $(wildcard src/include/$(package)/*.h)
INSTALL_DATA += init.conf
LOWDOWN := $(shell type -p lowdown)
ifdef LOWDOWN
GENERATE_HTML := $(shell doc/make-html.sh $(version))
GENERATE_MAN := $(shell doc/make-man.sh)
endif
INSTALL_HTML := $(wildcard doc/$(version)/html/*.html)
INSTALL_MAN := $(wildcard doc/man/*/*)
INSTALL_DATA := skel/halt skel/init skel/poweroff skel/rc.init skel/rc.init.container \
	skel/rc.shutdown skel/reboot skel/shutdown skel/rc.shutdown.final skel/init.conf

all: $(ALL_LIBS) $(ALL_BINS) $(ALL_INCLUDES)

clean:
	@exec rm -f $(ALL_LIBS) $(ALL_BINS) $(wildcard src/*/*.o src/*/*.lo src/*/*/*.o src/*/*/*.lo) \
	$(INSTALL_MAN) $(INSTALL_HTML) $(EXTRA_TARGETS)

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
	exec $(STRIP) -x -R .note -R .comment $(STATIC_LIBS)
endif
ifneq ($(strip $(ALL_BINS)$(SHARED_LIBS)),)
	exec $(STRIP) -R .note -R .comment $(ALL_BINS) $(SHARED_LIBS)
endif

install: install-dynlib install-libexec install-bin install-lib install-include install-data install-html install-man
install-dynlib: $(SHARED_LIBS:lib%.so.xyzzy=$(DESTDIR)$(dynlibdir)/lib%.so)
install-libexec: $(LIBEXEC_TARGETS:%=$(DESTDIR)$(libexecdir)/%)
install-bin: $(BIN_TARGETS:%=$(DESTDIR)$(bindir)/%)
install-lib: $(STATIC_LIBS:lib%.a.xyzzy=$(DESTDIR)$(libdir)/lib%.a)
install-include: $(ALL_INCLUDES:src/include/$(package)/%.h=$(DESTDIR)$(includedir)/$(package)/%.h)
install-data: $(INSTALL_DATA:skel/%=$(DESTDIR)$(skel)/%)
install-html: $(INSTALL_HTML:doc/$(version)/html/%.html=$(DESTDIR)$(datarootdir)/doc/$(package)/$(version)/%.html)
install-man: install-man1 install-man5 install-man8
install-man1: $(INSTALL_MAN:doc/man/man1/%.1=$(DESTDIR)$(mandir)/man1/%.1)
install-man5: $(INSTALL_MAN:doc/man/man5/%.5=$(DESTDIR)$(mandir)/man5/%.5)
install-man8: $(INSTALL_MAN:doc/man/man8/%.8=$(DESTDIR)$(mandir)/man8/%.8)

$(DESTDIR)$(datarootdir)/doc/$(package)/$(version)/%.html: doc/$(version)/html/%.html
	$(INSTALL) -D -m 644 $< $@ && \
	sed -e 's,%%shebangdir%%,$(shebangdir),g' \
		-e 's,%%skel%%,$(skel),g' \
        -e 's,%%livedir%%,$(livedir),g' \
		-e 's,%%system_dir%%,$(system_dir),g' \
		-e 's,%%system_log%%,$(system_log),g' \
		-e 's,%%s6log_user%%,$(s6log_user),g' \
		-e 's,%%s6log_timestamp%%,$(s6log_timestamp),g' \
		-e 's,%%s6log_notify%%,$(s6log_notify),g' \
		-e 's,%%service_system%%,$(service_system),g' \
		-e 's,%%module_system%%,$(module_system),g' \
		-e 's,%%script_system%%,$(script_system),g' \
		-e 's,%%service_adm%%,$(service_adm),g' \
		-e 's,%%module_adm%%,$(module_adm),g' \
		-e 's,%%service_admconf%%,$(service_admconf),g' \
		-e 's,%%user_dir%%,$(user_dir),g' \
		-e 's,%%service_user%%,$(service_user),g' \
		-e 's,%%module_user%%,$(module_user),g' \
		-e 's,%%script_user%%,$(script_user),g' \
		-e 's,%%service_userconf%%,$(service_userconf),g' \
		-e 's,%%user_log%%,$(user_log),g' $< > $@

$(DESTDIR)$(mandir)/man1/%.1: doc/man/man1/%.1
	$(INSTALL) -D -m 644 $< $@ && \
	sed -e 's,%%shebangdir%%,$(shebangdir),g' \
		-e 's,%%skel%%,$(skel),g' \
        -e 's,%%livedir%%,$(livedir),g' \
		-e 's,%%system_dir%%,$(system_dir),g' \
		-e 's,%%system_log%%,$(system_log),g' \
		-e 's,%%s6log_user%%,$(s6log_user),g' \
		-e 's,%%s6log_timestamp%%,$(s6log_timestamp),g' \
		-e 's,%%s6log_notify%%,$(s6log_notify),g' \
		-e 's,%%service_system%%,$(service_system),g' \
		-e 's,%%module_system%%,$(module_system),g' \
		-e 's,%%script_system%%,$(script_system),g' \
		-e 's,%%service_adm%%,$(service_adm),g' \
		-e 's,%%module_adm%%,$(module_adm),g' \
		-e 's,%%service_admconf%%,$(service_admconf),g' \
		-e 's,%%user_dir%%,$(user_dir),g' \
		-e 's,%%service_user%%,$(service_user),g' \
		-e 's,%%module_user%%,$(module_user),g' \
		-e 's,%%script_user%%,$(script_user),g' \
		-e 's,%%service_userconf%%,$(service_userconf),g' \
		-e 's,%%user_log%%,$(user_log),g' $< > $@

$(DESTDIR)$(mandir)/man5/%.5: doc/man/man5/%.5
	$(INSTALL) -D -m 644 $< $@ && \
	sed -e 's,%%shebangdir%%,$(shebangdir),g' \
		-e 's,%%skel%%,$(skel),g' \
        -e 's,%%livedir%%,$(livedir),g' \
		-e 's,%%system_dir%%,$(system_dir),g' \
		-e 's,%%system_log%%,$(system_log),g' \
		-e 's,%%s6log_user%%,$(s6log_user),g' \
		-e 's,%%s6log_timestamp%%,$(s6log_timestamp),g' \
		-e 's,%%s6log_notify%%,$(s6log_notify),g' \
		-e 's,%%service_system%%,$(service_system),g' \
		-e 's,%%module_system%%,$(module_system),g' \
		-e 's,%%script_system%%,$(script_system),g' \
		-e 's,%%service_adm%%,$(service_adm),g' \
		-e 's,%%module_adm%%,$(module_adm),g' \
		-e 's,%%service_admconf%%,$(service_admconf),g' \
		-e 's,%%user_dir%%,$(user_dir),g' \
		-e 's,%%service_user%%,$(service_user),g' \
		-e 's,%%module_user%%,$(module_user),g' \
		-e 's,%%script_user%%,$(script_user),g' \
		-e 's,%%service_userconf%%,$(service_userconf),g' \
		-e 's,%%user_log%%,$(user_log),g' $< > $@

$(DESTDIR)$(mandir)/man8/%.8: doc/man/man8/%.8
	$(INSTALL) -D -m 644 $< $@ && \
	sed -e 's,%%shebangdir%%,$(shebangdir),g' \
		-e 's,%%skel%%,$(skel),g' \
        -e 's,%%livedir%%,$(livedir),g' \
		-e 's,%%system_dir%%,$(system_dir),g' \
		-e 's,%%system_log%%,$(system_log),g' \
		-e 's,%%s6log_user%%,$(s6log_user),g' \
		-e 's,%%s6log_timestamp%%,$(s6log_timestamp),g' \
		-e 's,%%s6log_notify%%,$(s6log_notify),g' \
		-e 's,%%service_system%%,$(service_system),g' \
		-e 's,%%module_system%%,$(module_system),g' \
		-e 's,%%script_system%%,$(script_system),g' \
		-e 's,%%service_adm%%,$(service_adm),g' \
		-e 's,%%module_adm%%,$(module_adm),g' \
		-e 's,%%service_admconf%%,$(service_admconf),g' \
		-e 's,%%user_dir%%,$(user_dir),g' \
		-e 's,%%service_user%%,$(service_user),g' \
		-e 's,%%module_user%%,$(module_user),g' \
		-e 's,%%script_user%%,$(script_user),g' \
		-e 's,%%service_userconf%%,$(service_userconf),g' \
		-e 's,%%user_log%%,$(user_log),g' $< > $@

$(DESTDIR)$(skel)/%: skel/%
	exec $(INSTALL) -D -m 644 $< $@
	grep -- ^$(@F) < package/modes | { read name mode owner && \
	if [ x$$owner != x ] ; then chown -- $$owner $@ ; fi && \
	chmod $$mode $@ ; } && \
	exec sed -e "s/@BINDIR@/$(subst /,\/,$(bindir))/g" \
			-e "s/@EXECLINE_SHEBANGPREFIX@/$(subst /,\/,$(shebangdir))/g" \
			-e "s/@LIVEDIR@/$(subst /,\/,$(livedir))/g" \
			-e "s/@SKEL@/$(subst /,\/,$(skel))/g" $< > $@

$(DESTDIR)$(dynlibdir)/lib%.so $(DESTDIR)$(dynlibdir)/lib%.so.$(version_M): lib%.so.xyzzy
	$(INSTALL) -D -m 755 $< $@.$(version) && \
	$(INSTALL) -l $(@F).$(version) $@.$(version_M) && \
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

%.o: %.c
	exec $(CC) $(CPPFLAGS_ALL) $(CFLAGS_ALL) -c -o $@ $<

%.lo: %.c
	exec $(CC) $(CPPFLAGS_ALL) $(CFLAGS_ALL) $(CFLAGS_SHARED) -c -o $@ $<

$(ALL_BINS):
	exec $(CC) -o $@ $(CFLAGS_ALL) $(LDFLAGS_ALL) $(LDFLAGS_NOSHARED) $^ $(EXTRA_LIBS) $(LDLIBS)

lib%.a.xyzzy:
	exec $(AR) rc $@ $^
	exec $(RANLIB) $@

lib%.so.xyzzy:
	exec $(CC) -o $@ $(CFLAGS_ALL) $(CFLAGS_SHARED) $(LDFLAGS_ALL) $(LDFLAGS_SHARED) -Wl,-soname,$(patsubst lib%.so.xyzzy,lib%.so.$(version_M),$@) $^ $(EXTRA_LIBS) $(LDLIBS)

.PHONY: it all clean distclean tgz strip install install-dynlib install-libexec install-bin install-lib install-include install-data install-html install-man

.DELETE_ON_ERROR:
