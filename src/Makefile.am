SUBDIRS = sounds

AM_CPPFLAGS =	                                 	\
	-I$(top_srcdir)                          	\
	-DPREFIX="\"$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"              	\
	-DLIBDIR=\""$(libdir)"\"			\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	$(GOSSIP_CFLAGS) $(WARN_CFLAGS) $(BACKEND_CFLAGS)

if HAVE_DBUS
dbus_binding_sources = gossip-dbus.xml

gossip-dbus-glue.h: $(dbus_binding_sources)
	$(LIBTOOL) --mode=execute $(DBUSBINDINGTOOL) --prefix=gossip_dbus \
	--mode=glib-server --output=gossip-dbus-glue.h $(srcdir)/$(dbus_binding_sources)

dbus_sources = gossip-dbus.c gossip-dbus.h
dbus_generated = gossip-dbus-glue.h
endif

if HAVE_GALAGO
galago_sources = gossip-galago.c gossip-galago.h
endif

if HAVE_LIBNOTIFY
libnotify_sources = gossip-notify.c gossip-notify.h
endif

if HAVE_TELEPATHY
telepathy_sources = gossip-protocol-chooser.c gossip-protocol-chooser.h
endif

bin_PROGRAMS = gossip

gossip_SOURCES = 				                        	\
	$(dbus_sources)			     	                        	\
	$(galago_sources)		     	                        	\
	$(libnotify_sources)		     	                        	\
	$(telepathy_sources)		     	                        	\
	ephy-spinner.c                     ephy-spinner.h			\
	gossip-about-dialog.c              gossip-about-dialog.h		\
	gossip-accounts-dialog.c           gossip-accounts-dialog.h		\
	gossip-account-chooser.c	   gossip-account-chooser.h		\
	gossip-account-widget-jabber.c	   gossip-account-widget-jabber.h	\
	gossip-account-widget-msn.c	   gossip-account-widget-msn.h		\
	gossip-account-widget-generic.c	   gossip-account-widget-generic.h	\
	gossip-add-contact-dialog.c        gossip-add-contact-dialog.h		\
	gossip-app.c	 	           gossip-app.h				\
	gossip-avatar-image.c		   gossip-avatar-image.h		\
	gossip-cell-renderer-expander.c    gossip-cell-renderer-expander.h      \
	gossip-cell-renderer-text.c        gossip-cell-renderer-text.h          \
	gossip-chat.c		           gossip-chat.h			\
	gossip-chat-invite.c	           gossip-chat-invite.h			\
	gossip-chat-manager.c              gossip-chat-manager.h               	\
	gossip-chat-view.c                 gossip-chat-view.h                  	\
	gossip-chat-window.c	           gossip-chat-window.h			\
	gossip-chatrooms-window.c          gossip-chatrooms-window.h		\
	gossip-contact-groups.c	           gossip-contact-groups.h		\
	gossip-contact-list.c              gossip-contact-list.h               	\
	gossip-contact-list-iface.c        gossip-contact-list-iface.h         	\
	gossip-contact-info-dialog.c       gossip-contact-info-dialog.h         \
	gossip-edit-chatroom-dialog.c      gossip-edit-chatroom-dialog.h	\
	gossip-edit-contact-dialog.c       gossip-edit-contact-dialog.h		\
	gossip-ft-window.c		   gossip-ft-window.h			\
	gossip-geometry.c		   gossip-geometry.h			\
	gossip-group-chat.c	           gossip-group-chat.h			\
	gossip-idle.c		           gossip-idle.h			\
	gossip-image-chooser.c             gossip-image-chooser.h		\
	gossip-log-window.c	           gossip-log-window.h			\
	gossip-main.c				                        	\
	gossip-marshal-main.c                                                   \
	gossip-new-chatroom-dialog.c       gossip-new-chatroom-dialog.h		\
	gossip-new-message-dialog.c        gossip-new-message-dialog.h		\
	gossip-popup-button.c              gossip-popup-button.h		\
	gossip-presence-chooser.c          gossip-presence-chooser.h        	\
	gossip-preferences.c               gossip-preferences.h	        	\
	gossip-private-chat.c              gossip-private-chat.h               	\
	gossip-sound.c		           gossip-sound.h			\
	gossip-stock.c		           gossip-stock.h			\
	gossip-spell.c                     gossip-spell.h                       \
	gossip-spell-dialog.c 		   gossip-spell-dialog.h                \
	gossip-status-presets.c		   gossip-status-presets.h	       	\
	gossip-subscription-dialog.c       gossip-subscription-dialog.h		\
	gossip-ui-utils.c	           gossip-ui-utils.h			\
	gossip-vcard-dialog.c	    	   gossip-vcard-dialog.h		\
	gossip-theme-manager.c	    	   gossip-theme-manager.h		


gossip_LDADD = \
	$(top_builddir)/libgossip/libgossip.la					\
	$(GOSSIP_LIBS)								\
	$(XSS_LIBS)

gossip_LDFLAGS = $(BACKEND_LDFLAGS)

dtddir = $(datadir)/gossip
dtd_DATA = 									\
	gossip-contact-groups.dtd						\
	gossip-status-presets.dtd

gossip-marshal.h: gossip-marshal.list
	(cd $(srcdir) && \
	$(GLIB_GENMARSHAL) --prefix=gossip_marshal gossip-marshal.list --header) > xgen-gmh \
	&& (cmp -s xgen-gmh gossip-marshal.h || cp xgen-gmh gossip-marshal.h) \
	&& rm -f xgen-gmh xgen-gmh~

gossip-marshal.c: gossip-marshal.list
	(cd $(srcdir) && \
	$(GLIB_GENMARSHAL) --prefix=gossip_marshal gossip-marshal.list --body) > xgen-gmc \
	&& cp xgen-gmc gossip-marshal.c \
	&& rm -f xgen-gmc xgen-gmc~

gossip-marshal-main.c: gossip-marshal.c gossip-marshal.h

BUILT_SOURCES = 					\
	gossip-marshal.h				\
	gossip-marshal.c				\
	$(dbus_generated)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST =						\
	gossip-marshal.list                             \
	$(dtd_DATA)					\
	$(dbus_sources)					\
	$(dbus_binding_sources)				\
	$(galago_sources)				\
	$(libnotify_sources)

