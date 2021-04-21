#include "printf.hh"

#include <sstream>
#include <iomanip>
#include <tuple>
#include <ios>
#ifdef HAVE_CHARCONV
# include <charconv>
#endif

static const char DigitBufUp[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
static const char DigitBufLo[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

template<std::size_t CharSize>
static void StringConvert(PrintfFormatter& fmt, const void* data, std::size_t length)
{
    using CP  = std::tuple_element_t<CharSize-1, std::tuple<char, char16_t, char32_t/*nope*/, char32_t>>;
    using CT2 = std::conditional_t<CharSize==1, char, char32_t>;
    fmt.MakeFrom(std::basic_string_view<CT2>( std::basic_string<CT2>((const CP*)data, (const CP*)data + length)));
}
template<typename CT>
void PrintfFormatter::MakeFrom(std::basic_string_view<CT> format)
{
    if constexpr(!std::is_same_v<char, CT> && !std::is_same_v<char32_t, CT>)
    {
        // To reduce size of the code, we only do char and char32_t. Others are converted to one of these.
        return StringConvert<sizeof(CT)>(*this, format.data(), format.size());
    }
    for(std::size_t b = format.size(), a = 0; a < b; )
    {
        CT c = format[a];
        if(c == '%')
        {
            std::size_t percent_begin = a++;

            arg argument;
            if(a < format.size() && format[a] == '%') { ++a; goto literal_percent; }

            if(a < format.size() && format[a] == '-') { argument.leftalign = true; ++a; }
            if(a < format.size() && format[a] == '+') { argument.sign      = true; ++a; }
            if(a < format.size() && format[a] == '0') { argument.zeropad   = true; ++a; }
            if(a < format.size() && format[a] == '*') { argument.param_minwidth = true; ++a; }
            else while(a < format.size() && (format[a] >= '0' && format[a] <= '9'))
                argument.min_width = argument.min_width*10 + (format[a++] - '0');

            if(a < format.size() && format[a] == '.')
            {
                argument.max_width = 0;
                if(++a < format.size() && format[a] == '*')
                    { argument.param_maxwidth = true; ++a; }
                else while(a < format.size() && (format[a] >= '0' && format[a] <= '9'))
                    argument.max_width = argument.max_width*10 + (format[a++] - '0');
            }

       another_formatchar:
            if(a >= format.size()) goto invalid_format;
            switch(format[a++])
            {
                case 'z':
                case 'l': goto another_formatchar; // ignore 'l' or 'z'
                case 'S':
                case 's': argument.format = arg::as_string; break;
                case 'C':
                case 'c': argument.format = arg::as_char; break;
                case 'x': argument.base   = arg::hex;
                          argument.format = arg::as_int; break;
                case 'X': argument.base   = arg::hexup;
                          argument.format = arg::as_int; break;
                case 'o': argument.base   = arg::oct;
                          argument.format = arg::as_int; break;
                case 'b': argument.base   = arg::bin;
                          argument.format = arg::as_int; break;
                case 'i':
                case 'u':
                case 'd': argument.format = arg::as_int; break;
                // f: TODO: decimal notation.     Precision is assumed as 6 if not specified.
                // e: TODO: exponential notation. Precision is assumed as 6 if not specified.
                // g: TODO: Uses 'e' if exponent <= -4 or exponent >= precision.
                case 'g':
                case 'e':
                case 'f': argument.format = arg::as_float; break;
                default:
            invalid_format:
                    fprintf(stderr, "Invalid format...\n");
                    a = percent_begin + 1;
            literal_percent:
                    trail += '%';
                    continue;
            }

            argument.before.swap(trail);
            formats.push_back(std::move(argument));
        }
        else
        {
            trail += c;
            ++a;
        }
    }
}

// When no parameters are remaining
void PrintfFormatter::Execute(PrintfFormatter::State& state)
{
    for(std::size_t pos = (state.position + 3) / 4; pos < formats.size(); ++pos)
    {
        state.result.append( std::move(formats[pos].before) );
        state.result.append( (std::size_t) formats[pos].min_width, L' ' );
        formats[pos].before.clear();
    }
    state.result += std::move(trail); trail.clear();

    formats.clear();
    state.position = 0;
}

namespace PrintfPrivate
{
    // Use this function instead of "value < 0"
    // to avoid a compiler warning about expression being always false
    // due to limited datatype (when instantiated for unsigned types).
    template<typename T>
    static bool IsNegative(T&& value)
    {
        if constexpr(std::is_signed_v<std::remove_cvref_t<T>>)
            return value < 0;
        return false;
    }

    auto MakeInt(std::string_view part)
    {
        // We decide to judge it as a signed value.
        long long l = 0;
        if(!part.empty())
        {
          #ifdef HAVE_CHARCONV
            std::from_chars(&part[0], &part[0]+part.size(), l);
          #else
            bool nega = false;
            if(part[0]=='-') { nega=true; part.remove_prefix(1); }
            while(!part.empty() && part[0]>='0' && part[0]<='9')
            {
                l = l*10 + part[0]-'0';
                part.remove_prefix(1);
            }
            if(nega) l = -l;
          #endif
        }
        return l;
    }

    template<typename T>
    auto MakeInt(T&& part)
    {
        using TT = std::remove_cvref_t<T>;
        if constexpr(std::is_arithmetic_v<TT>)
        {
            // Also converts floats to long long
            return std::conditional_t<std::is_signed_v<T>, long long, unsigned long long>(part);
        }
        else
        {
            // Delegate it.
            if constexpr(std::is_pointer_v<T>)
            {
                // Automatically deduces character type and determines string length
                return MakeInt(std::basic_string_view(part));
            }
            else if constexpr(std::is_same_v<typename TT::value_type, char>)
            {
                return MakeInt(std::string_view(&part[0], part.size()));
            }
            else
            {
                // Convert into 8-bit string so we can use from_chars.
                return MakeInt(std::string_view(std::string(part.begin(), part.end())));
            }
            //return std::stoll(part); // Doesn't work with char32_t
            //                            Neither does std::from_chars.
        }
    }

    struct PrintfFormatDo
    {
        template<typename CT>
        static void DoString(
            PrintfFormatter::argsmall& arg,
            std::basic_string<char32_t> & result,
            std::basic_string_view<CT> part);

        template<typename FT, typename K = std::stringstream>
      #ifdef HAVE_CONCEPTS
        requires std::is_arithmetic_v<FT>
      #endif
        static void DoString(
            PrintfFormatter::argsmall& arg,
            std::basic_string<char32_t> & result,
            FT part)
        {
            if constexpr(std::is_integral_v<FT>)
            {
                // This deals with char16_t, char32_t which std::stringstream doesn't support
                return DoString(arg, result, MakeInt(part));
            }
            else
            {
                // Print float or int as string
                // TODO: Handle different formatting styles (exponents, automatic precisions etc.)
                //       better than this.
                K s; // This must be a template type; otherwise the constexpr-requires does not work at least in GCC.
                if(arg.sign) s << std::showpos;
                if(arg.base == PrintfFormatter::arg::hex) s << std::setbase(16);
                if(arg.base == PrintfFormatter::arg::oct) s << std::setbase(8);
                if(arg.base == PrintfFormatter::arg::bin) s << std::setbase(2);
                s << std::setprecision( arg.max_width);
                s << std::fixed;
                s << part;
                arg.max_width = ~0u;

                // Use view(), if the STL has support for it
            #if defined(HAVE_CONCEPTS) && (!defined(__GNUC__) || __GNUC__ >= 10)
                if constexpr(requires() { s.rdbuf()->view(); })
                {
                    DoString(arg, result, s.rdbuf()->view());
                }
                else
            #endif
                {
                    DoString(arg, result, std::string_view(s.rdbuf()->str()));
                }
            }
        }

        template<typename T>
        static void Do(PrintfFormatter::argsmall& arg, std::basic_string<char32_t>& result, T&& part)
        {
            using TT = std::remove_cvref_t<T>;
            switch(arg.format)
            {
                case PrintfFormatter::argsmall::as_char:
                {
                    // If int or float, interpret as character.
                    if constexpr(std::is_arithmetic_v<TT>)
                    {
                        //fprintf(stderr, "Formatting arith as char\n");
                        char32_t n = part;
                        DoString(arg, result, std::basic_string_view<char32_t>(&n, 1));
                        return;
                    }
                    else
                    {
                        using CT = std::remove_cvref_t<decltype(part[0])>;
                        //fprintf(stderr, "Formatting string as char\n");
                        DoString(arg, result, std::basic_string_view<CT>(part));
                    }
                    return;
                }
                case PrintfFormatter::argsmall::as_int:
                case PrintfFormatter::argsmall::as_float:
                {
                    if constexpr(std::is_floating_point_v<T>)
                    {
                        // If float, cast into integer, and reinterpret
                        Do(arg, result, (long long)(part));
                        return;
                    }
                    else if constexpr(!std::is_arithmetic_v<T>)
                    {
                        // If not integer, process as string
                        using CT = std::remove_cvref_t<decltype(part[0])>;
                        //fprintf(stderr, "Formatting string as arith\n");
                        DoString(arg, result, std::basic_string_view<CT>(part));
                    }
                    else if constexpr(std::is_integral_v<T> && std::is_signed_v<T> && !std::is_same_v<T, long long>)
                    {
                        // Convert into long long to reduce duplicated code
                        Do(arg, result, (long long)(part));
                        return;
                    }
                    else if constexpr(std::is_integral_v<T> && !std::is_signed_v<T> && !std::is_same_v<T, unsigned long long>)
                    {
                        // Convert into long long to reduce duplicated code
                        Do(arg, result, (unsigned long long)(part));
                        return;
                    }
                    else
                    {
                        //fprintf(stderr, "Formatting arith\n");
                        // Is integer type
                        std::string s;
                        std::make_unsigned_t<T> upart = part;

                        if(IsNegative(part))
                                          { s += '-'; upart = -part; }
                        else if(arg.sign)   s += '+';

                        std::string digitbuf;

                        const char* digits = (arg.base & 64) ? DigitBufUp : DigitBufLo;
                        unsigned base = arg.base & ~64;
                        while(upart != 0)
                        {
                            digitbuf += digits[ upart % base ];
                            upart /= base;
                        }

                        // Append the digits in reverse order
                        for(std::size_t a = digitbuf.size(); a--; )
                            s += digitbuf[a];
                        if(digitbuf.empty())
                            s += '0';

                        // Process the rest as a string (deals with width)
                        arg.max_width = ~0u;
                        DoString(arg, result, std::string_view(s));
                    }
                    break;
                }

                case PrintfFormatter::argsmall::as_string:
                {
                    if constexpr(std::is_arithmetic_v<TT>)
                    {
                        //fprintf(stderr, "Formatting num as string\n");
                        DoString(arg, result, std::forward<T>(part));
                    }
                    else
                    {
                        using CT = std::remove_cvref_t<decltype(part[0])>;
                        //fprintf(stderr, "Formatting string as string\n");
                        DoString(arg, result, std::basic_string_view<CT>(part));
                    }
                    break;
                }
            }
        }
    };

    template<typename CT>
    void PrintfFormatDo::DoString(
        PrintfFormatter::argsmall& arg,
        std::basic_string<char32_t> & result,
        std::basic_string_view<CT> part)
    {
        std::size_t length = std::min<std::size_t>(part.size(), arg.max_width);

        std::size_t pad       = (length < arg.min_width) ? arg.min_width-length : 0;
        std::size_t pad_left  = arg.leftalign ? 0 : pad;
        std::size_t pad_right = arg.leftalign ? pad : 0;
        /*fprintf(stderr, "Pad=%zu (size=%zu, length=%zu, min=%u, max=%u)\n",
            pad,
            part.size(), length, arg.min_width, arg.max_width);*/

        result.reserve(result.size() + length + pad);

        char32_t padding = arg.zeropad ? L'0' : L' ';

        result.append(pad_left, padding);
        result.insert(result.end(), part.begin(), part.begin() + length);
        result.append(pad_right, padding);
    }
}

using namespace PrintfPrivate;

template<typename T>
void PrintfFormatter::ExecutePart(PrintfFormatter::State& state, T part) /* Note: T is explicitly specified */
{
    std::size_t position = state.position, pos = position / 4,  subpos = position % 4;
    if(pos >= formats.size()) return;

    switch(subpos)
    {
        case 0: LIKELY
            state.result   += formats[pos].before;
            state.minwidth  = formats[pos].min_width;
            state.maxwidth  = formats[pos].max_width;
            state.leftalign = formats[pos].leftalign;
            //
            if(formats[pos].param_minwidth) UNLIKELY
            {
                // This param should be an integer.
                auto p = MakeInt(std::move(part));
                //fprintf(stderr, "minwidth=%lld\n", (long long)p);
                // Use this contrived expression rather than "p < 0"
                // to avoid a compiler warning about expression being always false
                // due to limited datatype (when instantiated for unsigned types)
                if(IsNegative(p))
                {
                    state.leftalign = true;
                    state.minwidth  = -p;
                }
                else
                    state.minwidth = p;

                state.position = pos*4 + 1;
                return;
            }
            [[fallthrough]];
        case 1:
            if(formats[pos].param_maxwidth) UNLIKELY
            {
                // This param should be an integer.
                auto p = MakeInt(std::move(part));
                //fprintf(stderr, "maxwidth=%lld\n", (long long)p);
                state.maxwidth = p;
                state.position = pos*4 + 2;
                return;
            }
            [[fallthrough]];
        case 2: default:
            /*{ std::stringstream temp;
              if constexpr(std::is_integral_v<std::remove_cvref_t<T>>)
                   temp << MakeInt(part);
              else temp << part;
              fprintf(stderr, "Formatting this param: <%s> (%zu)\n", temp.str().c_str(), temp.str().size());
            }*/
            argsmall a { state.minwidth,
                         state.maxwidth,
                         state.leftalign,
                         formats[pos].sign,
                         formats[pos].zeropad,
                         formats[pos].base,
                         formats[pos].format };
            PrintfFormatDo::Do(a, state.result, std::move(part));

            state.position = (pos+1)*4; // Sets subpos as 0
    }
}

template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, char);
#ifdef HAVE_CHAR8_T
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, char8_t);
#endif
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, char16_t);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, char32_t);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, short);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, int);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, long);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, long long);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, signed char);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, unsigned char);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, unsigned short);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, unsigned int);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, unsigned long);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, unsigned long long);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, bool);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, float);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, double);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, long double);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, const char*);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, const std::string&);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, const std::basic_string<char32_t>&);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, std::basic_string_view<char>);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, std::basic_string_view<char32_t>);

