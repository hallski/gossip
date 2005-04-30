SUBDIRS = sounds

AM_CPPFLAGS =	                                 \
	-I$(top_srcdir)                          \
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
dbus_sources =                                                         \
       gossip-dbus.c                   gossip-dbus.h
endif

bin_PROGRAMS = gossip

gossip_SOURCES = 				                        	\
	$(dbus_sources)			     	                        	\
	gossip-about.c                     gossip-about.h  			\
	gossip-account-dialog.c	           gossip-account-dialog.h		\
	gossip-add-contact.c	           gossip-add-contact.h			\
	gossip-app.c	 	           gossip-app.h				\
	gossip-cell-renderer-text.c        gossip-cell-renderer-text.h          \
	gossip-chat.c		           gossip-chat.h			\
	gossip-chat-manager.c              gossip-chat-manager.h               	\
	gossip-chat-view.c                 gossip-chat-view.h                  	\
	gossip-chat-window.c	           gossip-chat-window.h			\
	gossip-connect-dialog.c	           gossip-connect-dialog.h		\
	gossip-contact-list.c              gossip-contact-list.h               	\
	gossip-contact-list-iface.c        gossip-contact-list-iface.h         	\
	gossip-contact-info.c              gossip-contact-info.h                \
	gossip-favorite.c  	           gossip-favorite.h			\
	gossip-favorites-dialog.c          gossip-favorites-dialog.h		\
	gossip-group-chat.c	           gossip-group-chat.h			\
	gossip-idle.c		           gossip-idle.h			\
	gossip-join-dialog.c	           gossip-join-dialog.h			\
	gossip-log.c		           gossip-log.h				\
	gossip-main.c				                        	\
	gossip-notebook.c	           gossip-notebook.h			\
	gossip-preferences.c               gossip-preferences.h	        	\
	gossip-private-chat.c              gossip-private-chat.h               	\
	gossip-register.c	           gossip-register.h			\
	gossip-sound.c		           gossip-sound.h			\
	gossip-startup-druid.c	           gossip-startup-druid.h		\
	gossip-stock.c		           gossip-stock.h			\
	gossip-spell.c                     gossip-spell.h                       \
	gossip-spell-dialog.c 		   gossip-spell-dialog.h                \
	gossip-ui-utils.c	           gossip-ui-utils.h			\
	gossip-edit-groups-dialog.c        gossip-edit-groups-dialog.h		\
	gossip-vcard-dialog.c	    	   gossip-vcard-dialog.h		\
	eel-ellipsizing-label.c	    	   eel-ellipsizing-label.h		\
	eggtrayicon.c		           eggtrayicon.h

gossip_LDADD = @GOSSIP_LIBS@                            \
	       $(top_builddir)/libgossip/libgossip.la

gossip_LDFLAGS = -export-dynamic

stylesheetdir = $(datadir)/gossip
stylesheet_DATA = gossip-log.xsl

EXTRA_DIST =						\
	gossip-log.xsl					\
	$(dbus_sources)

