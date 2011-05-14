#include "apebitmap.h"

ApeBitMap::ApeBitMap()
{
	bits_ = NULL;
}

void ApeBitMap::reserve(uint32_t size)
{
	if (bits_)
		delete[] bits_;
	if (size > 0)
	{
		size_ = size;
		bits_ = new uint8_t[size];
	}
}

void ApeBitMap::tobuffer(void* bitbuffer)
{
    memcpy(bitbuffer, bits_, size_);
}

void ApeBitMap::frombuffer(void* bitbuffer, uint32_t size)
{
	reserve(size);
	memcpy(bits_, bitbuffer, size);
}

ApeBitMap::~ApeBitMap()
{
	reserve(0);
}

bool ApeBitMap::setbit(uint32_t bitnum)
{
    uint32_t bytenum = bitnum / 8;
    uint32_t bytebit = 7 - (bitnum % 8);
	uint8_t newbyte = bits_[bytenum] | (1 << bytebit);
    if (newbyte == bits_[bytenum])
		return false;
	bits_[bytenum] = newbyte;
	return true;
}

bool ApeBitMap::unsetbit(uint32_t bitnum)
{
    uint32_t bytenum = bitnum / 8;
    uint32_t bytebit = 7 - (bitnum % 8);
	uint8_t newbyte = bits_[bytenum] & (!(1 << bytebit));
    if (newbyte == bits_[bytenum])
		return false;
	bits_[bytenum] = newbyte;
	return true;
}

bool ApeBitMap::getbit(uint32_t bitnum)
{
    uint32_t bytenum = bitnum / 8;
    uint32_t bytebit = 7 - (bitnum % 8);
    return (bits_[bytenum] & (1 << bytebit)) != 0;
}

uint32_t ApeBitMap::findunsetbit()
{
    for (int i = 0; i < size_; i++)
    {
        if (bits_[i] != 255)
        {
            uint8_t byte = bits_[i];
            uint32_t bit = i * 8;
			uint8_t mask = 128;
            while (byte & mask != 0)
            {
                mask /= 2;
                bit++;
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

void ApeBitMap::setall()
{
	memset(bits_, 0xFF, size_);
}

	
void ApeBitMap::unsetall()
{
	memset(bits_, 0, size_);
}