/*

	Unix directorylister (replacement for ls command)
	Copyright (C) 1992,2000 Bisqwit (http://iki.fi/bisqwit/)
	
*/

#define VERSIONSTR \
    "DIRR "VERSION" copyright (C) 1992,2000 Bisqwit (http://iki.fi/bisqwit/)\n" \
    "This program is under GPL. dirr-"VERSION".{rar,zip,tar.{gz,bz2}}\n" \
    "are available at the homepage of the author.\n" \
    "About some ideas about this program, thanks to Warp.\n"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <cerrno>
#include <csignal>

#include "config.h"
#include "pwfun.hh"
#include "wildmatch.hh"
#include "setfun.hh"
#include "strfun.hh"
#include "cons.hh"
#include "colouring.hh"
#include "getname.hh"
#include "getsize.hh"
#include "totals.hh"
#include "argh.hh"

#include <algorithm>
#include <vector>
#include <string>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <unistd.h>
#ifdef HAVE_DIR_H
#include <dir.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

using namespace std;

static int RowLen;

static int LongestName;
static int LongestSize;
static int LongestUID;
static int LongestGID;

static bool Contents, PreScan, Sara;
static int DateTime;

static string Sorting; /* n,d,s,u,g */
static string DateForm;
static string FieldOrder;

static void EstimateFields();

static void SetDefaultOptions()
{
	PreScan = true; // Clear with -e
	Sara    = false;// Set with -C
	Compact = 0;    // Set with -m
	#ifdef S_ISLNK
	Links   = 3;    // Modify with -l#
	#endif
	Colors  = true; // Clear with -c
	Contents= true; // Clear with -D
	DateTime= 2;    // Modify with -d#
	Totals  = true; // Modify with -m
	Pagebreaks = false; // Set with -p
	AnsiOpt = true; // Clear with -P
	
	TotalSep= 0;    // Modify with -Mx
	
	Sorting = "pmgU";
	DateForm = "%d.%m.%y %H:%M";
				    // Modify with -F
				 
				    // Modify with -f
	#ifdef DJGPP
	FieldOrder = ".f.s_.d";
	#else
	FieldOrder = ".f.s_.a4_.d_.o_.g";
	#endif
	
	BlkStr = "<B%u,%u>";	// Modify with -db
	ChrStr = "<C%u,%u>";	// Modify with -dc
}

#include <map>
static class Inodemap
{
	typedef map<ino_t, string> inomap;
	typedef map<dev_t, inomap> devmap;
	devmap items;
	bool enabled;
public:
	Inodemap() : enabled(true) { }
	void disable()
	{
		enabled = false;
		items.clear();
	}
	void enable()
	{
		enabled = true;	
	}
	void insert(dev_t dev, ino_t ino, const string &name)
	{
		if(enabled && !has(dev, ino))
			items[dev][ino] = name;
	}
	bool has(dev_t dev, ino_t ino) const
	{
		devmap::const_iterator i = items.find(dev);
		if(i != items.end())
		{
			inomap::const_iterator j = i->second.find(ino);
			if(j != i->second.end())return true;
		}
		return false;
	}
	const char *get(dev_t dev, ino_t ino) const
	{
		devmap::const_iterator i = items.find(dev);
		if(i != items.end())
		{
			inomap::const_iterator j = i->second.find(ino);
			if(j != i->second.end())
				return j->second.c_str();
		}
		return NULL;
	}
} Inodemap;

