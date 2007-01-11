AM_CPPFLAGS =                                   \
	-I.                                     \
	-I$(top_srcdir)                         \
	-DPROTOCOLSDIR=\"$(datadir)/gossip/protocols\" 	\
	$(GOSSIP_TELEPATHY_CFLAGS) 		\
	$(WARN_CFLAGS)

noinst_LTLIBRARIES = libgossip-telepathy.la

libgossip_telepathy_la_LIBADD = $(GOSSIP_TELEPATHY_LIBS)

libgossip_telepathy_la_SOURCES =						\
	gossip-telepathy.c			gossip-telepathy.h		\
	gossip-telepathy-contacts.c		gossip-telepathy-contacts.h	\
	gossip-telepathy-group.c		gossip-telepathy-group.h	\
	gossip-telepathy-message.c		gossip-telepathy-message.h	\
	gossip-telepathy-chatrooms.c		gossip-telepathy-chatrooms.h	\
	gossip-telepathy-contact-list.c		gossip-telepathy-contact-list.h	\
	gossip-telepathy-cmgr.c		gossip-telepathy-cmgr.h	\
	gossip-telepathy-private.h

