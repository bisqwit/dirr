#ifndef dirr3_argh_hh
#define dirr3_argh_hh

#include <string>
#include <vector>

class arghandler
{
	typedef string (arghandler::*argfun) (const string &);
	
	class option;
	
	vector<option> options;
	vector<string> args;
	
protected:
	string a0;
	
public:
	arghandler(const char *defopts, int argc, const char *const *argv);
	virtual ~arghandler();
	void add(const char *Short, const char *Long, const string &Descr,
	         argfun handler);
	virtual void parse();
	virtual void defarg(const string &s) = 0;
	void argerror(char c);
	void argerror(const string &s, bool param = true);
	void listoptions();
	virtual void suggesthelp();
};

#endif
