#include <unistd.h>

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

static void TulostaNimi(unsigned maxlen, const string &buf)
{
	const unsigned char *s = (const unsigned char *)buf.c_str();
	
	while(*s)
	{
		if(!maxlen)break;
		if(*s < 32 || (*s >= 127 && *s <= 160))
			Gputch('?');
		else
			Gputch(*s);
		--maxlen;
		++s;
	}

	while(maxlen>0)Gputch(' ');
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
    
    TulostaNimi(i, Buf);
    Space -= i;

    #define PutSet(c) do if((++Len,Space)&&GetModeColor("info", c))Gputch(c),--Space;while(0)
    
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
            if(a)
            {
            	Buf.erase(a);
                GetModeColor("info", '@');
                Gprintf("%*s", -a, Buf.c_str());
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
    	        	if(Space)GetModeColor("type", '?');
    	        }
    	        else
    	        {
    	        	if(Space)GetModeColor("type", 'l');
    	        }
    	        maysublink = false;
			}
	        else if(Space)
    	    {
    	    	StatType Stat2;
    	    	if(LStatFunc(Buf.c_str(), &Stat2) >= 0 && S_ISLNK(Stat2.st_mode))
   	    	    	GetModeColor("type", 'l');
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
        
    	SetAttr(GetModeColor("info", '&'));
    	
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

    if(Fill)
        while(Space)
        {
            Gputch(' ');
            Space--;
        }

    return Len;
}
