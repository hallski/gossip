INCLUDES = 							\
	-DPKGDATADIR=\""$(pkgdatadir)"\"	 		\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DIMAGEDIR=\"$(datadir)/gossip\"			\
	$(PEEKABOO_CFLAGS) $(WARN_CFLAGS)

libexec_PROGRAMS = peekaboo-applet
peekaboo_applet_SOURCES = 					\
	peekaboo-applet.c 	peekaboo-applet.h		\
	peekaboo-dbus.c 	peekaboo-dbus.h			\
	peekaboo-galago.c 	peekaboo-galago.h		\
	peekaboo-stock.c 	peekaboo-stock.h		\
	peekaboo-utils.c 	peekaboo-utils.h	

peekaboo_applet_LDFLAGS = 	\
	$(PEEKABOO_LIBS)
