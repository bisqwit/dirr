#include <string>
#include <map>
#include <vector>
#include <array>

namespace re2c { class RegExp; }

class DFA_Matcher
{
    std::map<int, std::vector<re2c::RegExp*>> collection{};
    std::string hash_str{};

    /* state number -> { char number -> code }
     *                      code: 0         = fail
     *                            0x8000+n  = target (color)
     *                            1..0x7FFF = new state number
     */
    std::vector<std::array<unsigned short,256>> statemachine{};

public:
    DFA_Matcher() {}
    ~DFA_Matcher();
    DFA_Matcher(const DFA_Matcher&) = delete;
    void operator=(const DFA_Matcher&) = delete;

    void AddMatch(const std::string& wildpattern, bool icase, int target);
    void Compile();
    int Test(const std::string& s, int default_value) const;
};
