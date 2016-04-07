#include <string>
#include <vector>
#include <array>
#include <cstdio>  // std::FILE
#include <utility> // for std::pair

class DFA_Matcher
{
public:
    /* AddMatch: Add a pattern matching to the state machine
     *
     *   wildpattern : Pattern to be matched. May contain glob-like wildcards.
     *   icase       : true if pattern is case-insensitive, false if not.
     *   target      : Return value for Test() to produce when pattern matches.
     *                 target values must be within 0..7FFFFFFF range (31 bits).
     *
     * All patterns match the whole string. There are no "partial" matches.
     */
    void AddMatch(const std::string& wildpattern, bool icase, int target);
    void AddMatch(std::string&&      wildpattern, bool icase, int target);

    /* Test: Find out which pattern (if any) is matched
     *
     *   s             : String to be tested.
     *   default_value : Value to be returned if nothing matched.
     *
     * Return value:
     *   If s matched one of the patterns, the "target" value for that pattern is returned.
     *   If s did not match any pattern, default_value is returned.
     *   If s matched multiple patterns, one of the "target" values is returned.
     *                               (Which one is returned is undefined.)
     *
     * Before this function can be called, either Compile() must
     * have been called, or Load() must be called and return true.
     */
    int Test(const std::string& s, int default_value) const;

    /* Compile() : Builds the statemachine from the patterns submitted
     *             with AddMatch() beforehand.
     */
    void Compile();

    /* Load(): Attempts to load a previously compiled statemachine
     *         from the given file. The load will fail if the statemachine
     *         in the file does not match the strings submitted with AddMatch().
     */
    bool Load(std::FILE* fp);

    /* Save(): Saves the compiled statemachine into a file.
     *         Can only be called when a statemachine has either been compiled or loaded.
     */
    void Save(std::FILE* fp) const;

private:
    mutable std::pair<unsigned long long/*value*/, bool/*valid*/> hash{};

    /* Collect data from AddMatch() */
    std::vector<std::pair<std::string, std::pair<int,bool>>> matches{};

    /* state number -> { char number -> code }
     *              code: =numstates = fail
     *                    >numstates = target color +numstates+1
     *                    <numstates = new state number
     */
    std::vector<std::array<unsigned,256>> statemachine{};

private:
    void RecheckHash() const;
};
