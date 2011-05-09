#include "apebitmap.h"

ApeBitMap::ApeBitMap(uint32_t size)
{
    size_ = (size / 8) + (size % 8 ? 1 : 0);
    bits_ = new uint8_t[size_ / 8]();
}

void ApeBitMap::tobuffer(void* bitbuffer)
{
    memcpy(bits_, bitbuffer, size_ / 8);
}

ApeBitMap::~ApeBitMap()
{
    delete[] bits_;
}

void ApeBitMap::setbit(uint32_t bitnum)
{
    uint32_t bytenum = bitnum / 8;
    uint32_t bytebit = 7 - (bitnum % 8);
    bits_[bytenum] =  bits_[bytenum] | (1 << bytebit);
}

void ApeBitMap::unsetbit(uint32_t bitnum)
{
    uint32_t bytenum = bitnum / 8;
    uint32_t bytebit = 7 - (bitnum % 8);
    bits_[bytenum] =  bits_[bytenum] & (!(1 << bytebit));
}

bool ApeBitMap::getbit(uint32_t bitnum)
{
    uint32_t bytenum = bitnum / 8;
    uint32_t bytebit = 7 - (bitnum % 8);
    return (bits_[bytenum] & (1 << bytebit)) != 0;
}

uint32_t ApeBitMap::findunsetbit()
{
    uint32_t bytesize = size_ / 8;
    uint32_t i = 0;
    while (i < bytesize)
    {
        if (bits_[i])
        {
            uint8_t byte = bits_[i];
            uint32_t bit = (i + 1) * 8;
            while (byte != 1)
            {
                byte >>= 1;
                bit--;
            }
            return bit;
        }
    }
    return NOBIT;
}

inline uint32_t ApeBitMap::size() const
{
    return size_;
}
