//#define DEBUG

#include <list>
#include <array>
#include <vector>
#include <bitset>
#include <unordered_map>

#include <climits>   // For CHAR_BIT
#include <utility>   // for std::hash
#include <algorithm> // For sort, unique, min

#ifdef DEBUG
 #include <iostream> // For std::cout, used in DumpNFA and DumpDFA
 #include <iomanip>
#endif

#include "dfa_match.hh"

#include <assert.h>

#ifdef __GNUC__
# define likely(x)       __builtin_expect(!!(x), 1)
# define unlikely(x)     __builtin_expect(!!(x), 0)
#else
# define likely(x)   (x)
# define unlikely(x) (x)
#endif

/* CONFIGURATION OPTIONS */

// Whether to try and avoid redundancy in NFA as it is being generated.
// Slows down NFA generation a bit, but greatly rewarded in later phases.
// Benefits of >= 1: Makes determinization and minimization a lot faster.
// Benefits of >= 2: Makes determinization and minimization even faster.
// Valid values: 0, 1, 2.   2 = hardest minimization
static constexpr unsigned NFA_PRE_MINIMIZATION_LEVEL = 2;

// Whether to simplify rules that end in *, to not test whether '\0' is found
// Benefits: Huge improvements in statemachine size and minimization speed.
// Note: May change results in ambiguous grammars.
static constexpr bool NFA_SIMPLIFY_ACCEPTS         = true;

// Whether to minimize the DFA
// Benefits: Cache efficiency; Save-data will be a lot smaller.
static constexpr bool DFA_DO_MINIMIZATION          = true;

// Whether to delete unused nodes before minimizing the DFA
// Benefits: TBD
static constexpr bool DFA_PRE_DELETE_UNUSED_NODES  = true;

// Whether to delete unused nodes after minimizing the DFA
// Benefits: TBD
static constexpr bool DFA_POST_DELETE_UNUSED_NODES = true;

// Whether to sort the DFA so most used nodes are first.
// Benefits: Cache efficiency; Save-data will be smaller.
static constexpr bool DFA_POST_SORT_BY_USECOUNT    = true;


/* INTERNAL OPTIONS */

// NFA number space:
//  0<=n<NFAOPT_ACCEPT_OFFSET : Space for NFA node numbers (non-accepting states)
//  NFA_ACCEPT_OFFSET<=n      : Space for "target" values (accept states)
static constexpr unsigned NFA_ACCEPT_OFFSET               = 0x80000000u;

// DFA numbers space during minimization:
//  0<=n<DFAOPT_ACCEPT_OFFSET : Space for DFA node numbers (non-accepting states)
//  DFAOPT_ACCEPT_OFFSET<=n   : Space for "target" values (accept+fail states)
static constexpr unsigned DFAOPT_ACCEPT_OFFSET            = 0x80000000u;

// Character set size.
static constexpr unsigned CHARSET_SIZE = (1 << CHAR_BIT);



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
        result |= (unsigned long long)v << shift;
        shift += bits_to_take;
        nbits -= bits_to_take;
        bitpos += bits_to_take;
    }
    return result;
}

/* Variable bit I/O */
#ifndef PUTBIT_OPTIMIZER
static constexpr std::initializer_list<unsigned> VarBitCounts = {1,4,3,1,1,7,1,6,8,8,8,8,8,8,8};
static constexpr char hash_offset = '*';
#else
static std::vector<unsigned> VarBitCounts = {1,4,3,1,1,7,1,6,8,8,8,8,8,8,8};
static char hash_offset = '*';
#endif


// VarBitCounts[] is hand-optimized to produce the smallest save file for DIRR.
static unsigned long long LoadVarBit(const void* buffer, unsigned& bitpos)
{
    unsigned long long result = 0;
    unsigned shift = 0;
    for(unsigned n: VarBitCounts)
    {
        unsigned long long unit = GetBits(buffer, bitpos, n);
        result |= unit << shift;
        //if(unit==0)
        {
            if(!GetBits(buffer, bitpos, 1) || unlikely(shift >= sizeof(result)*CHAR_BIT)) break;
        }
        shift += n;
    }
    return result;
}
static void PutVarBit(void* buffer, unsigned& bitpos, unsigned long long V)
{
/*
Optimization idea: If first unit is zero, we stop reading
without reading the 1-bit marker.
This requires putting bits in the units most-significant first.

Saving a six bit value "100000" using 1 unit, 2 unit or 3 units:

               (1000)     (1)
    2 bits:       00      00    00
    3 bits:              000  0100
    4 bits:                   0000

 1 unit did not work, because 4 bits were unsaved.
 2 units did not work, because 4 bits were unsaved.
 3 units did not work, because first unit was zero.

This idea does not seem to work...
*/
    for(unsigned n: VarBitCounts)
    {
        unsigned long long unit = V % (1ull << n);
        PutBits(buffer, bitpos, unit, n);
        V >>= n;
        //if(unit==0)
        {
            PutBits(buffer, bitpos, V!=0, 1);
            if(V==0) break;
        }
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
    struct Match
    {
        std::string token;
        int target;
        bool icase;
        #ifdef __SUNPRO_CC
        // This set of methods is only needed because Solaris Studio
        // isn't smart enough to figure them out on its own.
        Match() : token{},target{},icase{} {}
        template<typename T>
        Match(T&& s, int i, bool c) : token(std::forward<T>(s)),target(i),icase(c) {}
        Match(const Match&)=default;
        Match(Match&&)=default;
        Match& operator=(const Match&)=default;
        Match& operator=(Match&&)=default;
        #endif
    };
    std::vector<Match> matches{};

    /* state number -> { char number -> code }
     *              code: =numstates = fail
     *                    >numstates = target color +numstates+1
     *                    <numstates = new state number
     */
    std::vector<std::array<unsigned,CHARSET_SIZE>> statemachine{};

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
            PutBits(&hash_buf[0], ptr, a.icase, 1);
            PutVarBit(hash_buf, ptr, a.target);
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
            m.icase  = GetBits(&hash_buf[0], ptr, 1);
            m.target = LoadVarBit(&hash_buf[0], ptr);
        }
    }
};


