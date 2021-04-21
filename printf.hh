#ifndef bqtEprintfHH
#define bqtEprintfHH

#include <string_view>
#include <type_traits>
#include <ostream>
#include <string>
#include <vector>
#include <memory>

namespace PrintfPrivate
{
    template<typename T>
    concept IsStringView = std::is_same_v<T, std::basic_string_view<typename T::value_type>>;

    template<typename T>
    concept PassAsCopy = (std::is_trivially_copyable_v<T> && sizeof(T) <= (2*sizeof(long long)))
                      || IsStringView<std::remove_cvref_t<T>>;

    struct Postpone{};
}

class PrintfFormatter
{
public: // ...blah (must be public because of PrintfFormatDo in printf.cc)
    struct argsmall
    {
        unsigned min_width  = 0,     max_width = ~0u;
        bool leftalign      = false, sign      = false, zeropad = false;
        enum basetype   : char { decimal=10, hex=16,  hexup=16+64,  oct=8, bin=2 } base   = decimal;
        enum formattype : char { as_char, as_int, as_float, as_string } format = as_string;

        argsmall() { }
        argsmall(unsigned mi,unsigned ma,bool la, bool si, bool zp, basetype b, formattype f)
            : min_width(mi), max_width(ma), leftalign(la), sign(si), zeropad(zp), base(b), format(f) { }
    };
    struct arg: public argsmall
    {
        std::basic_string<char32_t> before{};
        bool param_minwidth = false, param_maxwidth = false;
    };
    struct State
    {
        std::size_t position = 0;
        unsigned    minwidth = 0;
        unsigned    maxwidth = 0;
        bool        leftalign = false;
        std::basic_string<char32_t> result{};
    };

    std::vector<arg>            formats{};
    std::basic_string<char32_t> trail{};

public:
    template<typename C>
    void MakeFrom(std::basic_string_view<C> format);

    /* Execute() appends an individual parameter to the format.
     * This overload is for arithmetic types (including chars) and strings.
     */
    template<typename T, typename... T2>
        requires (std::is_arithmetic_v< std::remove_cv_t<T>>
               || std::is_assignable_v< std::basic_string<char>, T >
               || std::is_assignable_v< std::basic_string<char32_t>, T >)
    void Execute(State& state, const T& a, T2&&... rest)
    {
        // Use const reference or value-copy depending on which is more optimal
        using TT = std::conditional_t<PrintfPrivate::PassAsCopy<T>, T, const T&>;
        ExecutePart<TT>(state, a);
        Execute(state, std::forward<T2>(rest)...);
    }

    /* This overload is for string constants, which are converted into string_views.
     */
    template<typename CT, std::size_t N, typename... T2>
    void Execute(State& state, const CT (&arg)[N], T2&&... rest)
    {
        ExecutePart<std::basic_string_view<CT>>(state, std::basic_string_view<CT>(arg,N-1));
        Execute(state, std::forward<T2>(rest)...);
    }

    /* This overload is used in the % operator. It postpones the sentinel Execute() call. */
    void Execute(State&, PrintfPrivate::Postpone) {}

    // When no parameters are remaining
    void Execute(State& state);

    template<typename T>
    void ExecutePart(State& state, T part);
};




template<typename... T>
std::basic_string<char> Printf(PrintfFormatter& fmt, T&&... args)
{
    PrintfFormatter::State state;
    fmt.Execute(state, std::forward<T>(args)...);
    return std::basic_string<char>( state.result.begin(), state.result.end() );
}


/* This is the function you would use. */
template<typename CT, typename... T>
std::basic_string<char> Printf(std::basic_string_view<CT> format, T&&... args)
{
    PrintfFormatter Formatter;
    Formatter.MakeFrom(format);
    return Printf(Formatter, std::forward<T>(args)...);
}
/* Or this */
template<typename CT, std::size_t N, typename... T>
std::basic_string<char> Printf(const CT (&arr)[N], T&&... args)
{
    return Printf( std::basic_string_view<CT>(&arr[0], N-1), std::forward<T>(args)... );
}
/* Or this */
template<typename CP, typename... T>
    requires (std::is_pointer_v<CP> && std::is_integral_v<std::remove_pointer_t<CP>>)
std::basic_string<char> Printf(CP s, T&&... args)
{
    using CT = std::remove_pointer_t<CP>;
    return Printf( std::basic_string_view<CT>(s), std::forward<T>(args)... );
}
/* Or this */
template<typename CT, typename... T>
std::basic_string<char> Printf(const std::basic_string<CT>& s, T&&... args)
{
    return Printf( std::basic_string_view<CT>(s), std::forward<T>(args)... );
}



struct PrintfProxy
{
    using data_t = std::pair<PrintfFormatter, PrintfFormatter::State>;
    std::unique_ptr<data_t> data {};

    template<typename CT,typename TR,typename A>
    operator std::basic_string<CT,TR,A> () && // This implements str()
    {
        data->first.Execute(data->second);
        return {data->second.result.begin(), data->second.result.end()};
    }

    std::string str() && { return std::move(*this); }

    PrintfProxy() : data{std::make_unique<data_t>()} {}
    PrintfProxy(std::basic_string_view<char> fmt);
    inline ~PrintfProxy() = default;

    PrintfProxy(PrintfProxy&& b) = default;
    PrintfProxy& operator=(PrintfProxy&&) = default;

    PrintfProxy(const PrintfProxy&) = delete;
    PrintfProxy& operator= (const PrintfProxy&) = delete;

    // Parameter operands. These two functions have the same semantics as Execute(),
    // except that the actual formatting is only done when the string conversion operator is called.
    template<typename T>
    PrintfProxy operator % (T&& arg) &&
    {
        *this %= std::forward<T>(arg);
        return std::move(*this);
    }
    template<typename T>
    PrintfProxy& operator %= (T&& arg)
    {
        data->first.Execute( data->second, std::forward<T>(arg), PrintfPrivate::Postpone{} );
        return *this;
    }

    PrintfProxy& operator %= (PrintfProxy&& arg);

    // Concatenation operands
    template<typename T>
    PrintfProxy operator+ (T&& b) &&;
    PrintfProxy operator+ (PrintfProxy&& b) &&;
};

inline PrintfProxy operator ""_f(const char* format, std::size_t num)
{
    return PrintfProxy(std::string_view(format,num));
}

template<typename T>
PrintfProxy PrintfProxy::operator+ (T&& b) &&
{
    // Finish our string
    data->first.Execute(data->second);
    // Create a "%s" parameter
    data->first.formats.emplace_back(); // arg{} with default options
    // Continue our processing with "b" as a new parameter
    return std::move(*this) % std::forward<T>(b);
}

template<typename T, typename TR>
std::basic_ostream<T,TR>& operator<< (std::basic_ostream<T,TR>& out, PrintfProxy&& b)
{
    out << std::move(b).str();
    return out;
}

#endif
