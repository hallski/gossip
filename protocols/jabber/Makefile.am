AM_CPPFLAGS =                                   \
	-I.                                     \
	-I$(top_srcdir)                         \
	-DPROTOCOLSDIR=\"$(datadir)/gossip/protocols\" 	\
	$(GOSSIP_JABBER_CFLAGS)

noinst_LTLIBRARIES = libgossip-jabber.la

libgossip_jabber_la_LIBADD = $(GOSSIP_JABBER_LIBS)

libgossip_jabber_la_SOURCES =                   \
	gossip-jabber.c                         \
	gossip-jabber.h                         \
	gossip-jabber-private.h                 \
	gossip-jabber-chatrooms.c               \
	gossip-jabber-chatrooms.h               \
	gossip-jabber-helper.c                  \
	gossip-jabber-helper.h                  \
	gossip-jid.c                            \
	gossip-jid.h                            \
	gossip-transport-accounts.c             \
	gossip-transport-accounts.h             \
	gossip-transport-discover.c             \
	gossip-transport-discover.h             \
	gossip-transport-protocol.c             \
	gossip-transport-protocol.h             \
	gossip-transport-register.c             \
	gossip-transport-register.h

