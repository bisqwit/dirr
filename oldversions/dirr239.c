/*
	Unix directorylister (replacement for ls command)
	
	Can be compiled with the following command
		gcc -Wall -W -pedantic -O2 -g -o /usr/local/bin/dirr dirr.c
	or
		`grep WaIl dirr.c`	(change I to l)
	
	Works with djgpp also.
	
	If the compilation stops with errors about
	statfs, try gcc -O2 -D NO_STATFS -o dirr dirr.c
	
	Notice that in version 2.29 the type() and
	info() strings were modified a bit; you may
	need to update your DIRR_COLORS...
	
	In version 2.33, -dbx and -dcx functions were added.
	
	In version 2.34, the %Lf format was removed.
	
	TODO: If user is a member of multiple groups,
	      all groups be shown correctly in colouring.
	      
*/

#define _BSD_SOURCE 1

#define PRELOAD_UIDGID 0  /* Set to 1 if your passwd file is quick to load */

#define VERSION \
    "DIRR v2.39 copyright (C) 1992,1999 Bisqwit (http://iki.fi/bisqwit/)\n" \
    "This program is freeware. The source codes ("__FILE__") are available at the\n" \
    "homepage of the author. About some ideas about this program, thanks to Warp.\n"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#ifndef major
 #define major(dev) ((dev) >> 8)
 #define minor(dev) ((dev) & 255)
#endif

#ifdef DJGPP
 #include <dir.h>
#endif

#ifndef NAME_MAX
 #define NAME_MAX 255  /* Chars in a file name */
#endif
#ifndef PATH_MAX
 #define PATH_MAX 1023 /* Chars in a path name */
#endif

#define STRANGE 077642 /* May not have & 0111 */

#define DEFAULTATTR 7

static int LongestName;
static int LongestSize;
static int LongestUID;
static int LongestGID;

enum {SumDir=1,SumFifo,SumSock,SumFile,SumLink,SumChrDev,SumBlkDev};

static unsigned long SumCnt[10] = {0};
static unsigned long Summa[10]  = {0};

static int Sara, Compact, Colors, Contents, DateTime;
static int Totals, Pagebreaks, AnsiOpt, PreScan, TotalSep;
#ifdef S_ISLNK
static int Links;
#endif
static char BlkStr[64], ChrStr[64];

static char Sorting[64]; /* n,d,s,u,g */
static char DateForm[64];
static char FieldOrder[128];

static int TextAttr = DEFAULTATTR;
static int Dumping = 0; /* If 0, save some time */
static int LINES=25, COLS=80;
#define MAX_COLS 200 /* Expect no bigger COLS-values */

static void SetDefaultOptions(void)
{
	PreScan = 1; /* Clear with -e   */
	Sara    = 0; /* Set with -C     */
	Compact = 0; /* Set with -m     */
	#ifdef S_ISLNK
	Links   = 3; /* Modify with -l# */
	#endif
	Colors  = 1; /* Clear with -c   */
	Contents= 1; /* Clear with -D   */
	DateTime= 2; /* Modify with -d# */
	Totals  = 1; /* Clear with -t   */
	Pagebreaks=0;/* Set with -p     */
	AnsiOpt = 1; /* Clear with -P   */
	PreScan = 1; /* Clear with -e   */
	
	TotalSep= 0; /* Modify with -Mx */
	
	strcpy(Sorting, "pmgU");
	strcpy(DateForm, "%d.%m.%y %H:%M");
				 /* Modify with -F  */
				 
				 /* Modify with -f  */	
	#ifdef DJGPP
	strcpy(FieldOrder, ".f.s_.d");
	#else
	strcpy(FieldOrder, ".f.s_.a4_.d_.o_.g");
	#endif
	
	strcpy(BlkStr, "<B%u,%u>");	/* Modify with -db */
	strcpy(ChrStr, "<C%u,%u>");	/* Modify with -dc */
}

/***
 *** Settings - This is the default DIRR_COLORS
 ***
 *** The colour codes are one- or two-digit hexadecimal
 *** numbers, which consist of the following values:
 ***
 ***   80 = blink
 ***   40 = background color red
 ***   20 = background color green
 ***   10 = background color blue
 ***   08 = foreground high intensity
 ***   04 = foreground color red
 ***   02 = foreground color green
 ***   01 = foreground color blue
 ***
 *** Add those to make combination colors:
 ***
 ***   09 = 9 = 8+1         = bright blue
 ***   4E = 40+E = 40+8+4+2 = bright yellow (red + green) on red background
 ***    2 = 02              = green.
 ***
 *** I hope you understand the basics of hexadecimal arithmetic.
 *** If you have ever played with textattr() function in
 *** Borland/Turbo C++ or with TextAttr variable in
 *** Borland/Turbo Pascal or assigned colour values directly
 *** into the PC textmode video memory, you should understand
 *** how the color codes work.
 ***/
 
static char *Settings =
    /**************************************************************
	 * mode() - How to color the drwxr-xr-x string.
	 *
	 * ?               is the unknown case
	 * #               is for numeric modes
     " S,H,R,A         are for dos attributes
	 * l,d,x,c,b,p,s,- are the corresponding characters
	 *                 in the mode string respectively
     **************************************************************/

    #ifdef DJGPP
    "mode(SB,H8,R9,A3,-7,#3)"
    #else
    "mode(lB,d9,x9,cE,bE,p6,sD,-7,?C,#3)"
    #endif
    
    /**************************************************************
     * info() - How to color the type characters
     *          in the end of filenames.
     *          
     * / is for directories
     * * for executables
     * @ for links
     * = for sockets
     * | for pipes
     * ? for links which of destinations does not exist
     **************************************************************/
    "info(/1,*2,@3,=5,|8,?C)"
    
    /**************************************************************
     * owner() - How to color the file owner name.
     *
     * First is the color in the case of own file,
     * second for the case of file not owned by self.
     **************************************************************/
    "owner(4,8)"
    
    /**************************************************************
     * type() - How to color the file names, if it does
     *          not belong to any class of byext()'s.
     *
     * l=links, d=directories, x=executable
     * c=character devices, s=sockets
     * b=block devices, p=pipes.
     * ? is for file/link names which were not stat()able.
     **************************************************************/
    "type(lB,d9,xA,cE,bE,p6,sD,?3)"
    
    /**************************************************************
     * descr() - How to color the <DIR>, <PIPE> etc texts
     **************************************************************/
    "descr(3)"
    
    /**************************************************************
     * date() - How to color the datetimes
     **************************************************************/
    "date(7)"
    
    /**************************************************************
     * size() - How to color the numeric file sizes
     **************************************************************/
    "size(7)"
    
#ifndef DJGPP
    /**************************************************************
	 * nrlink() - How to color the number of hard links
     **************************************************************/
    "nrlink(7)"
#endif
	
    /**************************************************************
	 * num() - How to color the numbers in total-sums
     **************************************************************/
    "num(3)"
    
#ifndef DJGPP
    /**************************************************************
     * group() - How to color the file owner name.
     *
     * First is the color in the case of file of own group,
     * second for the case of file of not belonging the group to.
     **************************************************************/
    "group(4,8)"
#endif

    /**************************************************************
	 * txt() - The default color used for all other texts
     **************************************************************/
    "txt(7)"
    
    /**************************************************************
     * byext() - How to color the filenames that match one or more
     *           of the patterns in the list
     *
     * The first argument is the color code. It may have 'i' at end
     * of it. If it has 'i', the file name patterns are case insensitive.
     * 
     * There may be multiple byext() definitions.
     *
     * See the documentation at function WildMatch in this source
     * code for more information about the wildcards.
     **************************************************************/
    "byext(8i *~ *.bak *.old *.bkp *.st3 *.tmp *.$$$ tmp* *.~* core)"
    "byext(Ci *.tar *.tgz *.bz2 *.arj *.lzh *.z?? *.z *.gz *.rar *.deb *.rpm)"
    "byext(6i *.pas *.bas *.c *.cpp *.cc *.asm *.s *.inc *.h *.irc *.hpp *.src)"
    "byext(5i *.mod *.mtm *.s3m *.xm* *.mp* *.wav *.smp *.669 *.mid)"
    "byext(Di *.gif *.bmp *.x?m *.tif *.pcx *.lbm *.img *.jp*)"
    "byext(7i *.txt *.doc readme quickstart)"
;

/* Setting separator marks */
#define SetSep(c) ((c)== ')'|| \
                   (c)=='\t'|| \
                   (c)=='\n'|| \
                   (c)=='\r')

