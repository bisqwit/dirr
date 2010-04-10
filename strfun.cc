#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <unistd.h>

#include "config.h"
#include "strfun.hh"

string NameOnly(const string &Name)
{
    const char *q, *s = Name.c_str();

    while((q = strchr(s, '/')) && q[1])s = q+1;

    q = strchr(s, '/');
    if(!q)q = strchr(s, 0);

    return string(s, 0, q-s);
}

// Ends with '/'.
// If no directory, returns empty string.
string DirOnly(const string &Name)
{
	const char *q, *s = Name.c_str();
	q = strrchr(s, '/');
	if(!q)return "";
	
	return string(s, 0, q-s+1);
}

string LinkTarget(const string &link, bool fixit)
{
	char Target[PATH_MAX+1];

    int a = readlink(link.c_str(), Target, sizeof Target);
	if(a < 0)a = 0;
	Target[a] = 0;
	
	if(!fixit)return Target;

	if(Target[0] == '/')
	{
		// Absolute link
		return Target;
	}

	// Relative link
	return DirOnly(link) + Target;
}

void PrintNum(string &Dest, int Seps, const char *fmt, ...)
{
	int Len;
	
	Dest.resize(2048);
	
	va_list ap;
	va_start(ap, fmt);
	Dest.erase(vsprintf(const_cast<char *>(Dest.c_str()), fmt, ap));
	va_end(ap);
	
	if(Seps)
    {
        int SepCount;
        /* 7:2, 6:1, 5:1, 4:1, 3:0, 2:0, 1:0, 0:0 */

		const char *End = strchr(Dest.c_str(), '.');
		if(!End)End = Dest.c_str() + Dest.size();
		
		Len = (int)(End-Dest.c_str());
		
		SepCount = (Len - 1) / 3;
	
        for(; SepCount>0; SepCount--)
        {
        	Len -= 3;
        	Dest.insert(Len, " ");
        }
    }
}

string GetError(int e)
{
    return strerror(e); /*
	int a;
	static char Buffer[64];
	strncpy(Buffer, strerror(e), 63);
	Buffer[63] = 0;
	a=strlen(Buffer);
	while(a && Buffer[a-1]=='\n')Buffer[--a]=0;
	return Buffer; */
}

string Relativize(const string &base, const string &name)
{
	const char *b = base.c_str();
	const char *n = name.c_str();
	
//	fprintf(stderr, "Relativize(\"%s\", \"%s\") -> \"", b, n);	
	
	for(;;)
	{
		const char *b1 = strchr(b, '/');
		const char *n1 = strchr(n, '/');
		if(!b1 || !n1)break;
		if((b1-b) != (n1-n))break;
		if(strncmp(b, n, b1-b))break;
		
		b = b1+1;
		n = n1+1;		
	}
	
	string retval;
	
	while(*n=='.' && n[1]=='/') n += 2;
	
	if(*n=='/')
	{
		/* Absolute path, can do nothing */
		return n;
	}
	
	/* archives/tiedosto /WWW/src/tiedosto */
	
	for(;;)
	{
		while(*b=='.' && b[1]=='/') b += 2;
		const char *b1 = strchr(b, '/');
		if(!b1)break;
		b = b1+1;
		retval += "../";
	}
	retval += n;
	
//	fprintf(stderr, "%s\"\n", retval.c_str());
	
	return retval;
}

