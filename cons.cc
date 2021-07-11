#include <cstdio>
#include <cstdarg>
#include <string>
#include <cstdlib>
#include <cstring>
#include <algorithm>

using namespace std;

#include "config.h"
#include "cons.hh"
#include "setfun.hh"

#ifdef HAVE_IOCTL
 #ifdef HAVE_IOCTL_UNISTD_H
 // In SunOS, sys/ioctl.h doesn't define ioctl()
 // function, but it's instead in unistd.h.
  #include <unistd.h>
 #elif defined(HAVE_IOCTL_SYS_IOCTL_H)
  #include <sys/ioctl.h>
 #elif defined(HAVE_IOCTL_SYS_LINUX_IOCTL_H)
  // FREEBSD50
  #include <linux_ioctl.h>
  #define TCGETA LINUX_TCGETA
  #define TCSETA LINUX_TCSETA
 #endif
#endif

#ifdef DJGPP
#include <crt0.h>
#include <conio.h>
int __opendir_flags =
    __OPENDIR_PRESERVE_CASE
  | __OPENDIR_FIND_HIDDEN;
int _crt0_startup_flags =
    _CRT0_FLAG_PRESERVE_UPPER_CASE
  | _CRT0_FLAG_PRESERVE_FILENAME_CASE;
#define Ggetch getch
#endif

#ifdef HAVE_TERMIO_H
# include <termio.h>
#endif

#ifdef HAVE_TCGETATTR_TERMIOS_H
# include <termios.h>
#endif

bool Colors     = true;
bool AnsiOpt    = true;
bool Pagebreaks = false;

int WhereX=0;
int LINES=25, COLS=80;

/* Bigger COLS-values don't work. Change it if it's not ok. */
#define MAX_COLS 1024

int TextAttr = DEFAULTATTR;
int OldAttr  = DEFAULTATTR;

/* DOS-compatible attributes:
 *             BBBBffff   with BBBB=background, ffff=foreground
 *                          Furthermore
 *             B...I...       B=blink/bright, I=intensity/bold
 * Xterm-256color compatible attributes:
 *   uBBBBBBBB*ffffffff   where * = 0x100
 *                              u = 0x20000 for bold
 */

static unsigned BackgroundOf(unsigned attr) { return (attr & 0x100) ? ((attr >> 9) & 0xFF) : ((attr >> 4) & 0x07); }
static unsigned ForegroundOf(unsigned attr) { return (attr & 0x100) ? (attr & 0xFF)        : (attr & 0x07); }
static bool     BoldOf(unsigned attr)       { return (attr & 0x100) ? (attr & 0x20000)     : (attr & 0x8); }
static bool     BlinkOf(unsigned attr)      { return (attr & 0x100) ? false                : (attr & 0x80); }

static class GetScreenGeometry
{
public:
    GetScreenGeometry()
    {
        Get();
    }
    void Get()
    {
#if defined(DJGPP) || defined(__BORLANDC__)
        struct text_info w;
        gettextinfo(&w);
        LINES=w.screenheight;
        COLS =w.screenwidth;
#elif defined(TIOCGWINSZ)
        struct winsize w;
        if(ioctl(1, TIOCGWINSZ, &w) >= 0
        || ioctl(0, TIOCGWINSZ, &w) >= 0
        || ioctl(2, TIOCGWINSZ, &w) >= 0)
        {
            LINES=w.ws_row;
            COLS =w.ws_col;
        }
#elif defined(WIOCGETD)
        struct uwdata w;
        if(ioctl(1, WIOCGETD, &w) >= 0
        || ioctl(0, WIOCGETD, &w) >= 0
        || ioctl(2, WIOCGETD, &w) >= 0)
        {
            LINES = w.uw_height / w.uw_vs;
            COLS  = w.uw_width / w.uw_hs;
        }
#endif
    }
} ScreenInitializer;

void GetScreenGeometry()
{
    ScreenInitializer.Get();
}

