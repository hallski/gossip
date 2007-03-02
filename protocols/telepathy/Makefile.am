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
	gossip-telepathy-cmgr.c			gossip-telepathy-cmgr.h		\
	gossip-telepathy-private.h						\
	gossip-telepathy-marshal-main.c

gossip-telepathy-marshal.h: gossip-telepathy-marshal.list
	(cd $(srcdir) && \
	$(GLIB_GENMARSHAL) --prefix=gossip_telepathy_marshal gossip-telepathy-marshal.list --header) > xgen-gmh \
	&& (cmp -s xgen-gmh gossip-telepathy-marshal.h || cp xgen-gmh gossip-telepathy-marshal.h) \
	&& rm -f xgen-gmh xgen-gmh~

gossip-telepathy-marshal.c: gossip-telepathy-marshal.list
	(cd $(srcdir) && \
	$(GLIB_GENMARSHAL) --prefix=gossip_telepathy_marshal gossip-telepathy-marshal.list --body) > xgen-gmc \
	&& cp xgen-gmc gossip-telepathy-marshal.c \
	&& rm -f xgen-gmc xgen-gmc~

gossip-telepathy-marshal-main.c: gossip-telepathy-marshal.c gossip-telepathy-marshal.h

BUILT_SOURCES = \
	gossip-telepathy-marshal.h \
	gossip-telepathy-marshal.c

EXTRA_DIST = \
	gossip-telepathy-marshal.list

CLEANFILES = $(BUILT_SOURCES)
