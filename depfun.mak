# This is Bisqwit's generic depfun.mak, included from Makefile.
# The same file is used in many different projects.
#
# depfun.mak version 1.1.4
# 
# Required vars:
#
#  ${CPP}        - C preprocessor name, usually "gcc"
#  ${CPPFLAGS}   - preprocessor flags (including defs)
#
#  ${ARCHFILES}  - All file names to include in archive
#                  .depend, depfun.mak and Makefile are
#                  automatically included.
#  ${ARCHNAME}   - Name of program. Example: testprog-0.0.1
#
# Optional vars:
#
#  ${ARCHDIR}       - Directory for the archives.
#                     Must end with '/'.
#  ${INSTALLPROGS}  - Programs to be installed (space delimited)
#  ${BINDIR}        - Directory for installed programs (without /)
#                     Example: /usr/local/bin
#  ${INSTALL}       - Installer program, example: install

include .depend

.depend: ${ARCHFILES}
	touch .depend
	${MAKE} dep

depend: dep
dep:
	- ${CPP} ${CPPFLAGS} -MM *.c *.cc > .depend

# Makes the packages of various types...
pak: ${ARCHFILES}
	- mkdir ${ARCHNAME} ${ARCHDIR}
	cp -lfr ${ARCHFILES} depfun.mak Makefile ${ARCHNAME}/
	- rm -f ${ARCHDIR}${ARCHNAME}.zip
	- zip -9rq ${ARCHDIR}${ARCHNAME}.zip ${ARCHNAME}/
	- rar a ${ARCHDIR}${ARCHNAME}.rar -mm -m5 -r -s -inul ${ARCHNAME}/
	tar cf ${ARCHDIR}${ARCHNAME}.tar ${ARCHNAME}/
	rm -rf ${ARCHNAME}
	- bzip2 -c9 >${ARCHDIR}${ARCHNAME}.tar.bz2 < ${ARCHDIR}${ARCHNAME}.tar
	gzip -f9 ${ARCHDIR}${ARCHNAME}.tar

# This is Bisqwit's method to install the packages to web-server...
omabin: pak
	- @rm -f /WWW/src/${ARCHNAME}.{zip,rar,tar.{bz2,gz}}
	- cp -lf ${ARCHDIR}${ARCHNAME}.{zip,rar,tar.{bz2,gz}} /WWW/src/

install${DEPFUN_INSTALL}: ${INSTALLPROGS}
	for s in ${INSTALLPROGS}; do ${INSTALL} -c -s -o bin -g bin -m 755 $$s ${BINDIR}/$$s;done
	
deinstall${DEPFUN_INSTALL}: uninstall${DEPFUN_INSTALL}
uninstall${DEPFUN_INSTALL}:
	for s in ${INSTALLPROGS}; do rm -f ${BINDIR}/$$s;done
