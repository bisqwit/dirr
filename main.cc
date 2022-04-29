/*

    Unix directorylister (replacement for ls command)
    Copyright (C) 1992,2000 Bisqwit (http://iki.fi/bisqwit/)

*/

static const char VERSIONSTR[] =
    "DIRR %s copyright (C) 1992,2021 Bisqwit (http://iki.fi/bisqwit/)\n"
    "License: GPL. Source code: https://iki.fi/bisqwit/source/dirr.html\n";

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <cerrno>
#include <csignal>

#include "config.h"
#include "pwfun.hh"
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
#include <list>

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <unistd.h>

#ifdef HAVE_READDIR_DIR_H
# include <dir.h>
#endif
#ifdef HAVE_READDIR_DIRENT_H
# include <dirent.h>
#endif
#ifdef HAVE_READDIR_DIRECT_H
# include <direct.h>
#endif

#include "stat.h"

static int RowLen;

struct Estimation
{
    std::size_t Name{0};
    std::size_t Size{0}, SizeWithSeps{0}, SizeCompact{0};
    std::size_t UIDname{0}, UIDnumber{0};
    std::size_t GIDname{0}, GIDnumber{0};
    std::size_t Links{0};
};
static std::vector<Estimation> Longest; // Per-column limits

static bool ShowDotFiles = ALWAYS_SHOW_DOTFILES;
static bool Contents, PreScan, MultiColumn, VerticalColumns;
static unsigned CurrentColumn;
static int DateTime, MyUid=-1, MyGid=-1;

static std::string Sorting; /* n,d,s,u,g */
static std::string DateForm;
static std::string FieldOrder;

struct FieldInfo
{
    enum: unsigned {
        literal,      // info = which character to print (00-FF)
        attribute,    // info = attribute mode (0..9) -- 0=DOS, 1=-rw-r--r--, 2..9 = number of octal digits (at least 3)
        color,        // info = which color to set (00-FF)
        user_id,      // UID number (info: 1=fit, 0=don't fit)
        user_name,    // UID name   (info: 1=fit, 0=don't fit)
        group_id,     // GID number (info: 1=fit, 0=don't fit)
        group_name,   // GID name   (info: 1=fit, 0=don't fit)
        nrlinks,      // number of links    (info: 1=fit, 0=don't fit)
        name,         // filename           (info: 1=fit, 0=don't fit)
        size,         // filesize           (info: 1=fit, 0=don't fit)
        size_compact, // filesize compactly (info: 1=fit, 0=don't fit)
        size_sep,     // filesize with thousands-separator. info = which separator (00-7F) (&0x80 = fit)
        datetime,     // datetime, uses DateTime and DateForm
        num_different_fields
    } type; //
    unsigned info;
};
static void SetDefaultOptions()
{
    PreScan     = true; // Clear with -e
    MultiColumn = false;// Set with -C
    VerticalColumns = false; // Set with -vc
    Compact = 0;    // Set with -m
    #ifdef S_ISLNK
    Links   = 3;    // Modify with -l#
    #endif
    Colors  = isatty(1); // Clear with -c, set with -c1
    Contents= true; // Clear with -D
    DateTime= 2;    // Modify with -d#
    Totals  = true; // Modify with -m
    Pagebreaks = false; // Set with -p
    AnsiOpt = true; // Clear with -P
    ShowDotFiles = ALWAYS_SHOW_DOTFILES; // Set with -A

    TotalSep= 0;    // Modify with -Mx

    Sorting = "pmgU";
    DateForm = "%d.%m.%y_%H:%M";
                    // Modify with -F

                    // Modify with -f
    #ifdef DJGPP
    FieldOrder = ".f_.s_.d";
    #else
    FieldOrder = ".f_.s_.a4_.d_.o_.g";
    #endif

    BlkStr = "<B%u,%u>"; // Modify with -db
    ChrStr = "<C%u,%u>"; // Modify with -dc
}

struct FieldsToPrint: public std::vector<FieldInfo> // ParsedFieldOrder
{
    bool used[FieldInfo::num_different_fields]{};
    bool need_column_separator = false;

