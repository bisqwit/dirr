#include <vector>
#include <map>

#include "wildmatch.hh"
#include "setfun.hh"
#include "cons.hh"

bool Dumping = false; // If 0, save some time

/***
 *** Settings - This is the default DIRR_COLORS
 ***
 *** The colour codes are one- or two-digit hexadecimal
 *** numbers, which consist of the following values:
 ***
 ***   80 = blink
 ***   40 = background color red
 ***   20 = background color green
 ***   10 = background color blue
 ***   08 = foreground high intensity
 ***   04 = foreground color red
 ***   02 = foreground color green
 ***   01 = foreground color blue
 ***
 *** Add those to make combination colors:
 ***
 ***   09 = 9 = 8+1         = bright blue
 ***   4E = 40+E = 40+8+4+2 = bright yellow (red + green) on red background
 ***    2 = 02              = green.
 ***
 *** I hope you understand the basics of hexadecimal arithmetic.
 *** If you have ever played with textattr() function in
 *** Borland/Turbo C++ or with TextAttr variable in
 *** Borland/Turbo Pascal or assigned colour values directly
 *** into the PC textmode video memory, you should understand
 *** how the color codes work.
 ***/

static string Settings =
#include SETTINGSFILE
;

int WasNormalColor;

/* Setting separator marks */
#define SetSep(c) ((c)== ')'|| \
                   (c)=='\t'|| \
                   (c)=='\n'|| \
                   (c)=='\r')

#if CACHE_GETSET
static map <int, map<string, string> > gscache;
static const string sNULL = "\1";
#endif

static string GetSet(const char **S, const string &name, int index)
{
#if CACHE_GETSET
    map<int, map<string, string> >::const_iterator i = gscache.find(index);
    if(i != gscache.end())
    {
    	map<string, string>::const_iterator j = i->second.find(name);
    	if(j != i->second.end())return j->second;
    }
#else
	index=index;
#endif
    const char *s = *S;
    string t;
    
    if(s)
    {
        /* Setting string separators */
        unsigned namelen = name.size();
        
        for(;;)
        {
        	while(SetSep(*s))s++;

            if(!*s)break;

            if(!memcmp(s, name.c_str(), namelen) && s[namelen] == '(')
            {
            	const char *p;
                int Len;
                for(Len=0, p=s; *p && !SetSep(*p); ++Len)++p;
        
                *S = *p?p+1:NULL;
                if(!**S)*S=NULL;
                
                t.assign(s, 0, Len);
                goto Retu;
            }
        
            while(*s && !SetSep(*s))s++;
        }
    }
    t = sNULL;
    *S = NULL;
Retu:
#if CACHE_GETSET
	gscache[index][name] = t;
#endif
    return t;
}

int GetHex(int Default, const char **S)
{
    int eka, Color = Default;
    const char *s = *S;

    for(eka=1; *s && isxdigit((int)*s); eka=0, s++)
    {
        if(eka)Color=0;else Color<<=4;

        if(*s > '9')
            Color += 10 + toupper(*s) - 'A';
        else
            Color += *s - '0';
    }

    *S = s;
    return Color;
}

int GetModeColor(const string &text, int Chr)
{
    int Dfl = 7;

    const char *s = Settings.c_str();

    int Char = Chr<0?-Chr:Chr;

    for(;;)
    {
    	string T = GetSet(&s, text, 0);
        if(T == sNULL)break;

        const char *t = T.c_str() + text.size() + 1; /* skip 'text=' */

        while(*t)
        {
            int C, c=*t++;

            C = GetHex(Dfl, &t);

            if(c == Char)
            {
                if(Chr > 0)SetAttr(C);
                return C;
            }
            if(*t == ',')t++;
        }
    }

    Gprintf("DIRR_COLORS error: No color for '%c' found in '%s'\n", Char, text.c_str());

    return Dfl;
}

int GetDescrColor(const string &descr, int index)
{
    int Dfl = 7;
    if(!Colors || !Dumping)return Dfl;
    
    int ind = index<0 ? -index : index;
    
    const char *s = Settings.c_str();    
    string t = GetSet(&s, descr, 0);
    
    if(t == sNULL)
        Gprintf("DIRR_COLORS error: No '%s'\n", descr.c_str());
    else
    {
        const char *S = t.c_str() + descr.size();
        for(; ind; ind--)
        {	
        	++S;
        	Dfl = GetHex(Dfl, &S);
        }
        if(index>0)SetAttr(Dfl);
    }
    return Dfl;
}

