/*

	Unix directorylister (replacement for ls command)
	Copyright (C) 1992,2000 Bisqwit (http://iki.fi/bisqwit/)
	
*/

#define _BSD_SOURCE 1

#define VERSIONSTR \
    "DIRR "VERSION" copyright (C) 1992,2000 Bisqwit (http://iki.fi/bisqwit/)\n" \
    "This program is under GPL. dirr-"VERSION".tar.gz\n" \
    "is available at the homepage of the author.\n" \
    "About some ideas about this program, thanks to Warp.\n"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <cerrno>
#include <csignal>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

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

#include <algorithm>
#include <vector>
#include <string>

#ifdef HAVE_DIR_H
#include <dir.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif

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
                                *s=='f',
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
    char Source[PATH_MAX+1];
    char DirName[PATH_MAX+1];
    int Tried = 0;

    struct stat Stat;
    struct dirent *ent;
    DIR *dir;

    strcpy(DirName, Dirrikka);

    #ifdef DJGPP
    if(DirName[strlen(DirName)-1] == ':')
        strcat(DirName, ".");
    #endif

    strcpy(Source, DirName);

R1: if((dir = opendir(Source)) == NULL)
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
        else if(!Tried)
        {
        	strcat(Source, "/");
            Tried=1;
            goto R1;
        }
        else if(errno==EACCES)
        {
            Gprintf(" No access to %s\n", Source);
            goto End_ScanDir;
        }

        if(errno)
            Gprintf("\n%s - error: %d (%s)\n", Source, errno, GetError(errno).c_str());

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

