#include "config.h"

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#define StatType struct stat64
#define StatFunc stat64
#define LStatFunc lstat64

#define SizeType long long

#define SizeFormat "%lld"
#define SizeCast (long long)
