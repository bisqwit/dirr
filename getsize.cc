#include <cstdio>
#include <cstring>

#include "config.h"
#include "colouring.hh"
#include "setfun.hh"
#include "strfun.hh"
#include "getname.hh"

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
	unsigned Bufsize = Space<256 ? 256 : Space;
	char *Buf = new char[Bufsize];
    const char *descr = "descr";

    const StatType *Stat = &Sta;

#ifdef S_ISLNK
GotSize:
#endif
		 if(S_ISDIR (Stat->st_mode))sprintf(Buf, "%*s", -Space, "<DIR>");
    #ifdef S_ISFIFO
    else if(S_ISFIFO(Stat->st_mode))sprintf(Buf, "%*s", -Space, "<PIPE>");
    #endif
    #ifdef S_ISSOCK
    else if(S_ISSOCK(Stat->st_mode))sprintf(Buf, "%*s", -Space, "<SOCKET>");
    #endif
    else if(S_ISCHR (Stat->st_mode))
    {
    	char *e;
    	sprintf(Buf, ChrStr.c_str(),
    		(unsigned)MAJOR(Stat->st_rdev),
    		(unsigned)MINOR(Stat->st_rdev));
    	if(Space)
    	{
		    e = strchr(Buf, 0);
    		memset(e, ' ', (Buf + Bufsize)-e);
    		Buf[Space] = 0;
    	}
    }
    else if(S_ISBLK (Stat->st_mode))
    {
    	char *e;
    	sprintf(Buf, BlkStr.c_str(),
    		(unsigned)MAJOR(Stat->st_rdev),
    		(unsigned)MINOR(Stat->st_rdev));
    	if(Space)
    	{
	    	e = strchr(Buf, 0);
	    	memset(e, ' ', (Buf+Bufsize)-e);
    		Buf[Space] = 0;
    	}
    }
    #ifdef S_ISLNK
    else if(S_ISLNK (Stat->st_mode))
    {
    	if(!LinkFormatWithoutSize())goto P1;
LinkProblem:
        sprintf(Buf, "%*s", -Space, "<LINK>");
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
			descr = NULL;
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

        string TmpBuf;
        PrintNum(TmpBuf, Seps, SizeFormat, SizeCast l);

        TmpBuf += Suffix;
        sprintf(Buf, "%*s", Space, TmpBuf.c_str());

        if(descr)descr = "size";
    }

    if(descr)
	    GetDescrColor(descr, 1);
	else
    	GetModeColor("info", '@');

	string tmp(Buf);
	delete[] Buf;
    return tmp;
}
