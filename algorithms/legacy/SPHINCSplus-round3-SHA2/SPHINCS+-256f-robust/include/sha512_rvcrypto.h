#ifndef SPX_SHA512_RVCRYPTO_H
#define SPX_SHA512_RVCRYPTO_H

#include <stdint.h>

/* Keep the project-wide -march unchanged and enable Zknh only around the
 * SHA-512 mnemonics. These are RV64 instructions.
 */
#define SPX_SHA512_ALWAYS_INLINE static inline __attribute__((always_inline))

SPX_SHA512_ALWAYS_INLINE uint64_t spx_sha512sum0(uint64_t x)
{
    uint64_t r;
    __asm__ (".option push\n"
             ".option arch, +zknh\n"
             "sha512sum0 %0, %1\n"
             ".option pop"
             : "=r"(r)
             : "r"(x));
    return r;
}

SPX_SHA512_ALWAYS_INLINE uint64_t spx_sha512sum1(uint64_t x)
{
    uint64_t r;
    __asm__ (".option push\n"
             ".option arch, +zknh\n"
             "sha512sum1 %0, %1\n"
             ".option pop"
             : "=r"(r)
             : "r"(x));
    return r;
}

SPX_SHA512_ALWAYS_INLINE uint64_t spx_sha512sig0(uint64_t x)
{
    uint64_t r;
    __asm__ (".option push\n"
             ".option arch, +zknh\n"
             "sha512sig0 %0, %1\n"
             ".option pop"
             : "=r"(r)
             : "r"(x));
    return r;
}

SPX_SHA512_ALWAYS_INLINE uint64_t spx_sha512sig1(uint64_t x)
{
    uint64_t r;
    __asm__ (".option push\n"
             ".option arch, +zknh\n"
             "sha512sig1 %0, %1\n"
             ".option pop"
             : "=r"(r)
             : "r"(x));
    return r;
}

#undef SPX_SHA512_ALWAYS_INLINE

#endif
