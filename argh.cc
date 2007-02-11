#include <cstdlib>
#include <cstring>

#include "argh.hh"

#ifdef ColourPrints
#include "cons.hh"
#define GPRINTF_ARGS 
#else
#include <cstdio>

#define GPRINTF_ARGS stderr,
#define Gputch(x) putc((x), stderr)
#define Gprintf fprintf
#define SetAttr(x) 
#endif

/* This file is part of Bisqwit's dirr and remotegcc packages. */

arghandler::arghandler(const char *defopts, int argc, const char *const *argv)
{
    const char *q;
    
    q = strrchr(argv[0], '/');
    a0 = q ? q+1 : argv[0];
    
    if(defopts)
    {
        q = defopts;
        while(*q)
        {
            const char *p = strchr(q, ' ');
            if(!p)break;
            args.push_back(string(q, p-q));
            q = p+1;
        }
        args.push_back(q);
    }
    bool terminated = false;
    
    /* If you have 5 parameters and want to skip 2 parameters
     * at th ebeginning, specify argc as: (2-argc)*MAX_ARGC-2
     */
    if(argc < 0)
    {
        argc = -argc;
        argv += (argc%MAX_ARGC);
        argc /= MAX_ARGC;
    }
    while(--argc)
    {
        q = *++argv;
        if(!strcmp(q, "--"))
            terminated = true;
        else if(!terminated)
        {
            if(!strncmp(q, "--", 2))
            {
                const char *p = strchr(q, '=');
                if(p)
                {
                    string tmp(q, p-q);
                    args.push_back(tmp);
                    args.push_back(p+1);
                    continue;
                }
            }
        }
        args.push_back(q);
    }
}

arghandler::~arghandler()
{
    for(vector<option>::iterator i=options.begin(); i!=options.end(); ++i)
        delete i->handler;
}

void arghandler::subadd(const char *Short, const char *Long, const string &Descr, argfun handler)
{
    options.push_back(option(Short ? Short+1 : "",
                             Long ? Long+2 : "",
                             Descr, handler));
}

void arghandler::parse()
{
    bool terminated = false;
    unsigned a, b=args.size();
    for(a=0; a<b; ++a)
    {
        string s = args[a];
        if(s.size() > 1 && !terminated && s[0] == '-')
        {
            if(s == "--")
                terminated = true;
            else if(s[1] == '-')
            {
                s.erase(0, 2);
                vector<option>::iterator i;
                
                for(i=options.begin(); i!=options.end(); ++i)
                    if(*i->Long && s == i->Long)
                        break;
                   
                if(i == options.end())
                {
                    argerror(s, false);
                    return;
                }
                unsigned c = a+1;
                if(c == b)--c;
                s = args[c];
                s = i->handler->CallBack(this, s);
                if(s.size())
                {
                    if(s != args[c])
                    {
                        argerror(args[a]);
                        return;
                    }
                }
                else
                    a = c;
            }
            else
            {
                s.erase(0, 1);
                vector<option>::iterator i;
                
                while(s.size())
                {
                       for(i=options.begin(); i!=options.end(); ++i)
                           if(s.substr(0, strlen(i->Short)) == i->Short)
                               break;
                       if(i == options.end())
                       {
                           argerror(s[0]);
                           return;
                       }
                       s = i->handler->CallBack(this, s.substr(strlen(i->Short)));
                }
            }
        }
        else
            defarg(s);
    }
}

void arghandler::argerror(char c)
{
    Gprintf(GPRINTF_ARGS "%s: illegal option -- %c\n", a0.c_str(), c);
    suggesthelp();
    exit(1);
}

void arghandler::argerror(const string &s, bool param)
{
    Gprintf(GPRINTF_ARGS "%s: %s%s'\n",
        a0.c_str(),
        param ? "invalid parameter `" : "unrecognized option `--",
        s.c_str());
    if(!param)suggesthelp();
    exit(1);
}

void arghandler::suggesthelp()
{
    Gprintf(GPRINTF_ARGS "Try `%s --help' for more information.\n", a0.c_str());
}

void arghandler::listoptions()
{
    vector<option>::const_iterator i;
    unsigned longestshort=0, longestlong=0;
    for(i=options.begin(); i!=options.end(); ++i)
    {
        unsigned slen = strlen(i->Short);
        if(slen > longestshort)longestshort = slen;
        unsigned llen = strlen(i->Long);
        if(llen > longestlong)longestlong = llen;
    }
    unsigned space = longestshort + longestlong+1;
    for(i=options.begin(); i!=options.end(); ++i)
    {
        const char *s = i->Short;
        const char *l = i->Long;
        
        SetAttr(*s ? 3 : 0);
        Gprintf(GPRINTF_ARGS "  -");
        SetAttr(7);
        Gprintf(GPRINTF_ARGS "%s", s);
        
        SetAttr(*l ? 3 : 0);
        Gprintf(GPRINTF_ARGS ", --");
        SetAttr(7);
        Gprintf(GPRINTF_ARGS "%s", l);
        
        Gprintf(GPRINTF_ARGS "%*s", space-(strlen(s) + strlen(l)), "");
        
        SetAttr(7);
        
        const char *q = i->Descr.c_str();
        bool needspace = false;
        bool needeol = true;
        for(; *q; ++q)
        {
            if(needspace)
            {
                Gprintf(GPRINTF_ARGS "\n%*s", space+7, "");
                needspace = false;
            }
            needeol = true;
            if(*q != '\n')
            {
                Gputch(*q);
                continue;
            }
            needspace = true;
            needeol = false;
        }
        if(needspace || needeol)Gprintf(GPRINTF_ARGS "\n");
    }
}
