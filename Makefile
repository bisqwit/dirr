include Makefile.sets

# Building for Windows (opt/xmingw is for Gentoo):
#HOST=/usr/local/mingw32/bin/i586-mingw32msvc-
#HOST=/opt/xmingw/bin/i386-mingw32msvc-
#CFLAGS +=
#CPPFLAGS +=
#CXXFLAGS +=
##LDOPTS = -L/usr/local/mingw32/lib
#LDOPTS += -L/opt/xmingw/lib
#LDFLAGS +=

# Building for native:
#HOST=
#LDFLAGS +=

#CXX=$(HOST)g++
#CC=$(HOST)gcc
#CPP=$(HOST)gcc

#CXX += -flto
#CXX += -pg -flto
#OPTIM= -O2 -finline

#OPTIM=-Og -fno-inline
#CXXFLAGS += -fsanitize=address

VERSION = 3.36.5

BINDIR=/usr/local/bin
INSTALL=install

PROG=dirr
OBJS=main.o pwfun.o cons.o setfun.o strfun.o colouring.o \
     getname.o getsize.o totals.o argh.o \
     dfa_match.o printf.o

ARCHDIR=archives/
ARCHNAME=dirr-$(VERSION)
ARCHFILES=main.cc COPYING ChangeLog README dirrsets.hh config.h \
          configure config.sub1 config.sub2 config.sub3 \
          dfa_match.cc dfa_match.hh \
          colouring.cc colouring.hh \
          getname.cc getname.hh \
          getsize.cc getsize.hh \
          setfun.cc setfun.hh \
          strfun.cc strfun.hh \
          totals.cc totals.hh \
          pwfun.cc pwfun.hh \
          cons.cc cons.hh \
          argh.cc argh.hh \
          printf.cc printf.hh \
          stat.h likely.hh \
          TODO progdesc.php \
          Makefile.sets.in \
          Makefile.sets \
          dfa_match.cc \
          dfa_match.hh

INSTALLPROGS=$(PROG)

$(PROG): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

argh.o: argh.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -DColourPrints -o $@ -c $<

clean:
	rm -f $(PROG) $(OBJS)
distclean: clean
	rm -f Makefile.cfg config.h *~
realclean: distclean

include depfun.mak
