#ifndef dirr3_pwfun_h
#define dirr3_pwfun_h

/***********************************************
 *
 * Getpwuid(uid)
 * Getgrgid(gid)
 *
 *   Get user and group name, quickly using binary search
 *
 * ReadGidUid()
 *
 *   Builds the data structures for Getpwuid() and Getgrgid()
 *
 *************************************************************/

extern const char *Getpwuid(int uid);
extern const char *Getgrgid(int gid);
extern void ReadGidUid();

#endif
