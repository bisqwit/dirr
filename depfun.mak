# This is Bisqwit's generic depfun.mak, included from Makefile.
# The same file is used in many different projects.
#
# depfun.mak version 1.1.15
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
	@ ${MAKE} --silent dep

depend: dep
dep:
	- ${CPP} ${CPPFLAGS} -MM -MG *.c *.cc *.cpp >.depend 2>/dev/null

archpak: ${ARCHFILES}
	@if [ "${ARCHNAME}" = "" ]; then echo ARCHNAME not set\!;false;fi
	- mkdir ${ARCHNAME} ${ARCHDIR} 2>/dev/null
	cp --parents -lfr ${ARCHFILES} depfun.mak Makefile ${ARCHNAME}/
	if [ -f makediff.php ]; then ln makediff.php ${ARCHNAME}/; fi
	- rm -f ${ARCHDIR}${ARCHNAME}.zip
	- zip -9rq ${ARCHDIR}${ARCHNAME}.zip ${ARCHNAME}/
	- rar a ${ARCHDIR}${ARCHNAME}.rar -mm -m5 -r -s -inul ${ARCHNAME}/
	tar cf ${ARCHDIR}${ARCHNAME}.tar ${ARCHNAME}/
	#find ${ARCHNAME}/|/ftp/backup/bsort >.paktmp.txt
	#tar -c --no-recursion -f ${ARCHDIR}${ARCHNAME}.tar -T.paktmp.txt
	#rm -f .paktmp.txt
	rm -rf ${ARCHNAME}
	- bzip2 -9 >${ARCHDIR}${ARCHNAME}.tar.bz2 < ${ARCHDIR}${ARCHNAME}.tar
	gzip -f9 ${ARCHDIR}${ARCHNAME}.tar

# Makes the packages of various types...
pak: archpak
	if [ -f makediff.php ]; then php -q makediff.php ${ARCHNAME} ${ARCHDIR} 1; fi

# This is Bisqwit's method to install the packages to web-server...
omabin: archpak
	if [ -f makediff.php ]; then php -q makediff.php ${ARCHNAME} ${ARCHDIR}; fi
	- @rm -f /WWW/src/${ARCHNAME}.{zip,rar,tar.{bz2,gz}}
	- ln -f ${ARCHDIR}${ARCHNAME}.{zip,rar,tar.{bz2,gz}} /WWW/src/

install${DEPFUN_INSTALL}: ${INSTALLPROGS}
	- mkdir -p $(BINDIR) 2>/dev/null
	- mkdir $(BINDIR) 2>/dev/null
	for s in ${INSTALLPROGS}; do ${INSTALL} -c -s -o bin -g bin -m 755 $$s ${BINDIR}/$$s;done
	
deinstall${DEPFUN_INSTALL}: uninstall${DEPFUN_INSTALL}
uninstall${DEPFUN_INSTALL}:
	for s in ${INSTALLPROGS}; do rm -f ${BINDIR}/$$s;done