static void FlushSetAttr()
{
    if(TextAttr == OldAttr)return;
    if(Colors)
    {
#ifdef DJGPP
        textattr(TextAttr);
#else
        // max length for 16-colors:   "e[0;1;5;40;30m"           = 14 characters
        // max length for 256-colors:  "e[0;38;5;255;48;5;255;1m" = 24 characters
        char Buffer[32]={'\33','['};
        unsigned Buflen=2;

        if(TextAttr != DEFAULTATTR)
        {
            bool pp           = false; // semicolon needed
            bool reset_needed = false;
            auto param = [&]{ if(pp) Buffer[Buflen++]=';'; pp=true; };
            auto appint = [&](unsigned value)
            {
                param();
                if(value >= 100)
                {
                    if(value >= 10000) Buffer[Buflen++] = '0' + (value/10000u)%10u;
                    if(value >= 1000)  Buffer[Buflen++] = '0' + (value/1000u)%10u;
                    Buffer[Buflen++] = '0' + (value/100u)%10u;
                }
                if(value >= 10) Buffer[Buflen++] = '0' + (value/10u)%10u;
                Buffer[Buflen++] = '0' + (value/1u)%10u;
            };
            auto appcolor = [&](int value, int lowdelta, int highdelta, const char* xterm)
            {
                if(value < 16)
                {
                    appint((value & 7) + ((value & 8) ? highdelta : lowdelta));
                }
                else
                {
                    param();
                    memcpy(Buffer+Buflen, xterm, 5); Buflen += 5; pp=false;
                    appint(value);
                }
            };

            static const char Swap[8] = {0,4,2,6,1,5,3,7};
            if(AnsiOpt)
            {
                struct Data { bool legacy,blink,intens; unsigned bg,fg; } olddata, newdata;

                newdata = { !bool(TextAttr&0x100), BlinkOf(TextAttr),BoldOf(TextAttr), BackgroundOf(TextAttr),ForegroundOf(TextAttr)};
                olddata = { !bool(OldAttr&0x100 ), BlinkOf(OldAttr), BoldOf(OldAttr),  BackgroundOf(OldAttr), ForegroundOf(OldAttr) };

                if( (olddata.blink != newdata.blink)
                  + (olddata.intens != newdata.intens) >= 1
                &&  (olddata.bg != newdata.bg)
                  + (olddata.fg != newdata.fg) >= 1)
                {
                    reset_needed = true;
                }

                if(reset_needed)
                {
                    // '0' resets both blink and intensity flags
                    appint(0);
                    OldAttr = DEFAULTATTR; // Presumed default color
                    olddata = { !bool(OldAttr&0x100), BlinkOf(OldAttr), BoldOf(OldAttr),  BackgroundOf(OldAttr), ForegroundOf(OldAttr)};
                }
                if(newdata.intens && !olddata.intens) { appint(1); } // Add bold
                if(newdata.blink  && !olddata.blink)  { appint(5); } // Add blink
                if(!newdata.intens && olddata.intens) { appint(22); } // Remove bold
                if(!newdata.blink  && olddata.blink)  { appint(25); } // Remove blink
                if(newdata.bg != olddata.bg)
                {
                    // Renew background color
                    if(newdata.legacy)
                        appint(40 + Swap[newdata.bg]);
                    else
                        appcolor(newdata.bg, 40,100, "48;5;");
                }
                if(newdata.fg != olddata.fg)
                {
                    // Renew foreground color
                    if(newdata.legacy)
                        appint(30 + Swap[newdata.fg]);
                    else
                        appcolor(newdata.fg, 30,90, "38;5;");
                }
            }
            else
            {
                appint(0);
                if(TextAttr & 0x100) // 256color tag
                {
                    unsigned bg = (TextAttr>>9)&0xFF, fg = TextAttr&0xFF;
                    if(TextAttr & 0x20000) appint(1);
                    appcolor(bg, 40,100, "48;5;");
                    appcolor(fg, 30,90, "38;5;");
                }
                else
                {
                    unsigned bg = (TextAttr>>4)&7, fg = TextAttr&7;
                    if(TextAttr&0x08) appint(1);
                    if(TextAttr&0x80) appint(5);
                    appint(40 + Swap[bg]);
                    appint(30 + Swap[fg]);
                }
            }
        }
        Buffer[Buflen++] = 'm';
        std::fwrite(Buffer,1,Buflen,stdout);
#endif
    }
    OldAttr = TextAttr;
}

#ifndef DJGPP
static int Ggetch()
{
#ifdef HAVE_TCGETATTR_TERMIOS_H
    struct termios term, back;
    int c;
    tcgetattr(0, &term);
    tcgetattr(0, &back);
    term.c_lflag &= ~(ECHO | ICANON);
    term.c_cc[VMIN] = 1;
    tcsetattr(0, TCSAFLUSH, &term);
    c = getchar();
    tcsetattr(0, TCSAFLUSH, &back);
    return c;
#elif defined(HAVE_TERMIO_H)
    struct termio term, back;
    int c;
    ioctl(0, TCGETA, &term);
    ioctl(0, TCGETA, &back);
    term.c_lflag &= ~(ECHO | ICANON);
    term.c_cc[VMIN] = 1;
    ioctl(0, TCSETA, &term);
    c = getchar();
    ioctl(0, TCSETA, &back);
    return c;
#else
    return getchar();
#endif
}
#endif

void SetAttr(int newattr)
{
    TextAttr = newattr;
}

