#ifndef dirr3_getname_hh
#define dirr3_getname_hh

#include <string>
#include <sys/stat.h>

#ifdef S_ISLNK
extern int Links;
#endif

using std::string;

/***********************************************
 *
 * GetName(fn, Stat, Space, Fill)
 *
 *   Prints the filename
 *
 *     fn:     Filename
 *     Stat:   stat-bufferi, jonka modesta riippuen
 *             saatetaan tulostaa per‰‰n merkkej‰
 *             =,|,?,/,*,@. Myˆs ->:t katsotaan.
 *     Space:  Maximum usable printing space.
 *     Fill:   Jos, ylim‰‰r‰inen tila t‰ytet‰‰n
 *             spaceilla oikealta puolelta.
 * 
 *   Return value: True (not necessarily printed) length
 *
 **********************************************************/
 
extern int GetName(const string &fn, const struct stat &sta, int Space,
                   bool Fill, bool nameonly,
                   const char *hardlinkfn = NULL);

#endif
