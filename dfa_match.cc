#include <memory>
#include <utility>   // for std::hash
#include <bitset>
#include <unordered_map>
#include <algorithm> // For sort & unique
#include <climits>   // For CHAR_BIT
#include <list>
#include <set>

#include "dfa_match.hh"
#include "likely.hh"
#include "printf.hh"
#include "config.h"

#include <stdlib.h> // For getenv("HOME")

struct NFAnode
{
    // List the target states for each different input symbol.
    // Target state & 0x80000000u means terminal state (accept with state & 0x7FFFFFFFu).
    // Since this is a NFA, each input character may match to a number of different targets.
    std::array<std::set<unsigned/*target node number*/>, 256> targets{};

    // Use std::set rather than std::unordered_set, because we require
    // sorted access for efficient set_intersection (lower_bound).
};

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

static void PutBits(unsigned char* buffer, unsigned& bitpos, unsigned long V, unsigned nbits)
{
    while(nbits > 0)
    {
        unsigned bytepos = bitpos/CHAR_BIT, bits_remain = CHAR_BIT-bitpos%CHAR_BIT, bits_taken = CHAR_BIT-bits_remain;
        unsigned bits_to_write = std::min(nbits, bits_remain);
        unsigned value_mask     = (1 << bits_to_write)-1;
        unsigned value_to_write = V & value_mask;
        buffer[bytepos] = (buffer[bytepos] & ~(value_mask << bits_taken)) | (value_to_write << bits_taken);
        V >>= bits_to_write;
        nbits  -= bits_to_write;
        bitpos += bits_to_write;
    }
}
static unsigned long GetBits(unsigned char* buffer, unsigned& bitpos, unsigned nbits)
{
    unsigned long result = 0, shift=0;
    while(nbits > 0)
    {
        unsigned bytepos = bitpos/CHAR_BIT, bits_remain = CHAR_BIT-bitpos%CHAR_BIT, bits_taken = CHAR_BIT-bits_remain;
        unsigned bits_to_take = std::min(nbits, bits_remain);
        unsigned v = (buffer[bytepos] >> bits_taken) & ((1 << bits_to_take)-1);
        result |= v << shift;
        shift += bits_to_take;
        nbits -= bits_to_take;
        bitpos += bits_to_take;
    }
    return result;
}

static unsigned long LoadVarBit(unsigned char* buffer, unsigned& bitpos)
{
    unsigned long result = 0;
    unsigned n = 2, shift = 0;
    for(;;)
    {
        result |= GetBits(buffer, bitpos, n) << shift;
        if(!GetBits(buffer, bitpos, 1)) break;
        shift += n;
        if(n==2) n+=4; else if(n<10) n+=8;
    }
    return result;
}
static void PutVarBit(unsigned char* buffer, unsigned& bitpos, unsigned long V)
{
    unsigned n = 2;
    for(;;)
    {
        PutBits(buffer, bitpos, V, n);
        V >>= n;
        PutBits(buffer, bitpos, V!=0, 1);
        if(V==0) break;
        if(n==2) n+=4; else if(n<10) n+=8;
    }
}

static void DumpDFA(const char* title, const std::vector<std::array<unsigned,256>>& states)
{
    printf("%s\n", title);
    for(unsigned n=0; n<states.size(); ++n)
    {
        printf("State %u:\n", n);
        const auto& s = states[n];
        std::string prev; unsigned first=0;
        for(unsigned c=0; c<=256; ++c)
        {
            bool flush = false;
            std::string t;
            if(c < 256)
            {
                if(s[c] < states.size())
                    t += Printf(" %u", s[c]);
                else if(s[c] > states.size())
                    t += Printf(" accept %d", s[c] - states.size()-1);
            }
            if(c==256 || t != prev) flush = true;
            if(flush && c > 0 && !prev.empty())
            {
                printf("  in %5s .. %5s: %s\n", str(first).c_str(), str(c-1).c_str(), prev.c_str());
            }
            if(c == 0 || flush) { first = c; prev = t; }
        }
    }
}

