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
 *                              u = 0x20000 for underline
 */

static unsigned BackgroundOf(int attr) { return (attr & 0x100) ? (attr >> 9) & 0xFF : (attr >> 4) & 0x0F; }
static unsigned ForegroundOf(int attr) { return (attr & 0x100) ? (attr & 0xFF)      : (attr & 0x0F); }

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
            static const char Swap[8] = {'0','4','2','6','1','5','3','7'};
            if(AnsiOpt)
            {
                bool pp           = false; // semicolon needed
                bool reset_needed = false;
                auto param = [&]{ if(pp) Buffer[Buflen++]=';'; pp=true; };

                struct Data { bool legacy,blink,intens; unsigned bg,fg; } olddata, newdata;

                if(TextAttr&0x100) newdata={false,false,bool(TextAttr&0x20000), unsigned(TextAttr>>9), TextAttr&0xFFu};
                else               newdata={true,bool(TextAttr&0x80),bool(TextAttr&8), (TextAttr>>4u)&7u, TextAttr&7u };
                if(OldAttr&0x100)  olddata={false,false,bool(OldAttr&0x20000), unsigned(OldAttr>>9), OldAttr&0xFFu};
                else               olddata={true,bool(OldAttr&0x80),bool(OldAttr&8), (OldAttr>>4u)&7u, OldAttr&7u };

                if( (olddata.blink && !newdata.blink) || (olddata.intens && !newdata.intens) )
                    reset_needed = true;

                if(reset_needed)
                {
                    // '0' resets both blink and intensity flags
                    Buffer[Buflen++] = '0'; pp=true;
                    olddata.intens = false;
                    olddata.blink = false;
                    olddata.bg = BackgroundOf(DEFAULTATTR);
                    olddata.fg = ForegroundOf(DEFAULTATTR);
                    OldAttr = DEFAULTATTR; // Presumed default color
                }
                if(newdata.intens && !olddata.intens) { param(); Buffer[Buflen++] = '1'; }
                if(newdata.blink  && !olddata.blink)  { param(); Buffer[Buflen++] = '5'; }
                if(newdata.bg != olddata.bg)
                {
                    param();
                    if(newdata.legacy)
                    {
                        Buffer[Buflen++] = '4';
                        Buffer[Buflen++] = Swap[newdata.bg];
                    }
                    else
                    {
                        Buffer[Buflen++] = '4';
                        Buffer[Buflen++] = '8';
                        Buffer[Buflen++] = ';';
                        Buffer[Buflen++] = '5';
                        Buffer[Buflen++] = ';';
                        if(newdata.bg >= 100) Buffer[Buflen++] = '0' + (newdata.bg/100)%10;
                        if(newdata.bg >= 10) Buffer[Buflen++] = '0' + (newdata.bg/10)%10;
                        Buffer[Buflen++] = '0' + newdata.bg%10;
                    }
                }
                if(newdata.fg != olddata.fg)
                {
                    param();
                    if(newdata.legacy)
                    {
                        Buffer[Buflen++] = '3';
                        Buffer[Buflen++] = Swap[newdata.fg];
                    }
                    else
                    {
                        Buffer[Buflen++] = '3';
                        Buffer[Buflen++] = '8';
                        Buffer[Buflen++] = ';';
                        Buffer[Buflen++] = '5';
                        Buffer[Buflen++] = ';';
                        if(newdata.fg >= 100) Buffer[Buflen++] = '0' + (newdata.fg/100)%10;
                        if(newdata.fg >= 10) Buffer[Buflen++] = '0' + (newdata.fg/10)%10;
                        Buffer[Buflen++] = '0' + newdata.fg%10;
                    }
                }
            }
            else
            {
                Buffer[Buflen++] = '0';
                if(TextAttr & 0x100) // 256color tag
                {
                    unsigned bg = (TextAttr>>9)&0xFF, fg = TextAttr&0xFF;
                    if(TextAttr & 0x20000)
                    {
                        Buffer[Buflen++] = ';';
                        Buffer[Buflen++] = '1';
                    }
                    Buffer[Buflen++] = ';';
                    Buffer[Buflen++] = '3';
                    Buffer[Buflen++] = '8';
                    Buffer[Buflen++] = ';';
                    Buffer[Buflen++] = '5';
                    Buffer[Buflen++] = ';';
                    Buffer[Buflen++] = '0' + (fg/100)%10;
                    Buffer[Buflen++] = '0' + (fg/10)%10;
                    Buffer[Buflen++] = '0' + (fg/1)%10;
                    Buffer[Buflen++] = ';';
                    Buffer[Buflen++] = '4';
                    Buffer[Buflen++] = '8';
                    Buffer[Buflen++] = ';';
                    Buffer[Buflen++] = '5';
                    Buffer[Buflen++] = ';';
                    Buffer[Buflen++] = '0' + (bg/100)%10;
                    Buffer[Buflen++] = '0' + (bg/10)%10;
                    Buffer[Buflen++] = '0' + (bg/1)%10;
                }
                else
                {
                    unsigned bg = (TextAttr>>4)&7, fg = TextAttr&7;
                    if(TextAttr&0x08) { Buffer[Buflen++]=';';Buffer[Buflen++]='1'; }
                    if(TextAttr&0x80) { Buffer[Buflen++]=';';Buffer[Buflen++]='5'; }
                    Buffer[Buflen++] = '4';
                    Buffer[Buflen++] = Swap[bg];
                    Buffer[Buflen++] = '3';
                    Buffer[Buflen++] = Swap[fg];
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
    // When background color = foreground color, print only blanks
    if(BackgroundOf(TextAttr) == ForegroundOf(TextAttr) && x > ' ') x = ' ';

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

std::size_t Gwrite(const std::string& s)
{
    return WidthPrint<true>(~std::size_t(), s, false);
    //for(char c: s) Gputch(c);
    //return s.size();
}

std::size_t Gwrite(const std::string& s, std::size_t pad)
{
    std::size_t res = Gwrite(s);
    while(res < pad) { ++res; Gputch(' '); }
    return pad;
}

constexpr std::array width_table {
	std::pair<char32_t,char32_t>{ 0x1100, 0x115f },
	std::pair<char32_t,char32_t>{ 0x231a, 0x231b },
	std::pair<char32_t,char32_t>{ 0x2329, 0x232a },
	std::pair<char32_t,char32_t>{ 0x23e9, 0x23ec },
	std::pair<char32_t,char32_t>{ 0x23f0, 0x23f0 },
	std::pair<char32_t,char32_t>{ 0x23f3, 0x23f3 },
	std::pair<char32_t,char32_t>{ 0x25fd, 0x25fe },
	std::pair<char32_t,char32_t>{ 0x2614, 0x2615 },
	std::pair<char32_t,char32_t>{ 0x2648, 0x2653 },
	std::pair<char32_t,char32_t>{ 0x267f, 0x267f },
	std::pair<char32_t,char32_t>{ 0x2693, 0x2693 },
	std::pair<char32_t,char32_t>{ 0x26a1, 0x26a1 },
	std::pair<char32_t,char32_t>{ 0x26aa, 0x26ab },
	std::pair<char32_t,char32_t>{ 0x26bd, 0x26be },
	std::pair<char32_t,char32_t>{ 0x26c4, 0x26c5 },
	std::pair<char32_t,char32_t>{ 0x26ce, 0x26ce },
	std::pair<char32_t,char32_t>{ 0x26d4, 0x26d4 },
	std::pair<char32_t,char32_t>{ 0x26ea, 0x26ea },
	std::pair<char32_t,char32_t>{ 0x26f2, 0x26f3 },
	std::pair<char32_t,char32_t>{ 0x26f5, 0x26f5 },
	std::pair<char32_t,char32_t>{ 0x26fa, 0x26fa },
	std::pair<char32_t,char32_t>{ 0x26fd, 0x26fd },
	std::pair<char32_t,char32_t>{ 0x2705, 0x2705 },
	std::pair<char32_t,char32_t>{ 0x270a, 0x270b },
	std::pair<char32_t,char32_t>{ 0x2728, 0x2728 },
	std::pair<char32_t,char32_t>{ 0x274c, 0x274c },
	std::pair<char32_t,char32_t>{ 0x274e, 0x274e },
	std::pair<char32_t,char32_t>{ 0x2753, 0x2755 },
	std::pair<char32_t,char32_t>{ 0x2757, 0x2757 },
	std::pair<char32_t,char32_t>{ 0x2795, 0x2797 },
	std::pair<char32_t,char32_t>{ 0x27b0, 0x27b0 },
	std::pair<char32_t,char32_t>{ 0x27bf, 0x27bf },
	std::pair<char32_t,char32_t>{ 0x2b1b, 0x2b1c },
	std::pair<char32_t,char32_t>{ 0x2b50, 0x2b50 },
	std::pair<char32_t,char32_t>{ 0x2b55, 0x2b55 },
	std::pair<char32_t,char32_t>{ 0x2e80, 0x2e99 },
	std::pair<char32_t,char32_t>{ 0x2e9b, 0x2ef3 },
	std::pair<char32_t,char32_t>{ 0x2f00, 0x2fd5 },
	std::pair<char32_t,char32_t>{ 0x2ff0, 0x2ffb },
	std::pair<char32_t,char32_t>{ 0x3000, 0x303e },
	std::pair<char32_t,char32_t>{ 0x3041, 0x3096 },
	std::pair<char32_t,char32_t>{ 0x3099, 0x30ff },
	std::pair<char32_t,char32_t>{ 0x3105, 0x312f },
	std::pair<char32_t,char32_t>{ 0x3131, 0x318e },
	std::pair<char32_t,char32_t>{ 0x3190, 0x31e3 },
	std::pair<char32_t,char32_t>{ 0x31f0, 0x321e },
	std::pair<char32_t,char32_t>{ 0x3220, 0x3247 },
	std::pair<char32_t,char32_t>{ 0x3250, 0x4dbf },
	std::pair<char32_t,char32_t>{ 0x4e00, 0xa48c },
	std::pair<char32_t,char32_t>{ 0xa490, 0xa4c6 },
	std::pair<char32_t,char32_t>{ 0xa960, 0xa97c },
	std::pair<char32_t,char32_t>{ 0xac00, 0xd7a3 },
	std::pair<char32_t,char32_t>{ 0xf900, 0xfaff },
	std::pair<char32_t,char32_t>{ 0xfe10, 0xfe19 },
	std::pair<char32_t,char32_t>{ 0xfe30, 0xfe52 },
	std::pair<char32_t,char32_t>{ 0xfe54, 0xfe66 },
	std::pair<char32_t,char32_t>{ 0xfe68, 0xfe6b },
	std::pair<char32_t,char32_t>{ 0xff01, 0xff60 },
	std::pair<char32_t,char32_t>{ 0xffe0, 0xffe6 },
	std::pair<char32_t,char32_t>{ 0x16fe0, 0x16fe4 },
	std::pair<char32_t,char32_t>{ 0x16ff0, 0x16ff1 },
	std::pair<char32_t,char32_t>{ 0x17000, 0x187f7 },
	std::pair<char32_t,char32_t>{ 0x18800, 0x18cd5 },
	std::pair<char32_t,char32_t>{ 0x18d00, 0x18d08 },
	std::pair<char32_t,char32_t>{ 0x1b000, 0x1b11e },
	std::pair<char32_t,char32_t>{ 0x1b150, 0x1b152 },
	std::pair<char32_t,char32_t>{ 0x1b164, 0x1b167 },
	std::pair<char32_t,char32_t>{ 0x1b170, 0x1b2fb },
	std::pair<char32_t,char32_t>{ 0x1f004, 0x1f004 },
	std::pair<char32_t,char32_t>{ 0x1f0cf, 0x1f0cf },
	std::pair<char32_t,char32_t>{ 0x1f18e, 0x1f18e },
	std::pair<char32_t,char32_t>{ 0x1f191, 0x1f19a },
	std::pair<char32_t,char32_t>{ 0x1f200, 0x1f202 },
	std::pair<char32_t,char32_t>{ 0x1f210, 0x1f23b },
	std::pair<char32_t,char32_t>{ 0x1f240, 0x1f248 },
	std::pair<char32_t,char32_t>{ 0x1f250, 0x1f251 },
	std::pair<char32_t,char32_t>{ 0x1f260, 0x1f265 },
	std::pair<char32_t,char32_t>{ 0x1f300, 0x1f320 },
	std::pair<char32_t,char32_t>{ 0x1f32d, 0x1f335 },
	std::pair<char32_t,char32_t>{ 0x1f337, 0x1f37c },
	std::pair<char32_t,char32_t>{ 0x1f37e, 0x1f393 },
	std::pair<char32_t,char32_t>{ 0x1f3a0, 0x1f3ca },
	std::pair<char32_t,char32_t>{ 0x1f3cf, 0x1f3d3 },
	std::pair<char32_t,char32_t>{ 0x1f3e0, 0x1f3f0 },
	std::pair<char32_t,char32_t>{ 0x1f3f4, 0x1f3f4 },
	std::pair<char32_t,char32_t>{ 0x1f3f8, 0x1f43e },
	std::pair<char32_t,char32_t>{ 0x1f440, 0x1f440 },
	std::pair<char32_t,char32_t>{ 0x1f442, 0x1f4fc },
	std::pair<char32_t,char32_t>{ 0x1f4ff, 0x1f53d },
	std::pair<char32_t,char32_t>{ 0x1f54b, 0x1f54e },
	std::pair<char32_t,char32_t>{ 0x1f550, 0x1f567 },
	std::pair<char32_t,char32_t>{ 0x1f57a, 0x1f57a },
	std::pair<char32_t,char32_t>{ 0x1f595, 0x1f596 },
	std::pair<char32_t,char32_t>{ 0x1f5a4, 0x1f5a4 },
	std::pair<char32_t,char32_t>{ 0x1f5fb, 0x1f64f },
	std::pair<char32_t,char32_t>{ 0x1f680, 0x1f6c5 },
	std::pair<char32_t,char32_t>{ 0x1f6cc, 0x1f6cc },
	std::pair<char32_t,char32_t>{ 0x1f6d0, 0x1f6d2 },
	std::pair<char32_t,char32_t>{ 0x1f6d5, 0x1f6d7 },
	std::pair<char32_t,char32_t>{ 0x1f6eb, 0x1f6ec },
	std::pair<char32_t,char32_t>{ 0x1f6f4, 0x1f6fc },
	std::pair<char32_t,char32_t>{ 0x1f7e0, 0x1f7eb },
	std::pair<char32_t,char32_t>{ 0x1f90c, 0x1f93a },
	std::pair<char32_t,char32_t>{ 0x1f93c, 0x1f945 },
	std::pair<char32_t,char32_t>{ 0x1f947, 0x1f978 },
	std::pair<char32_t,char32_t>{ 0x1f97a, 0x1f9cb },
	std::pair<char32_t,char32_t>{ 0x1f9cd, 0x1f9ff },
	std::pair<char32_t,char32_t>{ 0x1fa70, 0x1fa74 },
	std::pair<char32_t,char32_t>{ 0x1fa78, 0x1fa7a },
	std::pair<char32_t,char32_t>{ 0x1fa80, 0x1fa86 },
	std::pair<char32_t,char32_t>{ 0x1fa90, 0x1faa8 },
	std::pair<char32_t,char32_t>{ 0x1fab0, 0x1fab6 },
	std::pair<char32_t,char32_t>{ 0x1fac0, 0x1fac2 },
	std::pair<char32_t,char32_t>{ 0x1fad0, 0x1fad6 },
	std::pair<char32_t,char32_t>{ 0x20000, 0x2fffd },
	std::pair<char32_t,char32_t>{ 0x30000, 0x3fffd },
};
bool is_doublewide(char32_t c)
{
    auto i = std::lower_bound(width_table.begin(), width_table.end(), c,
                              [](std::pair<char32_t,char32_t> a,
                                 char32_t b)
                              {
                                  return b < a.first;
                              });
    if(i == width_table.end()) return false;
    if(i->second > c) return false;
    return true;
}
