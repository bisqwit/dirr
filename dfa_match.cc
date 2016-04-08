//#define DEBUG

#include <memory>
#include <utility>   // for std::hash
#include <array>
#include <vector>
#include <bitset>
#include <unordered_map>
#include <algorithm> // For sort, unique, min
#include <climits>   // For CHAR_BIT
#include <list>
#include <set>

#ifdef DEBUG
 #include <iostream> // For std::cout, used in DumpNFA and DumpDFA
 #include <iomanip>
#endif

#include "dfa_match.hh"

#ifdef __GNUC__
# define likely(x)       __builtin_expect(!!(x), 1)
# define unlikely(x)     __builtin_expect(!!(x), 0)
#else
# define likely(x)   (x)
# define unlikely(x) (x)
#endif

/* CONFIGURATION OPTIONS */

// Whether to minimize the NFA as it is being generated.
// Recommended, as it makes pretty every step of Compile() faster,
// at the expense of slightly more work during the first phase.
static constexpr bool NFA_PRE_MINIMIZATION         = true;

// Whether to minimize the DFA (recommended, as it brings down
// the size of the state machine, and helps it stay better in cache)
static constexpr bool DFA_DO_MINIMIZATION          = true;

// Whether to check for unused nodes before minimizing the DFA
// (currently does not seem to be helpful)
static constexpr bool DFA_PRE_CHECK_UNUSED_NODES   = false;

// Whether to delete unused nodes after minimizing the DFA
// (currently seems to help a bit)
static constexpr bool DFA_POST_DELETE_UNUSED_NODES = true;

// Whether to sort the DFA so most used nodes are first
// (it may help with cache efficiency, and it will make
// the Save() data smaller)
static constexpr bool DFA_POST_SORT_BY_USECOUNT    = true;


/* INTERNAL OPTIONS */

// NFA number space:
//  0<=n<NFAOPT_NEWNODE_OFFSET                  : Space for NFA node numbers after parsing (non-accepting states)
//  NFAOPT_NEWNODE_OFFSET<=n<NFA_TERMINAL_OFFSET: Space for NFA node numbers after determinization (non-accepting states)
//  NFA_TERMINAL_OFFSET<=n                      : Space for "target" values (accept states)
static constexpr unsigned NFA_TERMINAL_OFFSET               = 0x80000000u;
static constexpr unsigned NFAOPT_NEWNODE_OFFSET             = 0x10000000u;

// DFA numbers space during minimization:
//  0<=n<DFAOPT_TERMINAL_OFFSET : Space for DFA node numbers (non-accepting states)
//  DFAOPT_TERMINAL_OFFSET<=n   : Space for "target" values (accept states)
static constexpr unsigned DFAOPT_TERMINAL_OFFSET            = 0x80000000u;

// A high enough value that it's guaranteed to put this group
// on top of the sorting list, but not too high that it will
// cause arithmetic overflow when incrementing the usecount:
static constexpr unsigned DFAOPT_IMPLAUSIBLE_HIGH_USE_COUNT = 0x80000000u;


