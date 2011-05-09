#ifndef APEBITMAP_H
#define APEBITMAP_H

#include <string.h>
#include <sys/types.h>
#include <stdint.h>

const uint32_t NOBIT = -1;

class ApeBitMap
{
    public:
        ApeBitMap(uint32_t size);
        ~ApeBitMap();
        void tobuffer(void* bitbuffer);
        void frombuffer(void* bitbuffer);
        void setbit(uint32_t bitnum);
        void unsetbit(uint32_t bitnum);
        bool getbit(uint32_t bitnum);
        uint32_t findunsetbit();
        uint32_t size() const;
    private:
        uint8_t* bits_;
        uint32_t size_;
};

#endif // APEBITMAP_H
