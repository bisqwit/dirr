#include "config.h"
#include "setfun.hh"
#include "cons.hh"
#include "colouring.hh"
#include "printf.hh"

int GetNameAttr(const StatType &Stat, const string &fn)
{
    int NameAttr = NameColor(fn, -1);
    if(NameAttr != -1) return NameAttr;

    #ifdef S_ISLNK
    if(S_ISLNK(Stat.st_mode))       return GetModeColor(ColorMode::TYPE, -'l');
    else
    #endif
         if(S_ISDIR(Stat.st_mode))  return GetModeColor(ColorMode::TYPE, -'d');
    else if(S_ISCHR(Stat.st_mode))  return GetModeColor(ColorMode::TYPE, -'c');
    else if(S_ISBLK(Stat.st_mode))  return GetModeColor(ColorMode::TYPE, -'b');
    #ifdef S_ISFIFO
    else if(S_ISFIFO(Stat.st_mode)) return GetModeColor(ColorMode::TYPE, -'p');
    #endif
    #ifdef S_ISSOCK
    else if(S_ISSOCK(Stat.st_mode)) return GetModeColor(ColorMode::TYPE, -'s');
    #endif
    #ifdef S_ISDOOR
    else if(S_ISDOOR(Stat.st_mode)) return GetModeColor(ColorMode::TYPE, -'D');
    #endif
    else if(Stat.st_mode& (S_IXUSR|S_IXGRP|S_IXOTH)) return GetModeColor(ColorMode::TYPE, -'x');
    else if(S_ISREG(Stat.st_mode))  return GetModeColor(ColorMode::TYPE, -'-');

    return GetModeColor(ColorMode::TYPE, -'?'); // Unknown type
}

int PrintAttr(const StatType &Stat, char Attrs
#ifdef DJGPP
    , unsigned int dosattr
#endif
)
{
    #define PutSet(c) do { GetModeColor(ColorMode::MODE, c);Gputch(c);Len++; } while(0)
    int Len = 0;
    switch(Attrs)
    {
        case '0':
#ifdef DJGPP
            if(dosattr&0x20)PutSet('A');else PutSet('-');
            if(dosattr&0x02)PutSet('H');else PutSet('-');
            if(dosattr&0x04)PutSet('S');else PutSet('-');
            if(dosattr&0x01)PutSet('R');else PutSet('-');
#endif
            break;
        case '1':
        {
#ifndef DJGPP
# ifdef S_ISLNK
                 if(S_ISLNK(Stat.st_mode))  PutSet('l');
            else
# endif
                 if(S_ISDIR(Stat.st_mode))  PutSet('d');
            else if(S_ISCHR(Stat.st_mode))  PutSet('c');
            else if(S_ISBLK(Stat.st_mode))  PutSet('b');
# ifdef S_ISFIFO
            else if(S_ISFIFO(Stat.st_mode)) PutSet('p');
# endif
# ifdef S_ISSOCK
            else if(S_ISSOCK(Stat.st_mode)) PutSet('s');
# endif
# ifdef S_ISDOOR
            else if(S_ISDOOR(Stat.st_mode)) PutSet('D');
# endif
            else if(S_ISREG(Stat.st_mode))  PutSet('-');
            else
            {
                // not dir, not link, not chr, not blk, not fifo, not sock, not file...
                // not even a door (?)... what is it then???
                PutSet('?');
            }

            auto mode = [&Stat](unsigned m) -> bool { return Stat.st_mode & m; };

        #if defined(SUNOS)||defined(__sun)||defined(SOLARIS)
            static const char patterns[16] = {'-','r','-','w', '-','x','S','s', '-','x','l','s', '-','x','T','t'};
        #else
            static const char patterns[16] = {'-','r','-','w', '-','x','S','s', '-','x','S','s', '-','x','T','t'};
        #endif

            int defcolor = GetModeColor(ColorMode::MODE, -('-'));
            int xcolor   = GetModeColor(ColorMode::MODE, -(mode(S_IXUSR)?'x':'-'));
            SetAttr(defcolor);
            Gputch(patterns[0+mode(S_IRUSR)]);
            Gputch(patterns[2+mode(S_IWUSR)]);

            SetAttr(xcolor);
            Gputch(patterns[4+mode(S_ISUID)*2+mode(S_IXUSR)]);

            SetAttr(defcolor);
            Gputch(patterns[0+mode(S_IRGRP)]);
            Gputch(patterns[2+mode(S_IWGRP)]);

            SetAttr(xcolor);
            Gputch(patterns[8+mode(S_ISGID)*2+mode(S_IXGRP)]);

            SetAttr(defcolor);
            Gputch(patterns[0+mode(S_IROTH)]);
            Gputch(patterns[2+mode(S_IWOTH)]);

            SetAttr(xcolor);
            Gputch(patterns[12+mode(S_ISVTX)*2+mode(S_IXOTH)]);

            Len += 9;

#endif /* not djgpp */
            break;

        }
        default: // anything else ('2'..'9')
        {
            GetModeColor(ColorMode::MODE, '#');
            std::string s = Printf("%032o", Stat.st_mode);
            Len += Gwrite(s.substr(s.size()- (Attrs-'0')));
        }
    }
    #undef PutSet
    return Len;
}
