#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <unistd.h>

#include "colouring.hh"
#include "setfun.hh"
#include "strfun.hh"
#include "getname.hh"

#ifndef MAJOR
 #define MAJOR(dev) ((dev) >> 8)
 #define MINOR(dev) ((dev) & 255)
#endif

string BlkStr, ChrStr;

string GetSize(const string &s, const struct stat &Sta, int Space, int Seps)
{
	unsigned Bufsize = Space<256 ? 256 : Space;
	char *Buf = new char[Bufsize];
    const char *descr = "descr";
    
    const struct stat *Stat = &Sta;

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
        if(Links != 1 && Links != 4)goto P1;
LinkProblem:
        sprintf(Buf, "%*s", -Space, "<LINK>");
    }
    #endif
    else
    {
        long l;

#ifdef S_ISLNK
P1:
		if(S_ISLNK(Stat->st_mode))
		{
			if(Links==2)descr = NULL;
			if(Links==3)
			{
				static struct stat Stat1;
				string Buf = LinkTarget(s);
       	     	
    	        // Target status
	    	    if(stat(Buf.c_str(), &Stat1) >= 0)
	    	    {
    		    	Stat = &Stat1;
    	    		goto GotSize;
				}
				goto LinkProblem;
			}
		}
#endif
        l = Stat->st_size;
        
        string TmpBuf;
        PrintNum(TmpBuf, Seps, "%lu", l);
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