#define ALLOCATE(p, type, len) \
	if((p = (type)malloc((size_t)(len))) == NULL) \
    { \
    	SetAttr(7); \
		fprintf(stderr, \
			"\nOut of memory at line %d\n" \
			"\tALLOCATE(%s, %s, %s);\n", __LINE__, #p, #type, #len); \
        exit(EXIT_FAILURE); \
    }

static int RowLen;

#ifndef __GNUC__
 #define __attribute__
#endif

#ifdef DJGPP
#include <crt0.h>
#include <conio.h>
int __opendir_flags =
	__OPENDIR_PRESERVE_CASE
  | __OPENDIR_FIND_HIDDEN;
int _crt0_startup_flags =
	_CRT0_FLAG_PRESERVE_UPPER_CASE
  | _CRT0_FLAG_PRESERVE_FILENAME_CASE;
#define Ggetch getch
#endif

static int WhereX=0;

static int OldAttr=DEFAULTATTR;
static void FlushSetAttr(void);
static void EstimateFields(void);
static void SetAttr(int newattr);
static void Summat(void);
static int GetDescrColor(const char *descr, int index);

static int Gprintf(const char *fmt, ...) __attribute__((format(printf,1,2)));

static int Gputch(int x)
{
	static int Mask[MAX_COLS]={0};
	int TmpAttr = TextAttr;
    if(x=='\n' && (TextAttr&0xF0))GetDescrColor("txt", 1);
    
    if(x!=' ' || ((TextAttr&0xF0) != (OldAttr&0xF0)))
		FlushSetAttr();
		
	#ifdef DJGPP
	(Colors?putch:putchar)(x);
	#else
	if(x=='\a')
	{
		putchar(x);
		return x;
	}
	if(AnsiOpt && Colors)
	{
		static int Spaces=0;
		if(x==' ')
		{
			Spaces++;
			return x;
		}
		while(Spaces)
		{
			int a=WhereX, Now, mask=Mask[a];
			
			for(Now=0; Now<Spaces && Mask[a]==mask; Now++, a++);
			
			Spaces -= Now;
			if(mask)
			{
			Fill:
				while(Now>0){Now--;putchar(' ');}
			}
			else if(Spaces || !(x=='\r' || x=='\n'))
			{
				if(Now<=4)goto Fill;
				printf("\33[%dC", Now);
			}
			WhereX += Now;
		}
	}
	putchar(x);	
	#endif	
	if(x=='\n')SetAttr(TmpAttr);
	if(x=='\r')
		WhereX=0;
	else if(x!='\b')
    {
		if(x >= ' ')Mask[WhereX++] = 1;
		else		memset(Mask, WhereX=0, sizeof Mask);
    }
	else if(WhereX)
		WhereX--;	
		
	return x;
}

#ifndef DJGPP
#include <termio.h>
#include <sys/ioctl.h>
static int Ggetch(void)
{
	struct termio term, back;
	int c;
	ioctl(STDIN_FILENO, TCGETA, &term);
	ioctl(STDIN_FILENO, TCGETA, &back);
	term.c_lflag &= ~(ECHO | ICANON);
	term.c_cc[VMIN] = 1;
	ioctl(STDIN_FILENO, TCSETA, &term);
	c = getchar();
	ioctl(STDIN_FILENO, TCSETA, &back);
	return c;
}
#endif

static const char *GetError(int e)
{
    return strerror(e); /*
	int a;
	static char Buffer[64];
	strncpy(Buffer, strerror(e), 63);
	Buffer[63] = 0;
	a=strlen(Buffer);
	while(a && Buffer[a-1]=='\n')Buffer[--a]=0;
	return Buffer; */
}

static void SetAttr(int newattr)
{
	TextAttr = newattr;
}
static void FlushSetAttr(void)
{
    if(TextAttr == OldAttr)return;
    if(!Colors)goto Ret;

    #ifdef DJGPP

    textattr(TextAttr);

    #else

    printf("\33[");

    if(TextAttr != 7)
    {
    	static char Swap[] = "04261537";
    	
    	if(AnsiOpt)
    	{
	    	int pp=0;
	    	
	    	if((OldAttr&0x80) > (TextAttr&0x80)
	    	|| (OldAttr&0x08) > (TextAttr&0x08))
	    	{
	    		putchar('0'); pp=1;
	    		OldAttr = 7;
	    	}    	
	    	
	    	if((TextAttr&0x08) && !(OldAttr&0x08)){if(pp)putchar(';');putchar('1');pp=1;}
	    	if((TextAttr&0x80) && !(OldAttr&0x80)){if(pp)putchar(';');putchar('5');pp=1;}
	       	
	       	if((TextAttr&0x70) != (OldAttr&0x70))
	       	{
	   	   		if(pp)putchar(';');
	       		putchar('4');
	       		putchar(Swap[(TextAttr>>4)&7]);
	       		pp=1;
	       	}
	       	
	      	if((TextAttr&7) != (OldAttr&7))
	      	{
	       		if(pp)putchar(';');
	       		putchar('3');
	       		putchar(Swap[TextAttr&7]);
	       	}
       	}
       	else
	    	printf("0%s%s;4%c;3%c",
    			(TextAttr&8)?";1":"",
    			(TextAttr&0x80)?";5":"",
    			Swap[(TextAttr>>4)&7],
	    		Swap[TextAttr&7]);
    }

    putchar('m');
    #endif
Ret:OldAttr = TextAttr;
}

static int ColorNums = -1;
static int ColorNumToggle = 0;
static int Gprintf(const char *fmt, ...)
{
    static int Line=2;
    char *s, Buf[2048];
    int a;

    va_list ap;
    va_start(ap, fmt);
    a=vsprintf(Buf, fmt, ap);
    va_end(ap);
    
    for(s=Buf; *s; s++)
    {
    	if(*s=='\1')ColorNumToggle ^= 1;
    	else if(*s=='\t')Gprintf("   ");
        else
        {
#ifdef DJGPP
            if(*s=='\n')Gputch('\r');
#endif
            if(ColorNumToggle)
            {
            	int ta = TextAttr;
            	SetAttr(ColorNums);
            	Gputch(*s);
            	SetAttr(ta);
            }
            else
            	Gputch(*s);
            if(*s=='\n')
            {
                if(++Line >= LINES)
                {
                    if(Pagebreaks)
                    {
                    	int More=LINES-2;
                        int ta = TextAttr;
                        SetAttr(0x70);
                        Gprintf("\r--More--");
					    GetDescrColor("txt", 1);
                        Gprintf(" \b");
                        fflush(stdout);
                        for(;;)
                        {
                        	int Key = Ggetch();
                        	if(Key=='q' 
                        	|| Key=='Q'
                        	|| Key==3){More=-1;break;}
                        	if(Key=='\r'|| Key=='\n'){More=1;break;}
                        	if(Key==' ')break;
                        	Gputch('\a');
                       	}
                        Gprintf("\r        \r");
                        if(More<0)exit(0);
                        SetAttr(ta);
                        Line -= More;
    }	}	}	}	}
    return a;
}

static char *GetSet(char **S, const char *name)
{
    char *s = *S;
    int namelen;

    if(!s)return NULL;

    /* Setting string separators */
    namelen = strlen(name);

    for(;;)
    {
    	while(SetSep(*s))s++;

        if(!*s)break;

        if(!strncmp(s, name, namelen) && s[namelen] == '(')
        {
            char *t = s;
            int Len;
            for(Len=0; *t && !SetSep(*t); Len++)t++;
    
            *S = t+1;
            if(!**S)*S=NULL;
    
    		ALLOCATE(t, char *, Len+1);
            strncpy(t, s, Len);
            t[Len] = 0;
            return t;
        }
    
        while(*s && !SetSep(*s))s++;
    }

    return (*S = NULL);
}

static int GetHex(int Default, char **S)
{
    int eka, Color = Default;
    char *s = *S;

    for(eka=1; *s && isxdigit((int)*s); eka=0, s++)
    {
        if(eka)Color=0;else Color<<=4;

        if(*s > '9')
            Color += 10 + toupper(*s) - 'A';
        else
            Color += *s - '0';
    }

    *S = s;
    return Color;
}

static int GetModeColor(const char *text, int Chr)
{
    int Char;
    int Dfl = 7;

    char *t, *s;

    s = Settings;
    Char = Chr<0?-Chr:Chr;

    for(;;)
    {
    	char *T = GetSet(&s, text);
        if(!T)break;

        t = T+strlen(text)+1; /* skip 'text=' */

        while(*t)
        {
            int C, c=*t++;

            C = GetHex(Dfl, &t);

            if(c == Char)
            {
                if(Chr > 0)SetAttr(C);
                return C;
            }
            if(*t == ',')t++;
        }

        free(T);
    }

    Gprintf("DIRR_COLORS error: No color for '%c' found in '%s'\n", Char, text);

    return Dfl;
}

static int GetDescrColor(const char *descr, int index)
{
    int ind, Dfl=7;
    char *s;
    
    if(!Colors || !Dumping)return Dfl;
    
    ind=index<0?-index:index;
    
    s = Settings;    
    s = GetSet(&s, descr);
    if(!s)
        Gprintf("DIRR_COLORS error: No '%s'\n", descr);
    else
    {
        char *S;
        for(S=s+strlen(descr); ind; ind--)
        {	
        	S++;
        	Dfl = GetHex(Dfl, &S);
        }
        if(index>0)SetAttr(Dfl);
        free(s);
    }
    return Dfl;
}

static void PrintSettings(void)
{
    char *s = Settings;
    int LineLen, Dfl;
    
    Dfl=GetDescrColor("txt", -1);
    
    for(LineLen=0;;)
    {
        char *n, *t, *T;
        int Len;

        while(SetSep(*s))s++;

        if(!*s)break;

        t = s;
        for(Len=0; *t && !SetSep(*t); Len++)t++;

        ALLOCATE(t, char *, Len+8);
        strncpy(T=t, s, Len);
        strcpy(t+Len, ")");

        if(LineLen && LineLen+strlen(t) > 75)
        {
            Gprintf("\n");
            LineLen=0;
        }
        if(!LineLen)Gprintf("\t");

        LineLen += strlen(t);
        
        n = t;

        while(*t!='(')Gputch(*t++);
        Gputch(*t++);

        if(n[4]=='('
        && (!strncmp(n, "mode", 4)
         || !strncmp(n, "type", 4)
         || !strncmp(n, "info", 4)))
        {
            while(*t)
            {
                int c;
                char *k;
                int len;

                c = *t++;

                k=t;    
                SetAttr(GetHex(Dfl, &k));

                Gputch(c);

                SetAttr(Dfl);

                for(len=k-t; len; len--)Gputch(*t++);

                if(*t != ',')break;
                Gputch(*t++);
            }
            Gprintf("%s", t);
        }
        else
        {
            int C=Dfl, len;
            char *k;
            for(;;)
            {
                k = t;
                SetAttr(C=GetHex(C, &k));
                for(len=k-t; len; len--)Gputch(*t++);
                SetAttr(Dfl);
                if(*t != ',')break;
                Gputch(*t++);
            }
            if(*t!=')')Gputch(*t++);
            SetAttr(C);
            while(*t!=')')
            {
            	if(!*t)
            	{
            		SetAttr(0x0C);
            		Gprintf("(Error:unterminated)");
            		SetAttr(Dfl);
            		break;
            	}
            	Gputch(*t++);
          	}
            SetAttr(Dfl);
            Gputch(')');
        }
        
        free(T);
        
        while(*s && !SetSep(*s))s++;
    }
    if(LineLen)
        Gprintf("\n");
}

const char *NameOnly(const char *Name)
{
    const char *s;
    while((s = strchr(Name, '/')) != NULL)Name = s+1;
    return Name;
}

/***********************************************
 *
 * Getpwuid(uid)
 * Getgrgid(gid)
 *
 *   Get user and group name, quickly using half seek
 *
 * ReadGidUid(void)
 *
 *   Builds the data structures for Getpwuid() and Getgrgid()
 *
 *************************************************************/

#if PRELOAD_UIDGID

typedef struct idItem { int id; char *Name; } idItem;
static idItem *GidItems=NULL, *UidItems=NULL;
static int GidCount, UidCount;

static char *Getpwuid(int uid)
{
	register int L=0,H=UidCount-1;
	while(L <= H)
	{
		register int i = (L+H)/2, C = UidItems[i].id - uid;
		if(!C)return UidItems[i].Name;
		if(C < 0)L=i+1;else H=i-1;
	}
	return NULL;
}
static char *Getgrgid(int gid)
{
	register int L=0,H=GidCount-1;
	while(L <= H)
	{
		register int i = (L+H)/2, C = GidItems[i].id - gid;
		if(!C)return GidItems[i].Name;
		if(C < 0)L=i+1;else H=i-1;
	}
	return NULL;
}
static int IdSort(const void *a, const void *b)
{
	return ((idItem *)a)->id - ((idItem *)b)->id;
}
static void ReadGidUid(void)
{
	int a;
	
	for(setpwent(), UidCount=0; getpwent() != NULL; UidCount++);
	ALLOCATE(UidItems, idItem *, UidCount * (sizeof(idItem)));
	for(setpwent(), a=0; a<UidCount; a++)
	{
		struct passwd *p = getpwent();
		ALLOCATE(UidItems[a].Name, char *, strlen(p->pw_name)+1);
		strcpy(UidItems[a].Name, p->pw_name);
		UidItems[a].id = (int)p->pw_uid;
	}
	endpwent();
	
	for(setgrent(), GidCount=0; getgrent() != NULL; GidCount++);
	ALLOCATE(GidItems, idItem *, GidCount * (sizeof(idItem)));
	for(setgrent(), a=0; a<GidCount; a++)
	{
		struct group *g = getgrent();
		ALLOCATE(GidItems[a].Name, char *, strlen(g->gr_name)+1);
		strcpy(GidItems[a].Name, g->gr_name);
		GidItems[a].id = (int)g->gr_gid;
	}
	endgrent();

	qsort(UidItems, UidCount, sizeof(idItem), IdSort);
	qsort(GidItems, GidCount, sizeof(idItem), IdSort);
}

#else /* no PRELOAD_UIDGID */
static char *Getpwuid(int uid)
{
	struct passwd *tmp = getpwuid(uid);
	if(!tmp)return NULL;
	return tmp->pw_name;
}
static char *Getgrgid(int gid)
{
	struct group *tmp = getgrgid(gid);
	if(!tmp)return NULL;
	return tmp->gr_name;
}
#define ReadGidUid()

#endif

/***********************************************
 *
 * WildMatch(Pattern, What)
 *
 *   Compares with wildcards.
 *
 *   This routine is a descendant of the fnmatch()
 *   function in the GNU fileutils-3.12 package.
 *
 *   Supported wildcards:
 *       *
 *           matches multiple characters
 *       ?
 *           matches one character
 *       [much]
 *           "much" may have - (range) and ^ or ! (negate)
 *       \
 *           quote next wildcard
 *
 *   Global variable IgnoreCase controls the case
 *   sensitivity of the operation as 0=case sensitive, 1=not.
 *
 *   Return value: 0=no match, 1=match
 *
 * rmatch(Name, Items)
 *
 *   Finds a name from the space-separated wildcard list
 *   Uses WildMatch() to compare.
 *
 *   Return value: 0=no match, >0=index of matched string
 *
 **********************************************************/
 
static int IgnoreCase;

static int WildMatch(const char *Pattern, const char *What)
{
	register const char *p=Pattern, *n = What;
	register char c;
 
	while((c = *p++) != 0)
    {
    	#define FOLD(c) (IgnoreCase ? toupper(c) : (c))
    	c = FOLD(c);
		switch(c)
		{
			case '?':
				if(!*n || *n=='/')return 0;
				break;
			case '\\':
				if(FOLD(*n) != c)return 0;
				break;
			case '*':
				for(c=*p++; c=='?' || c=='*'; c=*p++, ++n)
					if(*n == '/' || (c == '?' && *n == 0))return 0;
				if(!c)return 1;
				{
					char c1 = FOLD(c);
					for(--p; *n; n++)
						if((c == '[' || FOLD(*n) == c1) && WildMatch(p, n))
							return 1;
					return 0;
				}
			case '[':
			{
				/* Nonzero if the sense of the character class is inverted.  */
				register int not;
				if(!*n)return 0;
				not = (*p == '!' || *p == '^');
				if(not)p++;
				c = *p++;
				for(;;)
				{
					register char cstart, cend;
					cstart = cend = FOLD(c);
					if(!c)return 0;
					c = FOLD(*p);
					p++;
					if(c=='/')return 0;
					if(c=='-' && *p!=']')
					{
						if(!(cend = *p++))return 0;
						cend = FOLD(cend);
						c = *p++;
					}
					if(FOLD(*n) >= cstart && FOLD(*n) <= cend)
					{
						while(c != ']')
						{
							if(!c)return 0;
							c = *p++;
						}
						if(not)return 0;
						break;
					}
					if(c == ']')break;
				}
				if(!not)return 0;
				break;
			}
			default:
				if(c != FOLD(*n))return 0;
		}
		n++;
	}
	return !*n;
}

static int rmatch(const char *Name, const char *Items)
{
	char What[PATH_MAX+1];
    char Buffer[PATH_MAX+1];

	int Index;
    char *s, *n;
    if(!Name || !Items)return 0;

    strcpy(Buffer, Items);
    strcpy(What, Name);

    for(n=Buffer, Index=1; (s=strtok(n, " ")) != NULL; Index++)
    {
        if(WildMatch(s, What))goto Ret;
        n=NULL;
    }

    Index=0;

Ret:return Index;
}

static int WasNormalColor;
static int NameColor(const char *Name)
{
    char *s = Settings;
    int Normal = GetDescrColor("txt", -1);

    for(WasNormalColor=0;;)
    {
        int c, result;
        char *t, *T = GetSet(&s, "byext");        

        if(!T)break;

        t = T+6; /* skip "byext" */

        c = GetHex(Normal, &t);

        IgnoreCase = *t++ == 'i';

        result = rmatch(Name, t);

        free(T);

        if(result)return c;
    }

    WasNormalColor=1;

    return Normal;
}

static int GetNameAttr(const struct stat *Stat, const char *fn)
{
    int NameAttr = NameColor(fn);

    if(!WasNormalColor)return NameAttr;

    #ifdef S_ISLNK
    if(S_ISLNK(Stat->st_mode))       NameAttr=GetModeColor("type", -'l');
    else
    #endif
         if(S_ISDIR(Stat->st_mode))  NameAttr=GetModeColor("type", -'d');
    else if(S_ISCHR(Stat->st_mode))  NameAttr=GetModeColor("type", -'c');
    else if(S_ISBLK(Stat->st_mode))  NameAttr=GetModeColor("type", -'b');
    #ifdef S_ISFIFO
    else if(S_ISFIFO(Stat->st_mode)) NameAttr=GetModeColor("type", -'p');
    #endif
    #ifdef S_ISSOCK
    else if(S_ISSOCK(Stat->st_mode)) NameAttr=GetModeColor("type", -'s');
    #endif
    else if(Stat->st_mode&00111)     NameAttr=GetModeColor("type", -'x');

    return NameAttr;
}

static void PrintAttr(const struct stat *Stat, char Attrs, int *Len, unsigned int dosattr)
{
    switch(Attrs)
    {
        case '0':
            #define PutSet(c) {GetModeColor("mode", c);Gputch(c);(*Len)++;}
        	if(dosattr&0x20)PutSet('A')else PutSet('-')
        	if(dosattr&0x02)PutSet('H')else PutSet('-')
        	if(dosattr&0x04)PutSet('S')else PutSet('-')
        	if(dosattr&0x01)PutSet('R')else PutSet('-')
            break;
        case '1':
        #ifndef DJGPP
            #ifdef S_ISLNK
                 if(S_ISLNK(Stat->st_mode))  PutSet('l')
            else
            #endif
                 if(S_ISDIR(Stat->st_mode))  PutSet('d')
            else if(S_ISCHR(Stat->st_mode))  PutSet('c')
            else if(S_ISBLK(Stat->st_mode))  PutSet('b')
            #ifdef S_ISFIFO
            else if(S_ISFIFO(Stat->st_mode)) PutSet('p')
            #endif
            #ifdef S_ISSOCK
            else if(S_ISSOCK(Stat->st_mode)) PutSet('s')
            #endif
            else if(S_ISREG(Stat->st_mode))  PutSet('-')
            else                             PutSet('?')

            GetModeColor("mode", '-');
            Gputch((Stat->st_mode&00400)?'r':'-');
            Gputch((Stat->st_mode&00200)?'w':'-');

            GetModeColor("mode", (Stat->st_mode&00100)?'x':'-');
            Gputch((Stat->st_mode&04000)?'s':((Stat->st_mode&00100)?'x':'-'));

            GetModeColor("mode", '-');
            Gputch((Stat->st_mode&00040)?'r':'-');
            Gputch((Stat->st_mode&00020)?'w':'-');

            GetModeColor("mode", (Stat->st_mode&00010)?'x':'-');
            Gputch((Stat->st_mode&02000)?'s':((Stat->st_mode&00010)?'x':'-'));

            GetModeColor("mode", '-');
            Gputch((Stat->st_mode&00004)?'r':'-');
            Gputch((Stat->st_mode&00002)?'w':'-');

            GetModeColor("mode", (Stat->st_mode&00001)?'x':'-');
            Gputch((Stat->st_mode&01000)?'t':((Stat->st_mode&00001)?'x':'-'));

            (*Len) += 9;

        #endif
            break;

           	#undef PutSet
        case '2':
            Attrs = '3';
        default:
        {
    		char Buf[104]; /* 4 is max extra */
            Attrs -= '0';
            sprintf(Buf, "%0100o", (int)Stat->st_mode);
            GetModeColor("mode", '#');
            (*Len) += Gprintf("%s", Buf+100-Attrs);
        }
}   }

/***********************************************
 *
 * GetName(s, Stat, Space, Fill)
 *
 *   Prints the filename
 *
 *     s:      Filename
 *     Stat:   stat-bufferi, jonka modesta riippuen
 *             saatetaan tulostaa perään merkkejä
 *             =,|,?,/,*,@. Myös ->:t katsotaan.
 *     Space:  Maximum usable printing space
 *     Fill:   Jos muu kuin nolla, ylimääräinen tila
 *             täytetään spaceilla oikealta puolelta
 * 
 *   Return value: True (not necessarily printed) length
 *
 **********************************************************/
 
static int GetName(const char *s, const struct stat *Stat, int Space, int Fill)
{
    char Puuh[PATH_MAX+1];
    char Buf[PATH_MAX+1];

    int i, Len = 0;

    strcpy(Puuh, NameOnly(s));
    
#ifdef S_ISLNK
Redo:
#endif
    strcpy(Buf, Puuh);
    Len += (i = strlen(Buf));
    
    if(i > Space)i = Space;
    Buf[i] = 0;
    Gprintf("%*s", -i, Buf);
    Space -= i;

    #define PutSet(c) {Len++; if(Space){GetModeColor("info", c);Gputch(c);Space--;}}

    #ifdef S_ISSOCK
    if(S_ISSOCK(Stat->st_mode)) PutSet('=')
    #endif
    #ifdef S_ISFIFO
    if(S_ISFIFO(Stat->st_mode)) PutSet('|')
    #endif
    if(Stat->st_mode==STRANGE)  PutSet('?')

    #ifdef S_ISLNK
    if(S_ISLNK(Stat->st_mode))
    {
        if(Links >= 2)
        {
            int a;
            struct stat Stat1;
            char Target[PATH_MAX+1];

            strcpy(Buf, " -> ");

            Len += (a = strlen(Buf));
            
            if(a > Space)a = Space;
            if(a)
            {
	            Buf[a] = 0;
                GetModeColor("info", '@');
                Gprintf("%*s", -a, Buf);
            }
            Space -= a;

            a = readlink(s, Target, sizeof Target);
            if(a < 0)a=0;
            Target[a] = 0;
            
            /* Buf = Absolute target address */

            Buf[0] = 0;
            if(Target[0] != '/')
            {
                strcpy(Buf, s);
                *((char *)NameOnly(Buf)) = 0;
            }
            strcat(Buf, Target);

            /* Target status */
	        if(stat(Buf, &Stat1) < 0)
    	    {
        	    Stat1.st_mode = STRANGE;
            	if(Space)GetModeColor("type", '?');
			}
	        else
    	        if(Space)SetAttr(GetNameAttr(&Stat1, Buf));
            
            strcpy(Puuh, s=Target);
            Stat = &Stat1;
            goto Redo;
        }
        PutSet('@')
    }
    else
    #endif
    
    if(S_ISDIR(Stat->st_mode))  PutSet('/')
    else if(Stat->st_mode&00111)PutSet('*')

    #undef PutSet

    if(Fill)
        while(Space)
        {
            Gputch(' ');
            Space--;
        }

    return Len;
}

static void PrintNum(char *Dest, int Seps, const char *fmt, ...)
	__attribute__((format(printf,3,4)));
	
static void PrintNum(char *Dest, int Seps, const char *fmt, ...)
{
	int Len;
	char *End;
	
	va_list ap;
	va_start(ap, fmt);
	vsprintf(Dest, fmt, ap);
	va_end(ap);
	
	if(Seps)
    {
        char *s = Dest;
        int SepCount;
        /* 7:2, 6:1, 5:1, 4:1, 3:0, 2:0, 1:0, 0:0 */

		End = strchr(Dest, '.');
		if(!End)End = strchr(Dest, 0);
		
		Len = (int)(End-Dest);		
		
		SepCount = (Len - 1) / 3;
	
        for(s=End; SepCount>0; SepCount--)
        {
            s -= 3;
            memmove(s+1, s, strlen(s)+1);
            *s = Seps=='_'?' ':Seps;
        }
    }
}
 
static char *GetSize(const char *s, const struct stat *Stat, int Space, int Seps)
{
    static char Buf[128];
    char *descr = "descr";
    char TmpBuf[32]; /* 31-numeroisia longinttejähän ei ole, eihän? */

    if(Space >= (int)(sizeof Buf))
    {
    	fprintf(stderr, "\ndirr: FATAL internal error - line %d\n", __LINE__);
        exit(EXIT_FAILURE);
    }

#ifdef S_ISLNK
GotSize:
#endif
		 if(S_ISDIR (Stat->st_mode))sprintf(Buf, "%*s", -Space, "<DIR>");
    #ifdef S_ISFIFO
    else if(S_ISFIFO(Stat->st_mode))sprintf(Buf, "%*s", -Space, "<PIPE>");
    #endif
    #ifdef S_ISSOCK
    else if(S_ISSOCK(Stat->st_mode))sprintf(Buf, "%*s", -Space, "<SOCKET>");
    #endif
    else if(S_ISCHR (Stat->st_mode))
    {
    	char *e;
    	sprintf(Buf, ChrStr,
    		(unsigned)major(Stat->st_rdev),
    		(unsigned)minor(Stat->st_rdev));
    	if(Space)
    	{
		    e = strchr(Buf, 0);
    		memset(e, ' ', (Buf+(sizeof(Buf)))-e);
    		Buf[Space] = 0;
    	}
    }
    else if(S_ISBLK (Stat->st_mode))
    {
    	char *e;
    	sprintf(Buf, BlkStr,
    		(unsigned)major(Stat->st_rdev),
    		(unsigned)minor(Stat->st_rdev));
    	if(Space)
    	{
	    	e = strchr(Buf, 0);
	    	memset(e, ' ', (Buf+(sizeof(Buf)))-e);
    		Buf[Space] = 0;
    	}
    }
    #ifdef S_ISLNK
    else if(S_ISLNK (Stat->st_mode))
    {
        if((Links!=1)&&(Links!=4))goto P1;
        sprintf(Buf, "%*s", -Space, "<LINK>");
    }
    #endif
    else
    {
        long l;

#ifdef S_ISLNK
P1:
		if(S_ISLNK(Stat->st_mode))
		{
			if(Links==2)descr=NULL;
			if(Links==3)
			{
				char Buf[PATH_MAX+1];
				char Target[PATH_MAX+1];
				static struct stat Stat1;
				
	            int a = readlink(s, Target, sizeof Target);
    	        if(a < 0)a=0;
	       	    Target[a] = 0;
       	     
       	     	/* Buf = Absolute target address */
       	     
       	     	Buf[0] = 0;
	            if(Target[0] != '/')
	            {
	                strcpy(Buf, s);
    	            *((char *)NameOnly(Buf)) = 0;
        	    }
	            strcat(Buf, Target);
	
    	        /* Target status */
	    	    if(stat(Buf, &Stat1) >= 0)
	    	    {
    		    	Stat = &Stat1;
    	    		goto GotSize;
		}	}	}
#endif
        l = Stat->st_size;
        
        PrintNum(TmpBuf, Seps, "%lu", l);

        sprintf(Buf, "%*s", Space, TmpBuf);
        
        if(descr)descr = "size";
    }
    
    if(descr)
	    GetDescrColor(descr, 1);
	else
    	GetModeColor("info", '@');

    return Buf;
}

static void TellMe(const struct stat *Stat, const char *Name
#ifdef DJGPP
	, unsigned int dosattr
#endif
	)
{
    int Len;
    char OwNam[16];
    char GrNam[16];
    char *Passwd, *Group;
    int ItemLen, NameAttr;
    int NeedSpace=0;
    char *s;

    if(S_ISDIR(Stat->st_mode))
    {
        SumCnt[SumDir]++;
        Summa[SumDir] += Stat->st_size;
    }
    #ifdef S_ISFIFO
    else if(S_ISFIFO(Stat->st_mode))
    {
        SumCnt[SumFifo]++;
        Summa[SumFifo] += Stat->st_size;
    }
    #endif
    #ifdef S_ISSOCK
    else if(S_ISSOCK(Stat->st_mode))
    {
        SumCnt[SumSock]++;
        Summa[SumSock] += Stat->st_size;
    }
    #endif
    else if(S_ISCHR(Stat->st_mode))
    {
        SumCnt[SumChrDev]++;
        Summa[SumChrDev] += Stat->st_size;
    }
    else if(S_ISBLK(Stat->st_mode))
    {
        SumCnt[SumBlkDev]++;
        Summa[SumBlkDev] += Stat->st_size;
    }
    #ifdef S_ISLNK
    else if(S_ISLNK(Stat->st_mode))
    {
        SumCnt[SumLink]++;
        Summa[SumLink] += Stat->st_size;
    }
    #endif
    else
    {
        SumCnt[SumFile]++;
        Summa[SumFile] += Stat->st_size;
    }

    NameAttr = GetNameAttr(Stat, NameOnly(Name));

    Passwd = Getpwuid((int)Stat->st_uid);
    Group  = Getgrgid((int)Stat->st_gid);
    if(Passwd)strcpy(OwNam, Passwd);else sprintf(OwNam,"%d",(int)Stat->st_uid);
    if( Group)strcpy(GrNam,  Group);else sprintf(GrNam,"%d",(int)Stat->st_gid);

   	Len = strlen(OwNam); if(Len > LongestUID)LongestUID = Len;
   	Len = strlen(GrNam); if(Len > LongestGID)LongestGID = Len;

    for(ItemLen=0, s=FieldOrder; *s; )
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
                        int i = 0;
                        s++;
                        Len = '1';
                        if((*s>='0')&&(*s<='9'))Len = *s;
                        PrintAttr(Stat, Len, &i,
						#ifdef DJGPP
                        	dosattr
                        #else
                        	0
						#endif
                        	);
                        ItemLen += i;
                        RowLen += i;
                        break;
                    }
                    case 'x':
                        s++;
                        SetAttr(GetHex(7, &s));
                        s--;
                        NeedSpace=0;
                        break;
                    case 'o':
                    case 'O':
                    {
                        int i;
                        GetDescrColor("owner", (Stat->st_uid==getuid())?1:2);
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
                        GetDescrColor("group", (Stat->st_gid==getgid())?1:2);
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
                        Gprintf("%4d", (int)Stat->st_nlink);
                        ItemLen += 4;
                        RowLen += 4;
                        break;
#endif
                    case 'F':
                    case 'f':
                        SetAttr(NameAttr);
                        GetName(Name, Stat, LongestName, (Sara||s[1]) && !isupper((int)*s));
                        ItemLen += LongestName;
                        RowLen += LongestName;
                        break;
                    case 's':
                        Gprintf("%s", GetSize(Name, Stat, LongestSize, 0));
                        ItemLen += LongestSize;
                        RowLen += LongestSize;
                        break;
                    case 'S':
                    {
                        int i;
                        s++; /* 's' on sepittömälle versiolle */
                        i = Gprintf("%s", GetSize(Name, Stat, LongestSize+3, *s?*s:' '));
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
                            case 1: t = Stat->st_atime; break;
                            case 2: t = Stat->st_mtime; break;
                            case 3: t = Stat->st_ctime;
                        }

                        if(!strcmp(DateForm, "%u"))
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
                        else if(!strcmp(DateForm, "%z"))
                        {
                        	struct tm *TM = localtime(&t);
                            time_t now = time(NULL);
                            int m=TM->tm_mon, d=TM->tm_mday, y=TM->tm_year;
                        	if(localtime(&now)->tm_year == y)
                        	{
                        		sprintf(Buf, "%3d.%d", d,m+1);
                        		if(Buf[5])strcpy(Buf, Buf+1);
                        	}
                        	else
                        		sprintf(Buf, "%5d", y+1900);
                        }
                        else
                            strftime(Buf, sizeof Buf, DateForm, localtime(&t));

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
                *s = ' ';
            default:
                Gputch(*s);
                ItemLen++;
                RowLen++;
        }
        if(*s)s++;
    }
    if(FieldOrder[0])
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
}   }   }

