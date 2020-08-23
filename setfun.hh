#ifndef dirr3_setfun_hh
#define dirr3_setfun_hh

#include <string>

#define ColorDescrs(o) o(TEXT,"txt") o(OWNER,"owner") o(GROUP,"group") o(NRLINK,"nrlink") \
                       o(DATE,"date") o(NUM,"num") o(DESCR,"descr") o(SIZE,"size")
#define ColorModes(o)  o(TYPE,"type") o(MODE,"mode") o(INFO,"info")

#define o(val,str) val,
enum class ColorDescr { ColorDescrs(o) };
enum class ColorMode  { ColorModes(o) };
#undef o

using std::string;

extern bool Dumping;

extern bool WasNormalColor;
extern int GetModeColor(ColorMode m, signed char Chr);
extern int GetDescrColor(ColorDescr d, int index);
extern void PrintSettings();

extern int NameColor(const string &Name, int default_color);

#endif