const char *GetNextArg(int argc, const char *const *argv)
{
    static char *DirrBuf = NULL;
    static char *DirrVar = NULL;
    static int Argc=1;
    const char *Temp = NULL;
    static int Ekastrtok;

    if(!DirrBuf && !DirrVar)
    {
        DirrVar = getenv("DIRR");
        if(DirrVar)
        {
        	DirrBuf = new char[strlen(DirrVar)+1];
            DirrVar = strcpy(DirrBuf, DirrVar);
            Ekastrtok=1;
    	}
    }

Retry:
    if(DirrVar)
    {
        Temp = strtok(Ekastrtok?DirrVar:NULL, " \t\r\n");
        Ekastrtok=0;
        if(Temp)goto Ok;
        free(DirrBuf);
        DirrVar = NULL;
        DirrBuf = (char *)&DirrBuf; /* Kunhan vain on muuta kuin NULL */
    }

    if(Argc >= argc)return NULL;

    Temp = argv[Argc++];
Ok: if(Temp && !*Temp)goto Retry;
    return Temp;
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

static vector<string> Dirpuu, Argpuu;
static vector<unsigned> Virhepuu;

static unsigned RememberParam(vector<string> &Dirs, const char *s)
{
	Dirs.push_back(s);
	return Dirs.size();
}

static void DumpDirs()
{
    EstimateFields();
    
    unsigned a, b=Dirpuu.size();
    
    for(a=0; a<b; ++a)
        ScanDir(Dirpuu[a].c_str());
    
    Dirpuu.clear();
    
    DropOut();
}

static void DumpVirheet()
{
    if(!Virhepuu.size())return;

    Gprintf("Invalid option%s\n", Virhepuu.size()==1?"":"s");
    
    unsigned a, b=Argpuu.size();
    
    for(a=0; a<b; ++a)
        Gprintf("%s%3u: `%s'\n",
            binary_search(Virhepuu.begin(),
                          Virhepuu.end(),
                          a+1)?"-->":"\t", a+1, Argpuu[a].c_str());
    exit(EXIT_FAILURE);
}

int main(int argc, const char *const *argv)
{
    int Args = 0;
    int Help = 0;

    const char *Arggi;

    #ifdef DJGPP
    if(!isatty(fileno(stdout)))Colors = false;
    _djstat_flags &= ~(_STAT_EXEC_EXT | _STAT_WRITEBIT);
    #endif

    GetScreenGeometry();
	SetDefaultOptions();
	
	#ifdef SIGINT
    signal(SIGINT,  Term);
    #endif
    #ifdef SIGTERM
    signal(SIGTERM, Term);
    #endif

	#if CACHE_NAMECOLOR
	BuildNameColorCache();
	#endif
	
    while((Arggi = GetNextArg(argc, argv)) != NULL)
    {
        unsigned ArgIndex = RememberParam(Argpuu, Arggi);

        if(*Arggi=='-'
        #ifdef DJGPP
        || *Arggi=='/'
        #endif
        )
        {
            const char *s = Arggi;
            while(*s)
            {
                switch(*s)
                {
                    case '/':
                    case '-':
                        break;
                    case 'r':
                    	SetDefaultOptions();
                    	Inodemap.enable();
                    	break;
                    case 'd':
                    	switch(*++s)
                    	{
                    		case '1': DateTime=1; break;
                    		case '2': DateTime=2; break;
                    		case '3': DateTime=3; break;
                    		case 'b':
                    			BlkStr = s+1;
                    			goto ThisIsOk;
                    		case 'c':
                    			ChrStr = s+1;
                    			goto ThisIsOk;
                    		default:
                    			goto VirheParam;
                    	}
                        break;
                    case 'l':
                    	s++;
                    	if(*s=='a')goto VipuAL; /* Support -la and -al */
                    	#ifdef S_ISLNK
                        Links = 
                        #endif
                        strtol(s, (char **)&s, 10);
                        s--;
                        break;
                    case 'X':
                    	s++;
                    	if(*s=='0')
                    		printf("Window size = %dx%d\n", COLS, LINES);
                        COLS=strtol(s, (char **)&s, 10);
                        s--;
                        break;
                    case 'H':
                    	if(s[1]=='1')
                    	{
                    		++s;
                    		Inodemap.enable();
                    	}
                    	else
                    	{
                    		if(s[1]=='0')++s;
	                    	Inodemap.disable();
	                    }
                    	break;
                    case 'h':
                    case '?': Help = 1; break;
                    case 'C': Sara = true; break;
                    case 'D': Contents = false; break;
                    case 'c': Colors = false; break;
                    case 'p': Pagebreaks = true; break;
                    case 'P': AnsiOpt = false; break;
                    case 'e':
                    	PreScan = false;
                    	Sorting = "";
                    	break;
                   	case 'o':
                   		Sorting = s+1;
                   		goto ThisIsOk;
                    case 'F':
                    	DateForm = s+1;
                        goto ThisIsOk;
                    case 'f':
                    	FieldOrder = s+1;
                        goto ThisIsOk;
                    case 'v':
                    case 'V':
                    	printf(VERSIONSTR);
                    	return 0;
                    case 'a':
                    	switch(*++s)
                    	{
                    		case '0':
                    			FieldOrder = ".f_.s.d|";
                    			DateForm = "%z";
                            #ifdef S_ISLNK
                                Links = 1;
                            #endif
                                Sara = true;
                                Compact = 1;
                    			break;
                    		case '1':
                    			FieldOrder = ".xF|.a1.xF|.f.s.xF|.d.xF|.o_.g.xF|";
                    			break;
                    		case '2':
                            #ifdef S_ISLNK
                    			Links=0;
                            #endif
                    			Sara = true;
                    			FieldOrder = ".f_.a4_.o_.g.xF|";
                    			break;
                    		case '3':
                            #ifdef S_ISLNK
                    			Links=0;
                            #endif
                    			Sara = true;
                    			FieldOrder = ".f_.s_.o.xF|";
                    			break;
                    		case '4':
                            #ifdef S_ISLNK
                    			Links=1;
                            #endif
                    			Sara = true;
                    			FieldOrder = ".f_.s_.d_.o.xF|";
                    			DateForm = "%z";
                    			break;
                    		case 't':
                    			Sorting = "dn";
                    			PreScan = true;
                    		case 'l':
                    VipuAL:		FieldOrder = ".a1_.h__.o_.g_.s_.d_.f";
                    			DateForm = "%u";
                    			Inodemap.enable();
                    			break;
                    		default:
                    			goto VirheParam;
                    	}
                    	break;
                    case 'M':
                    case 'm':
                    	TotalSep = (*s=='M') ? s[2] : 0;
                    	switch(*++s)
                    	{
                    		case '0':
                    			Compact = 0;
                    			Totals = true;
                    			break;
                    		case '1':
		                    	Compact = 1;
        		            	Totals = true;
                    			break;
                    		case '2':
                    			Totals = false;
                    			break;
                    		case '3':
		                    	Compact = 2;
        		            	Totals = true;
                    			break;
                    		default:
                    			goto VirheParam;
                    	}
                    	if(TotalSep)s++;
                    	break;
                    case 'w':
                    case 'W':
                    	Compact = 1;
                    	Totals = true;
                    	FieldOrder = ".f";
                        Sorting = (isupper((int)*s)?"PCM":"pcm");
                        Inodemap.disable();
                    #ifdef S_ISLNK
                        Links = 1;
                    #endif
                        Sara = true;
                        break;
                    default:
            VirheParam:	;
                    {
                    	Virhepuu.push_back(ArgIndex);
                        goto ThisIsOk;
                    }
                }
                s++;
            }
ThisIsOk:;
        }
        else
        {
            RememberParam(Dirpuu, Arggi);
            Args=1;
        }
    }

    DumpVirheet();

    if(Help)
    {
    	bool c=Colors, a=AnsiOpt, p=Pagebreaks;
    	SetDefaultOptions();
    	Pagebreaks = p;
    	AnsiOpt = a;
    	Dumping = true;
    	Colors = c;

        GetDescrColor("txt", 1);
        Gprintf(
            #ifndef DJGPP
            "\r\33[K\r"
            #endif
            "Usage: %s [options] { dirs | files }\n\n"
            "Options:\n"
            "\t-a0 Short for -wf.f_.s.d| -F%%z\n"
            "\t-a1 Short for -f.xF|.a1.xF|.f.s.xF|.d.xF|.o_.g.xF|\n"
            "\t-a2 Short for -l0Cf.f_.a4_.o_.g.xF|\n"
            "\t-a3 Short for -l0Cf._s_.o.xF|\n"
            "\t-a4 Short for -l1Cf_.s_.d_.o.xF| -F%%z\n"
            "\t-al Short for -f.a1_.h__.o_.g_.s_.d_.f -F%%u\n"
            "\t-at Short for -alodn\n"
            "\t-c  Disables colors.\n"
            "\t-C  Enables multicolumn mode.\n"
            "\t-d1 Use atime, last access datetime (disables -d2 and -d3)\n"
            "\t-d2 Use mtime, last modification datetime (default, disables -d1 and -d3)\n"
            "\t-d3 Use ctime, creation datetime (disables -d1 and -d2)\n",
            argv[0]);
        Gprintf(
            "\t-dbx Specify how the blockdevices are shown\n"
            "\t     Example: `-db<B%%u,%%u>'\n"
            "\t     Default is `-db%s'\n"
            "\t-dcx Specify how the character devices are shown\n"
            "\t     Example: `-dc<C%%u,%%u>'\n"
            "\t     Default is `-dc%s'\n", BlkStr.c_str(), ChrStr.c_str());
        Gprintf(
            "\t-D  Show directory names instead of contents.\n"
            "\t-e  Quick list (uses space estimation). Disables sorting.\n"
            "\t-fx Output format\n"
            "\t    .s=Size,    .f=File,   .d=Datetime,     .o=Owner,   .g=Group,\n"
            "\t    .S#=size with thsep #, .x##=Color 0x##, .h=Number of hard links\n"
            #ifdef DJGPP
            "\t    .a0=SHRA style attributes      "
            #else
            "\t    .a1=drwxrwxrwx style attributes"
            #endif
                                                 "         .a#=Mode as #-decimal octal\n"
            "\t    .F and .G and .O are respectively, without space fitting\n"
            "\t    anything other=printed\n"
            "\t    Default is `-f%s'\n"
            "\t-Fx Specify new date format. Result maxlen 64.\n"
            "\t    Default is `-F%s'\n",
            FieldOrder.c_str(),
            DateForm.c_str());
        Gprintf(
            "\t-H  Disables verbose hardlink display.\n"
            "\t-H1 Enables verbose hardlink display (default).\n"
#ifdef S_ISLNK
            "\t-ln Specify how the links are shown\n"
            "\t  0 Show link name and stats of target\n"
            "\t  1 Show link name and <LINK>\n"
            "\t  2 Show link name, link's target and stats of link\n"
            "\t  3 Show link name, link's target and stats of target\n"
            "\t  4 Show link name, link's target and <LINK>\n"
#else
			"\n-l# Does nothing in this environment.\n"
#endif
            "\t-m0 Selects verbose \"total\" list. This is the default.\n"
            "\t-m1 Compactifies the \"total\" list.\n"
            "\t-m2 Disables total sums.\n"
            "\t    Instead of -mx you can use -Mxn and specify n\n"
            "\t    as the thousand separator in total sums.\n"
            "\t-m3 Compactified \"total\" list with exact free space.\n");
        Gprintf(            
            "\t-on Sort the list (disables -e), with n as combination of:\n"
            "\t    (n)ame, (s)ize, (d)ate, (u)id, (g)id, (h)linkcount\n"
            "\t    (c)olor, na(m)e case insensitively\n"
            "\t    g(r)oup dirs,files,links,chrdevs,blkdevs,fifos,socks\n"
            "\t    grou(p) dirs,links=files,chrdevs,blkdevs,fifos,socks\n"
            "\t    Use Uppercase for reverse order.\n"
            "\t    Default is `-o%s'\n", Sorting.c_str());
        Gprintf(
            "\t-p  Enables page pauses.\n"
            "\t-P  Disables the vt100 code optimization.\n"
            "\t-r  Undoes all options, including the DIRR environment variable.\n");
        Gprintf(
            "\t-w  Short for -HCm1l1f.f -opcm\n"
            "\t-W  Same as -w, but reverse sort order\n"
           	"\t-X# Force screen width\n"
            "You can set environment variable 'DIRR' for the options.\n"
            "You can also use 'DIRR_COLORS' -variable for color settings.\n"
            "Current DIRR_COLORS:\n"
        );

        PrintSettings();

        return 0;
    }

    if(!Args)
        RememberParam(Dirpuu, ".");

	ReadGidUid();
	
	Dumping = true;
    DumpDirs();

    Summat();

    return 0;
}
