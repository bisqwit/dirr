#include "config.h"
#include "pwfun.hh"

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#include <string>

using std::string;

#if PRELOAD_UIDGID||CACHE_UIDGID

#include <map>

using std::map;

typedef map<int, string> idCache;

static idCache GidItems, UidItems;

#endif

#if PRELOAD_UIDGID

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
    	int a;
    	
    	for(setpwent(); ;)
    	{
    		struct passwd *p = getpwent();
    		UidItems[p->pw_uid] = p->pw_name;
    	}
    	endpwent();
    	
    	for(setgrent(); ;)
    	{
    		struct group *p = getgrent();
    		UidItems[p->gr_gid] = p->gr_name;
    	}
    	endgrent();
	}
} PwLoader;

#else /* no PRELOAD_UIDGID */

#if CACHE_UIDGID

const char *Getpwuid(int uid)
{
	idCache::iterator i;
	i = UidItems.find(uid);
	if(i==UidItems.end())
	{
		const char *s;
		struct passwd *tmp = getpwuid(uid);
		s = tmp ? tmp->pw_name : "";
		UidItems[uid] = s;
		return s;
	}
	return i->second.c_str();
}
const char *Getgrgid(int gid)
{
	idCache::iterator i;
	i = GidItems.find(gid);
	if(i==GidItems.end())
	{
		const char *s;
		struct group *tmp = getgrgid(gid);
		s = tmp ? tmp->gr_name : "";
		GidItems[gid] = s;
		return s;
	}
	return i->second.c_str();
}

#else

const char *Getpwuid(int uid)
{
	struct passwd *tmp = getpwuid(uid);
	if(!tmp)return NULL;
	return tmp->pw_name;
}
const char *Getgrgid(int gid)
{
	struct group *tmp = getgrgid(gid);
	if(!tmp)return NULL;
	return tmp->gr_name;
}

#endif
#endif
