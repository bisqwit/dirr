VERSION = 3.18

# Obligated defines:
#   CACHE_GETSET     Recommended, adds speed
#   CACHE_NAMECOLOR  This too
#   CACHE_UIDGID     This too
#   NEW_WILDMATCH    Recommended, disable if you have problems with it
#   SUPPORT_BRACKETS They can be disabled, but I don't see why.
#   STARTUP_COUNTER  Enable, if you want a progress meter
#   PRELOAD_UIDGID   Set this to 1 if your passwd file is quick to load
#   HAVE_STATFS      Disable it if you have no statfs() function.
#   SETTINGSFILE     File containing the settings. Default: dirrsets.hh

include Makefile.cfg

DEFINES = -DCACHE_GETSET=1 \
          -DCACHE_NAMECOLOR=1 \
          -DCACHE_UIDGID=1 \
          -DNEW_WILDMATCH=1 \
          -DSUPPORT_BRACKETS=1 \
          -DSTARTUP_COUNTER=0 \
          -DPRELOAD_UIDGID=0 \
          -DHAVE_STATFS=$(HAVE_STATFS) \
          -DSETTINGSFILE=\"dirrsets.hh\"

CPP=gcc
CXX=g++
CPPFLAGS=-Wall -W -pedantic -DVERSION=\"$(VERSION)\" $(DEFINES)
CXXFLAGS=-O3 -fomit-frame-pointer
LDFLAGS=-s

BINDIR=/usr/local/bin

ARCHNAME=dirr-$(VERSION)
ARCHFILES=dirr.cc COPYING ChangeLog README dirrsets.hh \
          configure config.sub1 config.sub2 config.sub3

dirr: dirr.o
	$(CXX) $(LDFLAGS) -o $@ $^

clean:
	rm -f dirr dirr.o
distclean: clean
	rm -f Makefile.cfg config.h *~
realclean: distclean

install: dirr
	cp -p dirr ${BINDIR}/

include depfun.mak