static void TellMe(const struct stat &Stat, const string &Name
#ifdef DJGPP
	, unsigned int dosattr
#endif
	)
{
    int Len;
    char OwNam[16];
    char GrNam[16];
    const char *Passwd, *Group;
    int ItemLen, NameAttr;
    int NeedSpace=0;
    const char *s;

    if(S_ISDIR(Stat.st_mode))
    {
        SumCnt[SumDir]++;
        Summa[SumDir] += Stat.st_size;
    }
    #ifdef S_ISFIFO
    else if(S_ISFIFO(Stat.st_mode))
    {
        SumCnt[SumFifo]++;
        Summa[SumFifo] += Stat.st_size;
    }
    #endif
    #ifdef S_ISSOCK
    else if(S_ISSOCK(Stat.st_mode))
    {
        SumCnt[SumSock]++;
        Summa[SumSock] += Stat.st_size;
    }
    #endif
    else if(S_ISCHR(Stat.st_mode))
    {
        SumCnt[SumChrDev]++;
        Summa[SumChrDev] += 0; //Stat.st_size;
    }
    else if(S_ISBLK(Stat.st_mode))
    {
        SumCnt[SumBlkDev]++;
        Summa[SumBlkDev] += 0; //Stat.st_size;
    }
    #ifdef S_ISLNK
    else if(S_ISLNK(Stat.st_mode))
    {
        SumCnt[SumLink]++;
        Summa[SumLink] += Stat.st_size;
    }
    #endif
    else
    {
        SumCnt[SumFile]++;
        Summa[SumFile] += Stat.st_size;
    }

    NameAttr = GetNameAttr(Stat, NameOnly(Name));

    Passwd = Getpwuid((int)Stat.st_uid);
    Group  = Getgrgid((int)Stat.st_gid);
    if(Passwd && *Passwd)strcpy(OwNam, Passwd);else sprintf(OwNam,"%d",(int)Stat.st_uid);
    if( Group &&  *Group)strcpy(GrNam,  Group);else sprintf(GrNam,"%d",(int)Stat.st_gid);

   	Len = strlen(OwNam); if(Len > LongestUID)LongestUID = Len;
   	Len = strlen(GrNam); if(Len > LongestGID)LongestGID = Len;

    for(ItemLen=0, s=FieldOrder.c_str(); *s; )
    {
    	NeedSpace=0;
        switch(*s)
        {
            case '.':
            	NeedSpace=1;
                switch(*++s)
                {
                    case 'a':
                    {
                        s++;
                        Len = '1';
                        if((*s>='0')&&(*s<='9'))Len = *s;
                        int i = PrintAttr(Stat, Len
						#ifdef DJGPP
                        	, dosattr
						#endif
                    	);
                        ItemLen += i;
                        RowLen += i;
                        break;
                    }
                    case 'x':
                        s++;
                        SetAttr(GetHex(7, (const char **)&s));
                        s--;
                        NeedSpace=0;
                        break;
                    case 'o':
                    case 'O':
                    {
                        int i;
                        GetDescrColor("owner", (Stat.st_uid==getuid())?1:2);
                        if(isupper((int)*s) || !s[1])
                            i = Gprintf("%s", OwNam);
                        else
                            i = Gprintf("%*s", -LongestUID, OwNam);
                        ItemLen += i;
                        RowLen += i;
                        break;
                    }
#ifdef DJGPP
					case 'G':
                    case 'g':
                    case 'h':
                        break;
#else
					case 'G':
                    case 'g':
                    {
                        int i;
                        GetDescrColor("group", (Stat.st_gid==getgid())?1:2);
                        if(isupper((int)*s) || !s[1])
                            i = Gprintf("%s", GrNam);
                        else
                            i = Gprintf("%*s", -LongestGID, GrNam);
                        ItemLen += i;
                        RowLen += i;
                        break;
                    }
                    case 'h':
                        GetDescrColor("nrlink", 1);
                        Gprintf("%4d", (int)Stat.st_nlink);
                        ItemLen += 4;
                        RowLen += 4;
                        break;
#endif
                    case 'F':
                    case 'f':
                    {
                        SetAttr(NameAttr);
                        
                        const char *hardlinkfn = Inodemap.get(Stat.st_dev, Stat.st_ino);
                        
                        if(hardlinkfn && Name == hardlinkfn)
                        	hardlinkfn = NULL;
                        
                        GetName(Name, Stat, LongestName,
                                (Sara||s[1]) && (*s=='f'),
                                (*s=='f'),
                                hardlinkfn);
                        
                        ItemLen += LongestName;
                        RowLen += LongestName;
                        break;
                    }
                    case 's':
                        Gprintf("%s", GetSize(Name, Stat, LongestSize, 0).c_str());
                        ItemLen += LongestSize;
                        RowLen += LongestSize;
                        break;
                    case 'S':
                    {
                        int i;
                        s++; /* 's' on sepittömälle versiolle */
                        i = Gprintf("%s", GetSize(Name, Stat, LongestSize+3, *s?*s:' ').c_str());
                        ItemLen += i;
                        RowLen += i;
                        break;
                    }
                    case 'd':
                    {
                        char Buf[64]; /* For the date */
                    	char *s;
                        int i;

                        time_t t;

                        switch(DateTime)
                        {
                            case 1: t = Stat.st_atime; break;
                            case 2: t = Stat.st_mtime; break;
                            case 3: t = Stat.st_ctime;
                        }

                        if(DateForm == "%u")
                        {
                            time_t now = time(NULL);
                            strcpy(Buf, ctime(&t));
                            if(now > t + 6L * 30L * 24L * 3600L /* Old. */
                            || now < t - 3600L)       /* In the future. */
                            {
                                /* 6 months in past, one hour in future */
                                strcpy(Buf+11, Buf+19);
                            }
                            Buf[16] = 0;
                            strcpy(Buf, Buf+4);
                        }
                        else if(DateForm == "%z")
                        {
                        	struct tm *TM = localtime(&t);
                            time_t now = time(NULL);
                            int m = TM->tm_mon, d=TM->tm_mday, y=TM->tm_year;
                            struct tm *NOW = localtime(&now);
                        	if(NOW->tm_year == y || (y==NOW->tm_year-1 && m>NOW->tm_mon))
                        	{
                        		sprintf(Buf, "%3d.%d", d,m+1);
                        		if(Buf[5])strcpy(Buf, Buf+1);
                        	}
                        	else
                        		sprintf(Buf, "%5d", y+1900);
                        }
                        else
                            strftime(Buf, sizeof Buf, DateForm.c_str(), localtime(&t));

                        while((s=strchr(Buf,'_'))!=NULL)*s=' ';
                        GetDescrColor("date", 1);
                        i = Gprintf("%s", Buf);
                        ItemLen += i;
                        RowLen += i;
                        break;
                    }
                    default:
                        Gputch(*s);
                        ItemLen++;
                        RowLen++;
                }
                break;
            case '_':
                Gputch(' ');
                ItemLen++;
                RowLen++;
                break;
            default:
                Gputch(*s);
                ItemLen++;
                RowLen++;
        }
        if(*s)s++;
    }
    
    if(FieldOrder.size())
    {
        if(!Sara)goto P1;
        if(RowLen + ItemLen >= COLS)
        {
P1:         Gprintf("\n");
            RowLen = 0;
        }
        else
        {
			if(NeedSpace)Gputch(' ');
            RowLen++;
		}
	}
}

