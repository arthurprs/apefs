#ifndef APEBITMAP_H
#define APEBITMAP_H

#include <string.h>
#include <sys/types.h>
#include <stdint.h>

const uint32_t NOBIT = (-1);

class ApeBitMap
{
    public:
        ApeBitMap();
        ~ApeBitMap();
		void setall();
		void unsetall();
		void reserve(uint32_t size);
        void tobuffer(void* bitbuffer);
        void frombuffer(void* bitbuffer, uint32_t bytesize);
        bool setbit(uint32_t bitnum);
        bool unsetbit(uint32_t bitnum);
        bool getbit(uint32_t bitnum);
        uint32_t findunsetbit();
        uint32_t size() const; // in bytes!
    private:
        uint8_t* bits_;
        uint32_t size_;
};

#endif // APEBITMAP_H