static int Line;
int Gputch(int x)
{
    // When characters would be invisible, print only blanks
    if(BackgroundOf(TextAttr) == ForegroundOf(TextAttr) &&
       !BoldOf(TextAttr) && !BlinkOf(TextAttr) && x > ' ')
        x = ' ';

    // When printing a newline, always do that with default background color
    if(x=='\n' && BackgroundOf(TextAttr) != 0) GetDescrColor(ColorDescr::TEXT, 1);

    // If printing spaces, only change color when the background color changes
    if(x != ' ' || BackgroundOf(TextAttr) != BackgroundOf(OldAttr))
        FlushSetAttr();

    auto put = [](char c)
    {
#ifdef DJGPP
        (Colors?putch:putchar)(c);
#else
        putchar(c);
#endif
    };

    static int Spaces=0;
    switch(x)
    {
        case '\a':
            put(x);
            return x;
        case '\b':
            --Spaces;
            if(WhereX) --WhereX;
            return x;
        case '\r':
            Spaces=WhereX=0;
            put(x);
            return x;
    #ifndef DJGPP
        case ' ':
            ++Spaces;
            ++WhereX;
            return x;
    #endif
        case '\n':
        {
            Spaces=0; WhereX=0;
            #ifdef DJGPP
            put('\r');
            #endif
            put(x);
            if(++Line >= LINES)
            {
                if(Pagebreaks)
                {
                    int More=LINES-2;
                    int ta = TextAttr;
                    SetAttr(0x70);
                    Gprintf("\r--More--");
                    GetDescrColor(ColorDescr::TEXT, 1);
                    Gprintf(" \b");
                    fflush(stdout);
                    for(;;)
                    {
                        int Key = Ggetch();
                        if(Key=='q'
                        || Key=='Q'
                        || Key==3){More=-1;break;}
                        if(Key=='\r'|| Key=='\n'){More=1;break;}
                        if(Key==' ')break;
                        Gputch('\a');
                       }
                    Gprintf("\r\33[K        \r");
                    if(More<0) std::exit(0);
                    SetAttr(ta);
                    Line -= More;
                    GetScreenGeometry();
                }
            }
            return x;
        }
        default:
        {
            while(Spaces < 0) { ++Spaces; put('\b'); }
    #ifndef DJGPP
            static const char spacebuf[16] = {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};
            if(Spaces >= 5 && AnsiOpt && Colors)
            {
                // TODO: Don't do AnsiOpt if background color changed
                std::printf("\33[%dC", Spaces);
                Spaces = 0;
            }
            else
            {
                while(Spaces > 0)
                    Spaces -= std::fwrite(spacebuf, 1, std::min(int(sizeof spacebuf), Spaces), stdout);
            }
    #endif
            put(x);
            return x;
        }
    }
    return x;
}

int ColorNums = -1;

std::size_t Gwrite(std::string_view s)
{
    return WidthPrintHelper<true>( ~std::size_t(), s, false );
}

std::size_t Gwrite(std::string_view s, std::size_t pad)
{
    return WidthPrintHelper<true>( pad, s, true );
}

