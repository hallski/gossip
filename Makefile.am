SUBDIRS = ui libloudermouth libgossip src po peekaboo

if HAVE_HELP
SUBDIRS += help
endif

desktop_in_files=gossip.desktop.in
desktop_files=$(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

Applicationsdir = $(datadir)/applications
Applications_DATA = $(desktop_files)

schemasdir	 = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = gossip.schemas.in
schemas_DATA 	 = $(schemas_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

INTLTOOL = 			\
	intltool-extract.in 	\
	intltool-merge.in 	\
	intltool-update.in

EXTRA_DIST = 			\
        ChangeLog               \
        README                  \
	CONTRIBUTORS            \
	xmldocs.make            \
	omf.make                \
	gnome-doc-utils.make	\
	$(desktop_in_files)	\
	$(desktop_files)	\
	$(schemas_in_files)	\
	$(schemas_DATA)		\
	$(INTLTOOL)

DISTCLEANFILES = 		\
	intltool-extract	\
	intltool-merge		\
	intltool-update		\
	gnome-doc-utils.make	\
	$(desktop_files)	\
	$(schemas_DATA)

DISTCHECK_CONFIGURE_FLAGS =	\
	--disable-scrollkeeper

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
	  for p in $(schemas_DATA) ; do \
	    GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$$p ; \
	  done \
	fi
else
install-data-local:
endif

# Workaround broken scrollkeeper that doesn't remove its files on
# uninstall.
distuninstallcheck_listfiles = find . -type f -print | grep -v '^\./var/scrollkeeper'
