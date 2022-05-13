#ifndef dirr3_cons_hh
#define dirr3_cons_hh

#include <algorithm> // std::swap
#include "printf.hh"
#include "config.h"

extern bool Colors;
extern bool AnsiOpt;
extern bool Pagebreaks;

extern int WhereX;
extern int LINES, COLS;

extern int Gputch(int x);
extern void SetAttr(int newattr);
extern int ColorNums, TextAttr;

enum { DEFAULTATTR = 7 };

class GprintfProxy
{
    PrintfProxy p;
public:
    GprintfProxy(PrintfProxy&& q) : p(std::move(q))
    {
    }
    GprintfProxy(std::string_view str): p(str)
    {
    }
    GprintfProxy(GprintfProxy&&) = default;

    template<typename T>
    GprintfProxy& operator%= (T&& arg)
    {
        p %= std::forward<T>(arg);
        return *this;
    }
    operator std::size_t()
    {
        std::string str = std::move(p).str();
        int ta = TextAttr, cn = ColorNums >= 0 ? ColorNums : ta;
        std::size_t n = 0;
        for(char c: str)
            if(c != '\1') LIKELY
            {
                Gputch(c);
                ++n;
            }
            else
            {
                SetAttr(cn);
                std::swap(ta, cn);
            }
        return n;
    }
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
template<typename T>
inline GprintfProxy& operator % (GprintfProxy&& lhs, T&& arg)
{
    lhs %= std::forward<T>(arg);
    return lhs; // Note: Converts rvalue reference into lvalue reference
}
template<typename T>
inline GprintfProxy& operator % (GprintfProxy& lhs,  T&& arg)
{
    lhs %= std::forward<T>(arg);
    return lhs;
}
#pragma GCC diagnostic pop

inline GprintfProxy operator ""_g(const char* format, std::size_t num)
{
    return GprintfProxy(std::string_view(format,num));
}

template<typename... Args>
std::size_t Gprintf(std::string_view fmt, Args&&... args)
{
    GprintfProxy temp(fmt);
    return std::move( (temp %= ... %= std::forward<Args>(args)) );
}
template<std::size_t N, typename... Args>
std::size_t Gprintf(const char (&fmt)[N], Args&&... args)
{
    return Gprintf(std::string_view(fmt, N-1), std::forward<Args>(args)...);
}


extern std::size_t Gwrite(std::string_view s);
extern std::size_t Gwrite(std::string_view s, std::size_t pad);
extern bool is_doublewide(char32_t c);

extern void GetScreenGeometry();
template<typename F>
inline std::uint_fast64_t ParseUTF8(F&& getchar) // Return value: 32-bits char, >>32 = length in bytes. 0 = invalid sequence
{
    unsigned cache = 0/*, bytesleft = 0*/, length = 0, bytesleft = 0;
    for(;;)
    {
        unsigned c = getchar(length++);
        if(c & ~0xFF) break;
        if(bytesleft > 0)           { cache = cache * 0x40 + (c & 0x3F); --bytesleft;
                                      if((c & 0xC0) != 0x80) { /*fprintf(stderr, "rej=%02X\n", c);*/ break; }
                                    }
        else if((c & 0xE0) == 0xC0) { cache = c & 0x1F; bytesleft=1; }
        else if((c & 0xF0) == 0xE0) { cache = c & 0x0F; bytesleft=2; }
        else if((c & 0xF8) == 0xF0) { cache = c & 0x07; bytesleft=3; }
        else                        { cache = c & 0x7F; if(c >= 0x80) break; }
        /*fprintf(stderr, "utf8 seq len=%u c=%02X cache=%04X bytesleft=%u\n", length,c,cache,bytesleft);*/
        if(!bytesleft)
        {
            char32_t c = cache, mincode = 0;
            switch(length)
            {
                default:
                case 1: mincode = 0; break;
                case 2: mincode = 0x80; break;
                case 3: mincode = 0x800; break;
                case 4: mincode = 0x10000; break;
            }
            if(c < mincode) break; // Invalid sequence
            return c | (((std::uint_fast64_t)length) << 32);
        }
    }
    return 0; // Invalid sequence
};


/* Print maximum of "maxlen" characters from buf.
 * If buf is longer, print spaces.
 * Convert unprintable characters into question marks.
 * Printable (unsigned): 20..7E, A0..FF    Unprintable: 00..1F, 7F..9F
 * Printable (signed):   32..126, -96..-1, Unprintable: -128..-97, 0..31
 */
template<bool print>
std::size_t WidthPrintHelper(std::size_t maxlen, std::string_view buf, bool fill)
{
    std::size_t column = 0, bytepos = 0;
    for(; column<maxlen && bytepos<buf.size(); ++column)
    {
        /* Detect UTF8 sequence */
        auto utf8 = ParseUTF8([&](unsigned index) -> int
        {
            if(bytepos+index >= buf.size()) return -1;
            return (unsigned char)buf[bytepos+index];
        });
        if(utf8)
        {
            // Parse the UTF8 byte sequence
            char32_t seq = utf8 & 0xFFFFFFFFu;
            unsigned length = utf8 >> 32;

            if(seq >= 0xD800 && seq <= 0xDFFF)
            {
                if(seq >= 0xDC00) goto invalid_unicode_char;
                // Check for surrogate sequence
                auto another = ParseUTF8([&](unsigned index) -> int
                {
                    if(bytepos+length+index >= buf.size()) return -1;
                    return (unsigned char)buf[bytepos+length+index];
                });
                char32_t seq2 = another & 0xFFFFFFFu;
                if(seq2 >= 0xDC00 && seq2 <= 0xDFFF)
                {
                    length += another >> 32;
                    // Recompose from the surrogate
                    seq = ((seq - 0xD800u)*0x400u) + (seq2 - 0xDC00u) + 0x10000u;
                }
                else
                    goto invalid_unicode_char;
            }
            if(length == 1) LIKELY
            {
                // Use the code that avoids unprintable characters
                // Also, no single-byte character is double-wide.
                goto invalid_unicode_char;
            }
            // If a doublewide character would be too many columns,
            // end the loop now.
            if (column == maxlen-1 && is_doublewide(seq))
                break;
            // But print it byte by byte.
            if(print)
                for(unsigned n=0; n<length; ++n)
                    Gputch( buf[bytepos+n] );
            bytepos += length;
            /* If this is a double-wide character, add 1 column */
            if(is_doublewide(seq))
                ++column;
        }
        else invalid_unicode_char:
        {
            unsigned char c = buf[bytepos++];
            /*fprintf(stderr, "not valid %02X\n", c);*/
            if(print)
            {
                if((c >= 32 && c < 0x7F) || (c >= 0xA0) || c == '\n')
                    Gputch(c);
                else
                    Gputch('?');
            }
        }
    }
    //fprintf(stderr, "Printed %zu bytes: <%.*s>, %zu/%zu columns, fill=%s\n", bytepos, (int)bytepos, buf.data(), column, maxlen, fill?"Y":"N");
    if(fill && column < maxlen)
    {
        //if(print) fprintf(stderr, "Filled %ld spaces\n", long(maxlen-column));
        if(print) Gprintf("%*s", maxlen-column, "");
        column = maxlen;
    }
    return column;
}

inline std::size_t WidthPrint(std::size_t maxlen, std::string_view buf, bool fill)
{
    return WidthPrintHelper<true>( maxlen, buf, fill );
}

template<std::size_t N>
inline std::size_t WidthPrint(std::size_t maxlen, const char (&buf)[N], bool fill)
{
    return WidthPrintHelper<true>( maxlen, std::string_view(buf, N-1), fill );
}


inline std::size_t WidthInColumns(std::string_view buf)
{
    return WidthPrintHelper<false>( ~std::size_t(), buf, false );
}

#endif