    void ParseFrom(const std::string& s)
    {
        unsigned char work;
        for(std::size_t a=0, b=s.size(); a<b; ++a)
            switch(s[a])
            {
                case '.':
                    switch(s[++a])
                    {
                        case '\0': emplace_back(FieldInfo{FieldInfo::literal, '.'}); break;
                        case 'a':
                            if(s[a+1] >= '0' && s[a+1] <= '9')
                                emplace_back(FieldInfo{FieldInfo::attribute, (unsigned char) s[++a]});
                            else
                                emplace_back(FieldInfo{FieldInfo::attribute, '1'});
                            break;
                        case 'x':
                            #define x(c) (((c)>='0'&&(c)<='9') ? ((c)-'0') : ((c)>='A'&&(c)<='F') ? ((c)-'A'+10) : ((c)-'a'+10))
                            work=0;
                            if(std::isxdigit(s[a+1])) { ++a; work = work*16 + x(s[a]);
                            if(std::isxdigit(s[a+1])) { ++a; work = work*16 + x(s[a]); }}
                            else work = DEFAULTATTR;
                            emplace_back(FieldInfo{FieldInfo::color, work});
                            break;
                        case 'o': emplace_back(FieldInfo{FieldInfo::user_name,1}); break;
                        case 'O': emplace_back(FieldInfo{FieldInfo::user_name,0}); break;
                        case 'u': if(s[a+1]=='i' && s[a+2]=='d')
                                  { a+=2; emplace_back(FieldInfo{FieldInfo::user_id,1}); break; }
                                  goto dfl_opt;
                        case 'g': if(s[a+1]=='i' && s[a+2]=='d')
                                  { a+=2; emplace_back(FieldInfo{FieldInfo::group_id,1}); break; }
                                  emplace_back(FieldInfo{FieldInfo::group_name,1}); break;
                        case 'G': emplace_back(FieldInfo{FieldInfo::group_name,0}); break;
                        case 'h': emplace_back(FieldInfo{FieldInfo::nrlinks,1}); break;
                        case 'f': emplace_back(FieldInfo{FieldInfo::name,   1}); break;
                        case 'F': emplace_back(FieldInfo{FieldInfo::name,   0}); break;
                        case 's': emplace_back(FieldInfo{FieldInfo::size,   1}); break;
                        case 'z': emplace_back(FieldInfo{FieldInfo::size_compact, 1}); break;
                        case 'S': emplace_back(FieldInfo{FieldInfo::size_sep,     (unsigned char)(0x80 | (s[a+1] ? s[a+1] == '_' ? ' ' : s[++a] : ' '))}); break;
                        case 'd': emplace_back(FieldInfo{FieldInfo::datetime,     1}); break;
                        default: dfl_opt: emplace_back(FieldInfo{FieldInfo::literal, (unsigned char)s[a]});
                    }
                    break;
                case '_':
                    emplace_back(FieldInfo{FieldInfo::literal, ' '});
                    break;
                default:
                    emplace_back(FieldInfo{FieldInfo::literal, (unsigned char)s[a]});
            }
        // If the last item is a "fit" item, convert into a non-fit item
        if(!MultiColumn)
            for(auto i = rbegin(); i != rend(); ++i)
            {
                if(i->type == FieldInfo::attribute) break;
                if(i->type == FieldInfo::literal) break;
                if(i->type == FieldInfo::color) continue; // nonprinting
                if(i->type == FieldInfo::datetime) break; // always no-fitting
                if(i->type == FieldInfo::size_sep
                || i->type == FieldInfo::size) break; // always right-aligned

                if(i->type == FieldInfo::size_sep)
                    i->info &= ~0x80;
                else
                    i->info &= ~0x01;
            }
        // Collect info for which field types are needed for width calculation
        for(bool& c: used) c = false;
        for(auto i = begin(); i != end(); ++i)
        {
            //if(i->type == FieldInfo::name)
                used[i->type] = true;
            /*else
                used[i->type] = ((i->type == FieldInfo::size_sep) ? (i->info & 0x80) : (i->info & 0x01);*/
        }
        // Don't need column separator (space), if the last non-color item is a literal
        need_column_separator = true;
        for(auto i = rbegin(); i != rend(); ++i)
        {
            if(i->type == FieldInfo::color) continue;
            if(i->type == FieldInfo::literal)
                need_column_separator = false;
            break;
        }
    }
} FieldsToPrint;


#include <unordered_map>
static class Inodemap
{
    std::unordered_map<dev_t, std::unordered_map<ino_t, std::string>> items;
    bool enabled;
public:
    Inodemap() : items(), enabled(true) { }
    void disable()
    {
        enabled = false;
        items.clear();
    }
    void enable()
    {
        enabled = true;
    }
    void insert(dev_t dev, ino_t ino, const std::string &name)
    {
        if(enabled)
        {
            auto i = items.find(dev);
            if(i == items.end())
                items.emplace(dev, std::unordered_map<ino_t,string>{{ino,name}} );
            else
            {
                auto j = i->second.find(ino);
                if(j == i->second.end())
                    i->second.emplace(ino, name);
            }
        }
    }
    const char *get(dev_t dev, ino_t ino) const
    {
        auto i = items.find(dev);
        if(i != items.end())
        {
            auto j = i->second.find(ino);
            if(j != i->second.end())
                return j->second.c_str();
        }
        return nullptr;
    }
} Inodemap;

