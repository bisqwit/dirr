#ifndef dirr3_setfun_hh
#define dirr3_setfun_hh

#include <string>

extern bool Dumping;

extern int WasNormalColor;
extern int GetModeColor(const string &text, int Chr);
extern int GetDescrColor(const string &descr, int index);
extern void PrintSettings();

#if CACHE_NAMECOLOR
extern void BuildNameColorCache();
#endif

extern int WasNormalColor;
extern int NameColor(const string &Name);

extern int GetHex(int Default, const char **S);

#endif