void PrintSettings()
{
    const char *s = Settings.c_str();
    int LineLen, Dfl;
    
    Dfl = GetDescrColor("txt", -1);
    
    for(LineLen=0;;)
    {
        while(SetSep(*s))s++;
        if(!*s)break;
        
        int Len;
        const char *t = s;
        for(Len=0; *t && !SetSep(*t); Len++)t++;

        string T(s, 0, Len);
        T += ')';

        if(LineLen && LineLen+strlen(t) > 75)
        {
            Gprintf("\n");
            LineLen=0;
        }
        if(!LineLen)Gprintf("\t");

        LineLen += T.size();
        const char *n = t = T.c_str();

        while(*t!='(')Gputch(*t++);
        Gputch(*t++);

        if(n[4]=='('
        && (!strncmp(n, "mode", 4)
         || !strncmp(n, "type", 4)
         || !strncmp(n, "info", 4)))
        {
            while(*t)
            {
                int c;
                const char *k;
                int len;

                c = *t++;

                k=t;    
                SetAttr(GetHex(Dfl, &k));

                Gputch(c);

                SetAttr(Dfl);

                for(len=k-t; len; len--)Gputch(*t++);

                if(*t != ',')break;
                Gputch(*t++);
            }
            Gprintf("%s", t);
        }
        else
        {
            int C=Dfl, len;
            const char *k;
            for(;;)
            {
                k = t;
                SetAttr(C=GetHex(C, &k));
                for(len=k-t; len; len--)Gputch(*t++);
                SetAttr(Dfl);
                if(*t != ',')break;
                Gputch(*t++);
            }
            if(*t!=')')Gputch(*t++);
            SetAttr(C);
            while(*t!=')')
            {
            	if(!*t)
            	{
            		SetAttr(0x0C);
            		Gprintf("(Error:unterminated)");
            		SetAttr(Dfl);
            		break;
            	}
            	Gputch(*t++);
          	}
            SetAttr(Dfl);
            Gputch(')');
        }
        
        while(*s && !SetSep(*s))s++;
    }
    if(LineLen)
        Gprintf("\n");
}


/*
18:35|>Warp= 1) int NameColor(const char *s) - palauttaa
                nimeä vastaavan värin.
18:36|>Warp= 2) NameColor() käy kaikki byext():t läpi, tai siihen asti
                kunnes sieltä löytyy joku patterni johon annettu nimi menee.
18:36|>Warp= 3) Jokaisen byext():n sisällä on läjä patterneja.
                Stringi annetaan rmatch() -funktiolle.
18:36|>Warp= 4) rmatch() pilkkoo stringin ja testaa jokaista patternia
                erikseen wildmatch:lla.
18:37|>Warp= 5) wildmatch() palauttaa >0 jos täsmäsi.
18:37|>Warp= 6) rmatch() lopettaa testauksen jos wildmatch täsmäsi.
18:37|>Warp= 7) Jos rmatch() palautti 0 (ei täsmännyt) ja viimeinen
                byext() on käyty läpi, palautetaan default-väri.
18:38|>Warp= Ja byext():t tässä ovat niitä asetusstringejä, ei funktioita

*/

#if CACHE_NAMECOLOR
class NameColorItem
{
public:
	string pat;
	int ignorecase;
	int colour;
	NameColorItem(const string &p, int i, int c) : pat(p), ignorecase(i), colour(c) { }
};
static vector<NameColorItem> nccache;
static map<string, int> NameColorCache2;

void BuildNameColorCache()
{
    const char *DirrVar = getenv("DIRR_COLORS");
    if(DirrVar)Settings = DirrVar;

	const char *s = Settings.c_str();
	int Normal = GetDescrColor("txt", -1);
	int index=0;
	
	for(;;)
	{
		int c;
		string T = GetSet(&s, "byext", index++);
		if(T == sNULL)break;
		const char *t = T.c_str() + 6;
		c = GetHex(Normal, &t);
		IgnoreCase = *t++ == 'i';
		string Buffer = t;
		char *Q, *q = (char *)Buffer.c_str();
		for(; (Q=strtok(q, " ")) != NULL; q=NULL)
		{
			NameColorItem tmp(Q, IgnoreCase, c);
			nccache.push_back(tmp);
		}
	}
}

int NameColor(const string &Name)
{
	map<string, int>::const_iterator i = NameColorCache2.find(Name);
	if(i != NameColorCache2.end())return i->second;
	int colo;
	WasNormalColor=0;
	unsigned a, b=nccache.size();
	for(a=0; a<b; ++a)
	{
		IgnoreCase = nccache[a].ignorecase;
		if(WildMatch(nccache[a].pat.c_str(), Name.c_str()) > 0)
		{
			colo = nccache[a].colour;
			goto Done;
		}
	}
	WasNormalColor=1;
	colo = GetDescrColor("txt", -1);
Done:
	return NameColorCache2[Name] = colo;
}

#else

int NameColor(const string &Name)
{
    const char *s = Settings.c_str();
    int Normal = GetDescrColor("txt", -1);
    int index = 0;

    for(WasNormalColor=0;;)
    {
        int c, result;
        const char *t, *T = GetSet(&s, "byext", index++);

        if(!T)break;

        t = T+6; /* skip "byext" */

        c = GetHex(Normal, &t);

        IgnoreCase = *t++ == 'i';

        result = rmatch(Name, t);

        /* free(T); */

        if(result)return c;
    }

    WasNormalColor=1;

    return Normal;
}
#endif
