#include <cstring>
#include <cctype>

#include "config.h"
#include "wildmatch.hh"
 
int IgnoreCase;

#if NEW_WILDMATCH

int WildMatch(const char *pattern, const char *what)
{
	for(;;)
	{
		#define cc(x) (IgnoreCase ? tolower((int)(x)) : (x))
		if(*pattern == '*')
		{
			while(*++pattern == '*');
			for(;;)
			{
				register int a = WildMatch(pattern, what);
				if(a != 0)return a; /* Täystäsmä(1) tai meni ohi(-1) */
				what++;
			}
			return 0;
		}
		if(*pattern == '?' && *what)goto AnyMerk;
		#if SUPPORT_BRACKETS
		if(*pattern == '[')
		{
			int Not=0;
			register int mrk=*what;
			if(*++pattern=='^')++pattern, Not=1;
			while(*pattern != ']')
			{
				register int a, left = cc(*pattern++), m = cc(mrk);
				if(*pattern=='-')
					a = m >= left && m <= cc(pattern[1]), pattern += 2;
				else
					a = m == left;
				if(Not ^ !a)return 0;
			}
		}
		else
		#endif
		{
			if(*pattern == '\\')
			{
				++pattern;
				if(*pattern=='d') { if(!isdigit((int)*what))return 0; goto AnyMerk; }
				if(*pattern=='w') { if(!isalpha((int)*what))return 0; goto AnyMerk; }
			}
			if(!*what)return *pattern
				? -1		/* nimi loppui, patterni ei (meni ohi) */
				: 1;		/* molemmat loppui, hieno juttu        */
			if(cc(*pattern) != cc(*what))return 0; /* Epätäsmä.    */
		}
	AnyMerk:
		what++;
		pattern++;
		#undef cc
	}
}

#else

int WildMatch(const char *Pattern, const char *What)
{
	register const char *p=Pattern, *n = What;
	register char c;
	
	while((c = *p++) != 0)
    {
    	#define FOLD(c) (IgnoreCase ? toupper(c) : (c))
    	c = FOLD(c);
		switch(c)
		{
			case '?':
				if(!*n || *n=='/')return 0;
				break;
			case '\\':
				if(FOLD(*n) != c)return 0;
				break;
			case '*':
				for(c=*p++; c=='?' || c=='*'; c=*p++, ++n)
					if(*n == '/' || (c == '?' && *n == 0))return 0;
				if(!c)return 1;
				{
					char c1 = FOLD(c);
					for(--p; *n; n++)
						if((c == '[' || FOLD(*n) == c1) && WildMatch(p, n))
							return 1;
					return 0;
				}
			case '[':
			{
				/* Nonzero if the sense of the character class is inverted.  */
				register int Not;
				if(!*n)return 0;
				Not = (*p == '!' || *p == '^');
				if(Not)p++;
				c = *p++;
				for(;;)
				{
					register char cstart, cend;
					cstart = cend = FOLD(c);
					if(!c)return 0;
					c = FOLD(*p);
					p++;
					if(c=='/')return 0;
					if(c=='-' && *p!=']')
					{
						if(!(cend = *p++))return 0;
						cend = FOLD(cend);
						c = *p++;
					}
					if(FOLD(*n) >= cstart && FOLD(*n) <= cend)
					{
						while(c != ']')
						{
							if(!c)return 0;
							c = *p++;
						}
						if(Not)return 0;
						break;
					}
					if(c == ']')break;
				}
				if(!Not)return 0;
				break;
			}
			default:
				if(c != FOLD(*n))return 0;
		}
		n++;
	}
	return !*n;
}
#endif

int rmatch(const char *Name, const char *Items)
{
	int Index;
    char *s, *n;
    if(!Name || !Items)return 0;

    char *Buffer = new char[strlen(Items)+1];
    strcpy(Buffer, Items);

    for(n=Buffer, Index=1; (s=strtok(n, " ")) != NULL; Index++)
    {
        if(WildMatch(s, Name) > 0)goto Ret;
        n=NULL;
    }

    Index=0;

Ret:
	delete[] Buffer;
	return Index;
}
