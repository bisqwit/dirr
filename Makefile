VERSION = 3.26

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

CPP=gcc
CXX=g++
CPPFLAGS=-Wall -W -pedantic -DVERSION=\"$(VERSION)\" -pipe
CXXFLAGS=-g
#-O3 -fomit-frame-pointer
LDFLAGS=
#-s
BINDIR=/usr/local/bin
INSTALL=install

PROG=dirr
OBJS=dirr.o pwfun.o wildmatch.o cons.o setfun.o strfun.o colouring.o \
     getname.o getsize.o totals.o argh.o

ARCHDIR=archives/
ARCHNAME=dirr-$(VERSION)
ARCHFILES=dirr.cc COPYING ChangeLog README dirrsets.hh \
          configure config.sub1 config.sub2 config.sub3 \
          wildmatch.cc wildmatch.hh \
          colouring.cc colouring.hh \
          getname.cc getname.hh \
          getsize.cc getsize.hh \
          setfun.cc setfun.hh \
          strfun.cc strfun.hh \
          totals.cc totals.hh \
          pwfun.cc pwfun.hh \
          cons.cc cons.hh \
          argh.cc argh.hh \
          TODO

INSTALLPROGS=$(PROG)

${PROG}: ${OBJS}
	$(CXX) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(PROG) ${OBJS}
distclean: clean
	rm -f Makefile.cfg config.h *~
realclean: distclean

include depfun.mak