typedef struct StatPuu
{
	struct stat Stat;
    #ifdef DJGPP
    unsigned int dosattr;
    #endif
	char *Name;
    struct StatPuu *Next;
}StatTree;

StatTree *StatPuu = NULL;
static int StatPuuKoko = 0;
static StatTree **StatLatva = &StatPuu;

static void Puuhun(struct stat *Stat, const char *Name
#ifdef DJGPP
	, unsigned int attr
#endif
	)
{
	if(PreScan)
	{
		StatTree *tmp;
	
    	ALLOCATE(tmp, StatTree *, sizeof(StatTree));
	    ALLOCATE(tmp->Name, char *, strlen(Name)+1);
	
	    strcpy(tmp->Name, Name);
        #ifdef DJGPP
        tmp->dosattr = attr;
        #endif
    	tmp->Stat = *Stat;
	    tmp->Next = *StatLatva;
    	*StatLatva = tmp;
		StatLatva = &tmp->Next;
		
		if(Colors)
		{
			StatPuuKoko++;
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
				Gprintf("%d\r", StatPuuKoko);
		}
	}
	else
	{
		Dumping = 1;
		TellMe(Stat, Name
        #ifdef DJGPP
        , attr
        #endif
		);
		Dumping = 0;
	}
}

/* Returns the class code for grouping sort */
static int Class(const StatTree *a, int LinksAreFiles)
{
	if(S_ISDIR(a->Stat.st_mode))return 0;
    #ifdef S_ISLNK
	if(S_ISLNK(a->Stat.st_mode))return 2-LinksAreFiles;
    #endif
	if(S_ISCHR(a->Stat.st_mode))return 3;
	if(S_ISBLK(a->Stat.st_mode))return 4;
	#ifdef S_ISFIFO
	if(S_ISFIFO(a->Stat.st_mode))return 5;
	#endif
    #ifdef S_ISSOCK
	if(S_ISSOCK(a->Stat.st_mode))return 6;
	#endif
	return 1;
}

