#include <vector>
#include <cctype>
#include <map>
#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <fstream>

#include <stdlib.h> // For getenv("HOME")

//#include <regex>
//#include "wildmatch.hh"
#include "dfa_match.hh"

#include "config.h"
#include "setfun.hh"
#include "cons.hh"

#ifdef HAVE_FLOCK_SYS_FILE_H
# include <sys/file.h>
#endif
#ifdef HAVE_STDIO_FILEBUF
# include <ext/stdio_filebuf.h>
# include <unistd.h>
#endif
#include <fcntl.h>

using std::vector;
using std::map;
using std::isxdigit;

bool Dumping = false; // If 0, save some time

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

static std::string ColorModeName(ColorMode m)
{
    #define o(val,str) case ColorMode::val: return str;
    switch(m)
    {
        ColorModes(o)
        default: return "??"+std::to_string(int(m));
    }
    #undef o
}
static std::string ColorDescrName(ColorDescr m)
{
    #define o(val,str) case ColorDescr::val: return str;
    switch(m)
    {
        ColorDescrs(o)
        default: return "??"+std::to_string(int(m));
    }
    #undef o
}
static int ColorModeFromName(const std::string& m)
{
    #define o(val,str) if(m==str) return (int)ColorMode::val;
    ColorModes(o)
    #undef o
    return -1;
}
static int ColorDescrFromName(const std::string& m)
{
    #define o(val,str) if(m==str) return (int)ColorDescr::val;
    ColorDescrs(o)
    #undef o
    return -1;
}

static class Settings
{
    std::unordered_multimap<std::string/*key*/, std::string/*value*/> sets{};

public:
    // Key: int(ModeDescr) - contains a sorted-by-char vector of colors
    std::vector<std::vector<std::pair<char,int>>> mode_sets{};
    // Key: int(ColorDescr)
    std::vector<std::vector<int>> descr_sets{};
public:
    DFA_Matcher byext_sets{};

public:
    Settings() {}
    void Load()
    {
        if(sets.empty())
        {
            Load(
#include SETTINGSFILE
                );

            const char* var = getenv("DIRR_COLORS");
            if(var)
            {
                Load(var);
            }
            Parse();
        }
    }
    std::pair<std::unordered_multimap<std::string,std::string>::const_iterator,
              std::unordered_multimap<std::string,std::string>::const_iterator>
        find_range(const std::string& key) const
    {
        return sets.equal_range(key);
    }

