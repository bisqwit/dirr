#include <cstdlib>
#include <cstring>

#include "argh.hh"

#ifdef ColourPrints
# include "cons.hh"
# define GPRINTF_ARGS
#else
# include <cstdio>
# define GPRINTF_ARGS stderr,
# define Gputch(x) putc((x), stderr)
# define Gprintf fprintf
# define SetAttr(x)
#endif

/* This file is part of Bisqwit's dirr and remotegcc packages. */

arghandler::arghandler(const char *defopts, int argc, const char *const *argv)
{
    const char *q;

    q = std::strrchr(argv[0], '/');
    a0 = q ? q+1 : argv[0];

    if(defopts)
    {
        q = defopts;
        while(*q)
        {
            const char *p = std::strchr(q, ' ');
            if(!p)break;
            args.emplace_back(q, p-q);
            q = p+1;
        }
        args.push_back(q);
    }
    bool terminated = false;

    /* If you have 5 parameters and want to skip 2 parameters
     * at the beginning, specify argc as: (2-argc)*MAX_ARGC-2
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
        if(!std::strcmp(q, "--"))
            terminated = true;
        else if(!terminated)
        {
            if(!std::strncmp(q, "--", 2))
            {
                const char *p = std::strchr(q, '=');
                if(p)
                {
                    std::string tmp(q, p-q);
                    args.push_back(std::move(tmp));
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
    for(auto& o: options) delete o.handler;
}

void arghandler::subadd(const char *Short, const char *Long, const std::string &Descr, argfun handler)
{
    options.emplace_back(Short ? Short+1 : "",
                         Long ? Long+2 : "",
                         Descr, handler);
}

void arghandler::parse()
{
    bool terminated = false;
    for(std::size_t a=0, b=args.size(); a<b; ++a)
    {
        std::string s = args[a];
        if(s.size() > 1 && !terminated && s[0] == '-')
        {
            if(s == "--")
                terminated = true;
            else if(s[1] == '-')
            {
                s.erase(0, 2);

                bool ok = false;
                for(auto& o: options)
                    if(o.Long && s == o.Long)
                    {
                        unsigned c = a+1;
                        if(c == b) --c; // Don't access out of bounds
                        s = args[c];
                        s = o.handler->CallBack(this, s);
                        if(s.empty())
                        {
                            // It consumed the long parameter's option
                            a = c;
                        }
                        else if(s != args[c])
                        {
                            // It partially consumed the option. This is an error
                            argerror(args[a]);
                            return;
                        }
                        else
                        {
                            // It didn't use the long parameter's option.
                        }
                        ok = true;
                        break;
                    }
                if(!ok)
                {
                    argerror(s, false);
                    return;
                }
            }
            else
            {
                s.erase(0, 1);
                while(!s.empty())
                {
                    bool ok = false;
                    for(auto& o: options)
                        if(s.substr(0, std::strlen(o.Short)) == o.Short)
                        {
                            s = o.handler->CallBack(this, s.substr(std::strlen(o.Short)));
                            ok = true;
                            break;
                        }
                    if(!ok)
                    {
                        argerror(s[0]);
                        return;
                    }
                }
            }
        }
        else
            defarg(s);
    }
}

void arghandler::argerror(char c)
{
    Gprintf(GPRINTF_ARGS "%s: illegal option -- %c\n", a0, c);
    suggesthelp();
    exit(1);
}

void arghandler::argerror(const std::string &s, bool param)
{
    Gprintf(GPRINTF_ARGS "%s: %s%s'\n",
        a0,
        param ? "invalid parameter `" : "unrecognized option `--",
        s);
    if(!param) suggesthelp();
    exit(1);
}

void arghandler::suggesthelp()
{
    Gprintf(GPRINTF_ARGS "Try `%s --help' for more information.\n", a0);
}

void arghandler::listoptions()
{
    unsigned longestshort=0, longestlong=0;
    for(auto& o: options)
    {
        unsigned slen = std::strlen(o.Short);
        if(slen > longestshort) longestshort = slen;
        unsigned llen = std::strlen(o.Long);
        if(llen > longestlong) longestlong = llen;
    }
    unsigned space = longestshort + longestlong+1;
    for(auto& o: options)
    {
        const char *s = o.Short;
        const char *l = o.Long;

        SetAttr(*s ? 3 : 0);
        Gprintf(GPRINTF_ARGS "  -");
        SetAttr(DEFAULTATTR);
        Gprintf(GPRINTF_ARGS "%s", s);

        SetAttr(*l ? 3 : 0);
        Gprintf(GPRINTF_ARGS ", --");
        SetAttr(DEFAULTATTR);
        Gprintf(GPRINTF_ARGS "%s", l);

        Gprintf(GPRINTF_ARGS "%*s", space-(std::strlen(s) + std::strlen(l)), "");

        SetAttr(DEFAULTATTR);

        const char *q = o.Descr.c_str();
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
