#ifndef dirr3_setfun_hh
#define dirr3_setfun_hh

#include <string>

using std::string;

extern bool Dumping;

extern bool WasNormalColor;
extern int GetModeColor(const string &text, char Chr);
extern int GetDescrColor(const string &descr, int index);
extern void PrintSettings();

extern int NameColor(const string &Name);

#endif
