#ifndef dirr3_getsize_hh
#define dirr3_getsize_hh

#include <string>
#include <sys/stat.h>

extern string BlkStr, ChrStr;

extern string GetSize(const string &s, const struct stat &Sta, int Space, int Seps);

#endif
