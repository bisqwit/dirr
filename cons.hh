#ifndef dirr3_cons_hh
#define dirr3_cons_hh

#include <algorithm> // std::swap
#include "likely.hh"
#include "printf.hh"

extern bool Colors;
extern bool AnsiOpt;
extern bool Pagebreaks;

extern int WhereX;
extern int LINES, COLS;

extern int Gputch(int x);
extern void SetAttr(int newattr);
extern int ColorNums, TextAttr;

enum { DEFAULTATTR = 7 };

template<typename... Args>
std::size_t Gprintf(const std::string& fmt, Args&&... args)
{
    std::string str = Printf(fmt, std::forward<Args>(args)...);
    std::size_t end = str.size();
    int ta = TextAttr, cn = ColorNums >= 0 ? ColorNums : ta;
    for(char c: str)
        if(likely(c != '\1'))
            Gputch(c);
        else
        {
            SetAttr(cn);
            std::swap(ta, cn);
        }
    return end;
}

extern std::size_t Gwrite(const std::string& s);
extern std::size_t Gwrite(const std::string& s, std::size_t pad);

extern void GetScreenGeometry();

#endif
