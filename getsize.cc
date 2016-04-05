#include <cstdio>
#include <cstring>

#include "config.h"
#include "colouring.hh"
#include "setfun.hh"
#include "strfun.hh"
#include "getname.hh"
#include "printf.hh"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <unistd.h>

#ifndef MAJOR
 #define MAJOR(dev) ((dev) >> 8)
 #define MINOR(dev) ((dev) & 255)
#endif

string BlkStr, ChrStr;

static bool LinkFormatWithoutSize()
{
    return Links==1 || Links==4;
}
static bool LinkFormatStatsOfTarget()
{
    return Links==3 || Links==5;
}

string GetSize(const string &s, const StatType &Sta, int Space, int Seps)
{
    const StatType *Stat = &Sta;

    std::string result;
    ColorDescr descr = ColorDescr::DESCR;

#ifdef S_ISLNK
GotSize:
#endif
         if(S_ISDIR (Stat->st_mode)) result = "<DIR>";
    #ifdef S_ISFIFO
    else if(S_ISFIFO(Stat->st_mode)) result = "<PIPE>";
    #endif
    #ifdef S_ISSOCK
    else if(S_ISSOCK(Stat->st_mode)) result = "<SOCKET>";
    #endif
    else if(S_ISCHR (Stat->st_mode))
    {
        result = Printf(ChrStr, MAJOR(Stat->st_rdev), MINOR(Stat->st_rdev));
    }
    else if(S_ISBLK (Stat->st_mode))
    {
        result = Printf(BlkStr, MAJOR(Stat->st_rdev), MINOR(Stat->st_rdev));
    }
    #ifdef S_ISLNK
    else if(S_ISLNK (Stat->st_mode))
    {
        if(!LinkFormatWithoutSize())goto P1;
LinkProblem:
        result = "<LINK>";
    }
    #endif
    else
    {
        /* Tähän päädytään, jos kyseessä oli tavanomainen tiedosto */
        SizeType l;

#ifdef S_ISLNK
P1:
        /* Tämä iffi on tässä siksi uudestaan, kun tähän voidaan päätyä
         * sekä gotolla, että myös jos oli tavanomainen tiedosto.
         */
        if(S_ISLNK(Stat->st_mode))
        {
            if(LinkFormatStatsOfTarget())
            {
                static StatType Stat1;
                string Buf = LinkTarget(s);

                // Target status
                if(StatFunc(Buf.c_str(), &Stat1) >= 0)
                {
                    Stat = &Stat1;
                    goto GotSize;
                }
                goto LinkProblem;
            }
            /* different colour */
            descr = (ColorDescr)-1;
        }
#endif
        l = Stat->st_size;

        const char* Suffix = "";

        if(Seps == -1)
        {
            Seps=0;
            static const char SIsuffixTable[] = "\0\0k\0M\0G\0T\0P\0E\0Z\0Y";
            Suffix = SIsuffixTable;
            while(l >= 1000)
            {
                l /= 1000;
                Suffix += 2;
            }
        }

        PrintNum(result, Seps == -1 ? '\0' : Seps, SizeFormat, SizeCast l);
        result += Suffix;

        if(descr != ColorDescr(-1)) descr = ColorDescr::SIZE;
    }

    if(descr != ColorDescr(-1))
        GetDescrColor(descr, 1);
    else
        GetModeColor(ColorMode::INFO, '@');

    if(int(result.size()) < Space)
        result.insert(0, Space-result.size(), ' ');

    return result;
}