    int FindMode(ColorMode key, char Chr, int default_color) const
    {
        unsigned ukey = unsigned(key);
        if(ukey < mode_sets.size())
        {
            const auto& colors = mode_sets[ukey];
            auto i = std::lower_bound(colors.begin(), colors.end(), Chr,
                                      [](const std::pair<char,int>&a, char c) { return a.first < c; });

            if(i == colors.end() || i->first != Chr)
            {
                Gprintf("DIRR_COLORS error: No color for '%c' found in '%s'\n", Chr, ColorModeName(key));
                return default_color;
            }
            return i->second;
        }
        else
        {
            Gprintf("DIRR_COLORS error: No '%s'\n", ColorModeName(key));
            return default_color;
        }
    }
    int FindDescr(ColorDescr key, std::size_t index, int default_color) const
    {
        unsigned ukey = unsigned(key);
        if(ukey < descr_sets.size())
        {
            const auto& colors = descr_sets[ukey];
            if(colors.empty()) goto fail;
            return index < colors.size() ? colors[index] : default_color;
        }
        else
        {
        fail:;
            Gprintf("DIRR_COLORS error: No '%s'\n", ColorDescrName(key));
            return default_color;
        }
    }
private:
    void Load(const std::string& s)
    {
        /* Syntax: keyword '(' data ')' ... */
        std::size_t pos = 0;
        while(pos < s.size())
        {
            if(std::isspace(s[pos]) || s[pos]==')') { ++pos; continue; }
            auto parens_pos = s.find('(', pos);
            if(parens_pos == s.npos) parens_pos = s.size();
            std::size_t key_end = parens_pos;
            while(key_end > pos && std::isspace(s[key_end-1])) --key_end;
            std::string key = s.substr(pos, key_end-pos);

            // Replace previous entries, except for byext()
            if (key != "byext") {
                sets.erase(key);
            }

            if(parens_pos == s.size())
            {
                sets.emplace(std::move(key), std::string{});
                return;
            }
            std::size_t value_begin = ++parens_pos;
            while(value_begin < s.size() && std::isspace(s[value_begin]))
                ++value_begin;

            std::size_t end_parens_pos = s.find(')', value_begin);
            if(end_parens_pos == s.npos) end_parens_pos = s.size();

            std::size_t value_end = end_parens_pos;
            while(value_end > value_begin && std::isspace(s[value_end-1])) --value_end;

            // Allow "byext()" to clear all previous byext entries
            if (value_end == value_begin && key == "byext") {
                sets.erase(key);
            } else {
                sets.emplace(std::move(key), s.substr(value_begin, value_end-value_begin));
            }
            pos = end_parens_pos+1;
        }
    }

private:
    void Parse()
    {
        for(const auto& s: sets)
        {
            if(s.first == "mode" || s.first == "type" || s.first == "info")
            {
                // Parse "type", "mode" and "info".
                // These strings in DIRR_COLORS have the format: text(sC,...)
                // Where s is the character key
                // and   C is the color code.
                const std::string& t = s.second;
                int m = ColorModeFromName(s.first);
                if(int(mode_sets.size()) <= m) mode_sets.resize(m+1);
                auto& table = mode_sets[m];

                std::size_t pos = 0;
                while(pos < t.size())
                {
                    if(std::isspace(t[pos])) { ++pos; continue; }
                    char key = t[pos];
                    int color = std::stoi(t.substr(pos+1), nullptr, 16);

                    table.emplace_back(key, color);

                    std::size_t comma = t.find(',', pos);
                    if(comma == t.npos) break;
                    pos = comma+1;
                }
                std::sort(table.begin(), table.end());
            }
            else if(s.first == "byext")
            {
                const std::string& t = s.second;
                // Parse the string. It contains space-delimited tokens.
                // The first token is a hex code, that is optionally followed by 'i'.
                bool ignore_case = false;
                int  color = -1;

                std::size_t pos = 0;
                while(pos < t.size())
                {
                    if(std::isspace(t[pos])) { ++pos; continue; }
                    std::size_t spacepos = t.find(' ', pos);
                    if(spacepos == t.npos) spacepos = t.size();

                    std::string token = t.substr(pos, spacepos-pos);
                    if(color < 0)
                    {
                        color       = std::stoi(token, nullptr, 16);
                        ignore_case = token.back() == 'i';
                        pos = spacepos;
                        continue;
                    }
                    byext_sets.AddMatch(std::move(token), ignore_case, color);
                    pos = spacepos;
                }
            }
            else
            {
                // Parse "txt", "owner", "group", "nrlink", "date", "num", "descr", or "size".
                // These strings in DIRR_COLORS have the format: text(color,...)
                int m = ColorDescrFromName(s.first);
                if(m == -1)
                {
                    Gprintf("DIRR_COLORS error: Unknown setting '%s'\n", s.first);
                    continue;
                }
                if(int(descr_sets.size()) <= m) descr_sets.resize(m+1);
                auto& target = descr_sets[m];

                const std::string& t = s.second;
                std::size_t pos = 0;
                while(pos < t.size())
                {
                    if(std::isspace(t[pos])) { ++pos; continue; }
                    int color = std::stoi(t.substr(pos), nullptr, 16);
                    target.push_back(color);

                    std::size_t comma = t.find(',', pos);
                    if(comma == t.npos) break;
                    pos = comma+1;
                }
            }
        }

        // Load/Compile/Save byext_sets for use by NameColor()
        bool loaded = false, compiled = false;
        for(const char* path: std::initializer_list<const char*>{getenv("HOME"),"",getenv("TEMP"),getenv("TMP"),"/tmp"})
        {
            std::string hash_save_fn = std::string(path) + "/.dirr_dfa";
            #if defined(HAVE_FLOCK) && defined(HAVE_STDIO_FILEBUF)
            unsigned num_write_tries=0;
            retry_reading:;
            #endif
            try {
                std::ifstream f(hash_save_fn, std::ios_base::in | std::ios_base::binary);
                loaded = byext_sets.Load(f);
                if(loaded) break;
            }
            catch(const std::exception&)
            {
            }
            // File format error. Regenerate file.
            try {
            #if defined(HAVE_FLOCK) && defined(HAVE_STDIO_FILEBUF)
                // If flock() and stdio_filebuf are available, try to lock the file
                // for exclusive access. If this fails, it means that another instance
                // of DIRR is currently in the process of generating the file.
                // We will first wait for that process to complete, and then retry
                // reading the file.
                int fd = open(hash_save_fn.c_str(), O_WRONLY | O_CREAT, 0644);
                if(fd >= 0)
                {
                try_relock:;
                    if(flock(fd, LOCK_EX | (num_write_tries==0 ? LOCK_NB : 0)) >= 0)
                    {
                        if(num_write_tries > 0)
                        {
                            write(2, "\33[K\r", 5);
                            close(fd);
                            goto retry_reading;
                        }
                        // libstdc++-only feature: Create an std::ostream using a unix file descriptor.
                        __gnu_cxx::stdio_filebuf<char> buf{fd, std::ios_base::out | std::ios_base::binary};
                        std::ostream f{&buf};
                        if(!compiled) { compiled = true; byext_sets.Compile(); }
                        ftruncate(fd, 0);
                        byext_sets.Save(f);
                        close(fd);
                        break;
                    }
                    else if(errno == EINTR) goto try_relock;
                    else if(errno == EWOULDBLOCK)
                    {
                        std::string msg = "File " + hash_save_fn + " is locked, waiting...\r";
                        write(2, msg.c_str(), msg.size());

                        close(fd);
                        ++num_write_tries;
                        goto retry_reading;
                    }
                    close(fd);
                }
            #endif
                std::ofstream f(hash_save_fn, std::ios_base::out | std::ios_base::binary);
                if(!compiled) { compiled = true; byext_sets.Compile(); }
                byext_sets.Save(f);
                break;
            }
            catch(const std::exception&)
            {
            }
        }
        if(!loaded && !compiled) byext_sets.Compile();
    }
} Settings;

