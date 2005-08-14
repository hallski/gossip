sounddir = $(datadir)/sounds/gossip
sound_DATA = 		\
	chat1.wav	\
	chat2.wav	\
	offline.wav	\
	online.wav	\
	send.wav

soundlist_in_files = gossip.soundlist.in
soundlistdir = $(sysconfdir)/sound/events
soundlist_DATA = $(soundlist_in_files:.soundlist.in=.soundlist)
@INTLTOOL_SOUNDLIST_RULE@

EXTRA_DIST = 		\
	$(sound_DATA) 	\
	$(soundlist_in_files)

CLEANFILES  = gossip.soundlist
