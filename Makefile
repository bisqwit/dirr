VERSION = 3.14

# Explanations:
#   CACHE_GETSET     Recommended, adds speed
#   CACHE_NAMECOLOR  This too
#   CACHE_UIDGID     This too
#   NEW_WILDMATCH    This too, disable if you have problems with it
#   SUPPORT_BRACKETS They can be disabled
#   STARTUP_COUNTER  ...if you want a progress meter
#   PRELOAD_UIDGID   Set this to 1 if your passwd file is quick to load
#   HAVE_STATFS      Disable it if you have no statfs() function.

DEFINES = -DCACHE_GETSET=1 \
          -DCACHE_NAMECOLOR=1 \
          -DCACHE_UIDGID=1 \
          -DNEW_WILDMATCH=1 \
          -DSUPPORT_BRACKETS=1 \
          -DSTARTUP_COUNTER=0 \
          -DPRELOAD_UIDGID=0 \
          -DHAVE_STATFS=1

GXX=g++
CPPFLAGS=-Wall -W -pedantic -DVERSION=\"$(VERSION)\" $(DEFINES)
CXXFLAGS=-O -g
LDFLAGS=-g

BINDIR=/usr/local/bin

ARCHNAME=dirr-$(VERSION)
ARCHFILES=dirr.cc oldversions COPYING ChangeLog

dirr: dirr.o
	$(CXX) $(LDFLAGS) -o $@ $^

install: dirr
	cp -p dirr ${BINDIR}/
	strip ${BINDIR}/dirr

include depfun.mak
