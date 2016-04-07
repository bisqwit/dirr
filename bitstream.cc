#include <climits>   // CHAR_BIT
#include <algorithm> // std::min

#include "bitstream.hh"

void PutBits(void* memory, unsigned& bitpos, unsigned long V, unsigned nbits)
{
    unsigned char* buffer = reinterpret_cast<unsigned char*>(memory);
    while(nbits > 0)
    {
        unsigned bytepos = bitpos/CHAR_BIT, bits_remain = CHAR_BIT-bitpos%CHAR_BIT, bits_taken = CHAR_BIT-bits_remain;
        unsigned bits_to_write = std::min(nbits, bits_remain);
        unsigned value_mask     = (1 << bits_to_write)-1;
        unsigned value_to_write = V & value_mask;
        buffer[bytepos] = (buffer[bytepos] & ~(value_mask << bits_taken)) | (value_to_write << bits_taken);
        V >>= bits_to_write;
        nbits  -= bits_to_write;
        bitpos += bits_to_write;
    }
}

unsigned long GetBits(const void* memory, unsigned& bitpos, unsigned nbits)
{
    const unsigned char* buffer = reinterpret_cast<const unsigned char*>(memory);
    unsigned long result = 0, shift=0;
    while(nbits > 0)
    {
        unsigned bytepos = bitpos/CHAR_BIT, bits_remain = CHAR_BIT-bitpos%CHAR_BIT, bits_taken = CHAR_BIT-bits_remain;
        unsigned bits_to_take = std::min(nbits, bits_remain);
        unsigned v = (buffer[bytepos] >> bits_taken) & ((1 << bits_to_take)-1);
        result |= v << shift;
        shift += bits_to_take;
        nbits -= bits_to_take;
        bitpos += bits_to_take;
    }
    return result;
}
