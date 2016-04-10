#define PUTBIT_OPTIMIZER
#include "dfa_match.cc"

#include <sstream>
#include <random>
#include <fstream>
#include <iostream>
int main()
{
    DFA_Matcher mac;
    std::mt19937 rnd;
    #define rand(size) std::uniform_int_distribution<>(0, (size)-1)(rnd)

    auto run = [&mac](int color,bool icase, const std::initializer_list<const char*>& data)
        { for(auto d: data) mac.AddMatch(d, icase, color); };
    std::cout << "Parsing...\n";

    run(0x2,true, {R"(*.so)", R"(*.o)", R"(*.vxd)", R"(*.dll)", R"(*.drv)", R"(*.obj)", R"(*.dll)", R"(*.a)", R"(*.lo)", R"(*.la)", R"(*.so.*)"});
    run(0x3,true, {R"(*.txt)", R"(*.htm)", R"(*.html)", R"(*.xml)", R"(*.xhtml)", R"(*.1st)", R"(*.wri)", R"(*.ps)", R"(*.doc)", R"(*.docx)", R"(*.odt)"});
    run(0x3,true, {R"(readme)", R"(quickstart)", R"(install)"});
    run(0x4,true, {R"(core)", R"(DEADJOE)"});
    run(0x5,true, {R"(*.mid)", R"(*.mod)", R"(*.mtm)", R"(*.s3m)", R"(*.xm*)", R"(*.mp[23])", R"(*.wav)", R"(*.ogg)", R"(*.smp)", R"(*.au)", R"(*.ult)", R"(*.669)", R"(*.aac)", R"(*.spc)", R"(*.nsf)", R"(*.wma)"});
    run(0x6,true, {R"(*.bas)", R"(*.pas)", R"(*.php)", R"(*.phtml)", R"(*.pl)", R"(*.rb)", R"(*.c)", R"(*.cpp)", R"(*.cc)", R"(*.asm)", R"(*.S)", R"(*.s)", R"(*.inc)", R"(*.h)", R"(*.hh)", R"(*.pov)", R"(*.irc)", R"(*.hpp)"});
    run(0x6,true, {R"(*.src)", R"(*.ttt)", R"(*.pp)", R"(*.p)", R"(makefile)", R"(*.mak)", R"(*.in)", R"(*.am)"});
    run(0x201BA,true, {R"(configure)", R"(*.sh)"});
    run(0x13B,true, {R"(*~)", R"(*.bak)", R"(*.old)", R"(*.bkp)", R"(*.st3)", R"(*.tmp)", R"(*.$$$)", R"(tmp*)", R"(*.out)", R"(*.~*)"});
    run(0x14E,true, {R"(*.exe)", R"(*.com)", R"(*.bat)"});
    run(0x1A7,true, {R"(*.tar)", R"(*.gz)", R"(*.xz)", R"(*.bz)", R"(*.brotli)", R"(*.bz2)", R"(*.zip)", R"(*.arj)", R"(*.lzh)", R"(*.lha)", R"(*.rar)", R"(*.deb)", R"(*.rpm)", R"(*.arj)", R"(*.7z)", R"(*.lzma)"});
    run(0xD,true, {R"(*.gif)", R"(*.bmp)", R"(*.mpg)", R"(*.mpeg)", R"(*.mp4)", R"(*.avi)", R"(*.ogm)", R"(*.ogv)", R"(*.mkv)", R"(*.asf)", R"(*.x?m)", R"(*.mov)", R"(*.tga)", R"(*.png)", R"(*.tif)"});
    run(0xD,true, {R"(*.wmv)", R"(*.pcx)", R"(*.lbm)", R"(*.img)", R"(*.jpg)", R"(*.jpeg)", R"(*.fl\w)", R"(*.rm)", R"(*.tiff)"});
    run(0x69D7,false, {R"(*\x13)", R"(*\?)"});

    std::cout << "Compiling...\n";
    if(mac.Load(std::ifstream("/home/bisqwit/.dirr_dfa"), true))
        std::cout << "Nope, loaded instead\n" << std::flush;
    else
        mac.Compile();


    std::cout << "Brooding...\n" << std::flush;
    {std::stringstream out; mac.Save(out);
    std::cout << "Test save: " << out.str().size() << " bytes\n" << std::flush; }
//return 0;

    static const unsigned selection[] = {1,1,1,1,1, 2,2,2,2, 3,3,3, 4,4, 5, 6, 7, 8, 9, 10, 11, 12};

    std::vector<unsigned> best_sel = VarBitCounts;
    std::size_t best = ~0u, best_deviation = 0, best_deviation2 = 0;
    constexpr unsigned longbits = sizeof(unsigned long long)*CHAR_BIT;
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
        switch(rand(4))
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
            /*case 4:
            {
                hash_offset += rand(256);
                break;
            }*/
        }

        for(auto& r: VarBitCounts) r = std::min(r, longbits-1u);

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
            if(!mac.Load(out))
            {
                std::cout << "Load failed\n";
                continue;
            }
            std::cout << std::dec << size << " bytes with: ";

            std::cout << std::hex << std::showbase << unsigned(hash_offset) << std::dec;
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
