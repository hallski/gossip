SUBDIRS = sounds

INCLUDES =	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DPREFIX="\"$(prefix)"\"		\
	-DSYSCONFDIR=\""$(sysconfdir)"\"        \
	-DDATADIR=\""$(datadir)"\"              \
	-DLIBDIR=\""$(libdir)"\"		\
	@GOSSIP_CFLAGS@				\
	-DGLADEDIR=\"$(datadir)/gossip\"	\
	-DIMAGEDIR=\"$(datadir)/gossip\"	\
	-DSTYLESHEETDIR=\"$(datadir)/gossip\"	\
	-DDBUS_API_SUBJECT_TO_CHANGE

if HAVE_DBUS
dbus_sources = gossip-dbus.c gossip-dbus.h
endif

bin_PROGRAMS = gossip

gossip_SOURCES = 				\
	$(dbus_sources)				\
	gossip-about.c				\
	gossip-about.h				\
	gossip-account.c			\
	gossip-account.h			\
	gossip-account-dialog.c			\
	gossip-account-dialog.h			\
	gossip-add-contact.c			\
	gossip-add-contact.h			\
	gossip-app.c				\
	gossip-app.h				\
	gossip-chat.c				\
	gossip-chat.h				\
	gossip-chat-view.c                      \
	gossip-chat-view.h                      \
	gossip-chat-window.c			\
	gossip-chat-window.h			\
	gossip-connect-dialog.c			\
	gossip-connect-dialog.h			\
	gossip-contact.c                        \
	gossip-contact.h                        \
	gossip-contact-info.c                   \
	gossip-contact-info.h                   \
	gossip-contact-list.c                   \
	gossip-contact-list.h                   \
	gossip-favorite.c			\
	gossip-favorite.h			\
	gossip-favorites-dialog.c		\
	gossip-favorites-dialog.h		\
	gossip-group-chat.c			\
	gossip-group-chat.h			\
	gossip-idle.c				\
	gossip-idle.h				\
	gossip-jid.c                            \
	gossip-jid.h                            \
	gossip-join-dialog.c			\
	gossip-join-dialog.h			\
	gossip-log.c				\
	gossip-log.h				\
	gossip-main.c				\
	gossip-marshal-main.c			\
	gossip-notebook.c			\
	gossip-notebook.h			\
	gossip-preferences.c			\
	gossip-preferences.h	                \
	gossip-presence.c                       \
	gossip-presence.h                       \
	gossip-roster.c                         \
	gossip-roster.h                         \
	gossip-roster-view.c                    \
	gossip-roster-view.h                    \
	gossip-register.c			\
	gossip-register.h			\
	gossip-sound.c				\
	gossip-sound.h				\
	gossip-startup-druid.c			\
	gossip-startup-druid.h			\
	gossip-stock.c				\
	gossip-stock.h				\
	gossip-utils.c				\
	gossip-utils.h				\
	gossip-edit-groups-dialog.c		\
	gossip-edit-groups-dialog.h		\
						\
	eel-ellipsizing-label.c			\
	eel-ellipsizing-label.h			\
	eggtrayicon.c				\
	eggtrayicon.h				\
	disclosure-widget.c			\
	disclosure-widget.h

gossip_LDADD = @GOSSIP_LIBS@
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

stylesheetdir = $(datadir)/gossip
stylesheet_DATA = gossip-log.xsl

EXTRA_DIST =						\
	gossip-marshal.list				\
	gossip-log.xsl					\
	$(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)
