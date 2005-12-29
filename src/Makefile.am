SUBDIRS = sounds

AM_CPPFLAGS =	                                 	\
	-I$(top_srcdir)                          	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DPREFIX="\"$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"        	\
	-DDATADIR=\""$(datadir)"\"              	\
	-DLIBDIR=\""$(libdir)"\"			\
	-DGLADEDIR=\"$(datadir)/gossip\"		\
	-DIMAGEDIR=\"$(datadir)/gossip\"		\
	-DPROTOCOLSDIR=\"$(datadir)/gossip/protocols\" 	\
	-DSTYLESHEETDIR=\"$(datadir)/gossip\"		\
	-DDTDDIR=\"$(datadir)/gossip\"			\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	@GOSSIP_CFLAGS@					\
	@GTK_VER@


if HAVE_DBUS
dbus_sources =									\
	gossip-dbus.c                  	    gossip-dbus.h
endif

if HAVE_GALAGO
galago_sources =			                                        \
	gossip-galago.c                     gossip-galago.h
endif


bin_PROGRAMS = gossip

gossip_SOURCES = 				                        	\
	$(dbus_sources)			     	                        	\
	$(galago_sources)		     	                        	\
	gossip-about.c                     gossip-about.h  			\
	gossip-accounts-dialog.c           gossip-accounts-dialog.h		\
	gossip-account-button.c		   gossip-account-button.h		\
	gossip-account-chooser.c	   gossip-account-chooser.h		\
	gossip-add-contact-window.c        gossip-add-contact-window.h		\
	gossip-app.c	 	           gossip-app.h				\
	gossip-cell-renderer-text.c        gossip-cell-renderer-text.h          \
	gossip-chat.c		           gossip-chat.h			\
	gossip-chat-invite.c	           gossip-chat-invite.h			\
	gossip-chat-manager.c              gossip-chat-manager.h               	\
	gossip-chat-view.c                 gossip-chat-view.h                  	\
	gossip-chat-window.c	           gossip-chat-window.h			\
	gossip-chatrooms-dialog.c          gossip-chatrooms-dialog.h		\
	gossip-contact-groups.c	           gossip-contact-groups.h		\
	gossip-contact-list.c              gossip-contact-list.h               	\
	gossip-contact-list-iface.c        gossip-contact-list-iface.h         	\
	gossip-contact-info-dialog.c       gossip-contact-info-dialog.h         \
	gossip-ft-window.c		   gossip-ft-window.h			\
	gossip-group-chat.c	           gossip-group-chat.h			\
	gossip-idle.c		           gossip-idle.h			\
	gossip-log.c		           gossip-log.h				\
	gossip-main.c				                        	\
	gossip-marshal-main.c                                                   \
	gossip-new-account-window.c        gossip-new-account-window.h		\
	gossip-new-message-dialog.c        gossip-new-message-dialog.h		\
	gossip-notebook.c	           gossip-notebook.h			\
	gossip-presence-chooser.c          gossip-presence-chooser.h        	\
	gossip-preferences.c               gossip-preferences.h	        	\
	gossip-private-chat.c              gossip-private-chat.h               	\
	gossip-register.c	           gossip-register.h			\
	gossip-sound.c		           gossip-sound.h			\
	gossip-stock.c		           gossip-stock.h			\
	gossip-spell.c                     gossip-spell.h                       \
	gossip-spell-dialog.c 		   gossip-spell-dialog.h                \
	gossip-status-presets.c		   gossip-status-presets.h	       	\
	gossip-subscription-dialog.c       gossip-subscription-dialog.h		\
	gossip-ui-utils.c	           gossip-ui-utils.h			\
	gossip-edit-groups-dialog.c        gossip-edit-groups-dialog.h		\
	gossip-vcard-dialog.c	    	   gossip-vcard-dialog.h		\
	gossip-theme-manager.c	    	   gossip-theme-manager.h		\
	eggtrayicon.c		           eggtrayicon.h

gossip_LDADD = @GOSSIP_LIBS@                            			\
	$(top_builddir)/libgossip/libgossip.la

gossip_LDFLAGS = -export-dynamic

dtddir = $(datadir)/gossip
dtd_DATA = 									\
	gossip-contact-groups.dtd						\
	gossip-status-presets.dtd

stylesheetdir = $(datadir)/gossip
stylesheet_DATA = gossip-log.xsl

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
	gossip-marshal.c

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST =						\
	gossip-log.xsl					\
	gossip-marshal.list                             \
	$(dtd_DATA)					\
	$(dbus_sources)