static void TellMe(const StatType &Stat, std::string&& Name
#ifdef DJGPP
    , unsigned int dosattr
#endif
    )
{
    std::string OwNum,GrNum,OwNam,GrNam;
    std::size_t ItemLen = 0;

    if(true) // scope for updating totals
    {
        SizeType size = Stat.st_size;
        int category  = SumFile;

        if(S_ISDIR(Stat.st_mode)) { category = SumDir; }
        #ifdef S_ISFIFO
        else if(S_ISFIFO(Stat.st_mode)) { category = SumFifo; }
        #endif
        #ifdef S_ISSOCK
        else if(S_ISSOCK(Stat.st_mode)) { category = SumSock; }
        #endif
        else if(S_ISCHR(Stat.st_mode)) { category = SumChrDev; size = 0; }
        else if(S_ISBLK(Stat.st_mode)) { category = SumBlkDev; size = 0; }
        #ifdef S_ISLNK
        else if(S_ISLNK(Stat.st_mode)) { category = SumLink; }
        #endif
        SumCnt[category]   += 1;
        SumSizes[category] += size;
    }

    auto& Limits = CurrentColumn < Longest.size() ? Longest[CurrentColumn] : Longest.back();

    for(const auto& f: FieldsToPrint)
    {
        //fprintf(stderr, "ItemLen=%zu\n", ItemLen);
        switch(f.type)
        {
            case FieldInfo::literal:
            {
                Gputch(f.info);
                ++ItemLen;
                break;
            }
            case FieldInfo::color:
            {
                SetAttr(f.info);
                break;
            }
            case FieldInfo::attribute:
            {
                ItemLen += PrintAttr(Stat, f.info
                #ifdef DJGPP
                                    , dosattr
                #endif
                                    );
                break;
            }
            case FieldInfo::user_name:
            {
                if(MyUid<0)MyUid=getuid();
                GetDescrColor(ColorDescr::OWNER, ((int)Stat.st_uid==MyUid)?1:2);
                if(OwNam.empty())
                {
                    OwNam = Getpwuid((int)Stat.st_uid);
                    if(OwNam.empty()) { if(OwNum.empty()) OwNum = std::to_string(Stat.st_uid); OwNam = OwNum; }
                }
                ItemLen += (f.info ? Gwrite(OwNam, Limits.UIDname) : Gwrite(OwNam));
                break;
            }
            case FieldInfo::user_id:
            {
                if(MyUid<0) MyUid=getuid();
                GetDescrColor(ColorDescr::OWNER, ((int)Stat.st_uid==MyUid)?1:2);
                if(OwNum.empty()) OwNum = std::to_string(Stat.st_uid);
                ItemLen += (f.info ? Gwrite(OwNum, Limits.UIDnumber) : Gwrite(OwNum));
                break;
            }
            case FieldInfo::group_name:
            {
                if(MyGid<0) MyGid=getgid();
                GetDescrColor(ColorDescr::GROUP, ((int)Stat.st_gid==MyGid)?1:2);
                if(GrNam.empty())
                {
                    GrNam = Getgrgid((int)Stat.st_gid);
                    if(GrNam.empty()) { if(GrNum.empty()) GrNum = std::to_string(Stat.st_gid); GrNam = GrNum; }
                }
                ItemLen += (f.info ? Gwrite(GrNam, Limits.GIDname) : Gwrite(GrNam));
                break;
            }
            case FieldInfo::group_id:
            {
                if(MyGid<0) MyGid=getgid();
                GetDescrColor(ColorDescr::GROUP, ((int)Stat.st_gid==MyGid)?1:2);
                if(GrNum.empty()) GrNum = std::to_string(Stat.st_gid);
                ItemLen += (f.info ? Gwrite(GrNum, Limits.GIDnumber) : Gwrite(GrNum));
                break;
            }
            case FieldInfo::nrlinks:
            {
                GetDescrColor(ColorDescr::NRLINK, 1);
                ItemLen += (f.info ? Gwrite(std::to_string(Stat.st_nlink), Limits.Links) : Gwrite(std::to_string(Stat.st_nlink)));
                break;
            }
            case FieldInfo::name:
            {
                SetAttr( GetNameAttr(Stat, NameOnly(Name)) );

                const char *hardlinkfn = Inodemap.get(Stat.st_dev, Stat.st_ino);
                if(hardlinkfn && Name == hardlinkfn) hardlinkfn = nullptr;
                // Undocumented feature: If fitting is disabled, long pathful filenames are printed
                bool nameonly = f.info;
                if(!nameonly && Name.rfind('/')==1 && Name.substr(0,2)=="./") nameonly = true;
                auto i = GetName(Name, Stat, Limits.Name, MultiColumn || f.info, nameonly, hardlinkfn);
                ItemLen += std::max(std::size_t(i), Limits.Name);
                break;
            }
            case FieldInfo::size:
            {
                ItemLen += Gwrite(GetSize(Name, Stat, f.info ? Limits.Size : 0, 0));
                break;
            }
            case FieldInfo::size_compact:
            {
                ItemLen += Gwrite(GetSize(Name, Stat, f.info ? Limits.SizeCompact : 0, -1));
                break;
            }
            case FieldInfo::size_sep:
            {
                ItemLen += Gwrite(GetSize(Name, Stat, (f.info & 0x80) ? Limits.SizeWithSeps : 0, char(f.info & 0x7F)));
                break;
            }
            case FieldInfo::datetime:
            {
                std::string str;

                time_t t = Stat.st_mtime;
                switch(DateTime)
                {
                    case 1: t = Stat.st_atime; break;
                    //case 2: t = Stat.st_mtime; break;
                    case 3: t = Stat.st_ctime; break;
                }

                if(DateForm == "%u")
                {
                    char Buf[64];
                    time_t now = time(NULL);
                    strcpy(Buf, ctime(&t));

                    str = Buf+4;
                    if(!str.empty() && str.back()=='\n') str.erase(str.size()-1);

                    if(now > t + 6L * 30L * 24L * 3600L /* Old. */
                    || now < t - 3600L)       /* In the future. */
                    {
                        /* 6 months in past, one hour in future */
                        str.erase(7, 8); // delete time
                    }
                    if(str.size() > 12) str.erase(12);
                }
                else if(DateForm == "%z")
                {
                    struct tm *TM = localtime(&t);
                    time_t now = time(NULL);
                    int m = TM->tm_mon, d=TM->tm_mday, y=TM->tm_year;
                    struct tm *NOW = localtime(&now);
                    if(NOW->tm_year == y || (y==NOW->tm_year-1 && m>NOW->tm_mon))
                    {
                        str = "%3d.%d"_f % d % (m+1); // 5 characters
                        if(str.size() > 5) str.erase(0,1);
                    }
                    else
                        str = "%5d"_f % (y+1900);    // 5 characters
                }
                else
                {
                    char Buf[64];
                    strftime(Buf, sizeof Buf, DateForm.c_str(), localtime(&t));
                    str = Buf;
                }
                for(char& c: str) if(c=='_') c = ' ';
                GetDescrColor(ColorDescr::DATE, 1);
                ItemLen += Gwrite(str);
                break;
            }
            case FieldInfo::num_different_fields:;
        }
    }

    if(!FieldsToPrint.empty())
    {
        ++CurrentColumn;
        //fprintf(stderr, "ItemLen=%zu, RowLen becomes %zu\n", ItemLen, RowLen+ItemLen);
        RowLen += ItemLen;
        // Make a newline if the _next_ item of the same width would not fit
        if(!MultiColumn || (!VerticalColumns && int(RowLen + ItemLen) >= int(COLS)))
        {
            Gputch('\n');
            RowLen = 0;
            CurrentColumn = 0;
        }
        else if(FieldsToPrint.need_column_separator)
        {
            Gputch(' ');
            RowLen++;
        }
    }
}