class StatItem
{
public:
	struct stat Stat;
    #ifdef DJGPP
    unsigned dosattr;
    #endif
    string Name;
public:
	StatItem(const struct stat &t,
	#ifdef DJGPP
	         unsigned da,
	#endif
	         const string &n) : Stat(t),
	#ifdef DJGPP
	                            dosattr(da),
	#endif
	                            Name(n) { }
    
    /* Returns the class code for grouping sort */
    int Class(int LinksAreFiles) const
    {
    	if(S_ISDIR(Stat.st_mode))return 0;
        #ifdef S_ISLNK
    	if(S_ISLNK(Stat.st_mode))return 2-LinksAreFiles;
        #else
        LinksAreFiles = LinksAreFiles; /* Not used */
        #endif
    	if(S_ISCHR(Stat.st_mode))return 3;
    	if(S_ISBLK(Stat.st_mode))return 4;
    	#ifdef S_ISFIFO
    	if(S_ISFIFO(Stat.st_mode))return 5;
    	#endif
        #ifdef S_ISSOCK
    	if(S_ISSOCK(Stat.st_mode))return 6;
    	#endif
    	return 1;
    }
    
	bool operator> (const StatItem &toka) const { return compare(toka, true); }
	bool operator< (const StatItem &toka) const { return compare(toka, false); }
	
	bool compare (const StatItem &toka, bool suur) const
    {
    	register const class StatItem &eka = *this;
    	
        for(const char *s=Sorting.c_str(); *s; s++)
        {
        	int Result=0;
        	
        	switch(tolower(*s))
    	    {
    	    	case 'c':
    	    		Result = GetNameAttr(eka.Stat, eka.Name.c_str()) 
    	    			   - GetNameAttr(toka.Stat, toka.Name.c_str());
    	    		break;
    	   	 	case 'n':
       		 		Result = strcmp(eka.Name.c_str(), toka.Name.c_str());
       				break;
    	   	 	case 'm':
       		 		Result = strcasecmp(eka.Name.c_str(), toka.Name.c_str());
       				break;
    	   	 	case 's':
        			Result = eka.Stat.st_size - toka.Stat.st_size;
        			break;
    	    	case 'd':
        			switch(DateTime)
        			{
        				case 1: Result = eka.Stat.st_atime-toka.Stat.st_atime; break;
    	    			case 2: Result = eka.Stat.st_mtime-toka.Stat.st_mtime; break;
        				case 3: Result = eka.Stat.st_ctime-toka.Stat.st_ctime;
        			}
        			break;
    	    	case 'u':
        			Result = (int)eka.Stat.st_uid - (int)toka.Stat.st_uid;
        			break;
    	    	case 'g':
        			Result = (int)eka.Stat.st_gid - (int)toka.Stat.st_gid;
        			break;
    	    	case 'h':
        			Result = (int)eka.Stat.st_nlink - (int)toka.Stat.st_nlink;
        			break;
    		   	case 'r':
        			Result = eka.Class(0) - toka.Class(0);
        			break;
    		   	case 'p':
        			Result = eka.Class(1) - toka.Class(1);
        			break;
        		default:
        		{
        			const char *t = Sorting.c_str();
    		        SetAttr(GetDescrColor("txt", 1));
    			    Gprintf("\nError: `-o");
    			    while(t < s)Gputch(*t++);
    			    GetModeColor("info", '?');
    			    Gputch(*s++);
    		        SetAttr(GetDescrColor("txt", 1));
    		        Gprintf("%s'\n\n", s);
    		        return (Sorting = ""), 0;
    		    }
    		}
    		if(Result)
    			return isupper((int)*s)^suur ? Result>0 : Result<0;
    	}
        
        return false;
    }
};

