imagedir = $(datadir)/gossip

image_DATA =				\
	gossip-logo.png			\
	gossip-offline.png		\
	gossip-available.png    	\
	gossip-busy.png			\
	gossip-away.png			\
	gossip-extended-away.png 	\
	gossip-pending.png 		\
	gossip-message.png		\
	gossip-typing.png		\
	gossip-group-message.png 	\
	vcard_16.png			\
	vcard_48.png

icon16dir = $(datadir)/icons/hicolor/16x16/apps
icon16_DATA = 16x16/gossip.png

icon22dir = $(datadir)/icons/hicolor/22x22/apps
icon22_DATA = 22x22/gossip.png

icon32dir = $(datadir)/icons/hicolor/32x32/apps
icon32_DATA = 32x32/gossip.png

icon48dir = $(datadir)/icons/hicolor/48x48/apps
icon48_DATA = 48x48/gossip.png

svgicondir = $(datadir)/icons/hicolor/scalable/apps
svgicon_DATA = gossip.svg

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

EXTRA_DIST = 		\
	$(icon16_DATA)  \
	$(icon22_DATA)  \
	$(icon32_DATA)  \
	$(svgicon_DATA) \
	$(image_DATA)