class StatItem
{
public:
    StatType Stat;
    #ifdef DJGPP
    unsigned dosattr;
    #endif
    std::string Name;
public:
    StatItem(const StatType &t,
    #ifdef DJGPP
             unsigned da,
    #endif
             std::string&& n) : Stat(t),
    #ifdef DJGPP
                                dosattr(da),
    #endif
                                Name(std::move(n)) { }

    StatItem(StatItem&&) = default;
    StatItem(const StatItem&) = default;
    StatItem& operator=(StatItem&&) = default;
    StatItem& operator=(const StatItem&) = default;

    /* Returns the class code for grouping sort */
    int Class(int LinksAreFiles) const
    {
        if(S_ISDIR(Stat.st_mode)) return 0;
        #ifdef S_ISLNK
        if(S_ISLNK(Stat.st_mode)) return 2-LinksAreFiles;
        #else
        LinksAreFiles = LinksAreFiles; /* Not used */
        #endif
        if(S_ISCHR(Stat.st_mode)) return 3;
        if(S_ISBLK(Stat.st_mode)) return 4;
        #ifdef S_ISFIFO
        if(S_ISFIFO(Stat.st_mode)) return 5;
        #endif
        #ifdef S_ISSOCK
        if(S_ISSOCK(Stat.st_mode)) return 6;
        #endif
        return 1;
    }

    bool operator< (const StatItem &other) const
    {
        const class StatItem &me = *this;
        for(char c: Sorting)
        {
            SizeType Result=0;
            switch(c)
            {
                case 'c': Result = GetNameAttr(me.Stat, NameOnly(me.Name)) - GetNameAttr(other.Stat, NameOnly(other.Name)); break;
                case 'C': Result = GetNameAttr(other.Stat, NameOnly(other.Name)) - GetNameAttr(me.Stat, NameOnly(me.Name)); break;
                case 'e': Result = me.Name.size() - other.Name.size(); break;
                case 'E': Result = other.Name.size() - me.Name.size(); break;
                case 'n': Result = strcmp(me.Name.c_str(), other.Name.c_str()); break;
                case 'N': Result = strcmp(other.Name.c_str(), me.Name.c_str()); break;
                case 'm': Result = strcasecmp(me.Name.c_str(), other.Name.c_str()); break;
                case 'M': Result = strcasecmp(other.Name.c_str(), me.Name.c_str()); break;
                case 's': Result = me.Stat.st_size - other.Stat.st_size; break;
                case 'S': Result = other.Stat.st_size - me.Stat.st_size; break;
                case 'd':
                    switch(DateTime)
                    {
                        case 1: Result = me.Stat.st_atime-other.Stat.st_atime; break;
                        case 2: Result = me.Stat.st_mtime-other.Stat.st_mtime; break;
                        case 3: Result = me.Stat.st_ctime-other.Stat.st_ctime;
                    }
                    break;
                case 'D':
                    switch(DateTime)
                    {
                        case 1: Result = other.Stat.st_atime-me.Stat.st_atime; break;
                        case 2: Result = other.Stat.st_mtime-me.Stat.st_mtime; break;
                        case 3: Result = other.Stat.st_ctime-me.Stat.st_ctime;
                    }
                    break;
                case 'u': Result = (int)me.Stat.st_uid - (int)other.Stat.st_uid; break;
                case 'U': Result = (int)other.Stat.st_uid - (int)me.Stat.st_uid; break;
                case 'g': Result = (int)me.Stat.st_gid - (int)other.Stat.st_gid; break;
                case 'G': Result = (int)other.Stat.st_gid - (int)me.Stat.st_gid; break;
                case 'h': Result = (int)me.Stat.st_nlink - (int)other.Stat.st_nlink; break;
                case 'H': Result = (int)other.Stat.st_nlink - (int)me.Stat.st_nlink; break;
                case 'r': Result = me.Class(0) - other.Class(0); break;
                case 'R': Result = other.Class(0) - me.Class(0); break;
                case 'p': Result = me.Class(1) - other.Class(1); break;
                case 'P': Result = other.Class(1) - me.Class(1); break;
                default:
                {
                    const char *t = Sorting.c_str();
                    SetAttr(GetDescrColor(ColorDescr::TEXT, 1));
                    Gprintf("\nError: `-o");
                    while(*t != c)Gputch(*t++);
                    GetModeColor(ColorMode::INFO, '?');
                    Gputch(*t++);
                    SetAttr(GetDescrColor(ColorDescr::TEXT, 1));
                    Gprintf("%s'\n\n", t);
                    return (Sorting = ""), 0;
                }
            }
            if(Result) return Result < 0;
        }
        return false;
    }
};

static std::vector<StatItem> CollectedFilesForCurrentDirectory;

static std::size_t CalculateRowWidth(const Estimation& estimation, bool file_too)
{
    std::size_t RowLen = 0;
    for(const auto& f: FieldsToPrint)
        switch(f.type)
        {
            case FieldInfo::literal: ++RowLen; break;
            case FieldInfo::color: break;
            case FieldInfo::attribute:
                if(f.info == '1') RowLen += 10;
                #ifdef DJGPP
                else if(f.info == '0') RowLen += 4;
                #endif
                else RowLen += f.info - '0';
                break;
            case FieldInfo::nrlinks: RowLen += estimation.Links; break;
            case FieldInfo::name:
                if(file_too) RowLen += estimation.Name;
                break; // Not estimating
            case FieldInfo::size: RowLen += estimation.Size; break;
            case FieldInfo::size_compact: RowLen += estimation.SizeCompact; break;
            case FieldInfo::size_sep: RowLen += estimation.SizeWithSeps; break;
            case FieldInfo::datetime:
            {
                if(DateForm == "%u")
                    RowLen += 12;
                else if(DateForm == "%z")
                    RowLen += 5;
                else
                {
                    char Buf[64];
                    struct tm TM{};
                    strftime(Buf, sizeof Buf, DateForm.c_str(), &TM);
                    RowLen += strlen(Buf);
                }
                break;
            }
            case FieldInfo::user_id:    RowLen += estimation.UIDnumber; break;
            case FieldInfo::user_name:  RowLen += estimation.UIDname; break;
            case FieldInfo::group_id:   RowLen += estimation.GIDnumber; break;
            case FieldInfo::group_name: RowLen += estimation.GIDname; break;
            case FieldInfo::num_different_fields:;
        }
    if(file_too && FieldsToPrint.need_column_separator) ++RowLen;
    return RowLen;
}

