#define PUTBIT_OPTIMIZER
#include "dfa_match.cc"

#include <sstream>
#include <random>
#include <fstream>
#include <iostream>
#include <unordered_map>

#ifdef __SUNPRO_CC
extern "C"{
int __gthrw___pthread_key_create(void*,void(*)(void*)){return 0;}
}
#endif

constexpr unsigned longbits = sizeof(unsigned long long)*CHAR_BIT;

static std::mt19937 rnd;

static bool TestLoadBit()
{
    std::vector<unsigned long long> testvalues;
    for(unsigned bitness=1; bitness<=longbits; ++bitness)
    {
        unsigned n_random_values = std::min(100ull, 1ull << bitness);
        for(unsigned n=0; n<n_random_values; ++n)
        {
            unsigned long long value = std::max(1ull, (1ull << bitness) / n_random_values) * n;
            testvalues.push_back(value);
        }
    }
    std::sort(testvalues.begin(), testvalues.end());
    testvalues.erase(std::unique(testvalues.begin(),testvalues.end()),testvalues.end());
    bool errors = false;
    for(auto value: testvalues)
    {
        unsigned long long loaded;

        unsigned char Buffer[32] = {0};

        unsigned save_bitpos=0, load_bitpos=0;
        PutVarBit(Buffer, save_bitpos, value);
        loaded = LoadVarBit(Buffer, load_bitpos);

        std::fflush(stdout); std::fflush(stderr);
        if(save_bitpos != load_bitpos || value != loaded)
        {
            errors = true;
            fprintf(stderr, "Error: Saving %llX produced %u bits. After loading %u bits we got %llX\n",
                value, save_bitpos,
                load_bitpos, loaded);

            for(unsigned a=0; a<(save_bitpos+CHAR_BIT-1)/CHAR_BIT; ++a)
                fprintf(stderr, "%02X ", Buffer[a]);
            fprintf(stderr, "\n");
        }
        /*else
        {
            fprintf(stderr, "Ok: Saving %llX produced %u bits. After loading %u bits we got %llX\n",
                value, save_bitpos,
                load_bitpos, loaded);
        }*/
        std::fflush(stdout); std::fflush(stderr);
    }
    return !errors;
}

static void Load(DFA_Matcher& mac, const std::string& s)
{
    std::unordered_multimap<std::string/*key*/, std::string/*value*/> sets{};
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
    for(const auto& s: sets)
        if(s.first == "byext")
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
                mac.AddMatch(std::move(token), ignore_case, color);
                pos = spacepos;
            }
        }
}

