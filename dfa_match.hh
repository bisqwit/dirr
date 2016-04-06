#include <string>
#include <vector>
#include <array>
#include <cstdio> // std::FILE
#include <utility> // for std::pair

class DFA_Matcher
{
    // Collect data from AddMatch()
    unsigned long long hash{};
    std::vector<std::pair<std::string, std::pair<int,bool>>> matches{};

    /* state number -> { char number -> code }
     *              code: =numstates = fail
     *                    >numstates = target color +numstates+1
     *                    <numstates = new state number
     */
    std::vector<std::array<unsigned,256>> statemachine{};

public:
    void AddMatch(const std::string& wildpattern, bool icase, int target);
    void Compile();
    int Test(const std::string& s, int default_value) const;

private:
    void Generate();
    void Save(std::FILE* fp) const;
    bool Load(std::FILE* fp);
};