static vector <StatItem> StatPuu;

static void Puuhun(const struct stat &Stat, const string &Name
#ifdef DJGPP
	, unsigned int attr
#endif
	)
{
	if(PreScan)
	{
		StatItem t(Stat,
        #ifdef DJGPP
                   attr,
        #endif
                   Name);
        StatPuu.push_back(t);
        
		if(Colors)
		{
			#if STARTUP_COUNTER
			if(AnsiOpt)
			{
				int b, c, d;
				
				for(b=d=1; (c=StatPuuKoko/b) != 0; b*=10, d++)if(c%10)break;
				
				for(c=0; c<d; c++)Gputch('\b');
				
				while(c)
				{
					Gprintf("%d", (StatPuuKoko/b)%10);
					b /= 10;
					c--;
				}
				
				fflush(stdout);
			}
			else
				Gprintf("%u\r", StatPuu.size());
			#endif
		}
	}
	else
	{
		Dumping = true;
		TellMe(Stat, Name
        #ifdef DJGPP
        , attr
        #endif
		);
		Dumping = false;
	}
}

static void SortFiles()
{
	sort(StatPuu.begin(), StatPuu.end());
}

static void DropOut()
{
    EstimateFields();
    
    if(Sorting.size())SortFiles();
    
    Dumping = true;
    
    unsigned a, b = StatPuu.size();
    
    if(Colors && b && AnsiOpt)Gprintf("\r");
    
	for(a=0; a<b; ++a)
    {
        StatItem &tmp = StatPuu[a];
        TellMe(tmp.Stat, tmp.Name.c_str()
        #ifdef DJGPP
        	, tmp.dosattr
        #endif
        	);
    }
    
    StatPuu.clear();
    
    LongestName = 0;
    LongestSize = 0;
	
	Dumping = false;
}

/* This function is supposed to stat() to Stat! */
static void SingleFile(const string &Buffer, struct stat &Stat)
{
    #ifndef S_ISLNK
    int Links=0; /* hack */
    #endif

    #ifdef DJGPP
    struct ffblk Bla;
    if(findfirst(Buffer.c_str(), &Bla, 0x37))
    {
        if(Buffer[0]=='.')return;
        Gprintf("%s - findfirst() error: %d (%s)\n", Buffer.c_str(), errno, GetError(errno).c_str());
    }
    #endif

    if((lstat(Buffer.c_str(), &Stat) == -1) && !Links)
        Gprintf("%s - stat() error: %d (%s)\n", Buffer.c_str(), errno, GetError(errno).c_str());
    #ifdef S_ISLNK
    else if((lstat(Buffer.c_str(), &Stat) == -1) && Links)
        Gprintf("%s - lstat() error: %d (%s)\n", Buffer.c_str(), errno, GetError(errno).c_str());
    #endif
    else
    {
	    char OwNam[16];
    	char GrNam[16];
	    const char *Group, *Passwd;
    	
        string s = GetSize(Buffer, Stat, 0, 0);
        
        const char *hardlinkfn = Inodemap.get(Stat.st_dev, Stat.st_ino);
        if(hardlinkfn && Buffer == hardlinkfn)hardlinkfn = NULL;
        
        int i = GetName(Buffer, Stat, 0, false, true, hardlinkfn);
        
        if(i > LongestName)LongestName = i;
        i = s.size();
        if(i > LongestSize)LongestSize = i;
        
	    if(!S_ISDIR(Stat.st_mode))
		    Inodemap.insert(Stat.st_dev, Stat.st_ino, Buffer);
		
        Passwd = Getpwuid((int)Stat.st_uid);
		Group  = Getgrgid((int)Stat.st_gid);
	    if(Passwd && *Passwd)strcpy(OwNam, Passwd);else sprintf(OwNam,"%d",(int)Stat.st_uid);
    	if( Group &&  *Group)strcpy(GrNam,  Group);else sprintf(GrNam,"%d",(int)Stat.st_gid);
    	
    	i = strlen(OwNam); if(i > LongestUID)LongestUID = i;
    	i = strlen(GrNam); if(i > LongestGID)LongestGID = i;
        
        Puuhun(Stat, Buffer
        #ifdef DJGPP
        , Bla.ff_attrib
        #endif
        );
    }
}

