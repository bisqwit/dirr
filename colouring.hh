#ifndef dirr3_colouring_hh
#define dirr3_colouring_hh

#include <string>

#include "stat.h"

extern int GetNameAttr(const StatType &Stat, std::string_view fn);

// Case of Attrs:
//    0: AHSR
//    1: drwxrwxrwx
//   >2: 0755, with Attr decimals.

// Return value: length
extern int PrintAttr(const StatType &Stat, char Attrs
#ifdef DJGPP
	, unsigned int dosattr
#endif
);

#endif