static void EstimateFields()
{
    if(!PreScan)
    {
        // If pre-scanning was skipped with the -e option,
        // we don't have real information about the widths
        // of various fields. Make reasonable guesses.
        Longest.resize(1);
        for(auto& Limits: Longest)
        {
            Limits.Size         = MultiColumn?7:9;
            Limits.SizeWithSeps = Limits.Size+3;
            Limits.SizeCompact  = MultiColumn?5:6;
            Limits.GIDname  = 8; Limits.GIDnumber = 5;
            Limits.UIDname  = 8; Limits.UIDnumber = 5;
            Limits.Links    = 3;
        }
    }

    // Calculate the width of the string to be displayed.
    auto& Limits = Longest.front();
    std::size_t RowLen = CalculateRowWidth(Limits, false);

    // Calculate the room that there is for a filename
    int room = MultiColumn ? COLS/2 : COLS;
    room -= 2;
    room -= RowLen;
    if(room < 0) room = 0;

    if(!PreScan && !Limits.Name)
        Limits.Name = std::min(13, COLS/2);

    // If there is no room for the Limits name that we've found,
    // cut the name to the available room
    if(int(Limits.Name) > room)
        Limits.Name = room;
}

static void ResetEstimations()
{
    Longest.clear();
    Longest.resize(1);

    // In no-prescan mode (-e), we must begin with an initial estimate.
    if(!PreScan) EstimateFields();
}

static void UpdateEstimations(Estimation& Limits, const std::string& name, const StatType& Stat)
{
    if(!S_ISDIR(Stat.st_mode)) Inodemap.insert(Stat.st_dev, Stat.st_ino, name);

    if(FieldsToPrint.used[FieldInfo::name])
    {
        const char *hardlinkfn = Inodemap.get(Stat.st_dev, Stat.st_ino);
        if(hardlinkfn && name == hardlinkfn) hardlinkfn = nullptr;
        Limits.Name = std::max(Limits.Name, std::size_t(GetName(name, Stat, 0, false, true, hardlinkfn)));
    }

    if(FieldsToPrint.used[FieldInfo::size])
        Limits.Size = std::max(Limits.Size, GetSize(name, Stat, 0, 0).size());

    if(FieldsToPrint.used[FieldInfo::size_compact])
        Limits.SizeCompact  = std::max(Limits.SizeCompact,  GetSize(name, Stat, 0, -1).size());

    if(FieldsToPrint.used[FieldInfo::size_sep])
        Limits.SizeWithSeps = std::max(Limits.SizeWithSeps, GetSize(name, Stat, 0, '\'').size());

    if(FieldsToPrint.used[FieldInfo::nrlinks])
        Limits.Links = std::max(Limits.Links, std::to_string(Stat.st_nlink).size());

    if(FieldsToPrint.used[FieldInfo::user_id] || FieldsToPrint.used[FieldInfo::user_name])
    {
        std::string Passwd = Getpwuid(Stat.st_uid);
        std::string OwNum = std::to_string(Stat.st_uid);
        Limits.UIDname   = std::max(Limits.UIDname, Passwd.empty() ? OwNum.size() : Passwd.size());
        Limits.UIDnumber = std::max(Limits.UIDnumber, OwNum.size());
    }
    if(FieldsToPrint.used[FieldInfo::group_id] || FieldsToPrint.used[FieldInfo::group_name])
    {
        std::string Group = Getgrgid(Stat.st_gid);
        std::string GrNum = std::to_string(Stat.st_gid);
        Limits.GIDname   = std::max(Limits.GIDname, Group.empty() ? GrNum.size() : Group.size());
        Limits.GIDnumber = std::max(Limits.GIDnumber, GrNum.size());
    }
}

