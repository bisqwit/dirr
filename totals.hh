#ifndef dirr3_totals_hh
#define dirr3_totals_hh

#include <string>

enum {SumDir=1,SumFifo,SumSock,SumFile,SumLink,SumChrDev,SumBlkDev};

extern unsigned long SumCnt[10];
extern unsigned long Summa[10];

extern bool Totals;
extern int TotalSep;
extern int Compact;
extern string LastDir;

extern void Summat();

#endif