/* SometimesSortedVector is a wrapper for std::vector<T> that seeks a middle
 * ground between std::set and std::vector.
 * Upon request, it holds the sorted-unique property (elements are in ascending
 * order, and same element is stored only twice), but an option is also to save
 * without ordering, using a simple push_back.
 */
template<typename T>
class SometimesSortedVector
{
private:
    std::vector<T> data{};
    bool sorted{true};
public:
    using size_type = typename std::vector<T>::size_type;
    using reference = typename std::vector<T>::reference;
    using const_reference = typename std::vector<T>::const_reference;
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;
    using reverse_iterator = typename std::vector<T>::reverse_iterator;
    using const_reverse_iterator = typename std::vector<T>::const_reverse_iterator;
public:
    SometimesSortedVector(const SometimesSortedVector<T>& b) = default;
    SometimesSortedVector(SometimesSortedVector<T>&& b) = default;
    SometimesSortedVector& operator=(const SometimesSortedVector<T>&) = default;
    SometimesSortedVector& operator=(SometimesSortedVector<T>&&) = default;
    template<typename...A>
    SometimesSortedVector(A&&...args) : data{std::forward<A>(args)...}, sorted(autodetect()) {}
    template<typename...A>
    void assign(A&&...args) { data.assign(std::forward<A>(args)...); sorted=autodetect(); }
    void clear() { data.clear(); sorted=true; }
    void resize(size_type s) { if(s > size()) sorted = false; data.resize(s); }
    template<typename...A>
    iterator erase(A&&...args) { auto i = data.erase(std::forward<A>(args)...); if(!sorted) sorted=autodetect(); return i; }
    bool empty() const { return data.empty(); }
    size_type size() const { return data.size(); }
    size_type max_size() const { return data.max_size(); }
    size_type max_capacity() const { return data.max_capacity(); }
    void reserve(size_type s) { data.reserve(s); }
    reference front() { return data.front(); } const_reference front() const { return data.front(); }
    reference back() { return data.back(); } const_reference back() const { return data.back(); }
    iterator begin() { return data.begin(); } iterator end() { return data.end(); }
    const_iterator begin() const { return data.begin(); } const_iterator end() const { return data.end(); }
    const_iterator cbegin() const { return data.cbegin(); } const_iterator cend() const { return data.cend(); }
    reverse_iterator rbegin() { return data.rbegin(); } reverse_iterator rend() { return data.rend(); }
    const_reverse_iterator rbegin() const { return data.rbegin(); } const_reverse_iterator rend() const { return data.rend(); }
    const_reverse_iterator crbegin() const { return data.crbegin(); } const_reverse_iterator crend() const { return data.crend(); }
    const_reference operator[](size_type s) const { return data[s]; }
    reference       operator[](size_type s)       { sorted=false; return data[s]; }
public:
    void MakeSureIsSortedAndUnique()
    {
        if(likely(sorted)) return;
        std::sort(data.begin(), data.end());
        data.erase(std::unique(data.begin(),data.end()), data.end());
        sorted = true;
    }
    /* SortedUniqueInsert: Insert an element into the vector,
     * making sure it retains the sorted-unique property.
     */
    template<typename... A>
    void SortedUniqueInsert(A&&... args)
    {
        MakeSureIsSortedAndUnique();
        T value(std::forward<A>(args)...);
        auto i = std::lower_bound(data.begin(), data.end(), value);
        if(i == data.end() || *i != value)
            data.insert(i, std::move(value));
    }
    /* FastInsert: Insert an element into the vector using SortedUniqueInsert
     * if it already has the sorted-unique property; FastestInsert otherwise. */
    void FastInsert(const T& value)
    {
        if(sorted) SortedUniqueInsert(value);
        else       FastestInsert(value);
    }
    /* FastestInsert: Insert an element into the vector fastest way possible. */
    void FastestInsert(const T& value)
    {
        if(data.size()==1 && value==data.back()) return;
        if(sorted && !data.empty() && value > data.back()) { data.push_back(value); return; }
        data.push_back(value);
        sorted = autodetect();
    }
    void FastestInsert(const SometimesSortedVector<T>& other)
    {
        data.insert(data.end(), other.begin(), other.end());
        if(!other.empty())
            sorted = data.size() == other.size() && other.sorted;
    }
    void FastestInsert(SometimesSortedVector<T>&& other)
    {
        data.insert(data.end(), other.begin(), other.end());
        if(!other.empty())
            sorted = data.size() == other.size() && other.sorted;
        other.clear();
    }
    std::vector<T>&& StealSortedVector() { MakeSureIsSortedAndUnique(); return std::move(data); }
private:
    bool autodetect() const { return data.size() <= 1; }
};