static void PrintAllFilesCollectedSoFar()
{
    auto& f = CollectedFilesForCurrentDirectory;

    if(!Sorting.empty())
        std::sort(f.begin(), f.end());

    EstimateFields();

    if(Colors && !f.empty() && AnsiOpt) Gprintf("\r");
    CurrentColumn = 0;

    if(VerticalColumns && MultiColumn)
    {
        // Check how many columns we can get away with.
        // Try to get the shortest number of lines.

        // Using binary search, find the shortest number of lines
        // where printing still works.
        unsigned columns = 0;
        auto acceptable = [&f,&columns](unsigned lines)
        {
            Longest.clear();
            unsigned column = 1, line = 0, total_width = 0;
            //printf("Trying %u lines\n", lines);
            for(unsigned a=0; a<f.size(); ++a)
            {
                if(line == 0) Longest.emplace_back(); // add column
                UpdateEstimations(Longest.back(), f[a].Name, f[a].Stat);
                unsigned width = CalculateRowWidth(Longest.back(), true);
                //printf("item %u on line %u column %u: column width now %u+%u\n", a, line,column, total_width, width);
                if(total_width + width >= unsigned(COLS-1) && column > 1)
                {
                    // This number of columns does not work.
                    //printf("%u lines does not work at %u columns\n", lines,columns);
                    return false;
                }
                if(++line >= lines)
                {
                    total_width += width;
                    line = 0;
                    ++column;
                }
            }
            //printf("%u lines works at %u columns\n", lines,column);
            columns = column;
            return true;
        };

        unsigned minimum_lines = 1;
        unsigned maximum_lines = f.size();
        while(minimum_lines < maximum_lines)
        {
            unsigned middle = (minimum_lines + maximum_lines) / 2;
            bool accept = acceptable(middle);
            if(!accept) minimum_lines = middle+1;
            else        maximum_lines = middle;
        }
        unsigned lines = minimum_lines;
        acceptable(lines); // Repopulate "columns" and "longest[]"

        // Okay, this number works!
        //printf("Successful choice at %u lines, %u columns\n", lines, columns);
        Dumping = true;
        RowLen=0;
        for(unsigned line=0; line<lines; ++line)
        {
            for(unsigned c=0; c<columns; ++c)
            {
                if(line + c*lines >= f.size()) break;
                StatItem& tmp = f[line + c*lines];
                TellMe(tmp.Stat, std::move(tmp.Name)
                #ifdef DJGPP
                       , tmp.dosattr
                #endif
                      );
            }
            if(CurrentColumn) { Gputch('\n'); RowLen=0; CurrentColumn=0; }
        }
        Dumping = false;
    }
    else
    {
        for(StatItem& tmp: f)
            UpdateEstimations(Longest.front(), tmp.Name, tmp.Stat);
        EstimateFields(); // Make sure the file name remains clipped

        RowLen=0;
        Dumping = true;
        for(StatItem& tmp: f)
        {
            TellMe(tmp.Stat, std::move(tmp.Name)
            #ifdef DJGPP
                   , tmp.dosattr
            #endif
                  );
        }
        Dumping = false;
    }

    f.clear();
}

static void SingleFile(string&& Buffer)
{
    #ifndef S_ISLNK
    int Links=0;
    #endif

    #ifdef DJGPP
    struct ffblk Bla;
    if(findfirst(Buffer.c_str(), &Bla, 0x37))
    {
        if(Buffer[0]=='.')return;
        Gprintf("%s - findfirst() error: %d (%s)\n", Buffer.c_str(), errno, GetError(errno).c_str());
    }
    #endif

    StatType Stat;
    #ifdef S_ISLNK
    if(Links ? (LStatFunc(Buffer.c_str(), &Stat) == -1)
             : (StatFunc(Buffer.c_str(), &Stat) == -1))
        Gprintf("%s: %s (%d)\n", Buffer, GetError(errno), errno);
    #else
    if(LStatFunc(Buffer.c_str(), &Stat) == -1))
        Gprintf("%s: %s (%d)\n", Buffer, GetError(errno), errno);
    #endif
    else
    {
        if(PreScan)
        {
            CollectedFilesForCurrentDirectory.emplace_back(
                Stat,
                #ifdef DJGPP
                Bla.ff_attrib,
                #endif
                std::move(Buffer));
        }
        else
        {
            Dumping = true;
            UpdateEstimations(Longest.front(), Buffer, Stat);
            TellMe(Stat, std::move(Buffer)
                   #ifdef DJGPP
                   , attr
                   #endif
                  );
            Dumping = false;
        }
    }
}

static void DirChangeCheck(std::string_view Source)
{
    std::size_t ss = Source.size();
    // Remove trailing /./ and /
    // Do not remove singular / though.
    while(ss > 1 && Source[ss-1] == '/')
    {
        if(ss >= 3 && Source[ss-3] == '/' && Source[ss-2] == '.')
            ss -= 2; // Change dir/./ into dir/
        else
            ss -= 1; // Change dir/   into dir
    }
    Source.remove_suffix(Source.size() - ss);

    if(LastDir != Source)
    {
        PrintAllFilesCollectedSoFar();
        ResetEstimations();

        if(Totals)
        {
            static int Eka=1;
            GetDescrColor(ColorDescr::TEXT, 1);
            if(WhereX)Gprintf("\n");
            if(!Eka)
            {
                if(RowLen > 0)Gprintf("\n");
                PrintSums();
                Gprintf("\n");
            }
            Eka=0;
            Gprintf(" Directory of %s\n", Source.empty() ? "/" : Source);
        }
        CurrentColumn = 0;
        LastDir = Source;
    }
}

// ScanDir: Called with parameter = the verbatim string passed on commandline.
// Will not call recursively.
static void ScanDir(std::string&& Source) // Directory to list
{
    DIR *dir = nullptr;

    #ifdef DJGPP
    if(!Source.empty() && Source.back() == ':')
        Source += '.';
    #endif

    // Was null. It was not a directory, or could not be read.
    // Or, when we're not supposed to read its contents
    if(!Contents || ((dir = opendir(Source.c_str())) == NULL
    && (
    #if defined(DJGPP) || (defined(SUNOS)||defined(__sun)||defined(SOLARIS))
        errno==EACCES  ||
    #endif
        errno==ENOENT  ||   /* Might have been a link */
        errno==ENOTDIR ||
        errno==ELOOP        /* For circular loops */
      )))
    {
        // Then list it as a file.
        if(dir) closedir(dir);

        std::string_view Tmp = DirOnly(Source);
        if(Tmp.empty()) Tmp = "./";
        DirChangeCheck(Tmp);

        SingleFile(std::move(Source));
        return;
    }

    if(!dir && (Source.empty() || Source.back() != '/'))
    {
        dir = opendir((Source + "/").c_str());
    }

    if(!dir)
    {
        Gprintf("\n%s - error: %d (%s)\n", Source,
                errno, GetError(errno));
        return;
    }

    // Directory successfully opened.
    DirChangeCheck(std::string(Source)); // Operates on a copy of Source

    for(struct dirent *ent = readdir(dir); ent != NULL; ent = readdir(dir))
    {
        if(!ShowDotFiles && ent->d_name[0] == '.') continue;

        std::string Buffer = Source;
        if(Buffer.back() != '/') Buffer += '/';
        Buffer += ent->d_name;

        SingleFile(std::move(Buffer));
    }

    if(closedir(dir) != 0)
        Gprintf("\nclosedir(%s) - error: %d (%s)\n", Source,
                errno, GetError(errno));
}