#ifndef DJGPP
static int stricmp(char *eka, char *toka)
{
	while(toupper(*eka) == toupper(*toka))
	{
		if(!*eka)return 0;
		eka++;
		toka++;
	}
	return toupper((int)*eka) - toupper((int)*toka);
}
#endif

static int SubSort(const void *Eka, const void *Toka)
{
	#define eka (*((const StatTree **)Eka))
	#define toka (*((const StatTree **)Toka))
	
	char *s;
	
    for(s=Sorting; *s; s++)
    {
    	int Result=0;
    	
    	switch(tolower(*s))
	    {
	    	case 'c':
	    		Result = GetNameAttr(&eka->Stat,eka->Name) 
	    			   - GetNameAttr(&toka->Stat,toka->Name);
	    		break;
	   	 	case 'n':
   		 		Result = strcmp(eka->Name, toka->Name);
   				break;
	   	 	case 'm':
   		 		Result = stricmp(eka->Name, toka->Name);
   				break;
	   	 	case 's':
    			Result = eka->Stat.st_size - toka->Stat.st_size;
    			break;
	    	case 'd':
    			switch(DateTime)
    			{
    				case 1: Result = eka->Stat.st_atime-toka->Stat.st_atime; break;
	    			case 2: Result = eka->Stat.st_mtime-toka->Stat.st_mtime; break;
    				case 3: Result = eka->Stat.st_ctime-toka->Stat.st_ctime;
    			}
    			break;
	    	case 'u':
    			Result = eka->Stat.st_uid - toka->Stat.st_uid;
    			break;
	    	case 'g':
    			Result = eka->Stat.st_gid - toka->Stat.st_gid;
    			break;
		   	case 'r':
    			Result = Class(eka, 0) - Class(toka, 0);
    			break;
		   	case 'p':
    			Result = Class(eka, 1) - Class(toka, 1);
    			break;
    		default:
    		{
    			char *t = Sorting;
		        SetAttr(GetDescrColor("txt", 1));
			    Gprintf("\nError: `-o");
			    while(t < s)Gputch(*t++);
			    GetModeColor("info", '?');
			    Gputch(*s++);
		        SetAttr(GetDescrColor("txt", 1));
		        Gprintf("%s'\n\n", s);
		        return Sorting[0] = 0;
		    }
		}
		if(Result)
			return isupper((int)*s) ? -Result : Result;
	}
    
    return 0;
    
    #undef toka
    #undef eka
}

