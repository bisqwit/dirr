#include <unistd.h>
#include <cstring>
#include <cstdint>

#include "config.h"
#include "getname.hh"
#include "strfun.hh"
#include "setfun.hh"
#include "colouring.hh"
#include "cons.hh"

static const char SLinkArrow[] = " -> ";
static const char HLinkArrow[] = " = ";

#ifdef S_ISLNK
int Links;
#endif

int GetName(const string &fn, const StatType &sta, int Space,
            bool Fill, bool nameonly,
            const char *hardlinkfn)
{
    string Puuh, Buf, s=fn;
    const StatType *Stat = &sta;

    unsigned Len = 0;

    bool maysublink = true;
    bool wasinvalid = false;

    Puuh = nameonly ? NameOnly(s) : s;
#ifdef S_ISLNK
Redo:
#endif
    // T‰h‰n kohtaan hyp‰t‰‰n tulostamaan nimi.
    // Tekstin v‰ri on s‰‰detty jo valmiiksi siell‰
    // mist‰ t‰h‰n funktioon (tai Redo-labeliin) tullaan.

    Buf = Puuh;
    int i = WidthPrint<false>(~0u, Buf, false);
    Len += i;

    if(i > Space && nameonly) i = Space;
    int j = WidthPrint<true>(i, Buf, Fill);
    //fprintf(stderr, "i=%d, len=%d, Space=%d, j=%d\n", i, int(Buf.size()), Space, j);

    Space -= j;

    #define PutSet(c) do \
        if(Space) \
        { \
            char ch = (c); \
            ++Len; --Space; \
            GetModeColor(ColorMode::INFO, ch); \
            Gputch(ch); \
        } \
    while(0)

    if(wasinvalid)
    {
        PutSet('?');
    }
    else
    {
        #ifdef S_ISSOCK
        if(S_ISSOCK(Stat->st_mode)) PutSet('=');
        #endif
        #ifdef S_ISFIFO
        if(S_ISFIFO(Stat->st_mode)) PutSet('|');
        #endif
    }
    #ifdef S_ISLNK
    if(!wasinvalid && S_ISLNK(Stat->st_mode))
    {
        if(Links >= 2 && maysublink)
        {
            int a;

            Buf = SLinkArrow;

            Len += (a = WidthPrint<false>(~0u, Buf, false));

            if(a > Space)a = Space;
            if(a > 0)
            {
                GetModeColor(ColorMode::INFO, '@');
                WidthPrint<true>(a, Buf, false);
            }
            Space -= a;

            StatType Stat1;
            Buf = LinkTarget(s, true);

            /* Target status */
            if(StatFunc(Buf.c_str(), &Stat1) < 0)
            {
                if(LStatFunc(Buf.c_str(), &Stat1) < 0)
                {
                    wasinvalid = true;
                    if(Space)GetModeColor(ColorMode::TYPE, '?');
                }
                else
                {
                    if(Space)GetModeColor(ColorMode::TYPE, 'l');
                }
                maysublink = false;
            }
            else if(Space)
            {
                StatType Stat2;
                if(LStatFunc(Buf.c_str(), &Stat2) >= 0 && S_ISLNK(Stat2.st_mode))
                   GetModeColor(ColorMode::TYPE, 'l');
                else
                   SetAttr(GetNameAttr(Stat1, Buf));
            }

            Puuh = s = LinkTarget(s, false); // Unfixed link.
            Stat = &Stat1;
            goto Redo;
        }
        PutSet('@');
    }
    else
    #endif
    {
        if(S_ISDIR(Stat->st_mode))  PutSet('/');
        else if(Stat->st_mode & 00111)PutSet('*');
    }

    if(hardlinkfn && Space)
    {
        StatType Stat1;

        SetAttr(GetModeColor(ColorMode::INFO, '&'));

        int arrowlength = Gwrite(HLinkArrow);
        Len += arrowlength;
        Space -= arrowlength;

        Puuh = Relativize(s, hardlinkfn);

        StatFunc(hardlinkfn, &Stat1);
        SetAttr(GetNameAttr(Stat1, NameOnly(hardlinkfn)));
        hardlinkfn = NULL;
        Stat = &Stat1;

        maysublink = false;
        wasinvalid = false;

        if(Space < 0)Space = 0;

        goto Redo;
    }

    #undef PutSet

    if(Fill) while(Space > 0) { Gputch(' '); --Space; }
    return Len;
}
