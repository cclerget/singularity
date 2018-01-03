SUBDIRS = signal cleanup notify

MAINTAINERCLEANFILES = Makefile.in config.h config.h.in
DISTCLEANFILES = Makefile
CLEANFILES = core.* *~ *.la
AM_CFLAGS = -Wall -fpie -fPIC
AM_LDFLAGS = -pie
AM_CPPFLAGS = -DSYSCONFDIR=\"$(sysconfdir)\" -DLOCALSTATEDIR=\"$(localstatedir)\" -DLIBEXECDIR=\"$(libexecdir)\" $(SINGULARITY_DEFINES)
distlibdir = $(libdir)/singularity
distincludedir = $(includedir)/singularity

noinst_LTLIBRARIES = libinternal.la
libinternal_la_LIBADD = signal/libinternal.la cleanup/libinternal.la notify/libinternal.la
libinternal_la_SOURCES = event.c ../../util/fork.c ../../util/registry.c ../../util/message.c ../../util/config_parser.c ../../util/privilege.c ../../util/util.c ../../util/file.c ../../util/setns.c ../../util/capability.c ../../util/suid.c ../../util/mount.c ../../util/network.c
libinternal_la_CFLAGS = $(AM_CFLAGS) # This fixes duplicate sources in library and progs

distinclude_HEADERS = event.h
distlib_LTLIBRARIES = libsingularity-event.la

libsingularity_event_la_SOURCES =
libsingularity_event_la_LIBADD = $(noinst_LTLIBRARIES)
libsingularity_event_la_LDFLAGS = -version-info 1:0:0 
libsingularity_event_la_CFLAGS = $(AM_CFLAGS)

EXTRA_DIST = 


# These are kludges so they don't remove the $(DEPDIR) in ../../util/ otherwise
# the clean will fail when other Makefile tries to remove those directories
distclean: distclean-recursive
	-rm ./$(DEPDIR)
	-rm -f Makefile

maintainer-clean: maintainer-clean-recursive
	-rm ./$(DEPDIR)
	-rm -f Makefile