static void DirChangeCheck(string Source)
{
	register unsigned ss = Source.size();
	if(ss)
	{
		if(Source[ss-1] == '/')
			Source.erase(--ss);
		else if(ss>=2 && Source[0]=='.' && Source[1]=='/')
			Source.erase(0, 2);
	}
    if(LastDir != Source)
    {
    	DropOut();
        if(Totals)
        {
        	static int Eka=1;
	        GetDescrColor("txt", 1);
        	if(WhereX)Gprintf("\n");
        	if(!Eka)
        	{
        		if(RowLen > 0)Gprintf("\n");
        		Summat();
        		Gprintf("\n");
        	}
        	Eka=0;
			Gprintf(" Directory of %s\n", Source.size()?Source.c_str():".");
		}
		LastDir = Source;
    }
}

static void ScanDir(const char *Dirrikka)
{
	string Source;
	string DirName;
    int Tried = 0;

    struct stat Stat;
    struct dirent *ent;
    DIR *dir;
    
    DirName = Dirrikka;

    #ifdef DJGPP
    if(DirName.size() && DirName[DirName.size()-1] == ':')
    	DirName += '.';
    #endif
    
    Source = DirName;

R1: if((dir = opendir(Source.c_str())) == NULL)
    {
    	/* It was not a directory, or could not be read */
    	
    	if(
	    #ifdef DJGPP
    		errno==EACCES  ||
        #endif
        	errno==ENOENT  ||	/* Piti lisätä linkkejä varten */
        	errno==ENOTDIR ||
        	errno==ELOOP        /* Tämä myös, ln -s a a varten */
       	)
        {
P1:			string Tmp = DirOnly(Source);
			if(!Tmp.size())Tmp = "./";
			
	    	DirChangeCheck(Tmp.c_str());
			
			SingleFile(Source, Stat);
			goto End_ScanDir;
        }
        else if(!Tried && (!Source.size() || Source[Source.size()-1]!='/'))
        {
        	Source += '/';
            Tried = 1;
            goto R1;
        }
        else if(errno==EACCES)
        {
            Gprintf(" No access to %s\n", Source.c_str());
            goto End_ScanDir;
        }

        if(errno)
            Gprintf("\n%s - error: %d (%s)\n", Source.c_str(),
                    errno, GetError(errno).c_str());

        goto End_ScanDir;
    }    
    if(!Contents)
    {
        errno = 0;
        Tried = 1;
        goto P1;
    }
    
    DirChangeCheck(Source);
    
    while((ent = readdir(dir)) != NULL)
    {
    	string Buffer = Source;
        if(Buffer[Buffer.size()-1] != '/')Buffer += '/';
        Buffer += ent->d_name;

       	SingleFile(Buffer, Stat);
    }

    if(closedir(dir) != 0)
        Gprintf("Trouble #%d (%s) at closedir() call.\n",
            errno, GetError(errno).c_str());

End_ScanDir:
    return;
}

