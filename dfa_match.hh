#include <string>  // std::string
#include <istream> // std::istream
#include <ostream> // std::ostream

#if __cplusplus >= 201402L
# include <shared_mutex>
#else
# include <mutex>   // std::mutex for thread-safety
#endif

class DFA_Matcher
{
public:
    /* AddMatch: Add a pattern matching to the state machine
     *
     *   wildpattern : Pattern to be matched. May contain glob-like wildcards.
     *                 Wildcards recognized:
     *                    ?     Matches any byte
     *                    *     Matches zero or more bytes of anything
     *                    \d    Matches any digit (0..9)
     *                    \w    Matches any alphanumeric byte (0..9, A-Z, a-z)
     *                    \x##  Matches byte with hexadecimal value ##
     *                    \\    Matches the backslash
     *                    [...] Matches a byte matching the range, with:
     *                               ^    in the beginning of the range inverts the selection
     *                               b-e  matches characters b,c,d,e
     *                               \]   matches ]
     *                               Anything else matches the letter verbatim
     *
     *   icase       : true if pattern is case-insensitive, false if not.
     *   target      : Return value for Test() to produce when pattern matches.
     *                 target values must be within 0..7FFFFFFF range (31 bits).
     *
     * All patterns match the whole string. There are no "partial" matches.
     *
     * Note: Matching is done byte-per-byte (no UTF-8)
     * Note: Calling AddMatch() when Valid() is true may cause
     *       all previously added patterns to be forgotten.
     */
    void AddMatch(const std::string& wildpattern, bool icase, int target);
    void AddMatch(std::string&&      wildpattern, bool icase, int target);

    /* Test: Find out which pattern (if any) is matched
     *
     *   s             : String to be tested.
     *   default_value : Value to be returned if nothing matched.
     *
     * Return value:
     *
     *   If s did not match any pattern, default_value is returned.
     *   If s matched one of the patterns, the target value for that pattern is returned.
     *   If s matched multiple patterns, an undefined singular choice
     *   is made between the target values.
     *
     * When Valid() = false, the behavior of this function is undefined.
     */
    int Test(const std::string& s, int default_value) const noexcept;

    /* Compile() : Builds the statemachine from the patterns submitted
     *             with AddMatch() beforehand. Will cause Valid() = true.
     *
     * Note: It is not an error to call Compile() without any AddMatch() calls.
     *       In that case, the statemachine will just not match anything.
     * Note: If Compile() is called when Valid() is already true,
     *       the behavior is the same as if no AddMatch() calls were done.
     */
    void Compile();

    /* Load(): Attempts to load a previously compiled statemachine
     *         from the given file.
     *         The load will fail if the statemachine in the file
     *         does not match the strings submitted with AddMatch(),
     *         or if the file has invalid format.
     *         If the load succeeds, Valid() will be true.
     *
     * If ignore_hash=true, Load() can be used to load any
     * previously saved state machine, without calling AddMatch().
     *
     * Return value:
     *   true if the load succeeds.
     *   false if the load fails. Statemachine will not be modified.
     *
     * Exceptions:
     *   Any exception that can be thrown by std::vector::resize()
     *   or f.read().
     *
     * Purpose: Compile() may be slow when many complex patterns are
     * used. Using Save() you can save a precompiled machine into a file,
     * and with Load() you can omit the Compile() phase. You will still
     * need to do the AddMatch() calls; the load will fail if the file
     * contains a different state machine.
     */
    bool Load(std::istream& f, bool ignore_hash=false);
    bool Load(std::istream&& f, bool ignore_hash=false);

    /* Save(): Saves the compiled statemachine into a file.
     *         Can only be called when a statemachine is Valid().
     *
     * Exceptions:
     *   Any exception that can be thrown by std::vector::resize()
     *   or f.write().
     */
    void Save(std::ostream& f) const;
    void Save(std::ostream&& f) const;

    /* Valid(): Returns true if the statemachine has been successfully
     *          loaded with Load() or compiled with Compile().
     */
    bool Valid() const noexcept;

public:
    // The standard set of constructors, destructors, assign operators
    // Ones not marked noexcept can throw allocator-related exceptions.
    DFA_Matcher();
    DFA_Matcher(const DFA_Matcher&);
    DFA_Matcher& operator=(const DFA_Matcher&);
    virtual ~DFA_Matcher();

    DFA_Matcher(DFA_Matcher&&) noexcept;
    DFA_Matcher& operator=(DFA_Matcher&&) noexcept;

    // A convenience constructor that allows doing
    // multiple AddMatch calls in the single constructor call.
    // The final parameter can optionally be a reference
    // to another DFA_Matcher instance which gets extended.
    template<typename S, typename... Rest>
    DFA_Matcher(S&& s, bool i, int t, Rest&&... rest)
        : DFA_Matcher(std::forward<Rest>(rest)...)
        { AddMatch(std::forward<S>(s),i,t); }

private:
    struct Data;
    Data* data;
#if __cplusplus >= 201402L
    // Would use std::shared_mutex (C++17), but libstdc++
    // as of GCC 5.3.1 only supports std::shared_timed_mutex.
    // Same goes for libc++ as of Clang++ version 3.6.2.
    mutable std::shared_timed_mutex lock{};
#else
    mutable std::mutex lock{};
#endif
};
