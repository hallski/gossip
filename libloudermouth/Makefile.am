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
	lm-bs-receiver.c				\
	lm-bs-receiver.h				\
	lm-bs-sender.c					\
	lm-bs-sender.h					\
	lm-bs-session.c					\
	lm-bs-session.h					\
	lm-bs-transfer.c				\
	lm-bs-transfer.h				\
	lm-debug.h					\
	lm-connection.h					\
	lm-internals.h					\
	lm-parser.h					\
	lm-sha.h					\
	lm-sock.h					\
	lm-ssl-internals.h				\
	loudermouth.h

libloudermouth_la_LIBADD =             			\
	$(LIBLOUDERMOUTH_LIBS)

libloudermouthincludedir = $(includedir)/gossip/

