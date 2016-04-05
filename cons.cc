#include <cstdio>
#include <cstdarg>
#include <string>
#include <cstdlib>
#include <cstring>

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
        // max length: "e[0;1;5;40;30m" = 14 characters
        char Buffer[16]={'\33','['};
        unsigned Buflen=2;
        if(TextAttr != DEFAULTATTR)
        {
            static const char Swap[] = "04261537";

            if(AnsiOpt)
            {
                bool pp = false; // semicolon needed

                if((OldAttr&0x80) > (TextAttr&0x80)
                || (OldAttr&0x08) > (TextAttr&0x08))
                {
                    // '0' resets both blink and intensity flags
                    Buffer[Buflen++] = '0'; pp=true;
                    OldAttr = DEFAULTATTR; // Presumed default color
                }

                if((TextAttr&0x08) && !(OldAttr&0x08)){if(pp)Buffer[Buflen++]=';';Buffer[Buflen++]='1';pp=true;}
                if((TextAttr&0x80) && !(OldAttr&0x80)){if(pp)Buffer[Buflen++]=';';Buffer[Buflen++]='5';pp=true;}

                if((TextAttr&0x70) != (OldAttr&0x70))
                {
                    if(pp)Buffer[Buflen++] = (';');
                    Buffer[Buflen++] = ('4');
                    Buffer[Buflen++] = (Swap[(TextAttr>>4)&7]);
                    pp=true;
                }

                if((TextAttr&7) != (OldAttr&7))
                {
                    if(pp)Buffer[Buflen++] = (';');
                    Buffer[Buflen++] = ('3');
                    Buffer[Buflen++] = (Swap[TextAttr&7]);
                }
            }
            else
            {
                Buffer[Buflen++] = '0';
                if(TextAttr&0x08) { Buffer[Buflen++]=';';Buffer[Buflen++]='1'; }
                if(TextAttr&0x80) { Buffer[Buflen++]=';';Buffer[Buflen++]='5'; }
                Buffer[Buflen++] = '4';
                Buffer[Buflen++] = Swap[(TextAttr>>4)&7];
                Buffer[Buflen++] = '3';
                Buffer[Buflen++] = Swap[(TextAttr   )&7];
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
    int TmpAttr = TextAttr;

    // When background color = foreground color, print only blanks
    if((TmpAttr>>4)==(TmpAttr&0x0F) && x > ' ') x = ' ';

    // When printing a newline, always do that with default background color
    if(x=='\n' && (TextAttr&0xF0)) GetDescrColor(ColorDescr::TEXT, 1);

    // If printing spaces, only change color when the background color changes
    if(x!=' ' || ((TextAttr&0xF0) != (OldAttr&0xF0)))
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
            static const char spacebuf[16] = {' ',' ',' ',' '};
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
    for(char c: s) Gputch(c);
    return s.size();
}

std::size_t Gwrite(const std::string& s, std::size_t pad)
{
    if(pad < s.size()) return Gwrite(s);
    std::size_t res = Gwrite(s);
    while(res < pad) { ++res; Gputch(' '); }
    return pad;
}