/* This width_table comes from analyzing
 * http://ftp.unicode.org/Public/UNIDATA/EastAsianWidth.txt
 * You can regenerate it like this:

 (echo 'constexpr std::pair<char32_t,char32_t> width_table[] {';
  grep ';W' EastAsianWidth.txt| \
  perl -pe 's/;W.+//;s/^([^.]*)\.\.(.*)/{0x\1,0x\2},/;s/^([0-9A-Z]+)/{0x\1,0x\1},/';
  echo '};'
 )
*/
constexpr std::pair<char32_t,char32_t> width_table[] {
    { 0x1100, 0x115f },
    { 0x231a, 0x231b },
    { 0x2329, 0x232a },
    { 0x23e9, 0x23ec },
    { 0x23f0, 0x23f0 },
    { 0x23f3, 0x23f3 },
    { 0x25fd, 0x25fe },
    { 0x2614, 0x2615 },
    { 0x2648, 0x2653 },
    { 0x267f, 0x267f },
    { 0x2693, 0x2693 },
    { 0x26a1, 0x26a1 },
    { 0x26aa, 0x26ab },
    { 0x26bd, 0x26be },
    { 0x26c4, 0x26c5 },
    { 0x26ce, 0x26ce },
    { 0x26d4, 0x26d4 },
    { 0x26ea, 0x26ea },
    { 0x26f2, 0x26f3 },
    { 0x26f5, 0x26f5 },
    { 0x26fa, 0x26fa },
    { 0x26fd, 0x26fd },
    { 0x2705, 0x2705 },
    { 0x270a, 0x270b },
    { 0x2728, 0x2728 },
    { 0x274c, 0x274c },
    { 0x274e, 0x274e },
    { 0x2753, 0x2755 },
    { 0x2757, 0x2757 },
    { 0x2795, 0x2797 },
    { 0x27b0, 0x27b0 },
    { 0x27bf, 0x27bf },
    { 0x2b1b, 0x2b1c },
    { 0x2b50, 0x2b50 },
    { 0x2b55, 0x2b55 },
    { 0x2e80, 0x2e99 },
    { 0x2e9b, 0x2ef3 },
    { 0x2f00, 0x2fd5 },
    { 0x2ff0, 0x2ffb },
    { 0x3000, 0x303e },
    { 0x3041, 0x3096 },
    { 0x3099, 0x30ff },
    { 0x3105, 0x312f },
    { 0x3131, 0x318e },
    { 0x3190, 0x31e3 },
    { 0x31f0, 0x321e },
    { 0x3220, 0x3247 },
    { 0x3250, 0x4dbf },
    { 0x4e00, 0xa48c },
    { 0xa490, 0xa4c6 },
    { 0xa960, 0xa97c },
    { 0xac00, 0xd7a3 },
    { 0xf900, 0xfaff },
    { 0xfe10, 0xfe19 },
    { 0xfe30, 0xfe52 },
    { 0xfe54, 0xfe66 },
    { 0xfe68, 0xfe6b },
    { 0xff01, 0xff60 },
    { 0xffe0, 0xffe6 },
    { 0x16fe0, 0x16fe4 },
    { 0x16ff0, 0x16ff1 },
    { 0x17000, 0x187f7 },
    { 0x18800, 0x18cd5 },
    { 0x18d00, 0x18d08 },
    { 0x1b000, 0x1b11e },
    { 0x1b150, 0x1b152 },
    { 0x1b164, 0x1b167 },
    { 0x1b170, 0x1b2fb },
    { 0x1f004, 0x1f004 },
    { 0x1f0cf, 0x1f0cf },
    { 0x1f18e, 0x1f18e },
    { 0x1f191, 0x1f19a },
    { 0x1f200, 0x1f202 },
    { 0x1f210, 0x1f23b },
    { 0x1f240, 0x1f248 },
    { 0x1f250, 0x1f251 },
    { 0x1f260, 0x1f265 },
    { 0x1f300, 0x1f320 },
    { 0x1f32d, 0x1f335 },
    { 0x1f337, 0x1f37c },
    { 0x1f37e, 0x1f393 },
    { 0x1f3a0, 0x1f3ca },
    { 0x1f3cf, 0x1f3d3 },
    { 0x1f3e0, 0x1f3f0 },
    { 0x1f3f4, 0x1f3f4 },
    { 0x1f3f8, 0x1f43e },
    { 0x1f440, 0x1f440 },
    { 0x1f442, 0x1f4fc },
    { 0x1f4ff, 0x1f53d },
    { 0x1f54b, 0x1f54e },
    { 0x1f550, 0x1f567 },
    { 0x1f57a, 0x1f57a },
    { 0x1f595, 0x1f596 },
    { 0x1f5a4, 0x1f5a4 },
    { 0x1f5fb, 0x1f64f },
    { 0x1f680, 0x1f6c5 },
    { 0x1f6cc, 0x1f6cc },
    { 0x1f6d0, 0x1f6d2 },
    { 0x1f6d5, 0x1f6d7 },
    { 0x1f6eb, 0x1f6ec },
    { 0x1f6f4, 0x1f6fc },
    { 0x1f7e0, 0x1f7eb },
    { 0x1f90c, 0x1f93a },
    { 0x1f93c, 0x1f945 },
    { 0x1f947, 0x1f978 },
    { 0x1f97a, 0x1f9cb },
    { 0x1f9cd, 0x1f9ff },
    { 0x1fa70, 0x1fa74 },
    { 0x1fa78, 0x1fa7a },
    { 0x1fa80, 0x1fa86 },
    { 0x1fa90, 0x1faa8 },
    { 0x1fab0, 0x1fab6 },
    { 0x1fac0, 0x1fac2 },
    { 0x1fad0, 0x1fad6 },
    { 0x20000, 0x2fffd },
    { 0x30000, 0x3fffd },
};
bool is_doublewide(char32_t c)
{
    // find earliest element greater than value
    auto i = std::upper_bound(std::begin(width_table), std::end(width_table), c,
                              [](char32_t a, std::pair<char32_t,char32_t> b)
                              {
                                  return a < b.first;
                              });
    // if the first element is greater, then value is not in any range
    if(i == std::begin(width_table)) return false;
    // otherwise i is the earliest greater element, so
    // go back one to the earliest lesser-or-equal element
    i--;
    if(i->second < c) return false;
    return true;
}
