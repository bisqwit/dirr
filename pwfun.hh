#ifndef dirr3_pwfun_h
#define dirr3_pwfun_h
#include <string>

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

extern std::string Getpwuid(int uid);
extern std::string Getgrgid(int gid);

#endif
