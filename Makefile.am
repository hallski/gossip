SUBDIRS = ui src po

@INTLTOOL_DESKTOP_RULE@

desktop_in_files=gossip.desktop.in
desktop_files=$(desktop_in_files:.desktop.in=.desktop)

Applicationsdir = $(datadir)/applications
Applications_DATA = $(desktop_files)

INTLTOOL = 			\
	intltool-extract.in 	\
	intltool-merge.in 	\
	intltool-update.in

EXTRA_DIST = 			\
        ChangeLog               \
        README                  \
	gossip.spec		\
	gossip.spec.in		\
	$(desktop_in_files)	\
	$(desktop_files)	\
	$(INTLTOOL)

DISTCLEANFILES = 		\
	intltool-extract	\
	intltool-merge		\
	intltool-update		\
	$(desktop_files)
