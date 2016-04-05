#ifndef bqtEprintfHH
#define bqtEprintfHH

#include <type_traits>
#include <ostream>
#include <string>
#include <vector>

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
    void MakeFrom(const std::basic_string<C>& format);

    template<typename CT>
    void MakeFrom(const CT* format)
    {
        MakeFrom( std::basic_string<CT> (format) );
    }

    template<typename T, typename... T2>
    std::enable_if_t<
        std::is_arithmetic< std::remove_cv_t<T>>::value
     || std::is_assignable< std::string, T >::value
     || std::is_assignable< std::basic_string<char32_t>, T >::value,
        void> Execute(State& state, const T& a, T2&&... rest)
    {
        // TODO: Use is_trivially_copyable in conjunction with sizeof,
        //       rather than is_pod, once GCC supports it.
        // Choose the optimal manner of parameter passing:
        //using TT = typename
        //    std::conditional<std::is_pod<T>::value, T, const T&>::type;
        using TT = std::conditional_t<std::is_trivially_copyable<T>::value && sizeof(T) <= 16, T, const T&>;
        ExecutePart<TT>(state, a);
        Execute(state, std::forward<T2>(rest)...);
    }
    template<typename T, std::size_t size, typename... T2>
    void Execute(State& state, const T(& a)[size], T2&&... rest)
    {
        ExecutePart<const T*>(state, a);
        Execute(state, std::forward<T2>(rest)...);
    }

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
    std::basic_string<char> result( state.result.begin(), state.result.end() );
    return result;
}


/* This is the function you would use. */
template<typename CT, typename... T>
std::basic_string<char> Printf(const std::basic_string<CT>& format, T&&... args)
{
    PrintfFormatter Formatter;
    Formatter.MakeFrom(format);
    return Printf(Formatter, std::forward<T>(args)...);
}

/* This is the function you would use. */
template<typename CT, typename... T>
std::basic_string<char> Printf(const CT* format, T&&... args)
{
    PrintfFormatter Formatter;
    Formatter.MakeFrom(format);
    return Printf(Formatter, std::forward<T>(args)...);
}

struct PrintfProxy
{
    std::pair<PrintfFormatter, PrintfFormatter::State>* data = nullptr;

    template<typename CT,typename TR,typename A>
    operator std::basic_string<CT,TR,A> () &&
    {
        data->first.Execute(data->second);
        return {data->second.result.begin(), data->second.result.end()};
    }

    std::string str() && { return std::move(*this); }

    PrintfProxy() : data(new std::remove_reference_t<decltype(*data)>) { }
    PrintfProxy(const PrintfProxy&) = delete;
    PrintfProxy& operator= (const PrintfProxy&) = delete;
    PrintfProxy(PrintfProxy&& b) : data(b.data)
    {
        b.data  = nullptr;
    }
    PrintfProxy& operator=(PrintfProxy&&) = delete;
    inline ~PrintfProxy() { delete data; }

    // Parameter operands
    template<typename T>
    PrintfProxy operator %(const T& arg) &&
    {
        using TT = std::conditional_t<std::is_trivially_copyable<T>::value && sizeof(T) <= 16, T, const T&>;
        data->first.ExecutePart<TT>(data->second, arg);
        return std::move(*this);
    }
    template<typename CT, std::size_t size>
    PrintfProxy operator %(const CT(&arg)[size]) &&
    {
        data->first.ExecutePart<const CT*>(data->second, arg);
        return std::move(*this);
    }

    PrintfProxy operator %(PrintfProxy&& arg) &&;

    // Concatenation operands
    template<typename T>
    PrintfProxy operator+ (const T& b) &&;
    PrintfProxy operator+ (PrintfProxy&& b) &&;
};

PrintfProxy operator ""_f(const char* format, std::size_t num);

template<typename T>
PrintfProxy PrintfProxy::operator+ (const T& b) &&
{
    // Finish our string
    data->first.Execute(data->second);
    // Create a "%s" parameter
    data->first.formats.emplace_back(); // arg{} with default options
    // Continue our processing with "b" as a new parameter
    return std::move(*this) % b;
}

template<typename T, typename TR>
std::basic_ostream<T,TR>& operator<< (std::basic_ostream<T,TR>& out, PrintfProxy&& b)
{
    out << std::move(b).str();
    return out;
}

#endif
