AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)                                 \
	-DPROTOCOL_DIR=\""$(libdir)/gossip/protocols"\" \
	-DDTDDIR=\"$(datadir)/gossip\"			\
	-I$(top_srcdir)/protocols/jabber                \
	$(LIBGOSSIP_CFLAGS) $(WARN_CFLAGS)

noinst_LTLIBRARIES = libgossip.la

libgossip_la_SOURCES =             			\
	gossip-account.c           			\
	gossip-account.h           			\
	gossip-account-manager.c   			\
	gossip-account-manager.h   			\
	gossip-async.h             			\
	gossip-chatroom.c				\
	gossip-chatroom.h				\
	gossip-chatroom-manager.c 			\
	gossip-chatroom-manager.h 			\
	gossip-chatroom-provider.c 			\
	gossip-chatroom-provider.h 			\
	gossip-contact.c           			\
	gossip-contact.h           			\
	gossip-event.c             			\
	gossip-event.h             			\
	gossip-event-manager.c     			\
	gossip-event-manager.h     			\
	gossip-ft.c		 			\
	gossip-ft.h 					\
	gossip-ft-provider.c	 			\
	gossip-ft-provider.h 				\
	gossip-message.c           			\
	gossip-message.h           			\
	gossip-presence.c          			\
	gossip-presence.h          			\
	gossip-protocol.c          			\
	gossip-protocol.h          			\
	gossip-session.c           			\
	gossip-session.h           			\
	gossip-time.c              			\
	gossip-time.h              			\
	gossip-utils.c             			\
	gossip-utils.h					\
	gossip-vcard.c             			\
	gossip-vcard.h             			\
	gossip-version-info.c      			\
	gossip-version-info.h      			\
	libgossip-marshal-main.c

libgossip_la_LIBADD =              			\
	$(LIBGOSSIP_LIBS)          			\
	$(top_builddir)/protocols/jabber/libgossip-jabber.la

libgossipincludedir = $(includedir)/gossip/

dtddir = $(datadir)/gossip
dtd_DATA = 						\
	gossip-account.dtd				\
	gossip-chatroom.dtd

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

EXTRA_DIST =						\
	$(dtd_DATA)					\
	libgossip-marshal.list

CLEANFILES = $(BUILT_SOURCES)
