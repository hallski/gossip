if USE_TELEPATHY
SUBDIRS = telepathy
else
SUBDIRS = jabber
endif

protocolsdir = $(datadir)/gossip/protocols

protocols_DATA =		\
	gossip-protocol.dtd	\
	aim.protocol 		\
	icq.protocol 		\
	msn.protocol 		\
	yahoo.protocol 	

EXTRA_DIST = $(protocols_DATA)
