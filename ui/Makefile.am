SUBDIRS = pixmaps

if HAVE_PLATFORM_WIN32
SUBDIRS += icons
endif

gladedir = $(datadir)/gossip
glade_DATA = 				\
	chat.glade                      \
	group-chat.glade		\
	file-transfer.glade		\
	main.glade

EXTRA_DIST = $(glade_DATA)