/* Range set. To be implemented wiser */
class RangeSet: private std::bitset<CHARSET_SIZE>
{
public:
    using std::bitset<CHARSET_SIZE>::reset;
    using std::bitset<CHARSET_SIZE>::set;
    using std::bitset<CHARSET_SIZE>::test;
    using std::bitset<CHARSET_SIZE>::flip;
    void set_range(unsigned begin, unsigned end)
    {
        while(begin <= end) set(begin++);
    }
};


// List the target states for each different input symbol.
// Target state >= NFA_ACCEPT_OFFSET means accepting state (accept with state - NFA_ACCEPT_OFFSET).
// Since this is a NFA, each input character may match to a number of different targets.
typedef std::array<SometimesSortedVector<unsigned>, CHARSET_SIZE> NFAnode;
// Use std::set rather than std::unordered_set, because we require
// sorted access for efficient set_intersection (lower_bound).


/* Facilities for read/write locks */

#ifndef DFA_DISABLE_MUTEX
#if __cplusplus >= 201402L && !defined(NO_SHARED_TIMED_MUTEX)
# define lock_reading(lk,l) std::shared_lock<std::shared_timed_mutex> lk(l)
# define lock_writing(lk,l) std::lock_guard<std::shared_timed_mutex> lk(l)
#else
# define lock_reading(lk,l) std::lock_guard<std::mutex> lk(l)
# define lock_writing(lk,l) std::lock_guard<std::mutex> lk(l)
#endif
#else
# define lock_reading(lk,l)
# define lock_writing(lk,l)
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

template<typename F>
static void DumpStateMachine(std::ostream& o, const char* title, unsigned begin, unsigned end, F&& GetInfo)
{
    if(title && *title) o << title << '\n';
    for(unsigned n = begin; n < end; ++n)
    {
        std::array<std::string,CHARSET_SIZE> info;
        for(unsigned c=0; c<CHARSET_SIZE; ++c)
        {
            std::string res = GetInfo(n,c);
            if(res.empty()) res = " reject";
            info[c] = std::move(res);
        }

        std::string largest = [&info]
        {
            std::unordered_map<std::string,unsigned> counts;
            for(const auto& s: info) ++counts[s];
            return std::max_element(counts.begin(), counts.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; })->first;
        }();

        o << "  State " << std::dec << n << " ->" << largest << ":\n";

        std::string prev;
        unsigned first=0;
        for(unsigned c=0; c<=CHARSET_SIZE; ++c)
        {
            bool flush = false;
            std::string t;
            if(c < CHARSET_SIZE) t = std::move(info[c]);
            if(c==CHARSET_SIZE || t != prev) flush = true;
            if(flush && c > 0)
            {
                if(prev != largest)
                    str(str(o << "    in " << std::setw(4), first) << " to " << std::setw(4), c-1) << ": " << prev << '\n';
            }
            if(c == 0 || flush) { first = c; prev = t; }
        }
    }
    o << std::dec;
}

static void DumpNFA(std::ostream& o, const char* title, const std::vector<NFAnode>& states,
                    std::size_t begin=0,std::size_t end=~0u)
{
    DumpStateMachine(o, title, begin, std::min(end,states.size()), [&states](unsigned n, unsigned c) -> std::string
    {
        std::string t;
        // If states[n][c] is empty, blank string will be returned.
        for(auto d: states[n][c])
            if(d >= NFA_ACCEPT_OFFSET)
                { t += " accept "; t += std::to_string(d - NFA_ACCEPT_OFFSET); }
            else
                { t += ' '; t += std::to_string(d); }
        return t;
    });
}

static void DumpDFA(std::ostream& o, const char* title, const std::vector<std::array<unsigned,CHARSET_SIZE>>& states,
                    std::size_t begin=0,std::size_t end=~0u)
{
    DumpStateMachine(o, title, begin,std::min(end,states.size()), [&states](unsigned n, unsigned c) -> std::string
    {
        std::string t;
        auto& d = states[n][c];
        if(d > states.size())
            { t += " accept "; t += std::to_string(d - states.size()-1); }
        else if(d < states.size())
            { t += ' '; t += std::to_string(d); }
        return t;
    });
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

    if(target < 0)
        throw std::out_of_range("AddMatch: target must be in range 0..7FFFFFFF, " + std::to_string(target) + " given");

    data->matches.emplace_back(Data::Match{token,target,icase});
    data->hash_buf.clear();
}

void DFA_Matcher::AddMatch(std::string&& token, bool icase, int target)
{
    lock_writing(lk, lock);
    if(unlikely(!data)) throw std::runtime_error("AddMatch called on deleted DFA_Matcher");

    if(target < 0)
        throw std::out_of_range("AddMatch: target must be in range 0..7FFFFFFF, " + std::to_string(target) + " given");

    data->matches.emplace_back(Data::Match{std::move(token),target,icase});
    data->hash_buf.clear();
}