/* Bit I/O */
static void PutBits(void* memory, unsigned& bitpos, unsigned long long V, unsigned nbits)
{
    unsigned char* buffer = reinterpret_cast<unsigned char*>(memory);
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

static unsigned long long GetBits(const void* memory, unsigned& bitpos, unsigned nbits)
{
    const unsigned char* buffer = reinterpret_cast<const unsigned char*>(memory);
    unsigned long long result = 0;
    for(unsigned shift=0; nbits > 0; )
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

/* Variable bit I/O */
#ifndef PUTBIT_OPTIMIZER
static constexpr std::initializer_list<unsigned> VarBitCounts = {2,3,3,1,1,8,6,8,8,8,8,8,8};
static constexpr char hash_offset = '*';
#else
static std::vector<unsigned> VarBitCounts = {2,3,3,1,1,8,6,8,8,8,8,8,8};
static char hash_offset = '*';
#endif

// VarBitCounts[] is hand-optimized to produce the smallest save file for DIRR.
static unsigned long long LoadVarBit(const void* buffer, unsigned& bitpos)
{
    unsigned long long result = 0;
    unsigned shift = 0;
    for(unsigned n: VarBitCounts)
    {
        result |= GetBits(buffer, bitpos, n) << shift;
        if(!GetBits(buffer, bitpos, 1) || unlikely(shift >= sizeof(result)*CHAR_BIT)) break;
        shift += n;
    }
    return result;
}
static void PutVarBit(void* buffer, unsigned& bitpos, unsigned long long V)
{
    for(unsigned n: VarBitCounts)
    {
        PutBits(buffer, bitpos, V, n);
        V >>= n;
        PutBits(buffer, bitpos, V!=0, 1);
        if(V==0) break;
    }
}

static void PutVarBit(std::vector<char>& buffer, unsigned& bitpos, unsigned long long V)
{
    // Make sure there's room to work
    if(buffer.size() < (bitpos/CHAR_BIT) + 32)
    {
        buffer.resize(buffer.size() + (bitpos/CHAR_BIT) + 128);
    }
    PutVarBit(&buffer[0], bitpos, V);
}

/* Private data */
struct DFA_Matcher::Data
{
    mutable std::vector<char> hash_buf{};

    /* Collect data from AddMatch() */
    struct Match { std::string token; int target; bool icase; };
    std::vector<Match> matches{};

    /* state number -> { char number -> code }
     *              code: =numstates = fail
     *                    >numstates = target color +numstates+1
     *                    <numstates = new state number
     */
    std::vector<std::array<unsigned,256>> statemachine{};

public:
    void RecheckHash() const
    {
        // Check whether the hash is already valid
        if(likely(!hash_buf.empty())) return;
        // Compose hash string
        unsigned ptr = 0;
        PutVarBit(hash_buf, ptr, matches.size());
        for(const auto& a: matches)
        {
            for(char c: a.token) PutVarBit(hash_buf, ptr, (c-hash_offset) & 0xFF);
            PutVarBit(hash_buf, ptr, (0x00-hash_offset)&0xFF);
            PutVarBit(hash_buf, ptr, a.target);
            PutBits(&hash_buf[0], ptr, a.icase, 1);
        }
        hash_buf.resize((ptr+CHAR_BIT-1)/CHAR_BIT);
    }
    void LoadFromHash()
    {
        // Recompose matches[] from hash_buf
        if(hash_buf.empty()) return;
        unsigned ptr = 0, nmatches = LoadVarBit(&hash_buf[0], ptr);
        matches.assign(nmatches, Match{});
        for(auto& m: matches)
        {
            while(ptr/CHAR_BIT < hash_buf.size())
            {
                char c = (LoadVarBit(&hash_buf[0], ptr) + hash_offset) & 0xFF;
                if(!c) break;
                m.token += c;
            }
            m.target = LoadVarBit(&hash_buf[0], ptr);
            m.icase  = GetBits(&hash_buf[0], ptr, 1);
        }
    }
};

// List the target states for each different input symbol.
// Target state >= NFA_TERMINAL_OFFSET means terminal state (accept with state - NFA_TERMINAL_OFFSET).
// Since this is a NFA, each input character may match to a number of different targets.
typedef std::array<std::set<unsigned/*target node number*/>, 256> NFAnode;
// Use std::set rather than std::unordered_set, because we require
// sorted access for efficient set_intersection (lower_bound).


/* Facilities for read/write locks */

#if __cplusplus >= 201402L
# define lock_reading(lk,l) std::shared_lock<std::shared_timed_mutex> lk(l)
# define lock_writing(lk,l) std::lock_guard<std::shared_timed_mutex> lk(l)
#else
# define lock_reading(lk,l) std::lock_guard<std::mutex> lk(l)
# define lock_writing(lk,l) std::lock_guard<std::mutex> lk(l)
#endif


/* Debugging facilities */

#ifdef DEBUG
static std::ostream& str(std::ostream& o, char c)
{
    switch(c)
    {
        case '\n': return o << R"(\n)";
        case '\r': return o << R"(\r)";
        case '\t': return o << R"(\t)";
        case '\b': return o << R"(\b)";
        case '\0': return o << R"(\0)";
        case '\\': return o << R"(\\)";
        case '\"': return o << R"(\")";
        case '\'': return o << R"(\')";
        default: if(c<32 || c==127) return
                    o << std::showbase << std::hex << (c & 0xFFu);
    }
    return o << char(c);
}

static void DumpDFA(std::ostream& o, const char* title, const std::vector<std::array<unsigned,256>>& states)
{
    o << title << '\n';
    for(unsigned n=0; n<states.size(); ++n)
    {
        o << "State " << std::dec << n << ":\n";
        const auto& s = states[n];
        std::string prev; unsigned first=0;
        for(unsigned c=0; c<=256; ++c)
        {
            bool flush = false;
            std::string t;
            if(c < 256)
            {
                if(s[c] < states.size())
                    { t += ' '; t += std::to_string(s[c]); }
                else if(s[c] > states.size())
                    { t += " accept "; t += std::to_string(s[c] - states.size()-1); }
            }
            if(c==256 || t != prev) flush = true;
            if(flush && c > 0 && !prev.empty())
            {
                str(str(o << "  in " << std::setw(4), first) << " .. " << std::setw(4), c-1) << ": " << prev << '\n';
            }
            if(c == 0 || flush) { first = c; prev = t; }
        }
    }
}

static void DumpNFA(std::ostream& o, const char* title, const std::vector<NFAnode>& states)
{
    o << title << '\n';
    for(unsigned n=0; n<states.size(); ++n)
    {
        o << "State " << std::dec << n << ":\n";
        const auto& s = states[n];
        std::string prev; unsigned first=0;
        for(unsigned c=0; c<=256; ++c)
        {
            bool flush = false;
            std::string t;
            if(c < 256)
            {
                for(auto d: s[c])
                    if(d >= NFA_TERMINAL_OFFSET)
                        { t += " accept "; t += std::to_string(d - NFA_TERMINAL_OFFSET); }
                    else
                        { t += ' '; t += std::to_string(d); }
            }
            if(c==256 || t != prev) flush = true;
            if(flush && c > 0 && !prev.empty())
            {
                str(str(o << "  in " << std::setw(4), first) << " .. " << std::setw(4), c-1) << ": " << prev << '\n';
            }
            if(c == 0 || flush) { first = c; prev = t; }
        }
    }
}
#endif

/* The public API */

DFA_Matcher::DFA_Matcher() : data(new Data) {}
DFA_Matcher::DFA_Matcher(const DFA_Matcher& b) : data(nullptr) { operator=( b ); }
DFA_Matcher& DFA_Matcher::operator=(const DFA_Matcher& b)
{
    lock_writing(lk, lock);
    delete data;
    data = b.data ? new Data(*b.data) : nullptr;
    return *this;
}
DFA_Matcher::~DFA_Matcher()
{
    lock_writing(lk, lock);
    delete data;
}
DFA_Matcher::DFA_Matcher(DFA_Matcher&& b) noexcept : data(nullptr) { operator=( std::move(b) ); }
DFA_Matcher& DFA_Matcher::operator=(DFA_Matcher&& b) noexcept
{
    lock_writing(lk,  lock);
    lock_writing(lk2, b.lock);
    delete data;
    data = b.data;
    b.data = nullptr;
    return *this;
}

void DFA_Matcher::AddMatch(const std::string& token, bool icase, int target)
{
    lock_writing(lk, lock);
    if(unlikely(!data)) throw std::runtime_error("AddMatch called on deleted DFA_Matcher");

    if(target < 0 || target >= 0x80000000l)
        throw std::out_of_range("AddMatch: target must be in range 0..7FFFFFFF, " + std::to_string(target) + " given");

    data->matches.emplace_back(Data::Match{token,target,icase});
    data->hash_buf.clear();
}

void DFA_Matcher::AddMatch(std::string&& token, bool icase, int target)
{
    lock_writing(lk, lock);
    if(unlikely(!data)) throw std::runtime_error("AddMatch called on deleted DFA_Matcher");

    if(target < 0 || target >= 0x80000000l)
        throw std::out_of_range("AddMatch: target must be in range 0..7FFFFFFF, " + std::to_string(target) + " given");

    data->matches.emplace_back(Data::Match{std::move(token),target,icase});
    data->hash_buf.clear();
}

int DFA_Matcher::Test(const std::string& s, int default_value) const noexcept
{
    lock_reading(lk, lock);
    if(unlikely(!Valid())) return default_value;

    const auto& statemachine = data->statemachine;
    unsigned cur_state = 0;
    for(std::size_t a=0, b=s.size(); a<=b; ++a) // Use <= to iterate '\0'.
    {
        // Note: C++98 guarantees that operator[] const with index==size()
        //       returns a reference to '\0'.
        //       C++11 added the same guarantee to non-const operator[].
        //       Therefore, we do not need to do c_str() in this function.
        cur_state = statemachine[cur_state][ (unsigned char)s[a] ];
        if(cur_state >= statemachine.size())
        {
            cur_state -= statemachine.size();
            return cur_state ? cur_state-1 : default_value;
        }
    }
    return default_value;
}

bool DFA_Matcher::Load(std::istream&& f, bool ignore_hash)
{
    return Load(f, ignore_hash); // Calls lvalue reference version
}

bool DFA_Matcher::Load(std::istream& f, bool ignore_hash)
{
    lock_writing(lk, lock);

    if(unlikely(!data))
    {
        if(!ignore_hash) return false;
        data = new Data;
    }
    data->RecheckHash();

    std::vector<char> Buf(32);
    f.read(&Buf[0], Buf.size());
    unsigned position    = 0;
    unsigned num_bits    = LoadVarBit(&Buf[0], position);
    unsigned num_states  = LoadVarBit(&Buf[0], position);
    unsigned hash_length = LoadVarBit(&Buf[0], position);

    if(!ignore_hash && hash_length != data->hash_buf.size()) return false;

    if(hash_length > 0x200000) return false; // Implausible length
    if(num_states*sizeof(data->statemachine[0]) > 8000000
    || num_bits > num_states*256*42)
    {
        // implausible num_states
        return false;
    }
    unsigned num_bytes = (num_bits+CHAR_BIT-1)/CHAR_BIT;
    if(num_bytes > Buf.size())
    {
        unsigned already = Buf.size(), missing = num_bytes - already;
        Buf.resize(num_bytes);
        f.read(&Buf[already], missing);
    }

    std::vector<char> loaded_hash;
    for(unsigned l=0; l<hash_length; ++l)
    {
        if(position >= num_bits) return false;
        loaded_hash.push_back( LoadVarBit(&Buf[0], position) );
    }
    if(!ignore_hash && loaded_hash != data->hash_buf) return false;

    std::vector<std::array<unsigned,256>> newstatemachine(num_states);
    for(unsigned state_no = 0; state_no < num_states; ++state_no)
    {
        auto& target = newstatemachine[state_no];
        for(unsigned last = 0; last < 256; )
        {
            if(position >= num_bits) return false;
            unsigned end   = LoadVarBit(&Buf[0], position) + last;
            unsigned value = LoadVarBit(&Buf[0], position);
            if(end==last) end = last+256;
            while(last < end && last < 256) target[last++] = value;
        }
    }
    if(position != num_bits) return false;
    data->statemachine = std::move(newstatemachine);
#ifndef PUTBIT_OPTIMIZER
    data->matches.clear(); // Don't need it anymore
#endif
    if(!ignore_hash)
    {
        data->hash_buf = std::move(loaded_hash);
        data->LoadFromHash();
    }
    return true;
}

void DFA_Matcher::Save(std::ostream&& f) const
{
    Save(f); // Calls lvalue reference version
}

void DFA_Matcher::Save(std::ostream& f) const
{
    lock_writing(lk, lock);

    if(unlikely(!data)) return;
#ifdef PUTBIT_OPTIMIZER
    data->hash_buf.claer();
#endif
    data->RecheckHash();

    std::vector<char> Buf;
    unsigned numbits=0;
    for(unsigned round=0; ; )
    {
        unsigned ptr=0;
        PutVarBit(Buf, ptr, numbits);
        PutVarBit(Buf, ptr, data->statemachine.size());
        PutVarBit(Buf, ptr, data->hash_buf.size());
        for(char c: data->hash_buf) PutVarBit(Buf, ptr, c&0xFF);

        for(const auto& a: data->statemachine)
            for(unsigned sum=0, n=0; n<=256; ++n, ++sum)
                if(sum > 0 && (n == 256 || a[n] != a[n-1]))
                {
                    PutVarBit(Buf, ptr, sum);
                    PutVarBit(Buf, ptr, a[n-1]);
                    sum = 0;
                }
        // TODO: Figure out whether this loop is guaranteed
        //       to terminate eventually
        if(ptr == numbits) break;
        numbits = ptr;

        if(++round >= 256)
        {
            throw std::runtime_error("Potentially infinite loop in DFA_Matcher::Save()");
        }
    }
    f.write(&Buf[0], (numbits+CHAR_BIT-1)/CHAR_BIT);
}

void DFA_Matcher::Compile()
{
    lock_writing(lk, lock);
    if(unlikely(!data)) return;

    // STEP 1: Generate NFA
    std::vector<NFAnode> nfa_nodes(1); // node 0 = root

    // Parse each wildmatch expression into the NFA.
    // First, sort the matches so that we get a deterministic hash.
    /*std::sort(data->matches.begin(), data->matches.end(),
              [](const auto& a, const auto& b)
              { return (a.target!=b.target) ? a.target<b.target
                     : (a.token!=b.token) ? a.token<b.token
                     : a.icase<b.icase; });*/
    data->RecheckHash(); // Calculate the hash before deleting matches[]
    // Otherwise Save() will fail

    for(auto& m: data->matches)
    {
        std::bitset<256> states;

        auto addtransition = [&](unsigned root, unsigned newnewnode = ~0u) -> unsigned
        {
            if(NFA_PRE_MINIMIZATION)
            {
                // Check if there already is an node number that exists
                // on this root in all given target states but in none others.
                // This is quite optional, but it dramatically reduces
                // the number of NFA nodes, making NFA-DFA conversion faster
                // and produce fewer nodes, which in turn makes DFA minimization faster.
                // It is a form of pre-minimization.
                auto& cur = nfa_nodes[root];
                std::set<unsigned> include, exclude;
                bool first = true;
                for(unsigned c=0; c<256; ++c)
                {
                    const auto& list = cur[c];
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
            }

            unsigned next = newnewnode;
            if(next == ~0u)
            {
                // Make new node
                next = nfa_nodes.size();
                nfa_nodes.resize(next+1);
            }

            auto& cur = nfa_nodes[root];
            for(unsigned c=0; c<256; ++c)
                if(states.test(c))
                    cur[c].insert(next);
            return next;
        };

        /* Translate a glob pattern (wildcard match) into a NFA */
        std::string&& token = std::move(m.token);
        const int    target = m.target;
        const bool    icase = m.icase;

        unsigned pos=0;
        auto end  = [&] { return pos >= token.size(); };
        auto cget = [&] { return end() ? 0x00 : (unsigned char) token[pos++]; };
        auto unget = [&] { if(pos>0) --pos; };

        std::vector<unsigned> rootnodes = { 0 };

        while(!end())
            switch(unsigned c = cget())
            {
                case '*':
                {
                    states.set(); states.reset(0x00); // 01..FF, i.e. all but 00
                    // Repeat count of zero is allowed, so we begin with the same roots
                    std::set<unsigned> newroots(rootnodes.begin(), rootnodes.end());
                    // In each root, add an instance of these states
                    unsigned newnewnode = ~0u;
                    for(auto root: rootnodes)
                    {
                        unsigned nextnode = addtransition(root, newnewnode);
                        if(nextnode != root)
                        {
                            // Loop the next node into itself
                            for(unsigned c=0x01; c<0x100; ++c)
                                nfa_nodes[nextnode][c].insert(nextnode);
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
                case '\\': // ESCAPE
                {
                    switch(c = cget())
                    {
                        case 'd':
                        {
                            states.reset();
                            for(unsigned a='0'; a<='9'; ++a) states.set(a);
                            goto do_newnode;
                        }
                        case 'w':
                        {
                            states.reset();
                            for(unsigned a='0'; a<='9'; ++a) states.set(a);
                            for(unsigned a='A'; a<='Z'; ++a) states.set(a);
                            for(unsigned a='a'; a<='z'; ++a) states.set(a);
                            goto do_newnode;
                        }
                        case 'x':
                        {
                            #define x(c) (((c)>='0'&&(c)<='9') ? ((c)-'0') : ((c)>='A'&&(c)<='F') ? ((c)-'A'+10) : ((c)-'a'+10))
                            states.reset();
                            c = cget(); if(!std::isxdigit(c)) { unget(); states.set('x'); goto do_newnode; }
                            unsigned hexcode = x(c);
                            c = cget(); if(std::isxdigit(c)) { hexcode = hexcode*16 + x(c); } else unget();
                            states.set(hexcode);
                            #undef x
                            goto do_newnode;
                        }
                        case '\\': //passthru
                        default:
                        {
                            states.reset(); states.set(c);
                            goto do_newnode;
                        }
                    }
                    break;
                }
                case '[':
                {
                    bool inverse = false;
                    states.reset();
                    if(cget() == '^') inverse = true; else unget();
                    while(!end())
                    {
                        c = cget();
                        if(c == ']') break;
                        unsigned begin = c; if(c == '\\') begin = cget();
                        if(cget() != '-') { unget(); states.set(c); continue; }
                        unsigned end   = cget();
                        while(begin <= end) states.set(begin++);
                    }
                    if(inverse) states.flip();
                    goto do_newnode;
                }
                default:
                {
                    states.reset(); states.set(c);
                    if(icase && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
                        states.set(c ^ 0x20);
                do_newnode:;
                    std::set<unsigned> newroots;
                    unsigned newnewnode = ~0u;
                    for(auto root: rootnodes)
                    {
                        unsigned newnode = addtransition(root, newnewnode);
                        // newnewnode records the newly created node, so that in case
                        // we need to create new child nodes in multiple roots, we can
                        // use the same child node for all of them.
                        // This helps keep down the NFA size.
                        if(newnode != root) newnewnode = newnode;
                        newroots.insert(newnode);
                    }
                    rootnodes.assign(newroots.begin(), newroots.end());
                }
            }
        // Add the end-of-stream tag
        for(auto root: rootnodes)
            nfa_nodes[root][0x00].insert(target + NFA_TERMINAL_OFFSET);
    }
    // Don't need the matches[] anymore, since it's all assembled in nfa_nodes[]
#ifndef PUTBIT_OPTIMIZER
    data->matches.clear();
#endif

#ifdef DEBUG
    DumpNFA(std::cout, "Original NFA", nfa_nodes);
#endif

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
        //         the new map. New nodes are indicated being offset by NFAOPT_NEWNODE_OFFSET.
        auto AddSet = [&](std::vector<unsigned>&& targets)
        {
            auto l = targets.rbegin();
            if(l != targets.rend() && (*l >= NFA_TERMINAL_OFFSET))
            {
                // If one of these targets is an accept, keep one.
                // If there are many, the grammar is ambiguous, but
                // we will only keep one (the highest-numbered).
                return *l;
            }

            // Find a newnode that combines the given target nodes.
            std::string target_key;
            for(auto k: targets)
                target_key.append(reinterpret_cast<const char*>(&k), sizeof(k));

            // Find a node for the given list of targets
            auto i = combination_map.find(target_key);
            if(i != combination_map.end())
                return i->second;

            // Create if necessary.
            unsigned target_id = nodecounter++;
            if(target_id <= sources.size()) sources.resize(target_id+1);
            sources[target_id] = std::move(targets);
            // Don't resize newnodes[] here, as there are still references to it.
            pending.push_back(target_id);
            combination_map.emplace(target_key, target_id + NFAOPT_NEWNODE_OFFSET);
            return target_id + NFAOPT_NEWNODE_OFFSET;
        };
        AddSet({0});

        while(!pending.empty())
        {
            // Choose one of the pending states, and generate a new node for it.
            // Numbers in pending[] always refer to new nodes even without NFAOPT_NEWNODE_OFFSET.
            unsigned chosen = *pending.begin();
            unsigned cap   = *pending.rbegin();
            pending.erase(pending.begin());
            if(cap >= newnodes.size()) newnodes.resize(cap+1);
            NFAnode& st = newnodes[chosen];

            // Merge the transitions from all sources states of that new node.
            for(unsigned c=0; c<256; ++c)
            {
                std::vector<unsigned> merged_targets;
                for(auto src: sources[chosen])
                    if(src >= NFA_TERMINAL_OFFSET)
                        merged_targets.push_back(src);
                    else
                    {
                        const auto& source = (src >= NFAOPT_NEWNODE_OFFSET) ? newnodes[src - NFAOPT_NEWNODE_OFFSET]
                                                                            : nfa_nodes[src];
                        merged_targets.insert(merged_targets.end(),
                                              source[c].begin(),
                                              source[c].end());
                    }
                if(merged_targets.empty()) continue;
                // Sort and keep only unique values in the merged_targets.
                std::sort(merged_targets.begin(), merged_targets.end());
                merged_targets.erase(std::unique(merged_targets.begin(), merged_targets.end()), merged_targets.end());

                st[c].insert( AddSet(std::move(merged_targets)) );
            }
        }

        // Last: Fix up the node numbers, by removing the NFAOPT_NEWNODE_OFFSET
        // which was used to distinguish newnodes from the old nfa_nodes.
        nfa_nodes = std::move(newnodes);
        for(auto& n: nfa_nodes)
            for(auto& c: n)
            {
                // c should have 0 or 1 elements here.
                auto s = std::move(c);
                for(auto e: s) c.insert( (e >= NFA_TERMINAL_OFFSET) ? e : (e - NFAOPT_NEWNODE_OFFSET) );
            }
    } // scope for NFA-to-DFA
    // The NFA now follows DFA constraints.
#ifdef DEBUG
    DumpNFA(std::cout, "MERGED NODES", nfa_nodes);
#endif

    // STEP 3: Convert the DFA into the statemachine structure.
    data->statemachine.resize(nfa_nodes.size());
    for(unsigned c, n=0; n<nfa_nodes.size(); ++n)
        for(auto& s = nfa_nodes[c=0, n]; c<256; ++c)
        {
            unsigned& v = data->statemachine[n][c];
            if(s[c].empty()) { v = data->statemachine.size(); continue; }
            unsigned a = *s[c].begin();
            if(a >= NFA_TERMINAL_OFFSET) { v = data->statemachine.size() + 1 + (a - NFA_TERMINAL_OFFSET); }
            else                         { v = a; }
        }
    nfa_nodes.clear();

    // STEP 4: Finally, minimalize the DFA (state machine).
    if(DFA_DO_MINIMIZATION)
    {
#ifdef DEBUG
        DumpDFA(std::cout, "DFA before minimization", data->statemachine);
#endif

        // Find out if there are any unreachable states in the DFA
        if(DFA_PRE_CHECK_UNUSED_NODES)
        {
            std::set<unsigned> reached, unvisited{0};
            while(!unvisited.empty())
            {
                unsigned state = *unvisited.begin();
                unvisited.erase(unvisited.begin());
                reached.insert(state);
                for(auto v: data->statemachine[state])
                    if(v < data->statemachine.size())
                        if(reached.find(v) == reached.end())
                            unvisited.insert(v);
            }
        #ifdef DEBUG
            // I have never seen this find unused states.
            if(reached.size() != data->statemachine.size())
                std::cerr << data->statemachine.size()
                          << " states, but only " << reached.size()
                          << " reached\n";
        #endif
        }

        // Groups of states
        typedef std::list<unsigned> group_t;

        // State number to group number mapping
        std::unordered_map<unsigned,unsigned> mapping;

        // Create a single group for all non-accepting states.
        // Collect all states. Assume there are no unvisited states.
        struct groupdata
        {
            group_t group;
            bool    unused{};
            unsigned uses{}, originalnumber{};
            groupdata() : group{} {}
            groupdata(const group_t& g): group(g) {}
            groupdata(group_t&& g): group(std::move(g)) {}
            groupdata(groupdata&&) = default;
            groupdata& operator=(groupdata&&) = default;
        };
        std::vector<groupdata> groups(1);
        for(unsigned n=0; n<data->statemachine.size(); ++n)
            { groups[0].group.push_back(n); mapping.emplace(n, 0); }

        // Don't create groups for terminal states, since they are
        // not really states, but create a mapping for them, so that
        // we don't need special handling in the group-splitting code.
        for(unsigned n=0; n<data->statemachine.size(); ++n)
            for(auto v: data->statemachine[n])
                if(v >= data->statemachine.size()) // Accepting or failing state
                    mapping.emplace(v, (v - data->statemachine.size()) + DFAOPT_TERMINAL_OFFSET);

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
                    if(mapping.find(data->statemachine[first_state][c])->second
                    != mapping.find(data->statemachine[other_state][c])->second)
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
                groups.emplace_back(std::move(othergroup));
                goto rewritten;
            }
        }

        // Sort the groups according to how many times they are
        // referred, so that most referred groups get smallest numbers.
        // This helps shrink down the .dirr_dfa file size.
    recalculate_uses:
        if(DFA_POST_DELETE_UNUSED_NODES || DFA_POST_SORT_BY_USECOUNT)
        {
            // VERY IMPORTANT: Make sure that whichever group contains the
            // original state 0, emerges as group 0 in the new ordering.
            groups[mapping.find(0)->second].uses = DFAOPT_IMPLAUSIBLE_HIGH_USE_COUNT;
            for(unsigned groupno=0; groupno<groups.size(); ++groupno)
            {
                if(groups[groupno].unused) continue;
                groups[groupno].originalnumber = groupno;

                for(unsigned v: data->statemachine[*groups[groupno].group.begin()])
                    if(v < data->statemachine.size())
                        ++groups[mapping.find(v)->second].uses;
            }
        }
        if(DFA_POST_DELETE_UNUSED_NODES)
        {
            // Go over the groups and check if some of them were unused
            bool found_unused = false;
            for(auto& g: groups)
                if(!g.unused && g.uses == 0)
                    g.unused = found_unused = true;
            if(found_unused)
                { for(auto& g: groups) g.uses = 0; goto recalculate_uses; }
            // Didn't find new unused groups.
            // Now delete groups that were previously found to be unused.
            groups.erase(std::remove_if(groups.begin(),groups.end(),[](const groupdata& g)
            {
                return g.unused;
            }), groups.end());
        }
        if(DFA_POST_SORT_BY_USECOUNT)
        {
            // Sort groups according to their use.
            // The group that contained state 0 will emerge as group 0.
            std::sort(groups.begin(), groups.end(), [](const groupdata& a, const groupdata& b)
            {
                return a.uses > b.uses;
            });
        }
        if(DFA_POST_SORT_BY_USECOUNT || DFA_POST_DELETE_UNUSED_NODES)
        {
            // Rewrite the mappings to reflect the new sorted order
            std::unordered_map<unsigned,unsigned> rewritten_mappings;
            for(unsigned groupno=0; groupno<groups.size(); ++groupno)
                rewritten_mappings.emplace(groups[groupno].originalnumber, groupno);
            for(auto& m: mapping)
                if(m.second < DFAOPT_TERMINAL_OFFSET)
                {
                    auto i = rewritten_mappings.find(m.second);
                    if(i != rewritten_mappings.end())
                        m.second = i->second;
                    else
                        m.second = ~0u;
                }
        }

        // Create a new state machine based on these groups
        std::vector<std::array<unsigned,256>> newstates(groups.size());
        for(unsigned newstateno=0; newstateno<groups.size(); ++newstateno)
        {
            auto& group          = groups[newstateno];
            auto& newstate       = newstates[newstateno];
            // All states in this group are identical. Just take any of them.
            const auto& oldstate = data->statemachine[*group.group.begin()];
            // Convert the state using the mapping.
            for(unsigned c=0; c<256; ++c)
            {
                unsigned v = oldstate[c];
                if(v >= data->statemachine.size()) // Accepting or failing state?
                    newstate[c] = v - data->statemachine.size() + newstates.size();
                else
                    newstate[c] = mapping.find(v)->second; // group number
            }
        }
        data->statemachine = std::move(newstates);
#ifdef DEBUG
        DumpDFA(std::cout, "DFA after minimization", data->statemachine);
#endif
    }
}

bool DFA_Matcher::Valid() const noexcept
{
    lock_reading(lk, lock);
    return data && !data->statemachine.empty();
}
