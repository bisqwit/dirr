#ifndef dirr3_cons_hh
#define dirr3_cons_hh

#ifndef __GNUC__
 #define __attribute__
#endif

extern bool Colors;
extern bool AnsiOpt;
extern bool Pagebreaks;

extern int WhereX;
extern int LINES, COLS;

extern int Gputch(int x);
extern void SetAttr(int newattr);
extern int ColorNums;
extern int Gprintf(const char *fmt, ...) __attribute__((format(printf,1,2)));
extern void GetScreenGeometry();

#endif
