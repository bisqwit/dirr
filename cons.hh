#ifndef dirr3_cons_hh
#define dirr3_cons_hh

#include <algorithm> // std::swap
#include "likely.hh"
#include "printf.hh"

extern bool Colors;
extern bool AnsiOpt;
extern bool Pagebreaks;

extern int WhereX;
extern int LINES, COLS;

extern int Gputch(int x);
extern void SetAttr(int newattr);
extern int ColorNums, TextAttr;

enum { DEFAULTATTR = 7 };

template<typename... Args>
std::size_t Gprintf(const std::string& fmt, Args&&... args)
{
    std::string str = Printf(fmt, std::forward<Args>(args)...);
    std::size_t end = str.size();
    int ta = TextAttr, cn = ColorNums >= 0 ? ColorNums : ta;
    for(char c: str)
        if(likely(c != '\1'))
            Gputch(c);
        else
        {
            SetAttr(cn);
            std::swap(ta, cn);
        }
    return end;
}

extern std::size_t Gwrite(const std::string& s);
extern std::size_t Gwrite(const std::string& s, std::size_t pad);
extern bool is_doublewide(char32_t c);

extern void GetScreenGeometry();

template<bool print>
inline std::size_t WidthPrint(std::size_t maxlen, const string &buf, bool fill)
{
    /* Print maximum of "maxlen" characters from buf.
     * If buf is longer, print spaces.
     * Convert unprintable characters into question marks.
     * Printable (unsigned): 20..7E, A0..FF    Unprintable: 00..1F, 7F..9F
     * Printable (signed):   32..126, -96..-1, Unprintable: -128..-97, 0..31
     */
    auto ParseUTF8 = [](auto&& getchar) -> std::uint_fast64_t // Return value: 32-bits char, >>32 = length in bytes. 0 = invalid sequence
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
            else                        { cache = c & 0x7F;              }
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
                /*
                TODO: Implement surrogate support
                if(__builtin_expect(result.empty(), false)
                || __builtin_expect(c < 0xDC00 || c > 0xDFFF, true)
                || __builtin_expect(result.back() < 0xD800 || result.back() > 0xDBFF, false))
                */
                    return c | (((std::uint_fast64_t)length) << 32);
                /*
                else
                    result.back() = (result.back() - 0xD800u)*0x400u + (c - 0xDC00u) + 0x10000u;
                */
            }
        }
        return 0; // Invalid sequence
    };

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
            char32_t seq = utf8 & 0xFFFFFFFFu;
            unsigned length = utf8 >> 32;
            if(print)
                for(unsigned n=0; n<length; ++n)
                    Gputch( buf[bytepos+n] );
            bytepos += length;
            /* If this is a double-wide character, add 1 column */
            if(is_doublewide(seq))
                ++column;
        }
        else
        {
            unsigned char c = buf[bytepos++];
            /*fprintf(stderr, "not valid %02X\n", c);*/
            if(print)
            {
                if((c >= 32 && c < 0x7F) || (c >= 0xA0))
                    Gputch(c);
                else
                    Gputch('?');
            }
        }
    }
    /*fprintf(stderr, "Printed %zu bytes: <%.*s>\n", bytepos, (int)bytepos, buf.c_str());*/
    if(print && fill)
    {
        for(; column < maxlen; ++column) Gputch(' ');
    }
    return column;
}



#endif
