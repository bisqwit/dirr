#include "config.h"
#include "pwfun.hh"

#if defined(HAVE_GETPWENT_PWD_H) || defined(HAVE_GETPWUID_PWD_H)
# include <pwd.h>
#endif
#if defined(HAVE_GETGRENT_GRP_H) || defined(HAVE_GETGRGID_GRP_H)
# include <grp.h>
#endif
#include <string>
#include <unordered_map>

#if PRELOAD_UIDGID && defined(HAVE_GETPWENT_PWD_H) && defined(HAVE_GETGRENT_GRP_H)

std::string Getpwuid(int uid)
{
    static struct loader: public std::unordered_map<int, std::string>
    {
        loader()
        {
            for(setpwent(); ;)
            {
                struct passwd *p = getpwent(); if(!p) break;
                this->emplace(p->pw_uid, p->pw_name);
            }
            endpwent();
    }   } cache;
    auto i = cache.find(uid);
    return i == cache.end() ? std::string{} : i->second;
}

std::string Getgrgid(int gid)
{
    static struct loader: public std::unordered_map<int, std::string>
    {
        loader()
        {
            for(setgrent(); ;)
            {
                struct group *p = getgrent(); if(!p) break;
                this->emplace(p->gr_gid, p->gr_name);
            }
            endgrent();
    }   } cache;
    auto i = cache.find(gid);
    return i == cache.end() ? std::string{} : i->second;
}

#elif !(defined(HAVE_GETPWUID_PWD_H) && defined(HAVE_GETGRGID_GRP_H))

std::string Getpwuid(int) { return {}; }
std::string Getgrgid(int) { return {}; }

#else

std::string Getpwuid(int uid)
{
    static std::unordered_map<int, std::string> cache;
    auto i = cache.find(uid);
    if(i == cache.end())
    {
        struct passwd *tmp = getpwuid(uid);
        cache.emplace(uid, tmp ? tmp->pw_name : std::string{});
        return tmp ? tmp->pw_name : std::string{};
    }
    return i->second;
}
std::string Getgrgid(int gid)
{
    static std::unordered_map<int, std::string> cache;
    auto i = cache.find(gid);
    if(i == cache.end())
    {
        struct group *tmp = getgrgid(gid);
        cache.emplace(gid, tmp ? tmp->gr_name : std::string{});
        return tmp ? tmp->gr_name : std::string{};
    }
    return i->second;
}

#endif
