#ifndef dirr3_totals_hh
#define dirr3_totals_hh

#include <string>

#include "stat.h"

enum {SumDir=1,SumFifo,SumSock,SumFile,SumLink,SumChrDev,SumBlkDev};

extern SizeType SumCnt[10];
extern SizeType Summa[10];

extern bool Totals;
extern int TotalSep;
extern int Compact;
extern string LastDir;

extern void Summat();

#endif
