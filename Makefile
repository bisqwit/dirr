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

CPPFLAGS += -Ire2c

VERSION = 3.33

# Obligatory defines:
#   CACHE_GETSET     Recommended, adds speed
#   CACHE_NAMECOLOR  This too
#   CACHE_UIDGID     This too
#   NEW_WILDMATCH    Recommended, disable if you have problems with it
#   SUPPORT_BRACKETS They can be disabled, but I don't see why.
#   STARTUP_COUNTER  Enable, if you want a progress meter
#   PRELOAD_UIDGID   Set this to 1 if your passwd file is quick to load
#   HAVE_STATFS      Disable it if you have no statfs() function.
#   SETTINGSFILE     File containing the settings. Default: dirrsets.hh

#-s
BINDIR=/usr/local/bin
INSTALL=install

PROG=dirr
OBJS=main.o pwfun.o cons.o setfun.o strfun.o colouring.o \
     getname.o getsize.o totals.o argh.o \
     \
     dfa_match.o \
     re2c/src/ir/rule_rank.o \
     re2c/src/ir/dfa/minimization.o \
     re2c/src/ir/dfa/determinization.o \
     re2c/src/ir/dfa/fillpoints.o \
     re2c/src/ir/nfa/nfa.o \
     re2c/src/ir/nfa/calc_size.o \
     re2c/src/ir/nfa/split.o \
     re2c/src/ir/regexp/regexp.o \
     re2c/src/ir/regexp/fixed_length.o \
     re2c/src/ir/regexp/display.o \
     re2c/src/ir/regexp/encoding/enc.o \
     re2c/src/ir/regexp/encoding/range_suffix.o \
     re2c/src/util/range.o \
     re2c/src/codegen/label.o \
     re2c/src/codegen/go_construct.o \
     re2c/src/codegen/go_destruct.o \
     re2c/src/codegen/bitmap.o \
     re2c/src/ir/adfa/prepare.o \
     re2c/src/ir/adfa/adfa.o

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
          stat.h \
          TODO progdesc.php \
          Makefile.sets.in \
          Makefile.sets \
          re2c/src/codegen/bitmap.cc \
          re2c/src/codegen/bitmap.h \
          re2c/src/codegen/go.h \
          re2c/src/codegen/go_construct.cc \
          re2c/src/codegen/go_destruct.cc \
          re2c/src/codegen/input_api.h \
          re2c/src/codegen/label.cc \
          re2c/src/codegen/label.h \
          re2c/src/codegen/output.h \
          re2c/src/conf/opt.h \
          re2c/src/conf/warn.h \
          re2c/src/globals.h \
          re2c/src/ir/adfa/action.h \
          re2c/src/ir/adfa/adfa.cc \
          re2c/src/ir/adfa/adfa.h \
          re2c/src/ir/adfa/prepare.cc \
          re2c/src/ir/dfa/determinization.cc \
          re2c/src/ir/dfa/dfa.h \
          re2c/src/ir/dfa/fillpoints.cc \
          re2c/src/ir/dfa/minimization.cc \
          re2c/src/ir/nfa/calc_size.cc \
          re2c/src/ir/nfa/nfa.cc \
          re2c/src/ir/nfa/nfa.h \
          re2c/src/ir/nfa/split.cc \
          re2c/src/ir/regexp/display.cc \
          re2c/src/ir/regexp/empty_class_policy.h \
          re2c/src/ir/regexp/encoding/case.h \
          re2c/src/ir/regexp/encoding/enc.cc \
          re2c/src/ir/regexp/encoding/enc.h \
          re2c/src/ir/regexp/encoding/range_suffix.cc \
          re2c/src/ir/regexp/encoding/range_suffix.h \
          re2c/src/ir/regexp/fixed_length.cc \
          re2c/src/ir/regexp/regexp.cc \
          re2c/src/ir/regexp/regexp.h \
          re2c/src/ir/regexp/regexp_alt.h \
          re2c/src/ir/regexp/regexp_cat.h \
          re2c/src/ir/regexp/regexp_close.h \
          re2c/src/ir/regexp/regexp_match.h \
          re2c/src/ir/regexp/regexp_null.h \
          re2c/src/ir/regexp/regexp_rule.h \
          re2c/src/ir/rule_rank.cc \
          re2c/src/ir/rule_rank.h \
          re2c/src/ir/skeleton/path.h \
          re2c/src/ir/skeleton/skeleton.h \
          re2c/src/ir/skeleton/way.h \
          re2c/src/parse/code.h \
          re2c/src/parse/input.h \
          re2c/src/parse/loc.h \
          re2c/src/parse/rules.h \
          re2c/src/parse/scanner.h \
          re2c/src/test/range/test.h \
          re2c/src/util/allocate.h \
          re2c/src/util/attribute.h \
          re2c/src/util/c99_stdint.h \
          re2c/src/util/counter.h \
          re2c/src/util/forbid_copy.h \
          re2c/src/util/free_list.h \
          re2c/src/util/local_increment.h \
          re2c/src/util/ord_hash_set.h \
          re2c/src/util/range.cc \
          re2c/src/util/range.h \
          re2c/src/util/u32lim.h \
          re2c/src/util/uniq_vector.h \
          dfa_match.cc \
          dfa_match.hh

INSTALLPROGS=$(PROG)

$(PROG): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

argh.o: argh.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -DColourPrints -o $@ -c $<

clean:
	rm -f $(PROG) $(OBJS)
distclean: clean
	rm -f Makefile.cfg config.h *~
realclean: distclean

include depfun.mak