template void PrintfFormatter::MakeFrom(std::basic_string_view<char>);
template void PrintfFormatter::MakeFrom(std::basic_string_view<wchar_t>);
#ifdef HAVE_CHAR8_T
template void PrintfFormatter::MakeFrom(std::basic_string_view<char8_t>);
#endif
template void PrintfFormatter::MakeFrom(std::basic_string_view<char16_t>);
template void PrintfFormatter::MakeFrom(std::basic_string_view<char32_t>);

PrintfProxy PrintfProxy::operator+ (PrintfProxy&& b) &&
{
    // Finish our string
    ref().first.Execute(ref().second);
    // Finish their string
    b.ref().first.Execute(b.ref().second);
    // Append their result to us
    ref().second.result += b.ref().second.result;
    return std::move(*this);
}


PrintfProxy& PrintfProxy::operator %= (PrintfProxy&& arg)
{
    // Finish their string
    arg.ref().first.Execute(arg.ref().second);
    // Execute their outcome as a parameter to our printf
    ref().first.ExecutePart<const std::basic_string<char32_t>&>
        (ref().second, arg.ref().second.result);
    return *this;
}

template<typename CT,typename TR,typename A>
PrintfProxy::operator std::basic_string<CT,TR,A> () // Implements str()
{
    ref().first.Execute(ref().second); // Finally calls the no-parameters remaining function
    if constexpr(std::is_same_v<CT, char32_t>)
    {
        return std::move(ref().second.result);
    }
    else
    {
        return {ref().second.result.begin(), ref().second.result.end()};
    }
}

template PrintfProxy::operator std::basic_string<char> ();
template PrintfProxy::operator std::basic_string<char32_t> ();