static std::list<std::string> FilesToList_FromCommandline;

static void DumpDirs()
{
    // Reset the estimations for field widths
    ResetEstimations();

    // For each file that was listed on the commandline:
    for(std::string& s: FilesToList_FromCommandline)
        ScanDir( std::move(s) );

    // Don't need these anymore
    FilesToList_FromCommandline.clear();

    // Flush the output of the last directory
    PrintAllFilesCollectedSoFar();
}

class Handle : public arghandler
{
    bool Files;
    bool Help;
private:
    std::string opt_h(const std::string &s)
    {
        Help = true;
        return s;
    }
    std::string opt_r(const std::string &s)
    {
        SetDefaultOptions();
        Inodemap.enable();
        return s;
    }
    std::string opt_d1(const std::string &s) { DateTime = 1; return s; }
    std::string opt_d2(const std::string &s) { DateTime = 2; return s; }
    std::string opt_d3(const std::string &s) { DateTime = 3; return s; }
    std::string opt_db(const std::string &s) { BlkStr = s; return ""; }
    std::string opt_dc(const std::string &s) { ChrStr = s; return ""; }
#ifdef S_ISLNK
    std::string opt_l(const std::string &s)
    {
        const char *q = s.c_str();
        const char *p = q;
        Links = strtol(p, const_cast<char**>(&p), 10);
        if(Links < 0 || Links > 5)argerror(s);
        return s.substr(p-q);
    }
#endif
    std::string opt_X(const std::string &s)
    {
        const char *q = s.c_str();
        const char *p = q;
        if(*p == '0') { std::printf("Window size = %dx%d\n", COLS, LINES); }
        int v = strtol(p, const_cast<char**>(&p), 10);
        if(v) COLS = v;
        return s.substr(p-q);
    }
    std::string opt_H1(const std::string &s)
    {
        Inodemap.enable();
        return s;
    }
    std::string opt_H(const std::string &s)
    {
        Inodemap.disable();
        if(s.size() && s[0]=='0')return s.substr(1);
        return s;
    }
    std::string opt_c1(const std::string &s) { Colors = true;    return s; }
    std::string opt_c(const std::string &s) { Colors = false;    return s; }
    std::string opt_C(const std::string &s) { MultiColumn = true; return s; }
    std::string opt_D(const std::string &s) { Contents = false;  return s; }
    std::string opt_p(const std::string &s) { Pagebreaks = true; return s; }
    std::string opt_P(const std::string &s) { AnsiOpt = false;   return s; }
    std::string opt_e(const std::string &s) { PreScan = false; Sorting = ""; return s; }
    std::string opt_o(const std::string &s) { Sorting = s; return ""; }
    std::string opt_F(const std::string &s) { DateForm = s; return ""; }
    std::string opt_f(const std::string &s) { FieldOrder = s; return ""; }
    std::string opt_vc(const std::string& s) { VerticalColumns = true; return s; }
    std::string opt_V(const std::string &)
    {
        printf(VERSIONSTR, VERSION);
        exit(0);
    }
    std::string opt_a(const std::string &s)
    {
        const char *q = s.c_str();
        const char *p = q;
        switch(strtol(p, const_cast<char**>(&p), 10))
        {
            case 0:
                FieldOrder = ".f_.s.d|";
                DateForm = "%z";
#ifdef S_ISLNK
                Links = 1;
#endif
                MultiColumn = true;
                Compact = 1;
                break;
            case 1:
                FieldOrder = ".xF|.a1.xF|.f.s.xF|.d.xF|.o_.g.xF|";
                break;
            case 2:
#ifdef S_ISLNK
                Links = 0;
#endif
                MultiColumn = true;
                FieldOrder = ".f_.a4_.o_.g.xF|";
                break;
            case 3:
#ifdef S_ISLNK
                Links = 0;
#endif
                MultiColumn = true;
                FieldOrder = ".f_.s_.o.xF|";
                break;
            case 4:
#ifdef S_ISLNK
                Links = 1;
#endif
                MultiColumn = true;
                FieldOrder = ".f_.s_.d_.o.xF|";
                DateForm = "%z";
                break;
            case 5:
                FieldOrder = ".a1_.h__.o_.g_.s_.d_.f";
                DateForm = "%u";
                Inodemap.enable();
                break;
            case 6:
                FieldOrder = ".a4_.h_.uid_.gid_.z_.d_.F";
                Links = 3;
                Compact = 1; Totals = true;
                break;
            default:
                argerror(s);
        }
        return s.substr(p-q);
    }
    std::string opt_al(const std::string &s)
    {
        FieldOrder = ".a1_.h__.o_.g_.s_.d_.f";
        DateForm = "%u";
        Inodemap.enable();
        return s;
    }
    std::string opt_m(const std::string &s)
    {
        const char *q = s.c_str();
        const char *p = q;
        switch(strtol(p, const_cast<char**>(&p), 10))
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
    std::string opt_M(const std::string &s)
    {
        std::string q = opt_m(s);
        if(!q.size())argerror(s);
        if(q == "_") TotalSep = ' '; else TotalSep = q[0];
        return q.substr(1);
    }
    std::string opt_w(const std::string &s)
    {
        Compact = 1;
        Totals = true;
        FieldOrder = ".f";
        Sorting = "pcm";
        Inodemap.disable();
#ifdef S_ISLNK
        Links = 1;
#endif
        MultiColumn = true;
        VerticalColumns = true;
        return s;
    }
    std::string opt_A(const std::string &s)
    {
        ShowDotFiles = true;
        return s;
    }
    std::string opt_W(const std::string &s)
    {
        Compact = 1;
        Totals = true;
        FieldOrder = ".f";
        Sorting = "PCM";
        Inodemap.disable();
#ifdef S_ISLNK
        Links = 1;
#endif
        MultiColumn = true;
        VerticalColumns = true;
        return s;
    }
public:
    Handle(const char *defopts, int argc, const char *const *argv)
    : arghandler(defopts, argc, argv), Files(false), Help(false)
    {
        add("-A",  "--dotfiles",  "Show files where names begin with a '.'", &Handle::opt_A);
        add("-al", "--long",      "Equal to -a5. Provided for ls compatibility.", &Handle::opt_al);
        add("-a",  "--predef",    "Predefined aliases.\n"
                                  "a0: -f\".f_.s.d|\" -F\"%z\" -C -m1 -l1\n"
                                  "a1: -f\".xF|.a1.xF|.f.s.xF|.d.xF|.o_.g.xF|\" -l1\n"
                                  "a2: -f\".f_.a4_.o_.g.xF|\" -l0 -C\n"
                                  "a3: -f\".f_.s_.o.xF|\" -l0 -C\n"
                                  "a4: -f\".f_.s_.d_.o.xF|\" -F\"%z\" -C -l1\n"
                                  "a5: -f\".a1_.h__.o_.g_.s_.d_.f\" -r -F\"%u\"\n"
                                  "a6: -f\".a4_.h_.uid_.gid_.z_.d_.F\" -m1 -l3",
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
        add("-e",  "--noprescan", "Print files as they're encountered. Disables sorting and some formatting.", &Handle::opt_e);
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
                                  "  .uid and .gid are .o and .g but always in numeric form\n"
                                  "  .z is .s in \"human-readable\" format\n"
                                  "   anything else=printed, except _ produces space\n"
                                  "  Colors follow the same format as in DIRR_COLORS\n"
                                  "   Default is `--format="+FieldOrder+"'",
                                  &Handle::opt_f);
        add("-F",  "--dates",     "Specify new date format. man strftime. Underscore (_) produces space.\n"
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
        add("-o", "--sort",       "Sort the files (disables -e), with n as combination of:\n"
                                  "  (n)ame, (s)ize, (d)ate, (u)id, (g)id, (h)linkcount,\n"
                                  "  nam(e) length, na(m)e case insensitively, (c)olor,\n"
                                  "  g(r)oup dirs,files,links,chrdevs,blkdevs,fifos,socks,\n"
                                  "  grou(p) dirs,links=files,chrdevs,blkdevs,fifos,socks.\n"
                                  "Use Uppercase for reverse order.\n"
                                  "Default is `--sort="+Sorting+"'\n", &Handle::opt_o);
        add("-p",  "--paged",     "Use internal pager.", &Handle::opt_p);
        add("-P",  "--oldvt",     "Disables colour code optimizations.", &Handle::opt_P);
        add("-r",  "--restore",   "Undoes all options, including the DIRR environment variable.", &Handle::opt_r);
        add("-vc", "--vertical",  "Uses vertical columns rather than horizontal in -C modes.", &Handle::opt_vc);
        add("-v",  "--version",   "Displays the version.", &Handle::opt_V);
        add("-V",  NULL,          "Alias to -v.", &Handle::opt_V);
        add("-w",  "--wide",      "Equal to -l1HCm1f.f -opcm -vc", &Handle::opt_w);
        add("-W",  "--ediw",      "Same as -w, but with reverse sort order.", &Handle::opt_W);
        add("-X",  "--width",     "Force screen width, example: -X132. -X0 debugs autodetection.",
                                  &Handle::opt_X);

        parse();
    }
    virtual void defarg(const std::string &s)
    {
        FilesToList_FromCommandline.push_back(s);
        Files = true;
    }
    virtual void parse()
    {
        arghandler::parse();
        if(!Files)FilesToList_FromCommandline.push_back(".");
        if(Help)
        {
            Dumping = true;

            GetDescrColor(ColorDescr::TEXT, 1);

            Gprintf(VERSIONSTR, VERSION);

            Gprintf(
#ifndef DJGPP
                "\r\33[K\r"
#endif
                "Usage: %s [options] {dirs | files }\n", a0);

            SetAttr(15);
            Gprintf("\nOptions:\n");

            listoptions();

            GetDescrColor(ColorDescr::TEXT, 1);

            Gwrite(
                "\n"
                "You can set environment variable 'DIRR' for the options.\n"
                "You can also use 'DIRR_COLORS' environment variable for color settings.\n"
                "Current DIRR_COLORS:\n"
            );
            PrintSettings();
            Gwrite(
                "\n"
                "Attributes are expressed in hexadecimal, and interpreted bitwise:\n"
                "   When < 0x100,           0BbbbIfff\n"
                "   When >= 0x100, Ibbbbbbbb1ffffffff\n"
                "   Where B=blink, bb...=background, I=bold or intensity, ff...=foreground\n"
                "   Background and foreground color indexes are as in xterm-256color/ansi.\n");

            exit(0);
        }
    }
};

#if defined(SIGINT) || defined(SIGTERM)
static void Term(int /*dummy*/)
{
    Gprintf("^C\n");

    RowLen=1;
    PrintSums();

    exit(0);
}
#endif

int main(int argc, const char *const *argv)
{
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
    FieldsToPrint.ParseFrom(FieldOrder);

    Dumping = true;
    DumpDirs();

    if(RowLen > 0)Gprintf("\n");
    PrintSums();

    return 0;
}
