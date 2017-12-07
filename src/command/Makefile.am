SUBDIRS = action-lib

MAINTAINERCLEANFILES = Makefile.in config.h config.h.in
DISTCLEANFILES = Makefile
CLEANFILES = core.* *~ *.la *.o
AM_CFLAGS = -Wall -fpie -fPIC
AM_LDFLAGS = -pie
AM_CPPFLAGS = -DSYSCONFDIR=\"$(sysconfdir)\" -DLOCALSTATEDIR=\"$(localstatedir)\" -DLIBEXECDIR=\"$(libexecdir)\" $(SINGULARITY_DEFINES)
distlibdir = $(libdir)/singularity
distincludedir = $(includedir)/singularity

noinst_LTLIBRARIES = libcommand.la
libcommand_la_LIBADD = action-lib/libinternal.la ../lib/image/libsingularity-image.la ../lib/runtime/libsingularity-runtime.la ../lib/sif/libsingularity-sif.la
libcommand_la_SOURCES = action.c start.c mount.c ../util/registry.c ../util/message.c ../util/config_parser.c ../util/privilege.c ../util/util.c ../util/file.c ../util/suid.c ../util/capability.c ../util/mount.c ../util/signal.c ../util/daemon.c ../util/sessiondir.c
libcommand_la_CFLAGS = $(AM_CFLAGS) # This fixes duplicate sources in library and progs
