# This is Bisqwit's generic depfun.mak, included from Makefile.
# The same file is used in many different projects.
# 
# Required vars:
#
#  ${CPP}        - C preprocessor name
#  ${CPPFLAGS}   - preprocessor flags (including defs)
#
#  ${ARCHFILES}  - All file names to include in archive
#                  .depend, depfun.mak and Makefile are
#                  automatically included.
#  ${ARCHNAME}   - Name of program. Example: testprog-0.0.1
#

include .depend

.depend: ${ARCHFILES}
	touch .depend
	${MAKE} dep

depend: dep
dep:
	- ${CPP} ${CPPFLAGS} -MM *.c *.cc > .depend

# Makes the .tar.gz and .tar.bz2 and .zip -packages...
pak: ${ARCHFILES}
	- mkdir ${ARCHNAME}
	cp -lfr ${ARCHFILES} depfun.mak Makefile ${ARCHNAME}/
	- rm -f ${ARCHNAME}.zip
	zip -9rq ${ARCHNAME}.zip ${ARCHNAME}/
	tar cf ${ARCHNAME}.tar ${ARCHNAME}/
	rm -rf ${ARCHNAME}
	- bzip2 -c9 >${ARCHNAME}.tar.bz2 <${ARCHNAME}.tar
	gzip -f9 ${ARCHNAME}.tar

# This is Bisqwit's method to install the packages to web-server...
omabin: pak
	- @rm -f /WWW/src/${ARCHNAME}.{zip,tar.{bz2,gz}}
	- cp -lf ${ARCHNAME}.{zip,tar.{bz2,gz}} /WWW/src/
