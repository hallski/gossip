INCLUDES = 							\
	$(GTK_VER) $(PANEL_VER) 				\
	-DPKGDATADIR=\""$(pkgdatadir)"\"	 		\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	$(PEEKABOO_CFLAGS)

libexec_PROGRAMS = peekaboo-applet
peekaboo_applet_SOURCES = 	\
	peekaboo-applet.c 	\
	peekaboo-applet.h

peekaboo_applet_LDFLAGS = 	\
	$(PEEKABOO_LIBS)
