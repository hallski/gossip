AM_CPPFLAGS =                                             \
	-I.                                               \
	-I$(top_srcdir)                                   \
	-DPROTOCOL_DIR=\""$(libdir)/gossip/protocols"\"   \
	-DDTDDIR=\"$(datadir)/gossip\"			\
	-I$(top_srcdir)/protocols/jabber                  \
	$(LIBGOSSIP_CFLAGS)

noinst_LTLIBRARIES = libgossip.la

libgossip_la_SOURCES =             \
	gossip-account.c           \
	gossip-account-manager.c   \
	gossip-chatroom-provider.c \
	gossip-contact.c           \
	gossip-event.c             \
	gossip-event-manager.c     \
	gossip-message.c           \
	gossip-presence.c          \
	gossip-protocol.c          \
	gossip-session.c           \
	gossip-time.c              \
	gossip-utils.c             \
	gossip-vcard.c             \
	gossip-version-info.c      \
	libgossip-marshal-main.c   \
	gossip-account.h           \
	gossip-account-manager.h   \
	gossip-async.h             \
	gossip-chatroom-provider.h \
	gossip-contact.h           \
	gossip-event.h             \
	gossip-event-manager.h     \
	gossip-message.h           \
	gossip-presence.h          \
	gossip-protocol.h          \
	gossip-session.h           \
	gossip-time.h              \
	gossip-vcard.h             \
	gossip-version-info.h      \
	gossip-utils.h

libgossip_la_LIBADD =              \
	$(LIBGOSSIP_LIBS)          \
	$(top_builddir)/protocols/jabber/libgossip-jabber.la

libgossipincludedir = $(includedir)/gossip/

dtddir = $(datadir)/gossip
dtd_DATA = 						\
	gossip-account.dtd

GLIB_GENMARSHAL = $(shell pkg-config glib-2.0 --variable=glib_genmarshal)

libgossip-marshal.h: libgossip-marshal.list
	(cd $(srcdir) && \
	$(GLIB_GENMARSHAL) --prefix=libgossip_marshal libgossip-marshal.list --header) > xgen-gmh \
	&& (cmp -s xgen-gmh libgossip-marshal.h || cp xgen-gmh libgossip-marshal.h) \
	&& rm -f xgen-gmh xgen-gmh~

libgossip-marshal.c: libgossip-marshal.list
	(cd $(srcdir) && \
	$(GLIB_GENMARSHAL) --prefix=libgossip_marshal libgossip-marshal.list --body) > xgen-gmc \
	&& cp xgen-gmc libgossip-marshal.c \
	&& rm -f xgen-gmc xgen-gmc~

libgossip-marshal-main.c: libgossip-marshal.c libgossip-marshal.h

BUILT_SOURCES = 					\
	libgossip-marshal.h				\
	libgossip-marshal.c

EXTRA_DIST =                                            \
	libgossip-marshal.list

CLEANFILES = $(BUILT_SOURCES)
