#ifndef dirr3_colouring_hh
#define dirr3_colouring_hh

#include <string>
#include <sys/stat.h>

using std::string;

extern int GetNameAttr(const struct stat &Stat, const string &fn);

// Case of Attrs:
//    0: AHSR
//    1: drwxrwxrwx
//   >2: 0755, with Attr decimals.

// Return value: length
extern int PrintAttr(const struct stat &Stat, char Attrs
#ifdef DJGPP
	, unsigned int dosattr
#endif
);

#endif
