#include <cstdio>

#include "config.h"
#include "setfun.hh"
#include "cons.hh"
#include "colouring.hh"

int GetNameAttr(const struct stat &Stat, const string &fn)
{
    int NameAttr = NameColor(fn);

    if(!WasNormalColor)return NameAttr;

    #ifdef S_ISLNK
    if(S_ISLNK(Stat.st_mode))       NameAttr = GetModeColor("type", -'l');
    else
    #endif
         if(S_ISDIR(Stat.st_mode))  NameAttr = GetModeColor("type", -'d');
    else if(S_ISCHR(Stat.st_mode))  NameAttr = GetModeColor("type", -'c');
    else if(S_ISBLK(Stat.st_mode))  NameAttr = GetModeColor("type", -'b');
    #ifdef S_ISFIFO
    else if(S_ISFIFO(Stat.st_mode)) NameAttr = GetModeColor("type", -'p');
    #endif
    #ifdef S_ISSOCK
    else if(S_ISSOCK(Stat.st_mode)) NameAttr = GetModeColor("type", -'s');
    #endif
    else if(Stat.st_mode&00111)     NameAttr = GetModeColor("type", -'x');

    return NameAttr;
}

void PrintAttr(const struct stat &Stat, char Attrs, int &Len, unsigned int dosattr)
{
    switch(Attrs)
    {
        case '0':
            #define PutSet(c) {GetModeColor("mode", c);Gputch(c);Len++;}
        	if(dosattr&0x20)PutSet('A')else PutSet('-')
        	if(dosattr&0x02)PutSet('H')else PutSet('-')
        	if(dosattr&0x04)PutSet('S')else PutSet('-')
        	if(dosattr&0x01)PutSet('R')else PutSet('-')
            break;
        case '1':
        {
        #ifndef DJGPP
        	int Viva = GetModeColor("mode", '-');
            #ifdef S_ISLNK
                 if(S_ISLNK(Stat.st_mode))  PutSet('l')
            else
            #endif
                 if(S_ISDIR(Stat.st_mode))  PutSet('d')
            else if(S_ISCHR(Stat.st_mode))  PutSet('c')
            else if(S_ISBLK(Stat.st_mode))  PutSet('b')
            #ifdef S_ISFIFO
            else if(S_ISFIFO(Stat.st_mode)) PutSet('p')
            #endif
            #ifdef S_ISSOCK
            else if(S_ISSOCK(Stat.st_mode)) PutSet('s')
            #endif
            else if(S_ISREG(Stat.st_mode))  PutSet('-')
            else
            {
            	SetAttr(7);
                Gprintf("\n\n\nThis should never happen.\n");
                exit(-1);
                PutSet('?')
            }

            SetAttr(Viva);
            Gputch((Stat.st_mode&00400)?'r':'-');
            Gputch((Stat.st_mode&00200)?'w':'-');

            GetModeColor("mode", (Stat.st_mode&00100)?'x':'-');
            
            Gputch("-xSs"[((Stat.st_mode&04000)>>10)|((Stat.st_mode&00100)>>6)]);

            SetAttr(Viva);
            Gputch((Stat.st_mode&00040)?'r':'-');
            Gputch((Stat.st_mode&00020)?'w':'-');

            GetModeColor("mode", (Stat.st_mode&00010)?'x':'-');
        #if defined(SUNOS)||defined(__sun)||defined(SOLARIS)
            Gputch("-xls"[((Stat.st_mode&02000)>>9)|((Stat.st_mode&00010)>>3)]);
        #else
            Gputch("-xSs"[((Stat.st_mode&02000)>>9)|((Stat.st_mode&00010)>>3)]);
        #endif

            SetAttr(Viva);
            Gputch((Stat.st_mode&00004)?'r':'-');
            Gputch((Stat.st_mode&00002)?'w':'-');

            GetModeColor("mode", (Stat.st_mode&00001)?'x':'-');
            Gputch("-xTt"[((Stat.st_mode&01000)>>8)|((Stat.st_mode&00001))]);

            Len += 9;

        #endif /* not djgpp */
            break;

           	#undef PutSet
        }
        case '2':
            Attrs = '3';
        default:
        {
    		char Buf[104]; /* 4 is max extra */
            Attrs -= '0';
            sprintf(Buf, "%0100o", (int)Stat.st_mode);
            GetModeColor("mode", '#');
            Len += Gprintf("%s", Buf+100-Attrs);
        }
	}
}