/* Now this was quite interesting to implement. *
 * It sorts a unidirectionally linked list.     */
static void SortFiles(void)
{
	int a;
	StatTree **Tmp, *tmp;

	for(a=0, tmp=StatPuu; tmp; tmp=tmp->Next)a++;
	
	ALLOCATE(Tmp, StatTree **, (a+1) * sizeof(tmp));
	
	for(a=0, tmp=StatPuu; tmp; tmp=tmp->Next)Tmp[a++] = tmp;
	Tmp[a] = NULL;
	
	qsort(Tmp, a, sizeof(tmp), SubSort);
	
	StatPuu = Tmp[0];
	for(a=0; Tmp[a]; a++)Tmp[a]->Next = Tmp[a+1];
	
    free(Tmp);
}

static void DropOut(void)
{
    StatTree *tmp;
    
    EstimateFields();
    
    if(Sorting[0] && StatPuuKoko)SortFiles();
    
    if(Colors && StatPuuKoko && AnsiOpt)Gprintf("\r");
    
	for(Dumping=1; StatPuu; StatPuu=tmp->Next)
    {
        tmp=StatPuu;
        TellMe(&tmp->Stat, tmp->Name
        #ifdef DJGPP
        	, tmp->dosattr
        #endif
        	);
        free(tmp->Name);
        free(tmp);
    }
    
    StatPuu = NULL;
    StatLatva = &StatPuu;
    StatPuuKoko = 0;
    
    LongestName = 0;
    LongestSize = 0;
	
	Dumping=0;
}