int main()
{
    TestLoadBit();
    DFA_Matcher mac;
    #define rand(size) std::uniform_int_distribution<>(0, (size)-1)(rnd)

    std::cout << "Parsing...\n";

    Load(mac,
#include "dirrsets.hh"
    );

    std::cout << "Compiling 1 times...\n";
    if(mac.Load(std::ifstream("/home/bisqwit/.dirr_dfa"), true))
        std::cout << "Nope, loaded instead\n" << std::flush;
    else
    {
        for(unsigned n=0; n<1; ++n)
        mac.Compile();
    }

    std::cout << "Brooding...\n" << std::flush;

    if(1) // Sanity check and profiling scope
    {
        std::stringstream out, out2;
        mac.Save(out);
        std::cout << "Test save: " << out.str().size() << " bytes\n" << std::flush;
        mac.Load(out, true);
        mac.Save(out2);
        if(out.str() != out2.str())
        {
            std::cerr << "ERROR: Second save produced " << out2.str().size() << "; Load did not result in same save!\n";
            return 1;
        }
        else
            std::cerr << "Load ok\n";
    }
//return 0;

    static const unsigned selection[] = {1,1,1,1,1, 2,2,2,2, 3,3,3, 4,4, 5, 6, 7, 8, 9, 10, 11, 12};

    std::vector<unsigned> best_sel = VarBitCounts;
    std::size_t best = ~0u, best_deviation = 0, best_deviation2 = 0;
    for(;;)
    {
rerand:;
        switch(rand(32))
        {
            case 0: VarBitCounts.clear(); break;
            case 1: VarBitCounts = best_sel; break;
        }

        unsigned sum=0;
        for(auto r: VarBitCounts) sum += r;
        while(sum < longbits)
        {
            unsigned r = selection[rand(sizeof(selection)/sizeof(*selection))];
            VarBitCounts.push_back(r);
            sum += r;
        }

        if(VarBitCounts.size() <= 1) goto rerand;

        unsigned offset = rand(VarBitCounts.size());
        switch(rand(5))
        {
            case 0: // random addition to some slot
            {
                if(offset >= VarBitCounts.size()) goto rerand;
                auto& v = VarBitCounts[offset];
                if(rand(2) || v<=1) // add
                    v += rand(4);
                else // sub
                    v -= std::max(1, rand(v+1)/4);
                break;
            }
            case 1: // split random slot into two
            {
                if(offset >= VarBitCounts.size()) goto rerand;
                unsigned v = VarBitCounts[offset];
                if(v <= 1) goto rerand;
                unsigned part1 = 1 + rand(v);
                unsigned part2 = v - part1;
                VarBitCounts[offset] = part1;
                VarBitCounts.insert(VarBitCounts.begin()+offset, part2);
                break;
            }
            case 2: // merge two slots
            {
                if(offset+1 >= VarBitCounts.size()) goto rerand;
                VarBitCounts[offset] += VarBitCounts[offset+1];
                VarBitCounts.erase(VarBitCounts.begin()+offset+1);
                break;
            }
            case 3: // subtract from some slot, add in another
            {
                unsigned offset2 = rand(VarBitCounts.size());
                if(offset == offset2) goto rerand;
                unsigned& src = VarBitCounts[offset];
                if(src <= 1) goto rerand;
                unsigned delta = rand(src);
                src -= delta;
                VarBitCounts[offset2] += delta;
                break;
            }
            case 4:
            {
                //hash_offset += rand(256);
                swap50_rotation += rand(2)*2-1;
                break;
            }
        }

        for(auto& r: VarBitCounts) r = std::min(r, longbits-1u);
        VarBitCounts.erase(std::remove_if(VarBitCounts.begin(),VarBitCounts.end(),[](unsigned v) { return v==0; }), VarBitCounts.end());

        sum = 0;
        for(auto r: VarBitCounts) sum += r;
        while(sum < longbits)
        {
            unsigned r = selection[rand(sizeof(selection)/sizeof(*selection))];
            VarBitCounts.push_back(r);
            sum += r;
        }

        std::size_t deviation_one = 0, deviation_two = 0;
        sum=0;
        for(auto r: VarBitCounts) { sum += r; deviation_one += std::abs(int(sum%8)); }
        for(auto r: VarBitCounts) deviation_two += std::abs(int(r) - 8);

        std::stringstream out;
        mac.Save(out);
        std::size_t size = out.str().size();
        if((size != best) ? (size < best)
         : (deviation_one != best_deviation) ? (deviation_one < best_deviation)
         : (deviation_two != best_deviation2) ? (deviation_two < best_deviation2)
         : false
          )
        {
            auto oldstate = mac.getdata()->statemachine;
            if(!mac.Load(out))
            {
                std::cout << "Load failed\n";
                continue;
            }
            auto newstate = mac.getdata()->statemachine;
            if(oldstate != newstate)
            {
                std::cout << "Load produced a different statemachine\n";
                continue;
            }
            if(!TestLoadBit())
            {
                std::cerr << "ERROR HAPPENED WITH ";
                std::cerr << std::hex << std::showbase << unsigned(hash_offset) << std::dec;
                std::cerr << ", and: ";
                char sep=' ';
                for(unsigned r: VarBitCounts) { std::cerr << sep << r; sep = ','; }
                std::cerr << "\n";
                continue;
            }
            std::cout << size << " bytes with: ";

            std::cout << std::hex << std::showbase << unsigned(hash_offset) << std::dec;
            std::cout << ", rotation=" << std::dec << (swap50_rotation&0xFF);
            std::cout << ", and: ";
            char sep=' ';
            for(unsigned r: VarBitCounts) { std::cout << sep << r; sep = ','; }
            std::cout << "\n";
            best = size;
            best_sel = VarBitCounts;
            best_deviation = deviation_one;
            best_deviation2 = deviation_two;
        }
    }
}
