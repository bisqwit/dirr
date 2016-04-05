#include "config.h"
#include "pwfun.hh"

#if defined(HAVE_GETPWENT_PWD_H) || defined(HAVE_GETPWUID_PWD_H)
# include <pwd.h>
#endif
#if defined(HAVE_GETGRENT_GRP_H) || defined(HAVE_GETGRGID_GRP_H)
# include <grp.h>
#endif
#include <string>

#if PRELOAD_UIDGID||CACHE_UIDGID
#include <unordered_map>
typedef std::unordered_map<int, std::string> idCache;
static idCache GidItems, UidItems;
#endif


#if PRELOAD_UIDGID && defined(HAVE_GETPWENT_PWD_H) && defined(HAVE_GETGRENT_GRP_H)

const char *Getpwuid(int uid)
{
    return UidItems[uid].c_str();
}
const char *Getgrgid(int gid)
{
    return GidItems[gid].c_str();
}

static class ReadGidUid
{
public:
    ReadGidUid()
    {
        for(setpwent(); ;)
        {
            struct passwd *p = getpwent(); if(!p) break;
            UidItems.emplace(p->pw_uid, p->pw_name);
        }
        endpwent();

        for(setgrent(); ;)
        {
            struct group *p = getgrent(); if(!p) break;
            GidItems.emplace(p->gr_gid, p->gr_name);
        }
        endgrent();
    }
} PwLoader;

#elif !(defined(HAVE_GETPWUID_PWD_H) && defined(HAVE_GETGRGID_GRP_H))

const char *Getpwuid(int) { return nullptr; }
const char *Getgrgid(int) { return nullptr; }

#elif CACHE_UIDGID

const char *Getpwuid(int uid)
{
    auto i = UidItems.find(uid);
    if(i==UidItems.end())
    {
        const char *s;
        struct passwd *tmp = getpwuid(uid);
        s = tmp ? tmp->pw_name : "";
        UidItems.emplace(uid, s);
        return s;
    }
    return i->second.c_str();
}
const char *Getgrgid(int gid)
{
    auto i = GidItems.find(gid);
    if(i==GidItems.end())
    {
        const char *s;
        struct group *tmp = getgrgid(gid);
        s = tmp ? tmp->gr_name : "";
        GidItems.emplace(gid, s);
        return s;
    }
    return i->second.c_str();
}

#else

const char *Getpwuid(int uid)
{
    struct passwd *tmp = getpwuid(uid);
    if(!tmp)return nullptr;
    return tmp->pw_name;
}
const char *Getgrgid(int gid)
{
    struct group *tmp = getgrgid(gid);
    if(!tmp)return nullptr;
    return tmp->gr_name;
}

#endif
