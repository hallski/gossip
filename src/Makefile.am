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
	-DPROTOCOLSDIR=\"$(datadir)/gossip/protocols\" 	\
	-DSTYLESHEETDIR=\"$(datadir)/gossip\"	\
	-DDBUS_API_SUBJECT_TO_CHANGE

if HAVE_DBUS
dbus_sources = gossip-dbus.c gossip-dbus.h
endif

bin_PROGRAMS = gossip

gossip_SOURCES = 				                        	\
	$(dbus_sources)			     	                        	\
	gossip-about.c                     gossip-about.h  			\
	gossip-account.c                   gossip-account.h			\
	gossip-account-dialog.c	           gossip-account-dialog.h		\
	gossip-add-contact.c	           gossip-add-contact.h			\
	gossip-app.c	 	           gossip-app.h				\
	gossip-async.h                                                  	\
	gossip-cell-renderer-text.c        gossip-cell-renderer-text.h          \
	gossip-chat.c		           gossip-chat.h			\
	gossip-chat-manager.c              gossip-chat-manager.h               	\
	gossip-chatroom-provider.c         gossip-chatroom-provider.h          	\
	gossip-chat-view.c                 gossip-chat-view.h                  	\
	gossip-chat-window.c	           gossip-chat-window.h			\
	gossip-connect-dialog.c	           gossip-connect-dialog.h		\
	gossip-contact.c                   gossip-contact.h                    	\
	gossip-contact-info.c              gossip-contact-info.h               	\
	gossip-contact-list.c              gossip-contact-list.h               	\
	gossip-contact-list-iface.c        gossip-contact-list-iface.h         	\
	gossip-event-manager.c             gossip-event-manager.h              	\
	gossip-event.c                     gossip-event.h                      	\
	gossip-favorite.c  	           gossip-favorite.h			\
	gossip-favorites-dialog.c          gossip-favorites-dialog.h		\
	gossip-group-chat.c	           gossip-group-chat.h			\
	gossip-idle.c		           gossip-idle.h			\
	gossip-jabber.c                    gossip-jabber.h                     	\
	gossip-jabber-chatrooms.c          gossip-jabber-chatrooms.h           	\
	gossip-jabber-helper.c             gossip-jabber-helper.h              	\
	gossip-jid.c                       gossip-jid.h                        	\
	gossip-join-dialog.c	           gossip-join-dialog.h			\
	gossip-log.c		           gossip-log.h				\
	gossip-main.c				                        	\
	gossip-marshal-main.c			                        	\
	gossip-message.c                   gossip-message.h                    	\
	gossip-notebook.c	           gossip-notebook.h			\
	gossip-preferences.c               gossip-preferences.h	        	\
	gossip-presence.c                  gossip-presence.h                   	\
	gossip-private-chat.c              gossip-private-chat.h               	\
	gossip-protocol.c                  gossip-protocol.h                   	\
	gossip-register.c	           gossip-register.h			\
	gossip-session.c                   gossip-session.h                    	\
	gossip-sound.c		           gossip-sound.h			\
	gossip-startup-druid.c	           gossip-startup-druid.h		\
	gossip-stock.c		           gossip-stock.h			\
	gossip-time.c                      gossip-time.h                        \
	gossip-utils.c		           gossip-utils.h			\
	gossip-edit-groups-dialog.c        gossip-edit-groups-dialog.h		\
	gossip-version-info.c              gossip-version-info.h               	\
	gossip-vcard.c                     gossip-vcard.h                      	\
	gossip-vcard-dialog.c	    	   gossip-vcard-dialog.h		\
	gossip-transport-accounts.c        gossip-transport-accounts.h		\
	gossip-transport-accounts-window.c gossip-transport-accounts-window.h	\
	gossip-transport-add-window.c      gossip-transport-add-window.h	\
	gossip-transport-discover.c        gossip-transport-discover.h		\
	gossip-transport-protocol.c        gossip-transport-protocol.h		\
	gossip-transport-register.c   	   gossip-transport-register.h	   	\
	eel-ellipsizing-label.c	    	   eel-ellipsizing-label.h		\
	eggtrayicon.c		           eggtrayicon.h

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
