#ifndef bqtEprintfHH
#define bqtEprintfHH

#include <string_view>
#include <type_traits>
#include <ostream>
#include <string>
#include <vector>
#include <memory>
#include "config.h"

namespace PrintfPrivate
{
  #ifdef HAVE_CONCEPTS
    template<typename T>
    concept IsStringView = std::is_same_v<T, std::basic_string_view<typename T::value_type>>;
    template<typename T>
    concept IsString     = std::is_same_v<T, std::basic_string<typename T::value_type>>;
  #else
    template<typename T>
    struct IsStringView_s { static constexpr bool value = false; };
    template<typename CT>
    struct IsStringView_s<std::basic_string_view<CT>> { static constexpr bool value = true; };
    template<typename T>
    concept IsStringView = IsStringView_s<T>::value;

    template<typename T>
    struct IsString_s { static constexpr bool value = false; };
    template<typename CT>
    struct IsString_s<std::basic_string<CT>> { static constexpr bool value = true; };
    template<typename T>
    concept IsString = IsString_s<T>::value;
  #endif
  #ifdef HAVE_IS_TRIVIALLY_COPYABLE
    template<typename T>
    concept PassAsCopy = (std::is_trivially_copyable_v<T> && sizeof(T) <= (2*sizeof(long long)))
                      || IsStringView<std::remove_cvref_t<T>>;
  #else
    template<typename T>
    concept PassAsCopy = std::is_pod<T>::value || IsStringView<std::remove_cvref_t<T>>;
  #endif

    struct Postpone{};
}

struct PrintfProxy;
class PrintfFormatter
{
    friend struct PrintfProxy;

public:
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
private:
    std::vector<arg>            formats{};
    std::basic_string<char32_t> trail{};

public:
    template<typename C>
    void MakeFrom(std::basic_string_view<C> format);

    /* Execute() appends an individual parameter to the format.
     * This overload is for arithmetic types (including chars) and strings.
     */
    template<typename T, typename... T2>
  #ifdef HAVE_CONCEPTS
        requires (std::is_arithmetic_v< std::remove_cv_t<T>>
               || std::is_assignable_v< std::basic_string<char>, T >
               || std::is_assignable_v< std::basic_string<char32_t>, T >)
  #endif
    void Execute(State& state, const T& a, T2&&... rest)
    {
        // Use const reference or value-copy depending on which is more optimal
        using TT = std::conditional_t<PrintfPrivate::PassAsCopy<T>, T, const T&>;
        ExecutePart<TT>(state, a);
        Execute(state, std::forward<T2>(rest)...);
    }

    /* This overload is for string constants, which are converted into string_views. */
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

template<typename FmtT, typename... T>
std::basic_string<char> Printf(FmtT&& fmt, T&&... args)
{
    if constexpr(std::is_same_v<std::remove_reference_t<FmtT>, PrintfFormatter>)
    {
        PrintfFormatter::State state;
        fmt.Execute(state, std::forward<T>(args)...);
        return std::basic_string<char>( state.result.begin(), state.result.end() );
    }
    else if constexpr(PrintfPrivate::IsString<std::remove_cvref_t<FmtT>>)
    {
        // The format string may be a string
        using CT = typename std::remove_cvref_t<FmtT>::value_type;
        return Printf( std::basic_string_view<CT>(fmt), std::forward<T>(args)...);
    }
    else if constexpr(std::is_array_v<std::remove_cvref_t<FmtT>>)
    {
        // The format string may be a character array (string constant)
        using CT = std::remove_cvref_t<decltype(fmt[0])>;
        return Printf( std::basic_string_view<CT>(&fmt[0], std::size(fmt)-1), std::forward<T>(args)... );
    }
    else if constexpr(std::is_pointer_v<FmtT> && std::is_integral_v<std::remove_pointer_t<FmtT>>)
    {
        // The format string may be a character pointer
        using CT = std::remove_pointer_t<FmtT>;
        return Printf( std::basic_string_view<CT>(fmt), std::forward<T>(args)... );
    }
    else
    {
        // The format string may be a string view. The prototype of MakeFrom() will validate this.
        PrintfFormatter Formatter;
        Formatter.MakeFrom(std::forward<FmtT>(fmt));
        return Printf( Formatter, std::forward<T>(args)... );
    }
}

//#define USE_PRINTFPROXY_PTR

struct PrintfProxy
{
    using data_t = std::pair<PrintfFormatter, PrintfFormatter::State>;
  #ifdef USE_PRINTFPROXY_PTR
    data_t* data{};
  #else
    data_t data {};
  #endif

