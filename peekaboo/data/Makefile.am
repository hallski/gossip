# Panel applet bonobo stuff
serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_Peekaboo_Applet.server.in
server_DATA = $(server_in_files:.server.in=.server)


# GConf Schemas
#schemadir = $(GCONF_SCHEMA_FILE_DIR)
#schema_in_files = peekaboo-applet.schemas.in
#schema_DATA = $(schema_in_files:.schemas.in=.schemas)

# Misc data
resourcesdir = $(pkgdatadir)
resources_DATA = \
	GNOME_Peekaboo_Applet.xml
# 	peekaboo-applet.glade

peekaboobindir = $(libexecdir)

# Build rules
@INTLTOOL_SERVER_RULE@
@INTLTOOL_SCHEMAS_RULE@

$(server_in_files): $(server_in_files:.server.in=.server.in.in)
	sed -e "s|\@LIBEXECDIR\@|$(peekaboobindir)|" $< > $@

DISTCLEANFILES = \
	$(server_DATA) \
	$(server_in_files)

# 	$(schema_DATA)

EXTRA_DIST = \
	GNOME_Peekaboo_Applet.server.in.in \
	$(resources_DATA)

#	$(schema_in_files)