static void EstimateFields()
{
    int RowLen;
    
    if(PreScan)
    	LongestName++;
    else
    {
    	LongestSize = Sara?7:9;
    	LongestGID  = 8;
    	LongestUID  = 8;
   	}
   	const char *s;
    for(RowLen=0, s=FieldOrder.c_str(); *s; )
    {
        switch(*s)
        {
            case '.':
                s++;
                switch(*s)
                {
                    case 'x':
                        s++;
                        GetHex(7, (const char **)&s);
                        s--;
                        break;
                    case 'F':
                    case 'f':
                        break;
                    case 'S':
                        s++;
                        RowLen += 3;
                    case 's':
                        RowLen += LongestSize;
                        break;
                    case 'd':
                        if(DateForm == "%u")
                            RowLen += 12;
                        else if(DateForm == "%z")
                            RowLen += 5;
                        else
                        {
                            char Buf[64];
                            struct tm TM;
                            memset(&TM, 0, sizeof(TM));
                            strftime(Buf, sizeof Buf, DateForm.c_str(), &TM);
                            RowLen += strlen(Buf);
                        }
                        break;
#ifndef DJGPP
                    case 'h':
                        RowLen += 4;
                        break;
                    case 'g':
                    	RowLen += LongestGID;
                    	break;
#endif
                    case 'o':
                        RowLen += LongestUID;
                        break;
                    case 'a':
                    {
                        int Len = '1';
                        s++;
                        if((*s>='0')&&(*s<='9'))Len = *s;
                        RowLen += (Len < '2')?10:Len-'0';
                        break;
                    }
                    default:
                        RowLen++;
                }
                break;
            default:
                RowLen++;
        }
        if(*s)s++;
    }
    
    RowLen = (Sara?COLS/2:COLS)-1-RowLen;
    if(RowLen < 0)RowLen = 0;
    
    if(!PreScan || LongestName > RowLen)
    	LongestName = RowLen;
}

static vector<string> Dirpuu;

static void DumpDirs()
{
    EstimateFields();
    
    unsigned a, b=Dirpuu.size();
    
    for(a=0; a<b; ++a)
        ScanDir(Dirpuu[a].c_str());
    
    Dirpuu.clear();
    
    DropOut();
}