static void SingleFile(const char *Buffer, struct stat *Stat)
{
    #ifndef S_ISLNK
    int Links=0; /* hack */
    #endif

    #ifdef DJGPP
    struct ffblk Bla;
    if(findfirst(Buffer, &Bla, 0x37))
    {
        if(Buffer[0]=='.')return;
        Gprintf("%s - findfirst() error: %d (%s)\n", Buffer, errno, GetError(errno));
    }
    #endif

    if((stat(Buffer, Stat) == -1) && !Links)
        Gprintf("%s - stat() error: %d (%s)\n", Buffer, errno, GetError(errno));
    #ifdef S_ISLNK
    else if((lstat(Buffer, Stat) == -1) && Links)
        Gprintf("%s - lstat() error: %d (%s)\n", Buffer, errno, GetError(errno));
    #endif
    else
    {
	    char OwNam[16];
    	char GrNam[16];
	    char *Group, *Passwd;
    	
        char *s = GetSize(Buffer, Stat, 0, 0);
        int i;

        i = GetName(Buffer, Stat, 0, 0);
        if(i > LongestName)LongestName = i;
        i = strlen(s);
        if(i > LongestSize)LongestSize = i;
        
        Passwd = Getpwuid((int)Stat->st_uid);
		Group  = Getgrgid((int)Stat->st_gid);
	    if(Passwd)strcpy(OwNam, Passwd);else sprintf(OwNam,"%d",(int)Stat->st_uid);
    	if( Group)strcpy(GrNam,  Group);else sprintf(GrNam,"%d",(int)Stat->st_gid);       
    	
    	i = strlen(OwNam); if(i > LongestUID)LongestUID = i;
    	i = strlen(GrNam); if(i > LongestGID)LongestGID = i;
        
        Puuhun(Stat, Buffer
        #ifdef DJGPP
        , Bla.ff_attrib
        #endif
        );
    }
}

static char LastDir[PATH_MAX+1] = "";
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
    	if(
	    #ifdef DJGPP
    		errno==EACCES ||
        #endif
        	errno==ENOENT ||	/* Piti lisätä linkkejä varten */
        	errno==ENOTDIR)
        {
P1:
	    	if(!LastDir[0])strcpy(LastDir, Source);

			SingleFile(Source, &Stat);
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
            Gprintf("\n%s - error: %d (%s)\n", Source, errno,GetError(errno));

        goto End_ScanDir;
    }    
    if(!Contents)
    {
        errno = 0;
        Tried = 1;
        goto P1;
    }

    if(strcmp(LastDir, Source))
    {
    	DropOut();
    	
        if(Totals)
        {
        	static int Eka=1;
	        GetDescrColor("txt", 1);
        	if(WhereX)Gprintf("\n");
        	if(!Eka)
        	{
        		Summat();
        		Gprintf("\n");
        	}
        	Eka=0;
			Gprintf(" Directory of %s\n", Source);
		}
		strcpy(LastDir, Source);
    }
    
    while((ent = readdir(dir)) != NULL)
    {
        char *Buffer;

        ALLOCATE(Buffer, char *, strlen(Source)+strlen(ent->d_name)+8);
        strcpy(Buffer, Source);

        if(Buffer[strlen(Buffer)-1] != '/')strcat(Buffer, "/");
        strcat(Buffer, ent->d_name);

       	SingleFile(Buffer, &Stat);

        free(Buffer);
    }

    if(closedir(dir) != 0)
        Gprintf("Trouble #%d (%s) at closedir() call.\n",
            errno, GetError(errno));

End_ScanDir:
    return;
}

#ifndef NO_STATFS
 #if defined(linux) || defined(DJGPP) || defined(__hpux)
  #include <sys/vfs.h>
 #else
  #include <sys/mount.h>
 #endif
#endif