// GetModeColor(text, Chr):
//   text is either "type", "mode", or "info".
//   These strings in DIRR_COLORS have the format:  text(sC,...)
//   where s is the Chr given as parameter
//   and   C is the color code.
//   If Chr is positive, the color is also set using SetAttr()!
//   If Chr is negative, it is treated as positive but not set.
int GetModeColor(ColorMode m, signed char Chr)
{
    Settings.Load();

    bool set_color = true;
    if(Chr < 0) { set_color = false; Chr = -Chr; }

    int res = Settings.FindMode(m, Chr, 7);
    if(set_color) SetAttr(res);
    return res;
}

// GetDescrColor(descr, index)
//   descr is either "txt", "owner", "group", "nrlink", "date", or "num".
//   These strings in DIRR_COLORS have the format: text(color,...)
//   index is the index of color to read.
//           1 is the first
//           2 is the second, and so on.
//   If the number is positive, the color is also set using SetAttr()!
//   If the number is negative, it is treated as positive but not set.
int GetDescrColor(ColorDescr d, int index)
{
    int Dfl = DEFAULTATTR;
    if(!Colors || !Dumping)return Dfl;

    Settings.Load();

    bool set_color = true;
    if(index < 0) { set_color = false; index = -index; }

    if(d == ColorDescr::TEXT)
    {
        static int txt_color=-1;
        if(txt_color >= 0)
            Dfl = txt_color;
        else
        {
            int res = Settings.FindDescr(d, index-1, -1);
            if(res != -1) Dfl = res;
            txt_color = Dfl;
        }
    }
    else
    {
        int res = Settings.FindDescr(d, index-1, -1);
        if(res != -1) Dfl = res;
    }

    if(set_color) SetAttr(Dfl);
    return Dfl;
}

