#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <unistd.h>

#include "config.h"
#include "strfun.hh"

std::string_view NameOnly(std::string_view Name)
{
    std::size_t p = Name.rfind('/');
    // No path?
    if(p == Name.npos) { return Name; }
    // Regular file?
    if(p <= Name.size()) { Name.remove_prefix(p+1); return Name; }
    // It ends in '/'?
    p = Name.rfind('/', p-1);
    if(p == Name.npos) p = 0; else p = p+1;
    Name.remove_prefix(p);
    Name.remove_suffix(1);
    return Name;
}
/*
#include "cons.hh"
std::string NameOnly(std::string_view Name)
{
    auto r = NameOnly_(Name);
    Gprintf("NameOnly(%s)=(%s)\n", Name,r);
    return r;
}*/

// Return value ends with '/'.
// If no directory, returns empty std::string.
std::string_view DirOnly(std::string_view Name)
{
    std::size_t p = Name.rfind('/');
    if(p == Name.npos) return {};
    Name.remove_suffix( Name.size() - (p+1) ); // Remove everything except p+1 letters, which are kept
    return Name;
}

std::string LinkTarget(const std::string& link, bool fixit)
{
    char target[PATH_MAX+1];

    int length = readlink(link.c_str(), target, sizeof target);
    if(length < 0) length = 0;
    target[length] = 0;

    if(!fixit || target[0] == '/')
    {
        // Absolute link
        return std::string(target, length);
    }

    // Relative link
    //return std::string(DirOnly(link)) + target;
    auto dir = DirOnly(link);
    std::string result;
    result.reserve(dir.size() + length);
    result.insert(result.end(), dir.begin(),dir.end());
    result.insert(result.end(), target,     target+length);
    return result;
}

std::string GetError(int e)
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

std::string Relativize(std::string_view base, std::string_view name)
{
//    fprintf(stderr, "Relativize(\"%s\", \"%s\") -> \"", b, n);
    for(;;)
    {
        std::size_t slash_in_base = base.find('/');
        std::size_t slash_in_name = name.find('/');
        if(slash_in_base == base.npos
        || slash_in_name == name.npos) break;
        if(base.compare(0, slash_in_base,
                  name, 0, slash_in_base) != 0) break;
        base.remove_prefix(slash_in_base+1);
        name.remove_prefix(slash_in_name+1);
    }

    while(name.size() >= 2 && name[0]=='.' && name[1]=='/') // Remove sequences of ./
    {
        name.remove_prefix(2);
    }
    if(name.empty() || name[0] == '/')
    {
        /* Absolute path, can do nothing */
        return std::string(name);
    }

    /* archives/tiedosto /WWW/src/tiedosto */

    std::size_t num_dotdot = 0;
    for(;;)
    {
        while(base.size() >= 2 && base[0]=='.' && base[1]=='/') // Remove sequences of ./
        {
            base.remove_prefix(2);
        }
        std::size_t slash_in_base = base.find('/');
        if(slash_in_base == base.npos) break;
        base.remove_prefix(slash_in_base+1);
        ++num_dotdot; // Add one ../ to the beginning
    }
    // Create a string with num_dotdot times ../
    std::string retval(num_dotdot*3, '.');
    for(std::size_t n=0; n<num_dotdot; ++n) retval[n*3+2] = '/';
    // Append the name
    retval += name;
//    fprintf(stderr, "%s\"\n", retval.c_str());
    return retval;
}

