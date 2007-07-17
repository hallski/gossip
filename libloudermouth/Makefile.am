AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)                                 \
	-DDATADIR=\""$(datadir)"\"              	\
	-DLOCALEDIR=\""$(datadir)/locale"\"		\
	-DLM_COMPILATION		                \
	$(LIBLOUDERMOUTH_CFLAGS)			\
	$(WARN_CFLAGS)

noinst_LTLIBRARIES = libloudermouth.la

libloudermouth_la_SOURCES =           			\
	lm-bs-client.c					\
	lm-bs-client.h					\
	lm-bs-listener.c				\
	lm-bs-listener.h				\
	lm-bs-private.h					\
	lm-bs-receiver.c				\
	lm-bs-receiver.h				\
	lm-bs-sender.c					\
	lm-bs-sender.h					\
	lm-bs-session.c					\
	lm-bs-session.h					\
	lm-bs-transfer.c				\
	lm-bs-transfer.h				\
	lm-debug.h					\
	lm-internals.h					\
	lm-parser.h					\
	lm-sha.h					\
	lm-sock.h					\
	loudermouth.h					\
	libloudermouth-marshal-main.c

libloudermouth_la_LIBADD =             			\
	$(LIBLOUDERMOUTH_LIBS)

libloudermouthincludedir = $(includedir)/gossip/

libloudermouth-marshal.h: libloudermouth-marshal.list
	(cd $(srcdir) && \
	$(GLIB_GENMARSHAL) --prefix=libloudermouth_marshal libloudermouth-marshal.list --header) > xgen-gmh \
	&& (cmp -s xgen-gmh libloudermouth-marshal.h || cp xgen-gmh libloudermouth-marshal.h) \
	&& rm -f xgen-gmh xgen-gmh~

libloudermouth-marshal.c: libloudermouth-marshal.list
	(cd $(srcdir) && \
	$(GLIB_GENMARSHAL) --prefix=libloudermouth_marshal libloudermouth-marshal.list --body) > xgen-gmc \
	&& cp xgen-gmc libloudermouth-marshal.c \
	&& rm -f xgen-gmc xgen-gmc~

libloudermouth-marshal-main.c: libloudermouth-marshal.c libloudermouth-marshal.h

BUILT_SOURCES = 					\
	libloudermouth-marshal.h			\
	libloudermouth-marshal.c

EXTRA_DIST =						\
	libloudermouth-marshal.list

CLEANFILES = $(BUILT_SOURCES)
