#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <unistd.h>

#include "config.h"
#include "strfun.hh"

string NameOnly(const string &Name)
{
    std::size_t p = Name.rfind('/');
    // No path?
    if(p == Name.npos) return Name;
    // Regular file?
    if(p <= Name.size()) return Name.substr(p+1);
    // It ends in '/'?
    p = Name.rfind('/', p-1);
    if(p == Name.npos) p = 0; else p = p+1;
    return Name.substr(p, Name.size()-p-1);
}
/*
#include "cons.hh"
string NameOnly(const string &Name)
{
    auto r = NameOnly_(Name);
    Gprintf("NameOnly(%s)=(%s)\n", Name,r);
    return r;
}*/

// Return value ends with '/'.
// If no directory, returns empty string.
string DirOnly(const string &Name)
{
    std::size_t p = Name.rfind('/');
    if(p == Name.npos) return {};
    return Name.substr(0, p+1);
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

//    fprintf(stderr, "Relativize(\"%s\", \"%s\") -> \"", b, n);

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

//    fprintf(stderr, "%s\"\n", retval.c_str());

    return retval;
}

