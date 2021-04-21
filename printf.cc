#include "printf.hh"
#include <sstream>
#include <iomanip>
#include <ios>

static const char DigitBufUp[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
static const char DigitBufLo[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

template<typename CT>
void PrintfFormatter::MakeFrom(std::basic_string_view<CT> format)
{
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
            formats.push_back(argument);
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
        state.result.append(formats[pos].before);
        state.result.append( (std::size_t) formats[pos].min_width, L' ' );
    }
    state.result += trail;

    formats.clear();
    state.position = 0;
    trail.clear();
}

namespace
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

    // XXX: This is required with clang++'s libc++
    static void operator << (std::basic_stringstream<char>& o, const std::basic_string<char32_t>& part)
    {
        for(char c: part)
            o << c;
    }
    static void operator << (std::basic_stringstream<char>& o, std::basic_string_view<char32_t> part)
    {
        for(char c: part)
            o << c;
    }
    struct PrintfFormatDo
    {
        template<typename CT>
        static void DoString(
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

        template<typename FT> requires std::is_arithmetic_v<FT>
        static void DoString(
            PrintfFormatter::argsmall& arg,
            std::basic_string<char32_t> & result,
            FT part)
        {
            // Print float or int as string
            // TODO: Handle different formatting styles (exponents, automatic precisions etc.)
            //       better than this.
            std::stringstream s;
            if(arg.sign) s << std::showpos;
            if(arg.base == PrintfFormatter::arg::hex) s << std::setbase(16);
            if(arg.base == PrintfFormatter::arg::oct) s << std::setbase(8);
            if(arg.base == PrintfFormatter::arg::bin) s << std::setbase(2);
            s << std::setprecision( arg.max_width);
            s << std::fixed;
            if constexpr(std::is_integral_v<FT>)
                s << MakeInt(part); // This deals with char16_t, char32_t which are deleted
            else
                s << part;
            arg.max_width = ~0u;

            // Use view() once GCC supports it
            //DoString(arg, result, s.rdbuf()->view());
            DoString(arg, result, std::string_view(s.rdbuf()->str()));
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

        template<typename T>
        static auto MakeInt(T&& part)
        {
            using TT = std::remove_cvref_t<T>;

            if constexpr(std::is_integral_v<TT> && !std::is_same_v<TT, wchar_t>
                                                && !std::is_same_v<TT, char8_t>
                                                && !std::is_same_v<TT, char16_t>
                                                && !std::is_same_v<TT, char32_t>)
                return part;
            else if constexpr(std::is_arithmetic_v<TT>)
                return (long long) part; // Also converts floats to long long
            else
            {
                // Delegate it
                // XXX: Doesn't work with clang++'s libc++
                std::basic_stringstream<char> s;
                long long l = 0;
                s << part;
                s >> l;
                return l;
                //return std::stoll(part); // Doesn't work with char32_t
                //                            Neither does std::from_chars.
            }
        }
    };
}

template<typename T>
void PrintfFormatter::ExecutePart(PrintfFormatter::State& state, T part) /* Note: T is explicitly specified */
{
    std::size_t position = state.position, pos = position / 4,  subpos = position % 4;
    if(pos >= formats.size()) return;

    switch(subpos)
    {
        case 0:
            state.result   += formats[pos].before;
            state.minwidth  = formats[pos].min_width;
            state.maxwidth  = formats[pos].max_width;
            state.leftalign = formats[pos].leftalign;
            //
            if(formats[pos].param_minwidth)
            {
                // This param should be an integer.
                auto p = PrintfFormatDo::MakeInt(std::move(part));
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
            if(formats[pos].param_maxwidth)
            {
                // This param should be an integer.
                auto p = PrintfFormatDo::MakeInt(std::move(part));
                //fprintf(stderr, "maxwidth=%lld\n", (long long)p);
                state.maxwidth = p;
                state.position = pos*4 + 2;
                return;
            }
            [[fallthrough]];
        case 2: default:
            /*{ std::stringstream temp;
              if constexpr(std::is_integral_v<std::remove_cvref_t<T>>)
                   temp << PrintfFormatDo::MakeInt(part);
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
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, char16_t);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, char32_t);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, short);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, int);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, long);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, long long);
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
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, signed char);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, std::basic_string_view<char>);
template void PrintfFormatter::ExecutePart(PrintfFormatter::State&, std::basic_string_view<char32_t>);

template void PrintfFormatter::MakeFrom(std::basic_string_view<char>);
template void PrintfFormatter::MakeFrom(std::basic_string_view<char32_t>);

PrintfProxy::PrintfProxy(std::basic_string_view<char> fmt) : PrintfProxy()
{
    data->first.MakeFrom(fmt);
}

PrintfProxy PrintfProxy::operator+ (PrintfProxy&& b) &&
{
    // Finish our string
    data->first.Execute(data->second);
    // Finish their string
    b.data->first.Execute(b.data->second);
    // Append their result to us
    data->second.result += b.data->second.result;
    return std::move(*this);
}


PrintfProxy& PrintfProxy::operator %= (PrintfProxy&& arg)
{
    // Finish their string
    arg.data->first.Execute(arg.data->second);
    // Execute their outcome as a parameter to our printf
    data->first.ExecutePart<const std::basic_string<char32_t>&>
        (data->second, arg.data->second.result);
    return *this;
}