    //template<typename CT,typename TR,typename A>
    //operator std::basic_string<CT,TR,A> () &&; // This implements str()

    template<typename CT,typename TR,typename A>
    operator std::basic_string<CT,TR,A> (); // Implements str()

    std::string str() && { return std::move(*this); }

  #ifdef USE_PRINTFPROXY_PTR
    PrintfProxy() : data{new data_t{}} {}
    ~PrintfProxy() { delete data; }
    PrintfProxy(PrintfProxy&& b) : data(b.data) { b.data = nullptr; }
    PrintfProxy& operator=(PrintfProxy&& b) { if(this != &b) { data = b.data; b.data = nullptr; } return *this; }
  #else
    PrintfProxy() = default;
    PrintfProxy(PrintfProxy&& b) = default;
    PrintfProxy& operator=(PrintfProxy&&) = default;
    ~PrintfProxy() = default;
  #endif
    PrintfProxy(std::basic_string_view<char> fmt) : PrintfProxy()
    {
        ref().first.MakeFrom(fmt);
    }

    PrintfProxy(const PrintfProxy&) = delete;
    PrintfProxy& operator= (const PrintfProxy&) = delete;

    // Parameter operands. These two functions have the same semantics as Execute(),
    // except that the actual formatting is only done when the string conversion operator is called.
    /*template<typename T>
    PrintfProxy operator % (T&& arg) &&
    {
        return std::move(*this %= std::forward<T>(arg));
    }*/
    template<typename T>
    PrintfProxy& operator %= (T&& arg)
    {
        ref().first.Execute( ref().second, std::forward<T>(arg), PrintfPrivate::Postpone{} );
        return *this;
    }

    PrintfProxy& operator %= (PrintfProxy&& arg);

    // Concatenation operands
    template<typename T>
    PrintfProxy operator+ (T&& b) &&;
    PrintfProxy operator+ (PrintfProxy&& b) &&;

private:
    inline data_t& ref()
    {
  #ifdef USE_PRINTFPROXY_PTR
        return *data;
  #else
        return data;
  #endif
    }
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
template<typename T>
inline PrintfProxy& operator % (PrintfProxy&& lhs,  T&& arg)
{
    lhs %= std::forward<T>(arg);
    return lhs; // Note: Converts rvalue reference into lvalue reference
}
template<typename T>
inline PrintfProxy& operator % (PrintfProxy& lhs,  T&& arg)
{
    lhs %= std::forward<T>(arg);
    return lhs;
}
#pragma GCC diagnostic pop

inline PrintfProxy operator ""_f(const char* format, std::size_t num)
{
    return PrintfProxy(std::string_view(format,num));
}

template<typename T>
PrintfProxy PrintfProxy::operator+ (T&& b) &&
{
    // Finish our string
    ref().first.Execute(ref().second);
    // Create a "%s" parameter
    ref().first.formats.emplace_back(); // arg{} with default options
    // Continue our processing with "b" as a new parameter
    return std::move(*this) % std::forward<T>(b);
}

template<typename T, typename TR>
std::basic_ostream<T,TR>& operator<< (std::basic_ostream<T,TR>& out, PrintfProxy&& b)
{
    out << std::move(b).str();
    return out;
}
template<typename T, typename TR>
std::basic_ostream<T,TR>& operator<< (std::basic_ostream<T,TR>& out, PrintfProxy& b)
{
    out << std::move(b).str();
    return out;
}

#endif
