#include <cstdio>
#include <cstdarg>
#include <string>
#include <cstdlib>
#include <cstring>

using namespace std;

#include "config.h"
#include "cons.hh"

#ifdef FREEBSD50
#include <linux_ioctl.h>
#define TCGETA LINUX_TCGETA
#define TCSETA LINUX_TCSETA
#endif

#ifndef linux
// In SunOS, sys/ioctl.h doesn't define ioctl()
// function, but it's instead in unistd.h.
#include <unistd.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#define DEFAULTATTR 7

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
#else
#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#endif

bool Colors     = true;
bool AnsiOpt    = true;
bool Pagebreaks = false;

int WhereX=0;
int LINES=25, COLS=80;

/* Bigger COLS-values don't work. Change it if it's not ok. */
#define MAX_COLS 1024

static int TextAttr = DEFAULTATTR;
static int OldAttr  = DEFAULTATTR;
static int ColorNumToggle = 0;

extern int GetDescrColor(const string &descr, int index);

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
#else
#ifdef TIOCGWINSZ
	    struct winsize w;
	    if(ioctl(1, TIOCGWINSZ, &w) >= 0)
	    {
	        LINES=w.ws_row;
	        COLS =w.ws_col;
	    }
#else
#ifdef WIOCGETD
	    struct uwdata w;
	    if(ioctl(1, WIOCGETD, &w) >= 0)
	    {
	        LINES = w.uw_height / w.uw_vs;
	        COLS  = w.uw_width / w.uw_hs;
	    }
#endif
#endif
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
        printf("\33[");

        if(TextAttr != 7)
        {
        	static const char Swap[] = "04261537";
        	
        	if(AnsiOpt)
        	{
    	    	int pp=0;
    	    	
    	    	if((OldAttr&0x80) > (TextAttr&0x80)
    	    	|| (OldAttr&0x08) > (TextAttr&0x08))
    	    	{
    	    		putchar('0'); pp=1;
    	    		OldAttr = 7;
    	    	}    	
    	    	
    	    	if((TextAttr&0x08) && !(OldAttr&0x08)){if(pp)putchar(';');putchar('1');pp=1;}
    	    	if((TextAttr&0x80) && !(OldAttr&0x80)){if(pp)putchar(';');putchar('5');pp=1;}
    	       	
    	       	if((TextAttr&0x70) != (OldAttr&0x70))
    	       	{
    	   	   		if(pp)putchar(';');
    	       		putchar('4');
    	       		putchar(Swap[(TextAttr>>4)&7]);
    	       		pp=1;
    	       	}
    	       	
    	      	if((TextAttr&7) != (OldAttr&7))
    	      	{
    	       		if(pp)putchar(';');
    	       		putchar('3');
    	       		putchar(Swap[TextAttr&7]);
    	       	}
           	}
           	else
    	    	printf("0%s%s;4%c;3%c",
        			(TextAttr&8)?";1":"",
        			(TextAttr&0x80)?";5":"",
        			Swap[(TextAttr>>4)&7],
    	    		Swap[TextAttr&7]);
        }

        putchar('m');
#endif
    }
	OldAttr = TextAttr;
}

#ifndef DJGPP
static int Ggetch()
{
#ifdef HAVE_TERMIOS_H
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
#else
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
	if(!TmpAttr && x > ' ')x = ' ';
    if(x=='\n' && (TextAttr&0xF0))GetDescrColor("txt", 1);

    if(x!=' ' || ((TextAttr&0xF0) != (OldAttr&0xF0)))
		FlushSetAttr();
		
	#ifdef DJGPP
	
	(Colors?putch:putchar)(x);
	
	#else
	
	static int Mask[MAX_COLS]={0};
	
	if(x=='\a')
	{
		putchar(x);
		return x;
	}
	if(AnsiOpt && Colors)
	{
		static int Spaces=0;
		if(x==' ')
		{
			Spaces++;
			return x;
		}
		while(Spaces)
		{
			int a=WhereX, Now, mask=Mask[a];
			
			for(Now=0; Now<Spaces && Mask[a]==mask; Now++, a++);
			
			Spaces -= Now;
			if(mask)
			{
			Fill:
				while(Now>0){Now--;putchar(' ');}
			}
			else if(Spaces || !(x=='\r' || x=='\n'))
			{
				if(Now<=4)goto Fill;
				printf("\33[%dC", Now);
			}
			WhereX += Now;
		}
	}
	putchar(x);	
	
	#endif	
	
	if(x=='\n')
		SetAttr(TmpAttr);
	
	if(x=='\r')
		WhereX = 0;
	else if(x != '\b')
    {
#ifdef DJGPP
		if(x < ' ')WhereX = 0;
#else
		if(x >= ' ')
			Mask[WhereX++] = 1;
		else
		    memset(Mask, WhereX=0, sizeof Mask);
#endif
    }
	else if(WhereX)
		WhereX--;	
		
    // Newlinecheck
    if(x=='\n')
    {
        if(++Line >= LINES)
        {
            if(Pagebreaks)
            {
            	int More=LINES-2;
                int ta = TextAttr;
                SetAttr(0x70);
                Gprintf("\r--More--");
                GetDescrColor("txt", 1);
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
                Gprintf("\r        \r");
                if(More<0)exit(0);
                SetAttr(ta);
                Line -= More;
                GetScreenGeometry();
            }
        }
    }

	return x;
}

int ColorNums = -1;
int Gprintf(const char *fmt, ...)
{
    char Buf[2048];

    va_list ap;
    va_start(ap, fmt);
    int a = vsprintf(Buf, fmt, ap);
    va_end(ap);

    for(char *s=Buf; *s; s++)
    {
    	if(*s=='\1')
    		ColorNumToggle ^= 1;
        else
        {
#ifdef DJGPP
            if(*s=='\n')Gputch('\r');
#endif
            if(ColorNumToggle)
            {
            	int ta = TextAttr;
            	SetAttr(ColorNums);
            	Gputch(*s);
            	SetAttr(ta);
            }
            else
            	Gputch(*s);
	    }
    }
    return a;
}
