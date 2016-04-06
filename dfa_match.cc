#include <memory>
#include <utility> // for std::hash
#include <bitset>
#include <unordered_map>
#include <algorithm>
#include <deque>
#include <set>

#include "dfa_match.hh"
#include "printf.hh"

#include <stdlib.h> // For getenv("HOME")

static std::string str(char c)
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
}

void DFA_Matcher::AddMatch(const std::string& token, bool icase, int target)
{
    matches.emplace_back(token, std::pair<int,bool>{target,icase});
}

void DFA_Matcher::Compile()
{
    // Compose hash string
    std::string hash_str;
    for(const auto& a: matches)
        { hash_str += char(a.second.second); hash_str += std::to_string(a.second.first); }
    for(const auto& a: matches)
        { hash_str += a.first; hash_str += '\2'; }
    hash = std::hash<std::string>{}(hash_str);

    std::string hash_save_fn;
    std::FILE* save_fp = nullptr;
    for(const std::string& path: std::initializer_list<const char*>{getenv("HOME"),"",getenv("TEMP"),getenv("TMP"),"/tmp"})
    {
        hash_save_fn = path + "/.dirr_dfa";
        std::FILE* fp = std::fopen(hash_save_fn.c_str(), "rb");
        if(fp)
        {
            bool loaded = Load(fp);
            std::fclose(fp);
            if(loaded)
            {
                matches.clear(); //Don't need it anymore
                return;
            }
        }
        // Incorrect hash. Regenerate file.
        fp = std::fopen(hash_save_fn.c_str(), "wb");
        if(fp) { save_fp = fp; break; }
    }

    Generate();

    if(save_fp)
    {
        Save(save_fp);
        std::fclose(save_fp);
    }
}

