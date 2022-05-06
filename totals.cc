#include <cstring>

#include "config.h"

#ifdef HAVE_STATFS_SYS_VFS_H
# include <sys/vfs.h>
#elif defined(HAVE_STATFS_SYS_STATFS_H)
# include <sys/statfs.h>
#elif defined(HAVE_STATFS_SYS_MOUNT_H)
# include <sys/mount.h>
#elif defined(HAVE_STATFS_SYS_STATVFS_H)
# include <sys/statvfs.h>
#endif

#include "colouring.hh"
#include "cons.hh"
#include "setfun.hh"
#include "strfun.hh"
#include "totals.hh"

SizeType SumCnt[10] = {0};
SizeType SumSizes[10]  = {0};

string LastDir;

bool Totals;
int TotalSep;
int Compact;

void PrintSums()
{
#ifdef HAVE_STATFS
#if defined(SUNOS)||defined(__sun)||defined(SOLARIS)
#define STATFS(mountpoint, structp) statvfs(mountpoint, structp)
#define STATFST statvfs
#define STATFREEKB(tmp) ((tmp).f_bavail / 1024.0 * (tmp).f_frsize)
#else
#define STATFS(mountpoint, structp) statfs(mountpoint, structp)
#define STATFST statfs
#define STATFREEKB(tmp) ((tmp).f_bavail / 1024.0 * (tmp).f_bsize)
#endif
    struct STATFST tmp;
#endif
    SizeType Koko;

    string NumBuf;

    Dumping = true;
    GetDescrColor(ColorDescr::TEXT, 1);

    if(!Totals)
    {
        if(Colors)Gprintf("\r \r"); /* Ensure color */
        return;
    }

    Koko = /* Grand total */
        SumSizes[SumDir]
      + SumSizes[SumFifo]
      + SumSizes[SumFile]
      + SumSizes[SumLink]
      + SumSizes[SumChrDev]
      + SumSizes[SumBlkDev];

    ColorNums = GetDescrColor(ColorDescr::NUM, -1);

#ifdef HAVE_STATFS
    if(STATFS(LastDir.c_str(), &tmp))tmp.f_bavail = 0;
#endif

    if(Compact)
    {
        SizeType Tmp = SumCnt[SumChrDev] + SumCnt[SumBlkDev];
        SizeType Tmp2= SumCnt[SumFifo]+SumCnt[SumSock]+SumCnt[SumLink];

        PrintNum(NumBuf, TotalSep, SizeFormat, SizeCast SumCnt[SumDir]);
        Gprintf(" \1%s\1 dir%s%s", NumBuf,
#ifdef HAVE_STATFS
            (tmp.f_bavail > 0 && Tmp)?"":
#endif
            "ector",
            SumCnt[SumDir]==1?"y":
#ifdef HAVE_STATFS
            (tmp.f_bavail > 0 && Tmp)?"s":
#endif
            "ies");

        if(SumCnt[SumFile])
        {
            PrintNum(NumBuf, TotalSep, SizeFormat, SizeCast SumCnt[SumFile]);
            Gprintf(", \1%s\1 file%s",
                NumBuf,
                SumCnt[SumFile]==1?"":"s");
           }

        if(Tmp)
        {
            PrintNum(NumBuf, TotalSep, SizeFormat, SizeCast Tmp);
            Gprintf(", \1%s\1 device%s", NumBuf, Tmp==1?"":"s");
        }
        if(Tmp2)
        {
            PrintNum(NumBuf, TotalSep, SizeFormat, SizeCast Tmp2);
            Gprintf(", \1%s\1 other%s", NumBuf, Tmp2==1?"":"s");
        }

        PrintNum(NumBuf, TotalSep, SizeFormat, SizeCast Koko);
        Gprintf(", \1%s\1 bytes", NumBuf);

#ifdef HAVE_STATFS
        if(tmp.f_bavail > 0)
        {
            // Size = kilobytes
            double Size = STATFREEKB(tmp);

            if(Compact == 2)
            {
                PrintNum(NumBuf, TotalSep, "%.0f", Size*1024.0);
                Gprintf(", \1%s\1 bytes", NumBuf);
            }
            else if(Size >= 1024)
            {
                PrintNum(NumBuf, TotalSep, "%.1f", Size/1024.0);
                Gprintf(", \1%s\1 MB", NumBuf);
            }
            else if(Size >= 1048576*10)
            {
                PrintNum(NumBuf, TotalSep, "%.1f", Size/1048576.0);
                Gprintf(", \1%s\1 GB", NumBuf);
            }
            else
            {
                PrintNum(NumBuf, TotalSep, "%.1f", Size);
                Gprintf(", \1%s\1 kB", NumBuf);
            }

            Gprintf(" free(\1%.1f\1%%)",
                (double)tmp.f_bavail * 100.0 / tmp.f_blocks);
        }
#endif
        Gprintf("\n");
    }
    else
    {
        SizeType Tmp = SumCnt[SumChrDev] + SumCnt[SumBlkDev];

        if(Tmp)
        {
            PrintNum(NumBuf, TotalSep, SizeFormat, SizeCast Tmp);
            Gprintf("\1%5s\1 device%s (", NumBuf, (Tmp==1)?"":"s");

            if(SumCnt[SumChrDev])
            {
                PrintNum(NumBuf, TotalSep, SizeFormat, SizeCast SumCnt[SumChrDev]);
                Gprintf("\1%s\1 char", NumBuf);
            }
            if(SumCnt[SumChrDev]
            && SumCnt[SumBlkDev])Gprintf(", ");
            if(SumCnt[SumBlkDev])
            {
                PrintNum(NumBuf, TotalSep, SizeFormat, SizeCast SumCnt[SumBlkDev]);
                Gprintf("\1%s\1 block", NumBuf);
            }
            Gprintf(")\n");
        }

        if(SumCnt[SumDir])
        {
            string TmpBuf;
            PrintNum(NumBuf, TotalSep, SizeFormat, SizeCast SumCnt[SumDir]);
            PrintNum(TmpBuf, TotalSep, SizeFormat, SizeCast SumSizes[SumDir]);
            Gprintf("\1%5s\1 directories,\1%11s\1 bytes\n",
                NumBuf, TmpBuf);
        }

        if(SumCnt[SumFifo])
        {
            string TmpBuf;
            PrintNum(NumBuf, TotalSep, SizeFormat, SizeCast SumCnt[SumFifo]);
            PrintNum(TmpBuf, TotalSep, SizeFormat, SizeCast SumSizes[SumFifo]);
            Gprintf("\1%5s\1 fifo%s\1%17s\1 bytes\n",
                NumBuf, (SumCnt[SumFifo]==1)?", ":"s,", TmpBuf);
        }
        if(SumCnt[SumFile])
        {
            string TmpBuf;
            PrintNum(NumBuf, TotalSep, SizeFormat, SizeCast SumCnt[SumFile]);
            PrintNum(TmpBuf, TotalSep, SizeFormat, SizeCast SumSizes[SumFile]);
            Gprintf("\1%5s\1 file%s\1%17s\1 bytes\n",
                NumBuf, (SumCnt[SumFile]==1)?", ":"s,", TmpBuf);
        }
        if(SumCnt[SumLink])
        {
            string TmpBuf;
            PrintNum(NumBuf, TotalSep, SizeFormat, SizeCast SumCnt[SumLink]);
            PrintNum(TmpBuf, TotalSep, SizeFormat, SizeCast SumSizes[SumLink]);
            Gprintf("\1%5s\1 link%s\1%17s\1 bytes\n",
                NumBuf, (SumCnt[SumLink]==1)?", ":"s,", TmpBuf);
        }
        PrintNum(NumBuf, TotalSep, SizeFormat, SizeCast Koko);
        Gprintf("Total\1%24s\1 bytes\n", NumBuf);
#ifdef HAVE_STATFS
        if(tmp.f_bavail > 0)
        {
            double Size = STATFREEKB(tmp) * 1024.0;

            /* FIXME: Thousand separators for free space also      *
             *        Currently not implemented, because there     *
             *        may be more free space than 'unsigned long'  *
             *        is able to express.                          */

            PrintNum(NumBuf, TotalSep, "%.0f", Size);

            Gprintf("Free space\1%19s\1 bytes (\1%.1f\1%%)\n",
                NumBuf,
                (double)((double)tmp.f_bavail * 100.0 / tmp.f_blocks));
        }
#endif
    }

    ColorNums = -1;

    for(auto& i: SumCnt) i=0;
    for(auto& i: SumSizes) i=0;
}
