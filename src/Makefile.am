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
	gossip-idle.c				\
	gossip-idle.h				\
	gossip-roster.c				\
	gossip-roster.h				\
	gossip-register.c			\
	gossip-register.h			\
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
	gossip-preferences.c			\
	gossip-preferences.h			\
	gossip-sound.c				\
	gossip-sound.h				\
	eggtrayicon.c				\
	eggtrayicon.h				\
	disclosure-widget.c			\
	disclosure-widget.h

gossip_LDADD = @GOSSIP_LIBS@ $(XSS_LIBS)
gossip_LDFLAGS = -export-dynamic

gossip-marshal.h: gossip-marshal.list
	(cd $(srcdir) && \
	@GLIB_GENMARSHAL@ --prefix=gossip_marshal gossip-marshal.list --header) > xgen-gmh \
	&& (cmp -s xgen-gmh gossip-marshal.h || cp xgen-gmh gossip-marshal.h) \
	&& rm -f xgen-gmh xgen-gmh~

gossip-marshal.c: gossip-marshal.list
	(cd $(srcdir) && \
	@GLIB_GENMARSHAL@ --prefix=gossip_marshal gossip-marshal.list --body) > xgen-gmc \
	&& cp xgen-gmc gossip-marshal.c \
	&& rm -f xgen-gmc xgen-gmc~

gossip-marshal-main.c: gossip-marshal.c gossip-marshal.h

BUILT_SOURCES = 					\
	gossip-marshal.h				\
	gossip-marshal.c

EXTRA_DIST =						\
	gossip-marshal.list

CLEANFILES = $(BUILT_SOURCES)
