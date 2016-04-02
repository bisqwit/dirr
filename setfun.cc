#include <vector>
#include <cctype>
#include <map>
#include <unordered_map>
#include <cstring>
#include <cstdlib>

//#include <regex>
//#include "wildmatch.hh"
#include "dfa_match.hh"

#include "config.h"
#include "setfun.hh"
#include "cons.hh"

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

namespace std { template<> struct hash<std::pair<std::string,char>> {
    std::size_t operator() (const std::pair<std::string,char>& v) const
    {
        return std::hash<std::string>{}(v.first) ^ ((unsigned char)(v.second) * (~std::size_t(0)/255u));
}   };  }

static class Settings
{
    std::unordered_multimap<std::string/*key*/, std::string/*value*/> sets{};

public:
    // Cache of colors in "type", "mode" and "info"
    std::unordered_map<std::pair<std::string/*key*/,char/*key*/>, int> mode_sets{};
    std::unordered_map<std::string/*key*/,std::vector<int>> descr_sets{};
public:
    //std::vector<std::pair<std::regex, int/*color*/>> byext_sets{};
    DFA_Matcher byext_sets{};

public:
    Settings() {}
    void Load()
    {
        if(sets.empty())
        {
            const char* var = getenv("DIRR_COLORS");
            if(var)
                Load(var);
            else
            {
                Load(
#include SETTINGSFILE
                    );
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
    /*std::multimap<std::string,std::string>::const_iterator begin() const { return sets.cbegin(); }
    std::multimap<std::string,std::string>::const_iterator end()   const { return sets.cend();   }*/

    int FindMode(const std::string& key, char Chr, int default_color) const
    {
        auto i = mode_sets.find({key,Chr});
        if(i == mode_sets.end())
        {
            Gprintf("DIRR_COLORS error: No color for '%c' found in '%s'\n", Chr, key.c_str());
            return default_color;
        }
        return i->second;
    }
    int FindDescr(const std::string& key, std::size_t index, int default_color) const
    {
        auto i = descr_sets.find(key);
        if(i == descr_sets.end())
        {
            Gprintf("DIRR_COLORS error: No '%s'\n", key.c_str());
            return default_color;
        }
        if(index < i->second.size()) return i->second[index];
        if(i->second.empty()) return default_color;
        return i->second.back();
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

            sets.emplace(std::move(key), s.substr(value_begin, value_end-value_begin));
            pos = end_parens_pos+1;
        }
    }

private:
    void Parse()
    {
        //std::map<std::pair<int/*color*/,bool/*icase*/>, std::vector<std::string>> byext_patterns;
        for(const auto& s: sets)
        {
            if(s.first == "mode" || s.first == "type" || s.first == "info")
            {
                // Parse "text", "mode" and "info".
                // These strings in DIRR_SETS have the format: text(sC,...)
                // Where s is the character key
                // and   C is the color code.
                const std::string& t = s.second;
                std::size_t pos = 0;
                while(pos < t.size())
                {
                    if(std::isspace(t[pos])) { ++pos; continue; }
                    char key = t[pos];
                    int color = std::stoi(t.substr(pos+1), nullptr, 16);

                    mode_sets.emplace( std::pair<std::string,char>{s.first,key}, color );

                    std::size_t comma = t.find(',', pos);
                    if(comma == t.npos) break;
                    pos = comma+1;
                }
            }
            else if(s.first == "byext")
            {
                const std::string& t = s.second;
                // Parse the string. It contains space-delimited tokens.
                // The first token is a hex code, that is optionally followed by 'i'.
                bool ignore_case = false;
                int  color = -1;

                std::string dot  = "\\.";
                std::string star = ".*";
                std::string qmark = ".";
                std::string escd = "[0-9]";
                std::string escw = "[A-Za-z]";

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
#if 1
                    byext_sets.AddMatch(token, ignore_case, color);
#endif
                    /*
                    std::string pattern;
                    bool escmode = false;
                    for(char c: token)
                    {
                        if(escmode)
                        {
                            switch(c)
                            {
                                case 'd': pattern += escd; break;
                                case 'w': pattern += escw; break;
                                case '\\': pattern += "\\\\"; break;
                                default:  pattern += c; break;
                            }
                            escmode = false;
                        }
                        switch(c)
                        {
                            case '.': pattern += dot; break;
                            case '*': pattern += star; break;
                            case '?': pattern += qmark; break;
                            case '(': case '|': case ')':
                            case '[': case ']': case '^': case '$':
                                pattern += '\\';
                                pattern += c;
                                break;
                            case '\\':
                                escmode = true;
                                break;
                            default:
                                pattern += c;
                        }
                    }
                    byext_patterns[std::pair<int,bool>{color,ignore_case}].push_back(pattern);
                    */
                    pos = spacepos;
                }
            }
            else
            {
                // Parse "txt", "owner", "group", "nrlink", "date", or "num".
                // These strings in DIRR_SETS have the format: text(color,...)
                auto& target = descr_sets[s.first];
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
#if 0
        byext_sets.AddMatch("test",  false, 1);
        byext_sets.AddMatch("tes",   false, 2);
        //byext_sets.AddMatch("abra",  false, 1);
        //byext_sets.AddMatch("koe",   true,  2);
        //byext_sets.AddMatch("*.exe", false, 3);
#endif
        byext_sets.Compile();
        /*for(const auto& p: byext_patterns)
        {
            int color        = p.first.first;
            bool ignore_case = p.first.second;
            std::string patterns = "(?:";
            for(const auto& s: p.second)
            {
                if(!patterns.empty()) patterns += '|';
                patterns += s;
            }
            patterns += ")";

            if(ignore_case)
                printf("'%s'    { return %d; }\n", patterns.c_str(), color);
            else
                printf("\"%s\"  { return %d; }\n", patterns.c_str(), color);

            byext_sets.emplace_back(std::regex(patterns,
                std::regex_constants::syntax_option_type(
                    std::regex::optimize | (ignore_case ? std::regex::icase : 0)
                )),
                color);
        }*/
    }
} Settings;

// GetModeColor(text, Chr):
//   text is either "type", "mode", or "info".
//   These strings in DIRR_SETS have the format:  text(sC,...)
//   where s is the Chr given as parameter
//   and   C is the color code.
//   If Chr is positive, the color is also set using SetAttr()!
//   If Chr is negative, it is treated as positive but not set.
int GetModeColor(const string &text, char Chr)
{
    Settings.Load();

    bool set_color = true;
    if(Chr < 0) { set_color = false; Chr = -Chr; }

    int res = Settings.FindMode(text,Chr, 7);
    if(set_color) SetAttr(res);
    return res;
}

// GetDescrColor(descr, index)
//   descr is either "txt", "owner", "group", "nrlink", "date", or "num".
//   These strings in DIRR_SETS have the format: text(color,...)
//   index is the index of color to read.
//           1 is the first
//           2 is the second, and so on.
//   If the number is positive, the color is also set using SetAttr()!
//   If the number is negative, it is treated as positive but not set.
int GetDescrColor(const string &descr, int index)
{
    int Dfl = 7;
    if(!Colors || !Dumping)return Dfl;

    Settings.Load();

    bool set_color = true;
    if(index < 0) { set_color = false; index = -index; }

    int res = Settings.FindDescr(descr, index-1, -1);
    if(res != -1) Dfl = res;

    if(set_color) SetAttr(Dfl);
    return Dfl;
}

void PrintSettings()
{
    Settings.Load();

    int Dfl = GetDescrColor("txt", -1);
    const std::string indent(3,' ');

    if(true) // scope for modes
    {
        std::map<std::string,std::vector<std::pair<char,int>>> parsed_modes;
        for(const auto& s: Settings.mode_sets)
            parsed_modes[s.first.first].emplace_back(s.first.second, s.second);

        for(const auto& p: parsed_modes)
        {
            SetAttr(Dfl);
            Gprintf("%s%s(", indent.c_str(), p.first.c_str());
            bool sep = false;
            for(const auto& c: p.second)
            {
                if(sep) Gputch(',');
                sep = true;
                SetAttr(c.second); Gputch(c.first);
                SetAttr(Dfl);      Gprintf("%X", c.second);
            }
            SetAttr(Dfl);
            Gprintf(")\n");
        }
    }
    if(true) // scope for descr
    {
        std::map<std::string,std::vector<int>> sorted_sets
            ( Settings.descr_sets.begin(), Settings.descr_sets.end() );
        for(const auto& p: sorted_sets)
        {
            SetAttr(Dfl);
            Gprintf("%s%s(", indent.c_str(), p.first.c_str());
            for(std::size_t a=0; a<p.second.size(); ++a)
            {
                if(a > 0) { SetAttr(Dfl); Gputch(','); }
                SetAttr(p.second[a]);
                Gprintf("%X", p.second[a]);
            }
            SetAttr(Dfl);
            Gprintf(")\n");
        }
    }
    if(true) // scope for byext
    {
        auto pair = Settings.find_range("byext");
        while(pair.first != pair.second)
        {
            SetAttr(Dfl);
            Gprintf("%sbyext(", indent.c_str());
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
                Gprintf(" %s", token.c_str());
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

bool WasNormalColor;
int NameColor(const string &Name)
{
    Settings.Load();

    WasNormalColor = false;

    // NameColorCache2 contains the colors for
    // whole filenames that we have already matched.
    static std::unordered_map<string, int> NameColorCache2;

    auto i = NameColorCache2.find(Name);
    if(i != NameColorCache2.end()) return i->second;

    int colour = Settings.byext_sets.Test(Name, -1);
    /*for(const auto& p: Settings.byext_sets)
        if(std::regex_match(Name, p.first))
        {
            int colour = p.second;
            NameColorCache2.emplace(Name, colour);
            return colour;
        }*/
    if(colour == -1)
    {
        WasNormalColor = true;
        colour = GetDescrColor("txt", -1);
    }
    NameColorCache2.emplace(Name, colour);
    return colour;
}