static void DumpNFA(const char* title, const std::vector<NFAnode>& states)
{
    printf("%s\n", title);
    for(unsigned n=0; n<states.size(); ++n)
    {
        printf("State %u:\n", n);
        const auto& s = states[n];
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
}

void DFA_Matcher::AddMatch(const std::string& token, bool icase, int target)
{
    matches.emplace_back(token, std::pair<int,bool>{target,icase});
}

void DFA_Matcher::Compile()
{
    // Compose hash string
    {std::string hash_str;
    for(const auto& a: matches)
    {
        unsigned char Buf[16] {}; unsigned ptr = 0;
        PutBits(Buf, ptr, a.second.second, 1);
        PutVarBit(Buf, ptr, a.second.first);
        hash_str.append(Buf, Buf+(ptr+CHAR_BIT-1)/CHAR_BIT);
        hash_str += a.first;
    }
    hash = std::hash<std::string>{}(hash_str);}

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
        unsigned bracketstate = 0;
        // nonzero   = waiting for closing ]
        // &       2 = inverted polarity
        // &FFFF0000 = previous character (for ranges)
        // &    8000 = got a range begin
        std::bitset<256> states;

        for(char c: token)
        {
            if(bracketstate)
            {
                unsigned prev = bracketstate >> 16;
                if(!escmode)
                {
                    if(c == '\\') { escmode = true; continue; }
                    if(states.none() && !(bracketstate&2) && c == '^') { bracketstate |= 2; continue; }
                    if(c == ']')
                    {
                        if(prev) states.set(prev);
                        if(bracketstate&2) states.flip();
                        bracketstate = 0;
                        goto do_newnode;
                    }
                }
                if(escmode || c != '-')
                {
                    if(prev)
                    {
                        if(bracketstate & 0x8000)
                        {
                            for(unsigned m=prev; m<=(unsigned char)c; ++m)
                                states.set(m);
                            bracketstate &= 0x7FFF;
                            continue;
                        }
                        states.set(prev);
                    }
                    bracketstate = (bracketstate & 0xFFFF) | ((c&0xFF)<<16); continue;
                }
                else
                {
                    bracketstate |= 0x8000;
                    continue;
                }
            }
            else if(escmode)
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
                    case '[':
                    {
                        states.reset();
                        bracketstate = 1;
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

    //DumpNFA("Original NFA", nfa_nodes);

    // STEP 2: Convert NFA into DFA
    if(true) // scope
    {
        std::vector<NFAnode> newnodes;
        std::list<unsigned> pending;

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
            std::vector<unsigned char> Buf(targets.size()*8);
            unsigned ptr=0;
            for(auto k: targets) PutVarBit(&Buf[0], ptr, k);
            std::string target_key(&Buf[0], &Buf[(ptr+CHAR_BIT-1)/CHAR_BIT]);
            //fprintf(stderr, "Buf %u bytes -> ptr=%u (%u)\n", unsigned(Buf.size()), ptr, (ptr+CHAR_BIT-1)/CHAR_BIT);

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

    // STEP 3: Convert the DFA into the statemachine structure.
    statemachine.resize(nfa_nodes.size());
    for(unsigned c, n=0; n<nfa_nodes.size(); ++n)
        for(auto& s = nfa_nodes[c=0, n]; c<256; ++c)
        {
            unsigned& v = statemachine[n][c];
            if(s.targets[c].empty()) { v = statemachine.size(); continue; }
            unsigned a = *s.targets[c].begin();
            if(a & 0x80000000u)      { v = statemachine.size() + 1 + (a & 0x7FFFFFFFu); }
            else                     { v = a; }
        }
    nfa_nodes.clear();

    // STEP 4: Finally, minimalize the DFA (state machine).
    if(true)
    {
        //DumpDFA("DFA before minimization", statemachine);

        // Find out if there are any unreachable states in the DFA
        /*std::set<unsigned> reached, unvisited{0};
        while(!unvisited.empty())
        {
            unsigned state = *unvisited.begin();
            unvisited.erase(unvisited.begin());
            reached.insert(state);
            for(auto v: statemachine[state])
                if(v < statemachine.size())
                    if(reached.find(v) == reached.end())
                        unvisited.insert(v);
        }
        if(reached.size() != statemachine.size())
            printf("%u states, but only %u reached\n",
                (int)statemachine.size(), (int)reached.size());*/

        // Groups of states
        typedef std::list<unsigned> group_t;

        // State number to group number mapping
        std::unordered_map<unsigned,unsigned> mapping;

        // Create a single group for all non-accepting states.
        // Collect all states. Assume there are no unvisited states.
        struct groupdata
        {
            group_t group{};
            bool    unused=false;
            unsigned uses=0, originalnumber=0;
        };
        std::vector<groupdata> groups(1);
        for(unsigned n=0; n<statemachine.size(); ++n)
            { groups[0].group.push_back(n); mapping.emplace(n, 0); }

        // Don't create groups for terminal states, since they're
        // not really states, but create a mapping for them, so that
        // we don't need special handling in the group-splitting code.
        for(unsigned n=0; n<statemachine.size(); ++n)
            for(auto v: statemachine[n])
                if(v >= statemachine.size()) // Accepting or failing state
                    mapping.emplace(v, 0x80000000u | v);

    rewritten:;
        for(unsigned groupno=0; groupno<groups.size(); ++groupno)
        {
            group_t& group = groups[groupno].group;
            auto j = group.begin();
            unsigned first_state = *j++;
            if(j == group.end()) continue; // only 1 item

            // For each state in this group that differs
            // from first group, move them into the other group.
            group_t othergroup;
            while(j != group.end())
            {
                unsigned other_state = *j;
                bool differs = false;
                for(unsigned c=0; c<256; ++c)
                {
                    if(mapping.find(statemachine[first_state][c])->second
                    != mapping.find(statemachine[other_state][c])->second)
                    {
                        differs = true;
                        break;
                    }
                }
                if(differs)
                {
                    // Move this element into other group.
                    auto old = j++;
                    othergroup.splice(othergroup.end(), group, old, j);
                }
                else
                    ++j;
            }
            if(!othergroup.empty())
            {
                // Split the differing states into a new group
                unsigned othergroup_id = groups.size();
                // Change the mappings (we need overwriting, so don't use emplace here)
                for(auto n: othergroup) mapping[n] = othergroup_id;
                groups.emplace_back(groupdata{std::move(othergroup)});
                goto rewritten;
            }
        }

        // Sort the groups according to how many times they are
        // referred, so that most referred groups get smallest numbers.
        // This helps shrink down the .dirr_dfa file size.
    recalculate_uses:
        // VERY IMPORTANT: Make sure that whichever group contains the
        // original state 0, emerges as group 0 in the new ordering.
        groups[mapping.find(0)->second].uses = 0x80000000u;
        for(unsigned groupno=0; groupno<groups.size(); ++groupno)
        {
            if(groups[groupno].unused) continue;
            groups[groupno].originalnumber = groupno;

            for(unsigned v: statemachine[*groups[groupno].group.begin()])
                if(v < statemachine.size())
                    ++groups[mapping.find(v)->second].uses;
        }
        // Go over the groups and check if some of them were unused
        bool found_unused = false;
        for(auto& g: groups)
            if(!g.unused && g.uses == 0)
                g.unused = found_unused = true;
        if(found_unused)
            { for(auto& g: groups) g.uses = 0; goto recalculate_uses; }

        // Sort groups according to their use.
        // The group that contained state 0 will emerge as group 0.
        std::sort(groups.begin(), groups.end(), [](const groupdata& a, const groupdata& b)
        {
            return a.uses > b.uses;
        });

        // Delete groups that are never used
        groups.erase(std::remove_if(groups.begin(),groups.end(),[](const groupdata& g)
        {
            return g.unused;
        }), groups.end());

        // Rewrite the mappings to reflect the new sorted order
        {std::unordered_map<unsigned,unsigned> rewritten_mappings;
        for(unsigned groupno=0; groupno<groups.size(); ++groupno)
            rewritten_mappings.emplace(groups[groupno].originalnumber, groupno);
        for(auto& m: mapping)
            if(!(m.second & 0x80000000u))
            {
                auto i = rewritten_mappings.find(m.second);
                if(i != rewritten_mappings.end())
                    m.second = i->second;
                else
                    m.second = ~0u;
        }   }

        // Create a new state machine based on these groups
        std::vector<std::array<unsigned,256>> newstates(groups.size());
        for(unsigned newstateno=0; newstateno<groups.size(); ++newstateno)
        {
            auto& group          = groups[newstateno];
            auto& newstate       = newstates[newstateno];
            // All states in this group are identical. Just take any of them.
            const auto& oldstate = statemachine[*group.group.begin()];
            // Convert the state using the mapping.
            for(unsigned c=0; c<256; ++c)
            {
                unsigned v = oldstate[c];
                if(v >= statemachine.size()) // Accepting or failing state?
                    newstate[c] = v - statemachine.size() + newstates.size();
                else
                    newstate[c] = mapping.find(v)->second; // group number
            }
        }
        statemachine = std::move(newstates);
        //DumpDFA("DFA after minimization", statemachine);
    }
}

bool DFA_Matcher::Load(std::FILE* fp)
{
    char StrBuf[128];
    try {
        while(std::fgets(StrBuf, sizeof StrBuf, fp) && StrBuf[0]=='#') {}
        if(std::stoull(StrBuf, nullptr, 16) == hash)
        {
            // Read the rest of the state machine
            std::vector<unsigned char> Buf(32);
            std::fread(&Buf[0], 1, Buf.size(), fp);
            unsigned position   = 0;
            unsigned num_states = LoadVarBit(&Buf[0], position);
            unsigned num_bits   = LoadVarBit(&Buf[0], position);

            if(num_states*sizeof(statemachine[0]) > 2000000)
                throw std::invalid_argument("implausible num_states");

            unsigned num_bytes = (num_bits+CHAR_BIT-1)/CHAR_BIT;
            if(num_bytes > Buf.size())
            {
                unsigned already = Buf.size(), missing = num_bytes - already;
                Buf.resize(num_bytes);
                std::fread(&Buf[already], 1, missing, fp);
            }

            statemachine.resize(num_states);
            for(unsigned state_no=0; state_no<statemachine.size(); ++state_no)
            {
                auto& data = statemachine[state_no];
                //fprintf(stderr, "%u:", state_no);
                for(unsigned last = 0; last < 256 && position < num_bits; )
                {
                    unsigned end   = LoadVarBit(&Buf[0], position) + last;
                    unsigned value = LoadVarBit(&Buf[0], position);
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
    std::fprintf(save_fp,
        "# This file caches the DFA used by DIRR for parsing filenames.\n"
        "# The first number is the hash of DIRR_COLORS in hexadecimal.\n"
        "# The rest is a bitstream of a RLE-compressed state machine.\n"
        "%llX\n",
        hash);

    std::vector<unsigned char> Buf;
    unsigned numbits=0;
    for(;;)
    {
        Buf.resize(256);
        unsigned ptr=0;
        PutVarBit(&Buf[0], ptr, statemachine.size());
        PutVarBit(&Buf[0], ptr, numbits);
        for(const auto& a: statemachine)
            for(unsigned sum=0, n=0; n<=256; ++n, ++sum)
                if(sum > 0 && (n == 256 || a[n] != a[n-1]))
                {
                    if(ptr/CHAR_BIT + 16 > Buf.size())
                        Buf.resize(ptr/CHAR_BIT + 128);

                    PutVarBit(&Buf[0], ptr, sum);
                    PutVarBit(&Buf[0], ptr, a[n-1]);
                    sum = 0;
                }
        if(ptr == numbits) break;
        numbits = ptr;
    }
    std::fwrite(&Buf[0], 1, (numbits+CHAR_BIT-1)/CHAR_BIT, save_fp);
}

int DFA_Matcher::Test(const std::string& s, int default_value) const
{
    if(unlikely(statemachine.empty())) return default_value;
    unsigned cur_state = 0;
    const char* str = s.c_str(); // Use c_str() because we also need '\0'.
    for(std::size_t a=0, b=s.size(); a<=b; ++a) // Use <= to iterate '\0'.
    {
        cur_state = statemachine[cur_state][ (unsigned char) str[a] ];
        if(cur_state >= statemachine.size())
        {
            cur_state -= statemachine.size();
            return cur_state ? cur_state-1 : default_value;
        }
    }
    return default_value;
}
