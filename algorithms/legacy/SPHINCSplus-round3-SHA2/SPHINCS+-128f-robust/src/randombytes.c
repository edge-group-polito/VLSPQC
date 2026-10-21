/*
This code was taken from the SPHINCS reference implementation and is public domain.
*/

#include "randombytes.h"

void randombytes(unsigned char *x, unsigned long long xlen)
{
    static unsigned long long state = 0x9e3779b97f4a7c15ULL;

    while (xlen--) {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        *x++ = (unsigned char)((state * 0x2545f4914f6cdd1dULL) >> 56);
    }
}
