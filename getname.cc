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

int GetName(std::string fn /* modified, so operate on a copy */,
            const StatType &sta, int Space,
            bool Fill, bool nameonly,
            const char *hardlinkfn)
{
    const StatType *Stat = &sta;

    unsigned Len = 0;
    bool estimating = (Space == 0);
    bool maysublink = true;
    bool wasinvalid = false;
    bool showedtype = false;

    std::string fn_print = nameonly ? std::string(NameOnly(fn)) : fn;
#ifdef S_ISLNK
Redo:
#endif
    // We land here to print a name.
    // The color has already been prepared where
    // either this function is called, or Redo is jumped to.

    auto PrintIfRoom = [&](std::string_view what)
    {
        int i = WidthInColumns(what);
        Len += i;

        if(i > Space && nameonly) i = Space;

        int j = WidthPrint(i, what, Fill);
        Space -= j;
    };
    auto PutSet = [&](char ch)
    {
        if (showedtype)
            return;
        showedtype = true;

        if(estimating)
            ++Len;
        else if(Space)
        {
            ++Len;
            --Space;
            GetModeColor(ColorMode::INFO, ch);
            Gputch(ch);
        }
    };

    PrintIfRoom(fn_print);

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
            if(Space > 0) GetModeColor(ColorMode::INFO, '@');
            PrintIfRoom(SLinkArrow);

            StatType Stat1;
            std::string Buf = LinkTarget(fn, true);

            /* Analyze the link target. */
            if(StatFunc(Buf.c_str(), &Stat1) < 0)
            {
                if(LStatFunc(Buf.c_str(), &Stat1) < 0)
                {
                    wasinvalid = true;
                    if(Space > 0) GetModeColor(ColorMode::TYPE, '?');
                }
                else
                {
                    if(Space > 0) GetModeColor(ColorMode::TYPE, 'l');
                }
                maysublink = false;
            }
            else if(Space > 0)
            {
                StatType Stat2;
                if(LStatFunc(Buf.c_str(), &Stat2) >= 0 && S_ISLNK(Stat2.st_mode))
                   GetModeColor(ColorMode::TYPE, 'l');
                else
                   SetAttr(GetNameAttr(Stat1, Buf));
            }

            fn       = LinkTarget(fn, false); // Unfixed link.
            fn_print = fn;
            Stat = &Stat1;
            goto Redo;
        }
        PutSet('@');
    }
    else if(!wasinvalid)
    #endif
    {
        if(S_ISDIR(Stat->st_mode))     PutSet('/');
        else if(Stat->st_mode & 00111) PutSet('*'); // Executable by someone
    }

    if(hardlinkfn && Space)
    {
        StatType Stat1;

        GetModeColor(ColorMode::INFO, '&');
        PrintIfRoom(HLinkArrow);

        fn_print = Relativize(fn, hardlinkfn);

        StatFunc(hardlinkfn, &Stat1);
        SetAttr(GetNameAttr(Stat1, NameOnly(hardlinkfn)));
        hardlinkfn = NULL;
        Stat = &Stat1;

        maysublink = false;
        wasinvalid = false;
        showedtype = false;

        goto Redo;
    }

    if(Fill && Space > 0)
        Gprintf("%*s", Space, "");
    return Len;
}