int DFA_Matcher::Test(const std::string& s, int default_value) const noexcept
{
    lock_reading(lk, lock);
    if(unlikely(!data || data->statemachine.empty())) return default_value;

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
    if(!f.good()) return false;
    unsigned position    = 0;
    unsigned num_bits    = LoadVarBit(&Buf[0], position);
    unsigned num_states  = LoadVarBit(&Buf[0], position);
    unsigned hash_length = LoadVarBit(&Buf[0], position);

    if(!ignore_hash && hash_length != data->hash_buf.size()) return false;

    if(hash_length > 0x200000) return false; // Implausible length
    if(num_states*sizeof(data->statemachine[0]) > 8000000
    || num_bits > num_states*CHARSET_SIZE*42)
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

    std::vector<std::array<unsigned,CHARSET_SIZE>> newstatemachine(num_states);
    for(unsigned state_no = 0; state_no < num_states; ++state_no)
    {
        auto& target = newstatemachine[state_no];
        for(unsigned last = 0; last < CHARSET_SIZE; )
        {
            if(position >= num_bits) return false;
            unsigned end   = LoadVarBit(&Buf[0], position) + last;
            unsigned value = LoadVarBit(&Buf[0], position);
            if(end==last) end = last+CHARSET_SIZE;
            while(last < end && last < CHARSET_SIZE) target[last++] = value;
        }
    }
    // Allow maximum of 16 bits of blank in the end
    if(long(position) > long(num_bits) || long(position)+16 < long(num_bits)) return false;

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
    data->hash_buf.clear();
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
            for(unsigned sum=0, n=0; n<=CHARSET_SIZE; ++n, ++sum)
                if(sum > 0 && (n == CHARSET_SIZE || a[n] != a[n-1]))
                {
                    PutVarBit(Buf, ptr, sum);
                    PutVarBit(Buf, ptr, a[n-1]);
                    sum = 0;
                }
        if(ptr == numbits) break;

        // This loop does not necessarily terminate eventually.
        if(++round >= 8 && ptr < numbits)
        {
            PutBits(&Buf[0], ptr, 0, std::max(numbits,ptr) - std::min(numbits,ptr));
            numbits = ptr;
            break;
        }

        numbits = ptr;
    }
    f.write(&Buf[0], (numbits+CHAR_BIT-1)/CHAR_BIT);
}

class NFAcompiler
{
    std::vector<NFAnode> nfa_nodes{};

private:
    // PHASE 1 FUNCTIONS:
    std::vector<SometimesSortedVector<unsigned>> parents{};
    std::vector<bool> deadnodes{};
    RangeSet states{};