void PrintSettings()
{
    Settings.Load();

    int Dfl = GetDescrColor(ColorDescr::TEXT, -1);
    const std::string indent(3,' ');

    for(unsigned m=0; m<Settings.mode_sets.size(); ++m)
    {
        SetAttr(Dfl);
        Gprintf("%s%s(", indent, ColorModeName(ColorMode(m)));
        bool sep = false;
        for(const auto& c: Settings.mode_sets[m])
        {
            if(sep) Gputch(',');
            sep = true;
            SetAttr(c.second); Gputch(c.first);
            SetAttr(Dfl);      Gprintf("%X", c.second);
        }
        SetAttr(Dfl);
        Gprintf(")\n");
    }

    for(unsigned m=0; m<Settings.descr_sets.size(); ++m)
    {
        SetAttr(Dfl);
        Gprintf("%s%s(", indent, ColorDescrName(ColorDescr(m)));
        const auto& table = Settings.descr_sets[m];
        for(std::size_t a=0; a<table.size(); ++a)
        {
            if(a > 0) { SetAttr(Dfl); Gputch(','); }
            SetAttr(table[a]);
            Gprintf("%X", table[a]);
        }
        SetAttr(Dfl);
        Gprintf(")\n");
    }

    if(true) // scope for byext
    {
        auto pair = Settings.find_range("byext");
        while(pair.first != pair.second)
        {
            SetAttr(Dfl);
            Gprintf("%sbyext(", indent);
            const std::string& t = pair.first++->second;

            int color = -1;
            std::size_t pos = 0;
            while(pos < t.size())
            {
                if(std::isspace(t[pos])) { ++pos; continue; }
                std::size_t spacepos = t.find(' ', pos);
                if(spacepos == t.npos) spacepos = t.size();

                std::string token = t.substr(pos, spacepos-pos);
                if(color < 0)
                {
                    color       = std::stoi(token, nullptr, 16);
                    SetAttr(color); Gprintf("%X", color);
                    if(token.back()=='i') { SetAttr(Dfl); Gputch('i'); }
                    SetAttr(color);
                    pos = spacepos;
                    continue;
                }
                Gprintf(" %s", token);
                pos = spacepos;
            }
            SetAttr(Dfl);
            Gprintf(")\n");
        }
    }
}


/*
18:35|>Warp= 1) int NameColor(const char *s) - palauttaa
                nimeä vastaavan värin.
18:36|>Warp= 2) NameColor() käy kaikki byext():t läpi, tai siihen asti
                kunnes sieltä löytyy joku patterni johon annettu nimi menee.
18:36|>Warp= 3) Jokaisen byext():n sisällä on läjä patterneja.
                Stringi annetaan rmatch() -funktiolle.
18:36|>Warp= 4) rmatch() pilkkoo stringin ja testaa jokaista patternia
                erikseen wildmatch:lla.
18:37|>Warp= 5) wildmatch() palauttaa >0 jos täsmäsi.
18:37|>Warp= 6) rmatch() lopettaa testauksen jos wildmatch täsmäsi.
18:37|>Warp= 7) Jos rmatch() palautti 0 (ei täsmännyt) ja viimeinen
                byext() on käyty läpi, palautetaan default-väri.
18:38|>Warp= Ja byext():t tässä ovat niitä asetusstringejä, ei funktioita

*/

int NameColor(std::string_view name, int default_color)
{
    Settings.Load();
    return Settings.byext_sets.Test(name, default_color);
}
