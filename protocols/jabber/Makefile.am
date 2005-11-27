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
	gossip-jabber-chatrooms.c               \
	gossip-jabber-chatrooms.h               \
	gossip-jabber-ft.c			\
	gossip-jabber-ft.h			\
	gossip-jabber-ft-utils.c		\
	gossip-jabber-ft-utils.h		\
	gossip-jabber-private.h                 \
	gossip-jabber-vcard.c			\
	gossip-jabber-vcard.h			\
	gossip-jabber-services.c		\
	gossip-jabber-services.h		\
	gossip-jabber-utils.c			\
	gossip-jabber-utils.h			\
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