    unsigned AddTransition(unsigned root, unsigned newnewnode)
    {
        if(NFA_PRE_MINIMIZATION_LEVEL >= 1)
        {
            // Check if there already is an node number that exists
            // on this root in all given target states but in none others.
            // This is quite optional, but it dramatically reduces
            // the number of NFA nodes, making NFA-DFA conversion faster and
            // produce fewer nodes, which in turn makes DFA minimization faster.
            // It is a form of pre-minimization.
            auto& cur = nfa_nodes[root];
            SometimesSortedVector<unsigned> exclude, include;
            //printf("Considering root %d\n", root);
            for(unsigned c=0; c<CHARSET_SIZE; ++c)
            {
                auto& list = cur[c];
                if(!states.test(c))
                {
                    if(list.size()==1)
                        exclude.FastInsert(list.front());
                    else
                        exclude.FastestInsert(list);
                }
                else
                {
                    list.MakeSureIsSortedAndUnique();
                    if(include.empty())
                    {
                        include = list; // Here's our list of candidates now.
                        // Erase accepting state numbers, as these are not really states
                        include.erase(std::remove_if(include.begin(),include.end(),[](unsigned n)
                        {
                            return n >= NFA_ACCEPT_OFFSET;
                        }), include.end());
                    }
                    else
                    {
                        // For each include-element that is not in list[], delete it
                        // In other words, keep only those that are in both.
                        auto j = list.begin();
                        bool first = true;
                        for(auto i = include.begin(); i != include.end(); )
                        {
                            if(first)
                            {
                                j = std::lower_bound(list.begin(),list.end(), *i);
                                first = false;
                            }
                            else
                                while(j != list.end() && *j < *i) ++j;
                            if(j == list.end() || *j != *i)
                                i = include.erase(i);
                            else
                                ++i;
                        }
                    }
                    // If, as a consequence, the include list became empty,
                    // we are done; nothing to do here.
                    if(include.empty()) goto could_not_minimize;
                }
            }
            // include[] now has got a list of elements that exists in all
            // of those states that we want. Any of those is a valid candidate,
            // as long as it is _not_ found in exclude[].
            // If include[] has even a single element that is not found in exclude[],
            // return that element.
            exclude.MakeSureIsSortedAndUnique();
            auto i = exclude.cend();
            for(auto elem: include)
            {
                if(i == exclude.cend())
                    i = std::lower_bound(exclude.cbegin(), exclude.cend(), elem);
                else
                    while(i != exclude.cend() && *i < elem) ++i;
                if(i == exclude.cend() || *i != elem)
                {
                    // Reuse the arcs, and return the existing target node.
                    return elem;
                }
            }
        }
    could_not_minimize:;

        unsigned next = newnewnode;
        if(next == ~0u)
        {
            // Make new node
            next = nfa_nodes.size();
            nfa_nodes.resize(next+1);
            if(NFA_PRE_MINIMIZATION_LEVEL >= 2 || NFA_SIMPLIFY_ACCEPTS)
                parents.resize(next+1);
        }

        auto& cur = nfa_nodes[root];
        // TODO: Use _Find_next() or equivalent
        for(unsigned c=0; c<CHARSET_SIZE; ++c)
            if(states.test(c))
                cur[c].FastestInsert(next);

        if(next < parents.size() && (NFA_PRE_MINIMIZATION_LEVEL >= 2 || NFA_SIMPLIFY_ACCEPTS))
            parents[next].FastestInsert(root);
        return next;
    }

public:
    /* Translate a glob pattern (wildcard match) into a NFA */
    void AddMatch(std::string&& token, bool icase, int target)
    {
        unsigned pos=0;
        // Solaris Studio has problems with these lambdas, so using #defines instead.
        #ifdef __SUNPRO_CC
        # define t_end()   (pos >= token.size())
        # define t_cget()  (t_end() ? 0x00 : (unsigned char)token[pos++])
        # define t_unget() (pos>0 && --pos)
        #else
        auto t_end   = [&] { return pos >= token.size(); };
        auto t_cget  = [&] { return pos >= token.size() ? 0x00 : (token[pos++] & 0xFF); };
        auto t_unget = [&] { if(pos>0) --pos; };
        #endif

        SometimesSortedVector<unsigned> rootnodes{0u}; // Add 0 as root.
        if(nfa_nodes.empty()) nfa_nodes.emplace_back(); // Create node 0

        while(!t_end())
            switch(unsigned c = t_cget())
            {
                case '*':
                {
                    states.reset(); states.set_range(0x01,0xFF);
                    // Repeat count of zero is allowed, so we begin with the same roots
                    // In each root, add an instance of these states
                    unsigned newnewnode = ~0u;
                    for(unsigned a=0, b=rootnodes.size(); a<b; ++a) // b caches the size before we started adding
                    {
                        unsigned root = rootnodes[a], nextnode = AddTransition(root, newnewnode);
                        if(nextnode != root)
                        {
                            // Loop the next node into itself
                            AddTransition(nextnode, nextnode);

                            newnewnode = nextnode;
                            rootnodes.FastestInsert(nextnode);
                        }
                    }
                    rootnodes.MakeSureIsSortedAndUnique();
                    break;
                }
                case '?':
                {
                    states.reset(); states.set_range(0x01,0xFF);
                    goto do_newnode;
                }
                case '\\': // ESCAPE
                {
                    switch(c = t_cget())
                    {
                        case 'd':
                        {
                            states.reset();
                            states.set_range('0', '9');
                            goto do_newnode;
                        }
                        case 'w':
                        {
                            states.reset();
                            states.set_range('0', '9');
                            states.set_range('A', 'Z');
                            states.set_range('a', 'z');
                            goto do_newnode;
                        }
                        case 'x':
                        {
                            #define x(c) (((c)>='0'&&(c)<='9') ? ((c)-'0') : ((c)>='A'&&(c)<='F') ? ((c)-'A'+10) : ((c)-'a'+10))
                            states.reset();
                            c = t_cget(); if(!std::isxdigit(c)) { t_unget(); states.set('x'); goto do_newnode; }
                            unsigned hexcode = x(c);
                            c = t_cget(); if(std::isxdigit(c)) { hexcode = hexcode*16 + x(c); } else t_unget();
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
                    if(t_cget() == '^') inverse = true; else t_unget();
                    while(!t_end())
                    {
                        c = t_cget();
                        if(c == ']') break;
                        unsigned begin = c; if(c == '\\') begin = t_cget();
                        if(t_cget() != '-') { t_unget(); states.set(c); continue; }
                        unsigned end   = t_cget();
                        states.set_range(begin, end);
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
                    SometimesSortedVector<unsigned> newroots;
                    unsigned newnewnode = ~0u;
                    for(auto root: rootnodes)
                    {
                        unsigned newnode = AddTransition(root, newnewnode);
                        // newnewnode records the newly created node, so that in case
                        // we need to create new child nodes in multiple roots, we can
                        // use the same child node for all of them.
                        // This helps keep down the NFA size.
                        if(newnode != root) newnewnode = newnode;
                        newroots.FastestInsert(newnode);
                    }
                    rootnodes = std::move(newroots);
                    rootnodes.MakeSureIsSortedAndUnique();
                }
            }
        // Add the end-of-stream tag
        states.reset(); states.set(0x00);
        for(auto root: rootnodes)
            AddTransition(root, target + NFA_ACCEPT_OFFSET);
    }

    void RemapDuplicate(unsigned duplicate, unsigned better)
    {
        // Change all references to duplicate in parentnode into references to n
        parents[duplicate].MakeSureIsSortedAndUnique();
        for(auto parent: parents[duplicate])
            for(auto& c: nfa_nodes[parent])
                for(auto& b: c)
                    if(b == duplicate) b = better;
        // And dummy out the unused node
        if(duplicate >= deadnodes.size()) deadnodes.resize(duplicate+1);
        deadnodes[duplicate] = true;
        for(auto& c: nfa_nodes[duplicate]) c.clear();
    }

    void CheckDuplicate(unsigned root)
    {
        // Check if there is another node that has identical states as root;
        // if there is, replace all references to "root" to point to that another node.
        if(root < deadnodes.size() && deadnodes[root]) return;
        auto& rootnode = nfa_nodes[root];
        for(auto& c: rootnode) c.MakeSureIsSortedAndUnique();
        for(unsigned n=0; n<nfa_nodes.size(); ++n)
            if(n != root)
            {
                if(n < deadnodes.size() && deadnodes[n])
                {
                    notequal: continue;
                }
                auto& node = nfa_nodes[n];
                for(auto& c: node) c.MakeSureIsSortedAndUnique();
                for(unsigned c=0; c<CHARSET_SIZE; ++c)
                    if(node[c].size() != rootnode[c].size()
                    || !std::equal(node[c].begin(), node[c].end(), rootnode[c].begin()))
                        goto notequal;
                // Found equal!
                #ifdef DEBUG
                DumpNFA(std::cout, "Found identical state", nfa_nodes, root,root+1);
                DumpNFA(std::cout, "Merging to",            nfa_nodes, n, n+1);
                #endif
                RemapDuplicate(root, n);
                // Recursively apply remdup to all parents
                for(auto r: parents[root])
                    CheckDuplicate(r);
            }
    }

    void SimplifyAcceptingStates()
    {
        // If there is a node where all states lead into
        // either itself, or into the same accepting state,
        // change all its parent pointers into acception
        for(unsigned root=nfa_nodes.size(); root-- > 0; )
        {
            unsigned accept=0;
            for(auto& t: nfa_nodes[root])
            {
                if(t.empty()) goto non_candidate1; // neither accepting nor self-pointing
                for(auto v: t)
                    if(v >= NFA_ACCEPT_OFFSET)
                    {
                        if(accept == 0) accept = v;
                        else if(v != accept) goto non_candidate1;
                    }
                    else if(v != root) goto non_candidate1;
            }
            if(accept == 0) continue;
            RemapDuplicate(root, accept);
            //DumpNFA(std::cout, "Outcome", nfa_nodes);
        non_candidate1:;
        }
    }

    void Minimize()
    {
#ifdef DEBUG
        std::cout << "=====================================================\n";
        DumpNFA(std::cout, "Before NFA minimization", nfa_nodes);
        std::cout << "-----------------------------------------------------\n";
#endif
        // Find all nodes that contain only accepting states
        for(unsigned n=nfa_nodes.size(); n-- > 0; )
        {
            /*for(auto& t: nfa_nodes[n])
                for(auto v: t)
                    if(v < NFA_ACCEPT_OFFSET)
                        goto non_candidate2;*/
            CheckDuplicate(n);
        //non_candidate2:;
        }

        unsigned n_nodes_to_keep = nfa_nodes.size();
        while(n_nodes_to_keep > 0 && (n_nodes_to_keep-1) < deadnodes.size() && deadnodes[n_nodes_to_keep-1])
            --n_nodes_to_keep;
        if(n_nodes_to_keep < nfa_nodes.size())
        {
            nfa_nodes.resize(n_nodes_to_keep);
            deadnodes.resize(n_nodes_to_keep);
            parents.resize(n_nodes_to_keep);
        }
    }

private:
    // PHASE 2 functions
    std::list<std::pair<unsigned/*node id*/,std::vector<unsigned>/*sources*/>> pending{};
    // List of original state numbers -> new state number
    std::unordered_map<std::string, unsigned> combination_map{};
    unsigned nodecounter{};

    // AddSet: Generate a new node from the combination of given nodes
    //         Node numbers may refer to the original map, or to
    //         the new map. New nodes are indicated being offset by new_node_offset.
    unsigned AddSet(std::vector<unsigned>&& targets)
    {
        auto l = targets.rbegin();
        if(l != targets.rend() && (*l >= NFA_ACCEPT_OFFSET))
        {
            // If one of these targets is an accept, keep one.
            // If there are many, the grammar is ambiguous, but
            // we will only keep one (the highest-numbered).
            return *l;
        }

        // Find a newnode that combines the given target nodes.
        std::string target_key;
        target_key.reserve(targets.size()*sizeof(unsigned));
        for(auto k: targets)
            target_key.append(reinterpret_cast<const char*>(&k), sizeof(k));

        // Find a node for the given list of targets
        auto i = combination_map.find(target_key);
        if(i != combination_map.end())
            return i->second;

        // Create if necessary.
        unsigned target_id = nodecounter++;
        pending.emplace_back(target_id, std::move(targets));
        // Don't resize nfa_nodes[] here, as there are still references to it.
        combination_map.emplace(target_key, target_id);
        return target_id;
    }

public:
    void Determinize()
    {
#ifdef DEBUG
        DumpNFA(std::cout, "Original NFA", nfa_nodes);
#endif
        unsigned number_of_old_nodes = nfa_nodes.size();
        nodecounter = number_of_old_nodes;

        // Create the first new node from the combination of original node 0.
        AddSet({0});

        while(!pending.empty())
        {
            // Choose one of the pending states, and generate a new node for it.
            // Numbers in pending[] always refer to new nodes even without new_node_offset.
            // Note: If you have DFA_POST_SORT_BY_USECOUNT, it does not matter which
            // order you pick them. If you don't, you must ALWAYS pick 0 first.
            unsigned chosen = pending.begin()->first;
            auto sources    = std::move(pending.begin()->second);
            pending.erase(pending.begin());

            if(nodecounter >= nfa_nodes.size()) nfa_nodes.resize(nodecounter+4);
            NFAnode& st = nfa_nodes[chosen];

            // Merge the transitions from all sources states of that new node.
            for(unsigned c=0; c<CHARSET_SIZE; ++c)
            {
                SometimesSortedVector<unsigned> merged_targets;
                for(auto src: sources) // Always a sorted list
                    if(src >= NFA_ACCEPT_OFFSET)
                        merged_targets.FastestInsert(src);
                    else
                        merged_targets.FastestInsert(nfa_nodes[src][c]);
                if(merged_targets.empty()) continue;
                // Sort and keep only unique values in the merged_targets.
                st[c].FastestInsert(AddSet(std::move(merged_targets.StealSortedVector())));
            }
        }

        // Last: Delete all old nodes, and update the node numbers
        // by subtracting number_of_old_nodes.
        nfa_nodes.erase(nfa_nodes.begin() + nodecounter, nfa_nodes.end());
        nfa_nodes.erase(nfa_nodes.begin(), nfa_nodes.begin() + number_of_old_nodes);

        for(auto& n: nfa_nodes)
            for(auto& c: n)
            {
                c.MakeSureIsSortedAndUnique();
                // c.targets should have 0 or 1 elements here.
                for(unsigned& e: c)
                    if(e < NFA_ACCEPT_OFFSET)
                    {
                        assert(e >= number_of_old_nodes);
                        e -= number_of_old_nodes;
                    }
            }

        // The NFA now follows DFA constraints.
    #ifdef DEBUG
        DumpNFA(std::cout, "Determinized NFA", nfa_nodes);
    #endif
    }

    // PHASE 3: Convert into DFA format (assumed to already follow the constraints)
    std::vector<std::array<unsigned,CHARSET_SIZE>> LoadDFA()
    {
        std::vector<std::array<unsigned,CHARSET_SIZE>> result(nfa_nodes.size());
        for(unsigned n=0; n<nfa_nodes.size(); ++n)
        {
            auto& s = nfa_nodes[n];
            auto& t = result[n];
            for(unsigned c=0; c<CHARSET_SIZE; ++c)
            {
                s[c].MakeSureIsSortedAndUnique();
                assert(s[c].size() <= 1);
                if(s[c].empty())           { t[c] = result.size(); continue; }
                unsigned a = s[c].front();
                if(a >= NFA_ACCEPT_OFFSET) { t[c] = result.size() + 1 + (a - NFA_ACCEPT_OFFSET); }
                else                       { t[c] = a; }
            }
        }
        return result;
    }
};

static void DFA_Transform(std::vector<std::array<unsigned,CHARSET_SIZE>>& statemachine, bool sort, bool prune)
{
    if(!sort && !prune) return;
    if(statemachine.empty()) return;

    struct Info
    {
        unsigned originalnumber;
        unsigned uses;
        bool     unused;
    };
    std::vector<Info> info;
    info.reserve(statemachine.size());
    for(unsigned stateno=0; stateno<statemachine.size(); ++stateno)
        info.push_back({stateno,0,false});

    // Sort the states according to how many times they are
    // referred, so that most referred states get smallest numbers.
    // This helps shrink down the .dirr_dfa file size.
recalculate_uses:
    info[0].uses = ~0u; // Make sure state 0 stays at 0
    for(unsigned stateno=0; stateno<statemachine.size(); ++stateno)
        if(!info[stateno].unused)
            for(unsigned c=0; c<CHARSET_SIZE; ++c)
            {
                unsigned t = statemachine[stateno][c];
                if(t < statemachine.size()
                && (c == 0 || t != statemachine[stateno][c-1])
                  )
                    if(likely(info[t].uses < ~0u))
                        ++info[t].uses;
            }

    // Go over the states and check if some of them were unused
    bool found_unused = false;
    for(auto& m: info)
        if(m.uses == 0)
            if(!m.unused) { m.unused = true; found_unused = true; }
    if(found_unused)
        { for(auto& m: info) m.uses = 0; goto recalculate_uses; }

    // Didn't find new unused states.
    if(prune)
    {
        // Now delete states that were previously found to be unused.
        // State 0 will not be deleted, as it implicitly has an use count of ~0u.
        info.erase(std::remove_if(info.begin(),info.end(),[](const Info& m)
        {
            return m.unused==true;
        }), info.end());
    }

#ifdef DEBUG
    std::cout << (statemachine.size()-info.size()) << " DFA states deleted\n";
#endif

    if(sort)
    {
        // Sort states according to their use.
        // State 0 will remain as state 0 because it has the highest uses count.
        std::sort(info.begin(), info.end(), [](const Info& a, const Info& b)
        {
            return a.uses > b.uses;
        });
    }
    else
    {
        // Nothing was deleted?
        if(info.size() == statemachine.size())
            return;
    }

    // Create mappings to reflect the new sorted order
    std::unordered_map<unsigned,unsigned> rewritten_mappings;
    for(unsigned stateno=0; stateno<info.size(); ++stateno)
        rewritten_mappings.emplace(info[stateno].originalnumber, stateno);

    // Rewrite the state machine
    std::vector<std::array<unsigned,CHARSET_SIZE>> newstatemachine(info.size());
    for(unsigned stateno=0; stateno<info.size(); ++stateno)
    {
        auto& targets = statemachine[ info[stateno].originalnumber ];
        for(unsigned c=0; c<CHARSET_SIZE; ++c)
        {
            newstatemachine[stateno][c] = (targets[c] >= statemachine.size())
                ? (targets[c] - statemachine.size() + newstatemachine.size())
                : (rewritten_mappings.find(targets[c])->second);
        }
    }
    statemachine = std::move(newstatemachine);
}

static void DFA_Minimize(std::vector<std::array<unsigned,CHARSET_SIZE>>& statemachine)
{
#ifdef DEBUG
    DumpDFA(std::cout, "DFA before minimization", statemachine);
#endif
    // Groups of states
    typedef std::list<unsigned> group_t;

    // State number to group number mapping
    class mappings
    {
        std::vector<unsigned> lomap{}; // non-accepting states
        std::vector<unsigned> himap{}; // accepting states
    public:
        void set(unsigned value, unsigned target)
        {
            if(value < DFAOPT_ACCEPT_OFFSET)
                set_in(lomap, value, target);
            else
                set_in(himap, value-DFAOPT_ACCEPT_OFFSET, target);
        }
        unsigned get(unsigned value) const
        {
            return (value < DFAOPT_ACCEPT_OFFSET)
                ? lomap[value]
                : himap[value-DFAOPT_ACCEPT_OFFSET];
        }
    private:
        void set_in(std::vector<unsigned>& which, unsigned index, unsigned target)
        {
            if(which.size() <= index) which.resize(index+8);
            which[index] = target;
        }
    } mapping;

    // Create a single group for all non-accepting states.
    // Collect all states. Assume there are no unvisited states.
    std::vector<group_t> groups(1);
    for(unsigned n=0; n<statemachine.size(); ++n)
        { groups[0].push_back(n); mapping.set(n, 0); }

    // Don't create groups for accepting states, since they are
    // not really states, but create a mapping for them, so that
    // we don't need special handling in the group-splitting code.
    for(unsigned n=0; n<statemachine.size(); ++n)
        for(auto v: statemachine[n])
            if(v >= statemachine.size()) // Accepting or failing state
                mapping.set( v, (v - statemachine.size()) + DFAOPT_ACCEPT_OFFSET );

rewritten:;
    for(unsigned groupno=0; groupno<groups.size(); ++groupno)
    {
        group_t& group = groups[groupno];
        auto j = group.begin();
        unsigned first_state = *j++;
        if(j == group.end()) continue; // only 1 item

        // For each state in this group that differs
        // from first group, move them into the other group.
        group_t othergroup;
        while(j != group.end())
        {
            unsigned other_state = *j;
            auto old = j++;
            for(unsigned c=0; c<CHARSET_SIZE; ++c)
            {
                if(mapping.get(statemachine[first_state][c])
                != mapping.get(statemachine[other_state][c]))
                {
                    // Difference found.
                    // Move this element into other group.
                    othergroup.splice(othergroup.end(), group, old, j);
                    break;
                }
            }
        }
        if(!othergroup.empty())
        {
            // Split the differing states into a new group
            unsigned othergroup_id = groups.size();
            // Change the mappings (we need overwriting, so don't use emplace here)
            for(auto n: othergroup) mapping.set(n, othergroup_id);
            groups.emplace_back(std::move(othergroup));
            goto rewritten;
        }
    }

    // VERY IMPORTANT: Make sure that whichever group contains the
    // original state 0, emerges as group 0 in the new ordering.

    // Create a new state machine based on these groups
    std::vector<std::array<unsigned,CHARSET_SIZE>> newstates(groups.size());
    for(unsigned newstateno=0; newstateno<groups.size(); ++newstateno)
    {
        auto& group          = groups[newstateno];
        auto& newstate       = newstates[newstateno];
        // All states in this group are identical. Just take any of them.
        const auto& oldstate = statemachine[*group.begin()];
        // Convert the state using the mapping.
        for(unsigned c=0; c<CHARSET_SIZE; ++c)
        {
            unsigned v = oldstate[c];
            if(v >= statemachine.size()) // Accepting or failing state?
                newstate[c] = v - statemachine.size() + newstates.size();
            else
                newstate[c] = mapping.get(v); // group number
        }
    }
    statemachine = std::move(newstates);

    DFA_Transform(statemachine, DFA_POST_SORT_BY_USECOUNT, DFA_POST_DELETE_UNUSED_NODES);

#ifdef DEBUG
    DumpDFA(std::cout, "DFA after minimization", statemachine);
#endif
}

void DFA_Matcher::Compile()
{
    lock_writing(lk, lock);
    if(unlikely(!data)) return;

    // First, sort the matches so that we get a deterministic hash.
    // This is optional.
    std::sort(data->matches.begin(), data->matches.end(),
              [](const Data::Match& a, const Data::Match& b)
              { return (a.target!=b.target) ? a.target<b.target
                     : (a.token!=b.token) ? a.token<b.token
                     : a.icase<b.icase; });

    data->RecheckHash(); // Calculate the hash before deleting matches[]
    // Otherwise Save() will fail

    data->statemachine = [this]() -> std::vector<std::array<unsigned,CHARSET_SIZE>>
    {
        NFAcompiler nfa_compiler;

        // Parse each wildmatch expression into the NFA.
        for(auto& m: data->matches)
            nfa_compiler.AddMatch(std::move(m.token), m.icase, m.target);

        // Don't need the matches[] anymore, since it's all assembled in nfa_nodes[]
    #ifndef PUTBIT_OPTIMIZER
        data->matches.clear();
    #endif

        if(NFA_SIMPLIFY_ACCEPTS)
        {
            nfa_compiler.SimplifyAcceptingStates();
        }

        if(NFA_PRE_MINIMIZATION_LEVEL >= 2)
        {
            nfa_compiler.Minimize();
        }

        nfa_compiler.Determinize();

        return nfa_compiler.LoadDFA();
    }();

    DFA_Transform(data->statemachine, false, DFA_PRE_DELETE_UNUSED_NODES);

    // STEP 4: Finally, minimalize the DFA (state machine).
    if(DFA_DO_MINIMIZATION)
    {
        DFA_Minimize(data->statemachine);
    }
}

bool DFA_Matcher::Valid() const noexcept
{
    lock_reading(lk, lock);
    return data && !data->statemachine.empty();
}
