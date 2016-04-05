#include "dfa_match.hh"
#include "src/ir/regexp/regexp.h"
#include "src/ir/regexp/regexp_close.h"
#include "src/ir/regexp/regexp_rule.h"
#include "src/ir/regexp/regexp_null.h"
#include "src/parse/scanner.h"
#include "src/util/range.h"
#include "src/util/counter.h"
#include "src/ir/nfa/nfa.h"
#include "src/ir/dfa/dfa.h"
#include "src/ir/adfa/adfa.h"
#include "src/ir/skeleton/skeleton.h"

#include <map>
#include <stdlib.h> // getenv

using namespace re2c;

namespace re2c { Skeleton::~Skeleton() {} }

void DFA_Matcher::AddMatch(const std::string& token, bool icase, int target)
{
    hash_str += token;
    hash_str += char(icase);
    hash_str += char(target);
    hash_str += char(target>>8);

    /* Translate a glob pattern (wildcard match) into a re2c regexp structure */
    RegExp* root = nullptr;
    bool escmode = false;
    for(char c: token)
    {
        if(escmode)
        {
            switch(c)
            {
                case 'd':
                {
                    Range* digits = Range::ran('0', '9'+1);
                    root = doCat(root, Scanner::cls(digits));
                    break;
                }
                case 'w':
                {
                    Range* digits  = Range::ran('0', '9'+1);
                    Range* hialpha = Range::ran('A', 'Z'+1);
                    Range* loalpha = Range::ran('a', 'z'+1);
                    Range* alpha = Range::add(hialpha, loalpha);
                    root = doCat(root, Scanner::cls( Range::add(digits, alpha) ));
                    break;
                }
                case '\\': root = doCat(root, Scanner::schr('\\')); break;
                default:   root = doCat(root, Scanner::schr(c)); break;
            }
            escmode = false;
        }
        switch(c)
        {
            case '*':
            {
                root = doCat(root, new CloseOp(Scanner::cls(Range::ran(1,0x100))));
                break;
            }
            case '?':
            {
                root = doCat(root, Scanner::cls(Range::ran(1,0x100)));
                break;
            }
            case '\\':
                escmode = true;
                break;
            default:
                root = doCat(root, icase ? Scanner::ichr(c) : Scanner::schr(c));
                break;
        }
    }
    // Add the end-of-stream tag
    root = doCat(root, Scanner::schr('\0'));
    collection[target].push_back(root);
}

