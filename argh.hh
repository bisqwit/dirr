#ifndef dirr3_argh_hh
#define dirr3_argh_hh

#ifdef ARGH_VERSION_2
#include "argh2.hh"
#else

#include <string>
#include <vector>

using std::string;
using std::vector;

/* This file is part of Bisqwit's dirr and remotegcc packages. */

#define MAX_ARGC 100000
class arghandler
{
private:
    class MethodPtr
    {
    public:
        virtual string CallBack(arghandler *base, const string &param) = 0;
        virtual ~MethodPtr() {}
    };
    template<class T>
    class MethodPtrImplementation: public MethodPtr
    {
    public:
        typedef string(T::*ptr)(const string &);

        MethodPtrImplementation(ptr mptr): methodPtr(mptr) {}

        virtual string CallBack(arghandler *base, const string &param)
        {
            return (static_cast<T*>(base)->*methodPtr)(param);
        }

     private:
        ptr methodPtr;
    };

    typedef MethodPtr *argfun;
    //typedef string (arghandler::*argfun) (const string &);
    
    class option;
    
    vector<option> options;
    vector<string> args;
    
protected:
    string a0;
    void subadd(const char *Short, const char *Long, const string &Descr, argfun handler);
    
public:
    arghandler(const char *defopts, int argc, const char *const *argv);
    virtual ~arghandler();
    
    template<typename T>
    inline void add(const char *Short, const char *Long, const string &Descr, string(T::*handler)(const string &))
    {
        subadd(Short, Long, Descr, new MethodPtrImplementation<T>(handler));
    }
    
    virtual void parse();
    virtual void defarg(const string &s) = 0;
    void argerror(char c);
    void argerror(const string &s, bool param = true);
    void listoptions();
    virtual void suggesthelp();
};

#endif

#endif