static void Summat(void)
{
#ifndef NO_STATFS
	struct statfs tmp;
#endif
	char NumBuf[32];
	unsigned long Koko;
    
    Dumping=1;
    GetDescrColor("txt", 1);

    if(RowLen > 0)Gprintf("\n");
    if(!Totals)
    {
        if(Colors)Gprintf("\r \r"); /* Ensure color */
        return;
    }

    Koko = /* Grand total */
        Summa[SumDir]
      + Summa[SumFifo]
      + Summa[SumFile]
      + Summa[SumLink]
      + Summa[SumChrDev]
      + Summa[SumBlkDev];

    ColorNums = GetDescrColor("num", -1);
    
#ifndef NO_STATFS
    if(statfs(LastDir, &tmp))tmp.f_bavail = 0;
#endif
        	
    if(Compact)
    {
		unsigned long Tmp = SumCnt[SumChrDev] + SumCnt[SumBlkDev];
		unsigned long Tmp2= SumCnt[SumFifo]+SumCnt[SumSock]+SumCnt[SumLink];
		
        PrintNum(NumBuf, TotalSep, "%lu", SumCnt[SumDir]);
        Gprintf(" \1%s\1 dir%s%s", NumBuf,
#ifndef NO_STATFS
        	(tmp.f_bavail > 0 && Tmp)?"":
#endif        	
        	"ector",
        	SumCnt[SumDir]==1?"y":
#ifndef NO_STATFS
        	(tmp.f_bavail > 0 && Tmp)?"s":
#endif        	
        	"ies");
        		
        if(SumCnt[SumFile])
        {
        	PrintNum(NumBuf, TotalSep, "%lu", SumCnt[SumFile]);
        	Gprintf(", \1%s\1 file%s",
        		NumBuf,
        		SumCnt[SumFile]==1?"":"s");
       	}
        		
        if(Tmp)
        {
        	PrintNum(NumBuf, TotalSep, "%lu", Tmp);
        	Gprintf(", \1%s\1 device%s", NumBuf, Tmp==1?"":"s");
        }
        if(Tmp2)
        {
        	PrintNum(NumBuf, TotalSep, "%lu", Tmp2);
        	Gprintf(", \1%s\1 other%s", NumBuf, Tmp2==1?"":"s");
        }
        		
        PrintNum(NumBuf, TotalSep, "%lu", Koko);
        Gprintf(", \1%s\1 bytes", NumBuf);
        
#ifndef NO_STATFS
        if(tmp.f_bavail > 0)
        {
        	float Size = tmp.f_bavail / 1024.0 * tmp.f_bsize;
        	
        	if(Size >= 1024)
        	{
        		PrintNum(NumBuf, TotalSep, "%.1f", Size/1024.0);
        		Gprintf(", \1%s\1 M", NumBuf);
        	}
       		else 
       		{
       			PrintNum(NumBuf, TotalSep, "%.1f", Size);
        		Gprintf(", \1%s\1 k", NumBuf);
        	}
        		
        	Gprintf("B free(\1%.1f\1%%)",
        		(float)tmp.f_bavail * 100.0 / tmp.f_blocks);
        }
#endif	    
	    Gprintf("\n");
    }
    else
    {
		unsigned long Tmp = SumCnt[SumChrDev] + SumCnt[SumBlkDev];
		
        if(Tmp)
        {
        	PrintNum(NumBuf, TotalSep, "%lu", Tmp);
            Gprintf("\1%5s\1 device%s (", NumBuf, (Tmp==1)?"":"s");
    
            if(SumCnt[SumChrDev])
            {
            	PrintNum(NumBuf, TotalSep, "%lu", SumCnt[SumChrDev]);
            	Gprintf("\1%s\1 char", NumBuf);
            }
            if(SumCnt[SumChrDev]
            && SumCnt[SumBlkDev])Gprintf(", ");    
            if(SumCnt[SumBlkDev])
            {
            	PrintNum(NumBuf, TotalSep, "%lu", SumCnt[SumBlkDev]);
            	Gprintf("\1%s\1 block", NumBuf);
            }
            Gprintf(")\n");
        }

        if(SumCnt[SumDir])
        {
        	PrintNum(NumBuf,    TotalSep, "%lu", SumCnt[SumDir]);
        	PrintNum(NumBuf+16, TotalSep, "%lu", Summa[SumDir]);
            Gprintf("\1%5s\1 directories,\1%11s\1 bytes\n",
                NumBuf, NumBuf+16);
        }
    
        if(SumCnt[SumFifo])
        {
        	PrintNum(NumBuf,    TotalSep, "%lu", SumCnt[SumFifo]);
        	PrintNum(NumBuf+16, TotalSep, "%lu", Summa[SumFifo]);
            Gprintf("\1%5s\1 fifo%s\1%17s\1 bytes\n",
                NumBuf, (SumCnt[SumFifo]==1)?", ":"s,", NumBuf+16);
        }    
        if(SumCnt[SumFile])
        {
        	PrintNum(NumBuf,    TotalSep, "%lu", SumCnt[SumFile]);
        	PrintNum(NumBuf+16, TotalSep, "%lu", Summa[SumFile]);
            Gprintf("\1%5s\1 file%s\1%17s\1 bytes\n",
                NumBuf, (SumCnt[SumFile]==1)?", ":"s,", NumBuf+16);
    	}
        if(SumCnt[SumLink])
        {
        	PrintNum(NumBuf,    TotalSep, "%lu", SumCnt[SumLink]);
        	PrintNum(NumBuf+16, TotalSep, "%lu", Summa[SumLink]);
            Gprintf("\1%5s\1 link%s\1%17s\1 bytes\n",
                NumBuf, (SumCnt[SumLink]==1)?", ":"s,", NumBuf+16);
		}
		PrintNum(NumBuf, TotalSep, "%lu", Koko);
        Gprintf("Total\1%24s\1 bytes\n", NumBuf);
#ifndef NO_STATFS
        if(tmp.f_bavail > 0)
        {
        	double Size = (long double)tmp.f_bavail * tmp.f_bsize;
        	
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

    memset(&SumCnt, 0, sizeof SumCnt);
    memset(&Summa, 0, sizeof Summa);
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

void GetScreenGeometry(void)
{
#if defined(DJGPP) || defined(__BORLANDC__)
    struct text_info w;
    gettextinfo(&w);
    LINES=w.screenheight;
    COLS =w.screenwidth;
#else
#ifdef TIOCGWINSZ
    struct winsize w;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) >= 0)
    {
        LINES=w.ws_row;
        COLS =w.ws_col;
    }
#else
#ifdef WIOCGETD
    struct uwdata w;
    if(ioctl(STDOUT_FILENO, WIOCGETD, &w) >= 0)
    {
        LINES = w.uw_height / w.uw_vs;
        COLS  = w.uw_width / w.uw_hs;
    }
#endif
#endif
#endif
}

const char *GetNextArg(int argc, const char **argv)
{
    static char *DirrBuf = NULL;
    static char *DirrVar = NULL;
    static int Argc=1;
    const char *Temp = NULL;
    static int Ekastrtok;

    if(!DirrBuf && !DirrVar)
    {
        DirrVar = getenv("DIRR_COLORS");
        if(DirrVar)Settings = DirrVar;

        DirrVar = getenv("DIRR");
        if(DirrVar)
        {
        	ALLOCATE(DirrBuf, char *, strlen(DirrVar)+1);
            DirrVar = strcpy(DirrBuf, DirrVar);
            Ekastrtok=1;
    }   }

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

static void EstimateFields(void)
{
    char *s;
    int RowLen;
    
    if(PreScan)
    	LongestName++;
    else
    {
    	LongestSize = Sara?7:9;
    	LongestGID  = 8;
    	LongestUID  = 8;
   	}
    for(RowLen=0, s=FieldOrder; *s; )
    {
        switch(*s)
        {
            case '.':
                s++;
                switch(*s)
                {
                    case 'x':
                        s++;
                        GetHex(7, &s);
                        s--;
                        break;
                    case 'f':
                        break;
                    case 'S':
                        s++;
                        RowLen += 3;
                    case 's':
                        RowLen += LongestSize;
                        break;
                    case 'd':
                        if(!strcmp(DateForm, "%u"))
                            RowLen += 12;
                        else if(!strcmp(DateForm, "%z"))
                            RowLen += 5;
                        else
                        {
                            char Buf[64];
                            struct tm TM;
                            memset(&TM, 0, sizeof(TM));
                            strftime(Buf, sizeof Buf, DateForm, &TM);
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
    
    if(!PreScan || LongestName > RowLen)
    	LongestName = RowLen;
}

struct Puu
{
    int Index;
    char *Puu;
    struct Puu *Next;
};

int RememberParam(struct Puu **Dirs, const char *s)
{
    struct Puu *tmp2, *tmp;

    ALLOCATE(tmp2, struct Puu *, sizeof(struct Puu));
    ALLOCATE(tmp2->Puu, char *, strlen(s)+1);

    strcpy(tmp2->Puu, s);
    tmp2->Next = NULL;

    tmp = *Dirs;
    if(!tmp)
    {
        *Dirs = tmp2;
        return(tmp2->Index = 1);
    }

    while(tmp->Next)
    	tmp=tmp->Next;

    tmp->Next = tmp2;

    return(tmp2->Index = tmp->Index+1);
}

struct Puu *Dirpuu = NULL;
struct Puu *Argpuu = NULL;
struct Puu *Virhepuu = NULL;

static void DumpDirs(void)
{
    struct Puu *tmp;
    
    EstimateFields();    
    
    while(Dirpuu)
    {
        tmp=Dirpuu;
        ScanDir(tmp->Puu);
        Dirpuu = tmp->Next;
        free(tmp->Puu);
        free(tmp);
    }
    DropOut();
}

static void DumpVirheet(void)
{
    int Index;
    struct Puu *tmp, *tmp2;

    if(!Virhepuu)return;

    Gprintf("Invalid option%s\n", Virhepuu->Next?"s":"");

    for(Index=1; Argpuu; Index++)
    {
        int error;

        for(tmp2=Virhepuu; error=0, tmp2; tmp2=tmp2->Next)
        {
            error=strtol(tmp2->Puu, NULL, 10);
            if(error == Index)break;
        }

        tmp=Argpuu;
        Gprintf("%s%3d: `%s'\n", error?"-->":"\t", tmp->Index, tmp->Puu);
        Argpuu = tmp->Next;
        free(tmp->Puu);
        free(tmp);
    }
    exit(EXIT_FAILURE);
}

int main(int argc, const char **argv)
{
    int Args = 0;
    int Help = 0;

    const char *Arggi;

    #ifdef DJGPP
    if(!isatty(fileno(stdout)))Colors=0;
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

    while((Arggi = GetNextArg(argc, argv)) != NULL)
    {
        int ArgIndex = RememberParam(&Argpuu, Arggi);

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
                    	break;
                    case 'd':
                    	switch(*++s)
                    	{
                    		case '1': DateTime=1; break;
                    		case '2': DateTime=2; break;
                    		case '3': DateTime=3; break;
                    		case 'b':
                    			strncpy(BlkStr, s+1, (sizeof BlkStr)-1);
                    			goto ThisIsOk;
                    		case 'c':
                    			strncpy(ChrStr, s+1, (sizeof ChrStr)-1);
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
                    case 'h':
                    case '?': Help = 1; break;
                    case 'C': Sara = 1; break;
                    case 'D': Contents = 0; break;
                    case 'c': Colors = 0; break;
                    case 'p': Pagebreaks = 1; break;
                    case 'P': AnsiOpt = 0; break;
                    case 'e':
                    	PreScan = 0;
                    	Sorting[0] = 0;
                    	break;
                   	case 'o':
                   		strncpy(Sorting, s+1, (sizeof Sorting)-1);
                   		PreScan = 1;
                   		goto ThisIsOk;
                    case 'F':
                        strncpy(DateForm, s+1, (sizeof DateForm)-1);
                        goto ThisIsOk;
                    case 'f':
                        strncpy(FieldOrder, s+1, (sizeof FieldOrder)-1);
                        goto ThisIsOk;
                    case 'v':
                    case 'V':
                    	printf(VERSION);
                    	return 0;
                    case 'a':
                    	switch(*++s)
                    	{
                    		case '0':
                    			strcpy(FieldOrder, ".f_.s.d|");
                    			strcpy(DateForm,  "%z");
                            #ifdef S_ISLNK
                                Links = 1;
                            #endif
                                Sara = Compact = 1;
                    			break;
                    		case '1':
                    			strcpy(FieldOrder, ".xF|.a1.xF|.f.s.xF|.d.xF|.o_.g.xF|");
                    			break;
                    		case '2':
                            #ifdef S_ISLNK
                    			Links=0;
                            #endif
                    			Sara=1;
                    			strcpy(FieldOrder, ".f_.a4_.o_.g.xF|");
                    			break;
                    		case 't':
                    			strcpy(Sorting, "dn");
                    			PreScan=1;
                    		case 'l':
                    VipuAL:		strcpy(FieldOrder, ".a1_.h__.o_.g_.s_.d_.f");
                    			strcpy(DateForm, "%u");
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
                    			Totals = 1;
                    			break;
                    		case '1':
		                    	Compact = 1;
        		            	Totals = 1;
                    			break;
                    		case '2':
                    			Totals = 0;
                    			break;
                    		default:
                    			goto VirheParam;
                    	}
                    	if(TotalSep)s++;
                    	break;
                    case 'w':
                    case 'W':
                    	Compact = Totals = 1;
                        strcpy(FieldOrder, ".f");
                        strcpy(Sorting, isupper((int)*s)?"PCM":"pcm");
                    #ifdef S_ISLNK
                        Links = 1;
                    #endif
                        Sara = 1;
                        break;
                    default:
            VirheParam:	;
                    {
                        char Temp[6];
						sprintf(Temp, "%d", ArgIndex);
                        RememberParam(&Virhepuu, Temp);
                        goto ThisIsOk;
                    }
                }
                s++;
            }
ThisIsOk:;
        }
        else
        {
            RememberParam(&Dirpuu, Arggi);
            Args=1;
        }
    }

    DumpVirheet();

    if(Help)
    {
    	int p=Pagebreaks, a=AnsiOpt;
    	SetDefaultOptions();
    	Pagebreaks=p;
    	AnsiOpt=a;
    	Dumping=1;
    	
        GetDescrColor("txt", 1);
        Gprintf(
            #ifndef DJGPP
            "\r\33[K\r"
            #endif
            "Usage: %s [options] { dirs | files }\n\n"
            "Options:\n"
            "\t-c  Disables colors.\n"
            "\t-w  Short for -Cm1l1f.f -opcm\n"
            "\t-W  Same as -w, but reverse sort order\n"
            "\t-a0 Short for -wf.f_.s.d| -F%%z\n"
            "\t-a1 Short for -f.xF|.a1.xF|.f.s.xF|.d.xF|.o_.g.xF|\n"
            "\t-a2 Short for -l0Cf.f_.a4_.o_.g.xF|\n"
            "\t-al Short for -f.a1_.h__.o_.g_.s_.d_.f -F%%u\n"
            "\t-at Short for -alodn\n"
            "\t-m0 Selects verbose \"total\" list. This is the default.\n"
            "\t-m1 Compactifies the \"total\" list.\n"
            "\t-m2 Disables total sums.\n"
            "\t    Instead of -mx you can use -Mxn and specify n\n"
            "\t    as the thousand separator in total sums.\n"
            "\t-p  Enables page pauses.\n"
            "\t-P  Disables the vt100 code optimization.\n"
            "\t-D  Show directory names instead of contents.\n"
            "\t-e  Quick list (uses space estimation). Disables sorting.\n"
            "\t-r  Undoes all options, including the DIRR environment variable.\n"
            "\t-on Sort the list (disables -e), with n as combination of:\n"
            "\t    (n)ame, (s)ize, (d)ate, (u)id, (g)id,\n"
            "\t    (c)olor, na(m)e case insensitively\n"
            "\t    g(r)oup dirs,files,links,chrdevs,blkdevs,fifos,socks\n"
            "\t    grou(p) dirs,links=files,chrdevs,blkdevs,fifos,socks\n"
            "\t    Use Uppercase for reverse order.\n"
            "\t    Default is `-o%s'\n"
           	"\t-X# Force screen width\n"
            "\t-C  Enables multicolumn mode.\n"
            "\t-d1 Use atime, last access datetime (disables -d2 and -d3)\n"
            "\t-d2 Use mtime, last modification datetime (default, disables -d1 and -d3)\n"
            "\t-d3 Use ctime, creation datetime (disables -d1 and -d2)\n"
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
            argv[0],
            Sorting,
            FieldOrder,
            DateForm);
                    Gprintf(
            "\t-dbx Specify how the blockdevices are shown\n"
            "\t     Example: `-db<B%%u,%%u>'\n"
            "\t     Default is `-db%s'\n"
            "\t-dcx Specify how the character devices are shown\n"
            "\t     Example: `-dc<C%%u,%%u>'\n"
            "\t     Default is `-dc%s'\n", BlkStr, ChrStr);
#ifdef S_ISLNK
                    Gprintf(
            "\t-ln Specify how the links are shown\n"
            "\t  0 Show link name and stats of target\n"
            "\t  1 Show link name and <LINK>\n"
            "\t  2 Show link name, link's target and stats of link\n"
            "\t  3 Show link name, link's target and stats of target\n"
            "\t  4 Show link name, link's target and <LINK>\n"
#else
					Gprintf(
			"\n-l# Does nothing in this environment.\n"
#endif
            "You can set environment variable 'DIRR' for the options.\n"
            "You can also use 'DIRR_COLORS' -variable for color settings.\n"
            "Current DIRR_COLORS:\n"
        );

        PrintSettings();

        return 0;
    }

    if(!Args)
        RememberParam(&Dirpuu, ".");

	ReadGidUid();
	
	Dumping=1;
    DumpDirs();

    Summat();

    return 0;
}
