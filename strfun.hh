#ifndef dirr3_strfun_hh
#define dirr3_strfun_hh

#include <string>
#include <limits.h>

extern string NameOnly(const string &Name);

// Ends with '/'.
// If no directory, returns empty string.
extern string DirOnly(const string &Name);

// Without fixit, simply returns the link target text.
// With fixit, it makes the link absolute, if it was relative.
extern string LinkTarget(const string &link, bool fixit=false);

extern void PrintNum(string &Dest, int Seps, const char *fmt, ...)
	__attribute__((format(printf,3,4)));

#ifndef NAME_MAX
 #define NAME_MAX 255  /* Chars in a file name */
#endif
#ifndef PATH_MAX
 #define PATH_MAX 1023 /* Chars in a path name */
#endif

/* Does merely strerror() */
extern string GetError(int e);

// base="/usr/bin/diu", name="/usr/doc/dau", return="../doc/dau"
extern string Relativize(const string &base, const string &name);

#endif
