#include <unistd.h>
#include <cstring>

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

static std::size_t WidthPrint(std::size_t maxlen, const string &buf, bool fill)
{
    /* Print maximum of "maxlen" characters from buf.
     * If buf is longer, print spaces.
     * Convert unprintable characters into question marks.
     * Printable (unsigned): 20..7E, A0..FF    Unprintable: 00..1F, 7F..9F
     * Printable (signed):   32..126, -96..-1, Unprintable: -128..-97, 0..31
     */
    std::size_t n=0;
    for(std::size_t limit=std::min(maxlen, buf.size()); n<limit; ++n)
    {
        unsigned char c = buf[n];
        if((c >= 32 && c < 0x7F) || (c >= 0xA0))
            Gputch(c);
        else
            Gputch('?');
    }
    if(fill) for(; n < maxlen; ++n) Gputch(' ');
    return n;
}

int GetName(const string &fn, const StatType &sta, int Space,
            bool Fill, bool nameonly,
            const char *hardlinkfn)
{
    string Puuh, Buf, s=fn;
    const StatType *Stat = &sta;

    unsigned Len = 0;
    int i;

    bool maysublink = true;
    bool wasinvalid = false;

    Puuh = nameonly ? NameOnly(s) : s;
#ifdef S_ISLNK
Redo:
#endif
    // Tähän kohtaan hypätään tulostamaan nimi.
    // Tekstin väri on säädetty jo valmiiksi siellä
    // mistä tähän funktioon (tai Redo-labeliin) tullaan.

    Buf = Puuh;
    Len += (i = Buf.size());

    if(i > Space && nameonly)i = Space;
    Buf.erase(i);

    Space -= WidthPrint(i, Buf, Fill);

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

            Len += (a = Buf.size());

            if(a > Space)a = Space;
            if(a > 0)
            {
                Buf.erase(a);
                GetModeColor(ColorMode::INFO, '@');
                Gwrite(Buf, a);
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

        Len += Gprintf("%s", HLinkArrow);
        Space -= strlen(HLinkArrow);

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