void DFA_Matcher::Generate()
{
    // STEP 1: Generate NFA
    struct NFAnode
    {
        // List the target states for each different input symbol.
        // Target state & 0x80000000u means terminal state (accept with state & 0x7FFFFFFFu).
        // Since this is a NFA, each input character may match to a number of different targets.
        std::array<std::set<unsigned/*target node number*/>, 256> targets{};
    };
    std::vector<NFAnode> nfa_nodes(1); // node 0 = root

    // Parse each wildmatch expression into the NFA.
    for(auto& m: matches)
    {
        std::string&& token = std::move(m.first);
        const int    target = m.second.first;
        const bool    icase = m.second.second;

        auto makenode = [&]
        {
            unsigned result = nfa_nodes.size();
            nfa_nodes.resize(result+1);
            return result;
        };

        std::vector<unsigned> rootnodes = { 0 };

        auto addtransition = [&](unsigned root, const std::bitset<256>& states, unsigned newnewnode = ~0u) -> unsigned
        {
            // Check if there already is an node number that exists
            // on this root in all given target states but in none others.
            // This is quite optional, but it reduces the number of NFA nodes.
            // It is a form of pre-minimization.
            auto& cur = nfa_nodes[root];
            std::set<unsigned> include, exclude;
            bool first = true;
            for(unsigned c=0; c<256; ++c)
            {
                const auto& list = cur.targets[c];
                if(states.test(c))
                {
                    if(first)
                    {
                        first = false;
                        include.insert( list.begin(), list.end() );
                    }
                    else
                    {
                        if(include.empty()) break;
                        // Delete those include-elements that are not in list[]
                        // (Set intersection)
                        auto j = list.begin();
                        bool first = true;
                        for(auto i = include.begin(); i != include.end(); )
                        {
                            if(first)
                                { j = list.lower_bound(*i); first = false; }
                            else
                                while(j != list.end() && *j < *i) ++j;
                            if(j == list.end() || *j != *i)
                                i = include.erase(i);
                            else
                                ++i;
                        }
                    }
                }
                else
                {
                    exclude.insert( list.begin(), list.end() );
                }
            }
            for(auto e: exclude) include.erase(e);
            if(!include.empty()) return *include.begin();

            unsigned next = newnewnode;
            if(next == ~0u) next = makenode();

            auto& cur1 = nfa_nodes[root];
            for(unsigned c=0; c<256; ++c)
                if(states.test(c))
                    cur1.targets[c].insert(next);
            return next;
        };

        /* Translate a glob pattern (wildcard match) into a NFA*/
        bool escmode = false;
        for(char c: token)
        {
            std::bitset<256> states;
            if(escmode)
            {
                switch(c)
                {
                    case 'd':
                    {
                        states.reset();
                        for(unsigned a='0'; a<='9'; ++a) states.set(a);
                    do_newnode:;
                        std::set<unsigned> newroots;
                        unsigned newnewnode = ~0u;
                        for(auto root: rootnodes)
                        {
                            unsigned newnode = addtransition(root, states, newnewnode);
                            // newnewnode records the newly created node, so that in case
                            // we need to create new child nodes in multiple roots, we can
                            // use the same child node for all of them.
                            // This helps keep down the NFA size.
                            if(newnode != root) newnewnode = newnode;
                            newroots.insert(newnode);
                        }
                        rootnodes.assign(newroots.begin(), newroots.end());
                        break;
                    }
                    case 'w':
                    {
                        states.reset();
                        for(unsigned a='0'; a<='9'; ++a) states.set(a);
                        for(unsigned a='A'; a<='Z'; ++a) states.set(a);
                        for(unsigned a='a'; a<='z'; ++a) states.set(a);
                        goto do_newnode;
                    }
                    case '\\': //passthru
                    default:
                    {
                        states.reset(); states.set(c);
                        goto do_newnode;
                    }
                }
                escmode = false;
            }
            else
            {
                // not escmode
                switch(c)
                {
                    case '*':
                    {
                        states.set(); states.reset(0x00);
                        // Repeat count of zero is allowed, so we begin with the same roots
                        std::set<unsigned> newroots(rootnodes.begin(), rootnodes.end());
                        // In each root, add an instance of these states
                        unsigned newnewnode = ~0u;
                        for(auto root: rootnodes)
                        {
                            unsigned nextnode = addtransition(root, states, newnewnode);
                            if(nextnode != root)
                            {
                                // Loop the next node into itself
                                for(unsigned c=0x01; c<0x100; ++c)
                                    nfa_nodes[nextnode].targets[c].insert(nextnode);
                                newnewnode = nextnode;
                            }
                            newroots.insert(nextnode);
                        }
                        rootnodes.assign(newroots.begin(), newroots.end());
                        break;
                    }
                    case '?':
                    {
                        states.set(); states.reset(0x00); // 01..FF, i.e. all but 00
                        goto do_newnode;
                    }
                    case '\\':
                    {
                        escmode = true;
                        break;
                    }
                    default:
                    {
                        states.reset(); states.set(c);
                        if(icase && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
                            states.set(c ^ 0x20);
                        goto do_newnode;
                    }
                }
            }
        }
        // Add the end-of-stream tag
        for(auto root: rootnodes)
            nfa_nodes[root].targets[0x00].insert(target | 0x80000000u);
    }
    // Don't need the matches[] anymore, since it's all assembled in nfa_nodes[]
    matches.clear();

    auto DumpNFA = [](const char* title, std::vector<NFAnode>& nodes)
    {
        printf("%s\n", title);
        for(unsigned n=0; n<nodes.size(); ++n)
        {
            printf("State %u:\n", n);
            const auto& s = nodes[n];
            std::string prev; unsigned first=0;
            for(unsigned c=0; c<=256; ++c)
            {
                bool flush = false;
                std::string t;
                if(c < 256)
                    for(auto d: s.targets[c])
                        if(d & 0x80000000u)
                            t += Printf(" accept %d", d & 0x7FFFFFFFu);
                        else
                            t += Printf(" %u", d);
                if(c==256 || t != prev) flush = true;
                if(flush && c > 0 && !prev.empty())
                {
                    printf("  in %5s .. %5s: %s\n", str(first).c_str(), str(c-1).c_str(), prev.c_str());
                }
                if(c == 0 || flush) { first = c; prev = t; }
            }
        }
    };
    //DumpNFA("Original NFA", nfa_nodes);

    // STEP 2: Convert NFA into DFA
    if(true) // scope
    {
        std::vector<NFAnode> newnodes;
        std::deque<unsigned> pending;

        // List of original state numbers -> new state number
        std::unordered_map<std::string, unsigned> combination_map;
        std::vector<std::vector<unsigned>> sources;
        unsigned nodecounter=0;

        // AddSet: Generate a new node from the combination of given nodes
        //         Node numbers may refer to the original map, or to
        //         the new map. New nodes are indicated by 0x40000000u bit.
        auto AddSet = [&](std::vector<unsigned>&& targets)
        {
            auto l = targets.rbegin();
            if(l != targets.rend() && *l & 0x80000000u)
            {
                // If one of these targets is an accept, keep one.
                // If there are many, the grammar is ambiguous.
                return *l;
            }

            std::string target_key;
            for(auto k: targets)
                { target_key += '_'; target_key += std::to_string(k); }

            // Find a node for the given list of targets
            auto i = combination_map.find(target_key);
            if(i != combination_map.end())
                return i->second;

            // Create if necessary.
            unsigned target_id = nodecounter++;
            if(target_id <= sources.size()) sources.resize(target_id+1);
            sources[target_id] = std::move(targets);
            // Don't resize newnodes[] here, as there are still references to it.
            pending.push_back(target_id |= 0x40000000u);
            combination_map.emplace(target_key, target_id);
            return target_id;
        };
        AddSet({0});

        while(!pending.empty())
        {
            // Choose one of the pending states, and generate a new node for it.
            unsigned chosen = *pending.begin() & 0x3FFFFFFFu;
            unsigned cap = *pending.rbegin() & 0x3FFFFFFFu;
            pending.erase(pending.begin());
            if(cap >= newnodes.size()) newnodes.resize(cap+1);
            NFAnode& st = newnodes[chosen];

            // Merge the transitions from all sources states of that new node.
            for(unsigned c=0; c<256; ++c)
            {
                std::vector<unsigned> merged_targets;
                for(auto src: sources[chosen])
                    if(src & 0x80000000u)
                        merged_targets.push_back(src);
                    else
                    {
                        const auto& source = (src & 0x40000000u) ? newnodes[src & 0x3FFFFFFFu]
                                                                 : nfa_nodes[src];
                        merged_targets.insert(merged_targets.end(),
                                              source.targets[c].begin(),
                                              source.targets[c].end());
                    }
                if(merged_targets.empty()) continue;
                std::sort(merged_targets.begin(), merged_targets.end());
                merged_targets.erase(std::unique(merged_targets.begin(), merged_targets.end()), merged_targets.end());

                st.targets[c].insert( AddSet(std::move(merged_targets)) );
            }
        }

        // Last: Fix up the node numbers, by removing the 0x40000000 bit
        // which was used to distinguish newnodes from the old nfa_nodes.
        nfa_nodes = std::move(newnodes);
        for(auto& n: nfa_nodes)
            for(auto& c: n.targets)
            {
                std::set<unsigned> conv_set;
                for(auto e: c) conv_set.insert(e & ~0x40000000u);
                c = std::move(conv_set);
            }
    } // scope for NFA-to-DFA
    // The NFA now follows DFA constraints.
    //DumpNFA("MERGED NODES", nfa_nodes);

    // STEP 3: Finally, minimize the DFA.
    // TODO

    // STEP 4: Convert the DFA into the statemachine structure.
    statemachine.resize(nfa_nodes.size());
    for(unsigned c, n=0; n<nfa_nodes.size(); ++n)
        for(auto& s = nfa_nodes[c=0, n]; c<256; ++c)
            statemachine[n][c] = s.targets[c].empty() ? 0
                : ( (*s.targets[c].begin() / 0x80000000u)
                + 2*(*s.targets[c].begin() & 0x7FFFFFFFu));
}

bool DFA_Matcher::Load(std::FILE* fp)
{
    auto LoadVarLen = [](const unsigned char* buffer, unsigned& ptr)
    {
        unsigned long V=0, Shift=0, C;
        do V |= ((unsigned long)(C = buffer[ptr++]) & 0x7F) << (Shift++)*7; while(C & 0x80);
        return V;
    };
    auto LoadVarLenFP = [](std::FILE* fp)
    {
        unsigned long V=0, Shift=0, C;
        do V |= ((unsigned long)(C = std::fgetc(fp)) & 0x7F) << (Shift++)*7; while(C & 0x80);
        return V;
    };

    char StrBuf[128];
    try {
        while(std::fgets(StrBuf, sizeof StrBuf, fp) && StrBuf[0]=='#') {}
        if(std::stoull(StrBuf, nullptr, 16) == hash)
        {
            // Read the rest of the state machine
            std::fgets(StrBuf, sizeof StrBuf, fp);
            unsigned num_states = std::stoi(StrBuf);

            if(num_states*sizeof(statemachine[0]) > 2000000)
                throw std::invalid_argument("implausible num_states");

            statemachine.resize(num_states);
            for(unsigned state_no=0; state_no<statemachine.size(); ++state_no)
            {
                unsigned char Buf[256*7];
                unsigned n = LoadVarLenFP(fp);
                if(n == 0 || n > sizeof(Buf)) break;
                std::fread(Buf, 1, n, fp);

                auto& data = statemachine[state_no];
                //fprintf(stderr, "%u:", state_no);
                for(unsigned last = 0, position = 0; last < 256 && position < n; )
                {
                    unsigned end   = LoadVarLen(Buf, position) + last;
                    unsigned value = LoadVarLen(Buf, position);
                    if(end==last) end = last+256;
                    while(last < end && last < 256) data[last++] = value;
                }
            }
            return true;
        }
    }
    catch(const std::invalid_argument&)
    {
        // Lands here if std::stoi fails to parse a number.
    }
    return false;
}

void DFA_Matcher::Save(std::FILE* save_fp) const
{
    auto PutVarLen = [](unsigned char* buffer, unsigned& ptr, unsigned long V)
    {
        while(V > 0x7F) { buffer[ptr++] = 0x80 | (V & 0x7F); V >>= 7; }
        buffer[ptr++] = V;
    };

    std::fprintf(save_fp,
        "# This file caches the DFA used by DIRR for parsing filenames.\n"
        "# The first number is the hash of DIRR_COLORS, the second number is\n"
        "# the number of states and the rest is a RLE-compressed state machine.\n"
        "%llX\n%lu\n",
        hash,
        (unsigned long) statemachine.size());
    for(const auto& a: statemachine)
    {
        // upper bound for varlen(value): ceil(32/7) = 5
        // upper bound for varlen(sum):   ceil( 8/7) = 2
        // upper bound for buffer: 256*7
        // upper bound for varlen(buflen): ceil(log2(256*7)/7) = 2
        const unsigned offset = 8; // just in case
        unsigned char Buf[offset+256*7];
        unsigned sum=0, ptr=0;
        for(unsigned n=0; n<=256; ++n, ++sum)
            if(sum > 0 && (n == 256 || a[n] != a[n-1]))
            {
                PutVarLen(Buf+offset, ptr, sum);
                PutVarLen(Buf+offset, ptr, a[n-1]);
                sum = 0;
            }
        unsigned n=0; sum=0;
        PutVarLen(Buf,          n,   ptr);
        PutVarLen(Buf+offset-n, sum, ptr);
        std::fwrite(Buf+offset-n, 1, n+ptr, save_fp);
    }
}

int DFA_Matcher::Test(const std::string& s, int default_value) const
{
    unsigned cur_state = 0;
    const char* str = s.c_str(); // Use c_str() because we also need '\0'.
    for(std::size_t a=0, b=s.size(); a<=b; ++a) // Use <= to iterate '\0'.
    {
        if(cur_state >= statemachine.size()) return default_value; // Invalid state machine
        unsigned code = statemachine[cur_state][ (unsigned char) str[a] ];
        if(code == 0) return default_value;
        if(code & 1)  return code >> 1;
        else          cur_state = code >> 1;
    }
    return default_value;
}
