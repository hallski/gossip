SUBDIRS = sounds

INCLUDES =					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DPREFIX="\"$(prefix)"\"		\
	-DSYSCONFDIR=\""$(sysconfdir)"\"        \
	-DDATADIR=\""$(datadir)"\"              \
	-DLIBDIR=\""$(libdir)"\"		\
	@GOSSIP_CFLAGS@				\
	-DGLADEDIR=\"$(datadir)/gossip\"	\
	-DIMAGEDIR=\"$(datadir)/gossip\"

bin_PROGRAMS = gossip

gossip_SOURCES = 				\
	gossip-marshal-main.c			\
	gossip-main.c				\
	gossip-add-contact.c			\
	gossip-add-contact.h			\
	gossip-app.c				\
	gossip-app.h				\
	gossip-contact-info.c                   \
	gossip-contact-info.h                   \
	gossip-roster.c				\
	gossip-roster.h				\
	gossip-message.c			\
	gossip-message.h			\
	gossip-chat.c				\
	gossip-chat.h				\
	gossip-group-chat.c			\
	gossip-group-chat.h			\
	gossip-favorite.c			\
	gossip-favorite.h			\
	gossip-favorites-dialog.c		\
	gossip-favorites-dialog.h		\
	gossip-jid.c                            \
	gossip-jid.h                            \
	gossip-startup-druid.c			\
	gossip-startup-druid.h			\
	gossip-join-dialog.c			\
	gossip-join-dialog.h			\
	gossip-connect-dialog.c			\
	gossip-connect-dialog.h			\
	gossip-accounts-dialog.c		\
	gossip-accounts-dialog.h		\
	gossip-account.c			\
	gossip-account.h			\
	gossip-utils.c				\
	gossip-utils.h				\
	gossip-sound.c				\
	gossip-sound.h				\
	eggtrayicon.c				\
	eggtrayicon.h				\
	disclosure-widget.c			\
	disclosure-widget.h

gossip_LDADD = @GOSSIP_LIBS@
gossip_LDFLAGS = -export-dynamic

gossip-marshal-main.c: gossip-marshal.c gossip-marshal.h

gossip-marshal.h: gossip-marshal.list
	cd $(srcdir) && \
	( @GLIB_GENMARSHAL@ --prefix=gossip_marshal gossip-marshal.list --header > gossip-marshal1.tmp \
	&& mv gossip-marshal1.tmp gossip-marshal.h ) \
	|| ( rm -f gossip-marshal1.tmp && exit 1 )

gossip-marshal.c: gossip-marshal.h
	cd $(srcdir) && \
	( @GLIB_GENMARSHAL@ --prefix=gossip_marshal gossip-marshal.list --body > gossip-marshal2.tmp \
	&& mv gossip-marshal2.tmp gossip-marshal.c ) \
	|| ( rm -f gossip-marshal2.tmp && exit 1 )

EXTRA_DIST =					\
	gossip-marshal.list