void DFA_Matcher::Compile()
{
    /*auto str = [](char c) -> std::string
    {
        switch(c)
        {
            case '\n': return "\\n";
            case '\r': return "\\r";
            case '\t': return "\\t";
            case '\b': return "\\b";
            case '\0': return "\\0";
            case '\\': return "\\\\";
            case '\"': return "\\\"";
            case '\'': return "\\'";
            default: if(c<32 || c==127) return Printf("\\%04o", (unsigned char)c);
        }
        return Printf("%c", c);
    };*/

    const long long hash = std::hash<std::string>{}(hash_str);
    std::string hash_save_fn;
    std::FILE* save_fp = nullptr;
    for(const std::string& path: std::initializer_list<const char*>{getenv("HOME"),"",getenv("TEMP"),getenv("TMP"),"/tmp"})
    {
        hash_save_fn = path + "/.dirr_dfa";
        std::FILE* fp = std::fopen(hash_save_fn.c_str(), "rb");
        if(fp)
        {
            char StrBuf[128];
            try {
                while(std::fgets(StrBuf, sizeof StrBuf, fp) && StrBuf[0]=='#') {}
                if(std::stoll(StrBuf, nullptr, 16) == hash)
                {
                    // Read the rest of the state machine
                    std::fgets(StrBuf, sizeof StrBuf, fp);
                    unsigned num_states = std::stoi(StrBuf);

                    if(num_states*sizeof(statemachine[0]) > 2000000)
                        throw std::invalid_argument("implausible num_states");

                    statemachine.resize(num_states);
                    for(unsigned state_no=0; state_no<statemachine.size(); ++state_no)
                    {
                        unsigned char Buf[3*256] = {0,0};
                        if(std::fread(Buf, 1, 2, fp) < 1) break;
                        unsigned n = Buf[0] + Buf[1] * 256;
                        if(n == 0 || n > sizeof(Buf)) break;
                        std::fread(Buf, 1, n, fp);
                        auto& data = statemachine[state_no];
                        //fprintf(stderr, "%u:", state_no);
                        for(unsigned last = 0, ptr = 0; last < 256 && ptr+3 <= 3*256; ptr += 3)
                        {
                            unsigned end = last + Buf[ptr], c = Buf[ptr+1] + Buf[ptr+2]*256;
                            if(end==last) end = last+256;
                            /*if(end==last+1)
                                fprintf(stderr, "\t%u\t%s\n",  c, str(last).c_str());
                            else
                                fprintf(stderr, "\t%u\t%s-%s\n", c, str(last).c_str(), str(end-1).c_str());*/
                            while(last < end && last < 256) data[last++] = c;
                        }
                    }
                    std::fclose(fp);
                    collection.clear();
                    return;
                }
            }
            catch(const std::invalid_argument&)
            {
                // Lands here if std::stoi fails to parse a number.
            }
            std::fclose(fp);
        }
        // Incorrect hash. Regenerate file.
        fp = std::fopen(hash_save_fn.c_str(), "wb");
        if(fp) { save_fp = fp; break; }
    }

    counter_t<rule_rank_t> rank_counter;
    rules_t rules;

    RegExp* all_exps = nullptr;
    for(auto& p: collection)
    {
        RegExp* root = nullptr;
        for(auto c: p.second)
            if(!root) root = c; else root = mkAlt(root, c);

        auto rank = rank_counter.next();
        auto& info = rules[rank];
        info.line  = p.first; // target number
        root = new RuleOp({"",0}, root, new NullOp, rank, nullptr/*code*/, nullptr/*cond*/);
        if(!all_exps) all_exps = root; else all_exps = mkAlt(all_exps, root);
    }
    collection.clear(); // All pointers are now accounted for

    /* BEGIN: This code is mostly copied from re2c/src/ir/compile.cc */
    std::set<uint32_t> bounds;
    all_exps->split(bounds);
    bounds.insert(0);
    bounds.insert(0x100);
    charset_t cs(bounds.begin(), bounds.end());

    // Convert into NFA
    nfa_t nfa(all_exps);
    delete all_exps;

    // Convert into DFA
    dfa_t dfa(nfa, cs, rules);

    minimization(dfa);

    std::vector<std::size_t> fill;
    fillpoints(dfa, fill);

    // Convert into ADFA
    DFA* adfa = new DFA(dfa, fill, nullptr/*skeleton*/, cs, {}/*name*/, {}/*cond*/, {}/*line*/);

    adfa->reorder();
    adfa->prepare();
    adfa->calc_stats();

    /* END: Copy from re2c/src/ir/compile.cc */

    std::map<State*, unsigned> state_numbers{};
    auto GetState = [&state_numbers,this](State* p)
    {
        auto i = state_numbers.lower_bound(p);
        if(i == state_numbers.end() || i->first != p)
        {
            unsigned result = statemachine.size();
            statemachine.resize(result+1);
            state_numbers.insert({p, result});
            return result;
        }
        return i->second;
    };
    GetState(adfa->head);
    for(State* s = adfa->head; s; s = s->next)
    {
        // Only save those states that are used
        if(state_numbers.find(s) == state_numbers.end()) continue;

        unsigned state_no = GetState(s), code = 0;
        /*fprintf(stderr, "State %u (%p) action_type(%d) go_type(%d) pre(%d)base(%d),fill(%d):\n",
            state_no, s, s->action.type, s->go.type,
            s->isPreCtxt,s->isBase,int(s->fill)
        );

        if(s->rule)
            fprintf(stderr, "Rule: %d\n", rules.find(s->rule->rank)->second.line);*/

        for(Span* span = s->go.span; span < s->go.span + s->go.nSpans; ++span)
        {
            unsigned end = span->ub;
            int target_state = 0;
            if(span->to->rule)
                target_state = 0x8000 | rules.find(span->to->rule->rank)->second.line;
            else if(span->to->go.type == Go::EMPTY)
                target_state = 0;
            else
            {
                while(span->to->isBase) span->to = span->to->go.span[0].to;
                target_state = GetState(span->to);
            }

            //fprintf(stderr, "\tCodes %s - %s: Target=%p\n", str(code).c_str(), str(end-1).c_str(), span->to);

            auto& s = statemachine[state_no];
            for(; code < end; ++code) s.at(code) = target_state;
        }
    }
    if(save_fp)
    {
        std::fprintf(save_fp,
            "# This file caches the DFA used by DIRR for parsing filenames.\n"
            "# The first number is the hash of DIRR_SETS, the second number is\n"
            "# the number of states and the rest is a RLE-compressed state machine.\n"
            "%llX\n%lu\n",
            hash,
            (unsigned long) statemachine.size());
        for(const auto& a: statemachine)
        {
            unsigned char Buf[3*256+2];
            unsigned sum=0, ptr=0;
            for(unsigned n=0; n<=256; ++n, ++sum)
                if(sum > 0 && (n == 256 || a[n] != a[n-1]))
                {
                    Buf[ptr+2] = sum;
                    Buf[ptr+3] = a[n-1] & 0xFF;
                    Buf[ptr+4] = a[n-1] >> 8;
                    ptr += 3;
                    sum = 0;
                }
            Buf[0] = ptr & 0xFF;
            Buf[1] = ptr >> 8;
            std::fwrite(Buf, 1, ptr+2, save_fp);
        }
        std::fclose(save_fp);
    }
    delete adfa;
}

int DFA_Matcher::Test(const std::string& s, int default_value) const
{
    unsigned cur_state = 0;
    const char* str = s.c_str(); // Use c_str() because we also need '\0'.
    for(std::size_t a=0, b=s.size(); a<=b; ++a) // Use <= to iterate '\0'.
    {
        if(cur_state >= statemachine.size())
            return default_value; // Invalid state machine
        unsigned short code = statemachine[cur_state][ (unsigned char) str[a] ];
        if(code == 0)
            return default_value;
        else if(code & 0x8000)
            return code & 0x7FFF;
        else
            cur_state = code;
    }
    return default_value;
}

DFA_Matcher::~DFA_Matcher()
{
    // Regexps deal with their own deletion.
    collection.clear();
}
