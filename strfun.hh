#ifndef dirr3_strfun_hh
#define dirr3_strfun_hh

#include <string>
#include <limits.h>

#include "printf.hh"

extern std::string_view NameOnly(std::string_view Name);

// Ends with '/'.
// If no directory, returns empty std::string.
extern std::string_view DirOnly(std::string_view Name);

// Without fixit, simply returns the link target text.
// With fixit, it makes the link absolute, if it was relative.
// Parameter is not a string_view,
// because it cals a POSIX function that requires nul terminator.
extern std::string LinkTarget(const std::string& link, bool fixit=false);

template<typename... Args>
void PrintNum(std::string &Dest, char Seps, std::string_view fmt, Args&&... args)
{
    Dest = Printf(fmt, std::forward<Args>(args)...);
    if(Seps)
    {
        std::size_t Len = Dest.find('.');
        if(Len == Dest.npos) Len = Dest.size();

        /* 7:2, 6:1, 5:1, 4:1, 3:0, 2:0, 1:0, 0:0 */
        for(unsigned SepCount = (Len - 1) / 3; SepCount>0; SepCount--)
        {
            Len -= 3;
            Dest.insert(Len, 1, Seps);
        }
    }
}

template<std::size_t N, typename... Args>
inline void PrintNum(std::string &Dest, char Seps, const char (&fmt)[N], Args&&... args)
{
    PrintNum(Dest, Seps, std::string_view(fmt, N-1), std::forward<Args>(args)...);
}

#ifndef NAME_MAX
 #define NAME_MAX 255  /* Chars in a file name */
#endif
#ifndef PATH_MAX
 #define PATH_MAX 1023 /* Chars in a path name */
#endif

/* Does merely strerror() */
extern std::string GetError(int e);

// base="/usr/bin/diu", name="/usr/doc/dau", return="../doc/dau"
extern std::string Relativize(std::string_view base, std::string_view name);

#endif