class Handle : public arghandler
{
	bool Files;
	bool Help;
private:
    string opt_h(const string &s)
    {
    	Help = true;
    	return s;
    }
    string opt_r(const string &s)
    {
    	SetDefaultOptions();
        Inodemap.enable();
        return s;
    }
    string opt_d1(const string &s) { DateTime = 1; return s; }
    string opt_d2(const string &s) { DateTime = 2; return s; }
    string opt_d3(const string &s) { DateTime = 3; return s; }
    string opt_db(const string &s) { BlkStr = s; return ""; }
    string opt_dc(const string &s) { ChrStr = s; return ""; }
#ifdef S_ISLNK
    string opt_l(const string &s)
    {
    	const char *q = s.c_str();
    	const char *p = q;
    	Links = strtol(p, (char **)&p, 10);
    	if(Links < 0 || Links > 5)argerror(s);
    	return s.substr(p-q);
    }
#endif
    string opt_X(const string &s)
    {
    	const char *q = s.c_str();
    	const char *p = q;
    	if(*p == '0')printf("Window size = %dx%d\n", COLS, LINES);
    	COLS = strtol(p, (char **)&p, 10);
    	return s.substr(p-q);
    }
    string opt_H1(const string &s)
    {
    	Inodemap.enable();
    	return s;
    }
    string opt_H(const string &s)
    {
    	Inodemap.disable();
    	if(s.size() && s[0]=='0')return s.substr(1);
    	return s;
    }
    string opt_c1(const string &s) { Colors = true;    return s; }
    string opt_c(const string &s) { Colors = false;    return s; }
    string opt_C(const string &s) { Sara = true;       return s; }
    string opt_D(const string &s) { Contents = false;  return s; }
    string opt_p(const string &s) { Pagebreaks = true; return s; }
    string opt_P(const string &s) { AnsiOpt = false;   return s; }
    string opt_e(const string &s) { PreScan = false; Sorting = ""; return s; }
    string opt_o(const string &s) { Sorting = s; return ""; }
    string opt_F(const string &s) { DateForm = s; return ""; }
    string opt_f(const string &s) { FieldOrder = s; return ""; }
    string opt_V(const string &)
    {
		printf(VERSIONSTR);
		exit(0);
    }
    string opt_a(const string &s)
    {
    	const char *q = s.c_str();
    	const char *p = q;
    	switch(strtol(p, (char **)&p, 10))
    	{
    		case 0:
				FieldOrder = ".f_.s.d|";
		        DateForm = "%z";
#ifdef S_ISLNK
		        Links = 1;
#endif
		        Sara = true;
		        Compact = 1;
		        break;
		    case 1:
				FieldOrder = ".xF|.a1.xF|.f.s.xF|.d.xF|.o_.g.xF|";
				break;
			case 2:
#ifdef S_ISLNK
				Links = 0;
#endif
				Sara = true;
				FieldOrder = ".f_.a4_.o_.g.xF|";
				break;
			case 3:
#ifdef S_ISLNK
				Links = 0;
#endif
				Sara = true;
        		FieldOrder = ".f_.s_.o.xF|";
        		break;
        	case 4:
#ifdef S_ISLNK
				Links = 1;
#endif
				Sara = true;
				FieldOrder = ".f_.s_.d_.o.xF|";
				DateForm = "%z";
				break;
    		default:
    			argerror(s);
		}
    	return s.substr(p-q);
    }
    string opt_al(const string &s)
    {
		FieldOrder = ".a1_.h__.o_.g_.s_.d_.f";
        DateForm = "%u";
        Inodemap.enable();
        return s;
    }
    string opt_m(const string &s)
    {
    	const char *q = s.c_str();
    	const char *p = q;
    	switch(strtol(p, (char **)&p, 10))
    	{
    		case 0: Compact=0; Totals=true; break;
    		case 1: Compact=1; Totals=true; break;
    		case 2: Totals=false; break;
    		case 3: Compact=2; Totals=true; break;
    		default:
    			argerror(s);
    	}
    	return s.substr(p-q);
    }
    string opt_M(const string &s)
    {
    	string q = opt_m(s);
    	if(!q.size())argerror(s);
    	TotalSep = q[0];
    	return q.substr(1);
    }
    string opt_w(const string &s)
    {
    	Compact = 1;
    	Totals = true;
    	FieldOrder = ".f";
    	Sorting = "pcm";
        Inodemap.disable();
#ifdef S_ISLNK
        Links = 1;
#endif
        Sara = true;
    	return s;
    }
    string opt_W(const string &s)
    {
    	Compact = 1;
    	Totals = true;
    	FieldOrder = ".f";
    	Sorting = "PCM";
        Inodemap.disable();
#ifdef S_ISLNK
        Links = 1;
#endif
        Sara = true;
    	return s;
    }
public:
	Handle(const char *defopts, int argc, const char *const *argv)
	: arghandler(defopts, argc, argv), Files(false), Help(false)
	{
        add("-al", "--long",      "\"Standard\" listing format", &Handle::opt_al);
        add("-a",  "--predef",    "-a0 to -a4: Some predefined formats. "
                                  "You may want to play with them to find out "
                                  "how extensive this program is :)",
                                  &Handle::opt_a);
        add("-c1", "--colours",   "Enables colours (default, if tty output).", &Handle::opt_c1);
        add("-c",  "--nocolor",   "Disables colours.", &Handle::opt_c);
        add("-C",  "--columns",   "Enables multiple column mode.", &Handle::opt_C);
		add("-d1", "--useatime",  "Use atime, last access datetime for date fields.", &Handle::opt_d1);
		add("-d2", "--usemtime",  "Use mtime, last modification datetime.", &Handle::opt_d2);
		add("-d3", "--usectime",  "Use ctime, file creation datetime.", &Handle::opt_d3);
		add("-db", "--blkdev",    "Specify how the blockdevices are shown\n"
                                    " Example: `--blkdev=<B%u,%u>'\n"
                                    " Default is `-db" + BlkStr + "'",
                                    &Handle::opt_db);
		add("-dc", "--chrdev",    "Specify how the character devices are shown\n"
                                    " Example: `--chrdev=<C%u,%u>'\n"
                                    " Default is `-dc" + ChrStr + "'",
                                    &Handle::opt_dc);
        add("-D",  "--notinside", "Show directory names instead of contents.", &Handle::opt_D);
        add("-e",  "--noprescan", "Undocumented evil option.", &Handle::opt_e);
        add("-f",  "--format",    "Output format\n"
                                  "  .s=Size,    .f=File,   .d=Datetime,     .o=Owner,   .g=Group,\n"
                                  "  .S#=size with thsep #, .x##=Color 0x##, .h=Number of hard links\n"
#ifdef DJGPP
                                  "  .a0=SHRA style attributes      "
#else
                                  "  .a1=drwxrwxrwx style attributes"
#endif
                                                                   "         .a#=Mode as #-decimal octal\n"
                                  "  .F and .G and .O are respectively, without space fitting\n"
                                  "   anything else=printed\n"
                                  "   Default is `--format="+FieldOrder+"'",
                                  &Handle::opt_f);
        add("-F",  "--dates",     "Specify new date format. man strftime.\n"
                                  "Default is `-F"+DateForm+"'",
                                  &Handle::opt_F);
		add("-h",  "--help",      "mm.. familiar?", &Handle::opt_h);
        add("-H1", "--hl",        "Enables mapping hardlinks (default)", &Handle::opt_H1);
        add("-H",  "--nohl",      "Disables mapping hardlinks", &Handle::opt_H);
		add("-?",  NULL,          "Alias to -h", &Handle::opt_h);
        add("-la", NULL,          "Alias to -al", &Handle::opt_al);
#ifdef S_ISLNK
        add("-l",  "--links",     "Specify how the links are shown:\n"
                                    "  0 Show link name and stats of link\n"
                                    "  1 Show link name and <LINK>\n"
                                    "  2 Show link name, link's target and stats of link\n"
                                    "  3 Show link name, link's target and stats of target\n"
                                    "  4 Show link name, link's target and <LINK>\n"
                                    "  5 Show link name and stats of target\n",
                                    &Handle::opt_l);
#endif
		add("-m", "--tstyle",     "Selects \"total\" list style.\n"
		                          " -m0: Verbose (default)\n"
		                          " -m1: Compact.\n"
		                          " -m2: None.\n"
		                          " -m3: Compact with exact numbers.", &Handle::opt_m);
        add("-M", "--tstylsep",   "Like -m, but with a thousand separator. Example: -M0,", &Handle::opt_M);
        add("-o", "--sort",       "Sort the list (disables -e), with n as combination of:\n"
                                  "(n)ame, (s)ize, (d)ate, (u)id, (g)id, (h)linkcount, "
                                  "(c)olor, na(m)e case insensitively, "
                                  "g(r)oup dirs,files,links,chrdevs,blkdevs,fifos,socks, "
                                  "grou(p) dirs,links=files,chrdevs,blkdevs,fifos,socks\n"
                                  "Use Uppercase for reverse order.\n"
                                  "Default is `--sort="+Sorting+"'\n", &Handle::opt_o);
        add("-p",  "--paged",     "Use internal pager.", &Handle::opt_p);
        add("-P",  "--oldvt",     "Disables colour code optimizations.", &Handle::opt_P);
		add("-r",  "--restore",   "Undoes all options, including the DIRR environment variable.", &Handle::opt_r);
        add("-v",  "--version",   "Displays the version.", &Handle::opt_V);
        add("-V",  NULL,          "Alias to -v.", &Handle::opt_V);
        add("-w",  "--wide",      "Equal to -l1HCm1f.f -opcm", &Handle::opt_w);
        add("-W",  "--ediw",      "Same as -w, but with reverse sort order.", &Handle::opt_W);
        add("-X",  "--width",     "Force screen width, example: -X132",
                                  &Handle::opt_X);
        
        parse();
	}
	virtual void defarg(const string &s)
	{
		Dirpuu.push_back(s);
	    Files = true;
	}
    virtual void parse()
    {
    	arghandler::parse();
		if(!Files)Dirpuu.push_back(".");
		if(Help)
		{
           	Dumping = true;

            GetDescrColor("txt", 1);
            
			Gprintf(VERSIONSTR);
			
            Gprintf(
#ifndef DJGPP
            	"\r\33[K\r"
#endif
                "Usage: %s [options] {dirs | files }\n", a0.c_str());
            
            SetAttr(15);
            Gprintf("\nOptions:\n");
            
            listoptions();
            
            GetDescrColor("txt", 1);
            
            Gprintf(
            	"\n"
                "You can set environment variable 'DIRR' for the options.\n"
                "You can also use 'DIRR_COLORS' -variable for color settings.\n"
                "Current DIRR_COLORS:\n"
            );
            
            PrintSettings();
            
            exit(0);
		}
    }
};

#if defined(SIGINT) || defined(SIGTERM)
static void Term(int dummy)
{
	Gprintf("^C\n");
    dummy=dummy;

    RowLen=1;
    Summat();
    
    exit(0);
}
#endif

int main(int argc, const char *const *argv)
{
    if(!isatty(1))Colors = false;
    
    #ifdef DJGPP
    _djstat_flags &= ~(_STAT_EXEC_EXT | _STAT_WRITEBIT);
    #endif

	SetDefaultOptions();
	
	#ifdef SIGINT
    signal(SIGINT,  Term);
    #endif
    
    #ifdef SIGTERM
    signal(SIGTERM, Term);
    #endif

    // cute
    Handle parameters (getenv("DIRR"), argc, argv);
	
	Dumping = true;
    DumpDirs();

	if(RowLen > 0)Gprintf("\n");
    Summat();

    return 0;
}
