icon = gossip.ico
resource_file = resource.rc
resource_object = resource.o

noinst_DATA = $(resource_file) $(icon)
icondir = $(datadir)/gossip

RC = @WINDRES@
RCFLAGS = --use-temp-file --verbose -O COFF

$(resource_object): $(resource_file) $(icon)
	$(RC) $(RCFLAGS) -o $@ $<

all: $(resource_object)

EXTRA_DIST = $(noinst_DATA)

CLEANFILES = $(resource_object)
