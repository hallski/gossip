imagedir = $(datadir)/gossip

image_DATA =			\
	gossip-available.png    \
	gossip-away.png		\
	gossip-busy.png		\
	gossip-extended-away.png \
	gossip-message.png	\
	gossip-group-message.png \
	gossip-offline.png	\
	gossip-typing.png	\
	channel-old4.png	\
	my-user_48.png		\
	query_48.png		\
	watermark.png		\
	vcard_16.png		\
	vcard_48.png

icondir = $(datadir)/icons/hicolor/48x48/apps
icon_DATA = gossip.png
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

EXTRA_DIST = $(image_DATA) $(icon_DATA) $(svgicon_DATA)


