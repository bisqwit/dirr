#ifndef dirr3_colouring_hh
#define dirr3_colouring_hh

#include <string>
#include <sys/stat.h>

extern int GetNameAttr(const struct stat &Stat, const string &fn);
extern void PrintAttr(const struct stat &Stat, char Attrs, int &Len, unsigned int dosattr);

#endif
