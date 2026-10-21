#ifndef SPX_SHA256_RVCRYPTO_H
#define SPX_SHA256_RVCRYPTO_H

#include <stdint.h>

/* The run script keeps CVA6's base -march, so enable Zknh only around each
 * standard SHA256 mnemonic instead of relying on command-line ISA attributes.
 * Force inlining: a function call per SHA sigma operation costs far more than
 * the instruction itself in the SHA-256 compression loop.
 */
#define SPX_SHA256_ALWAYS_INLINE static inline __attribute__((always_inline))

SPX_SHA256_ALWAYS_INLINE uint32_t spx_sha256sum0(uint32_t x)
{
    uint32_t r;
    __asm__ (".option push\n"
             ".option arch, +zknh\n"
             "sha256sum0 %0, %1\n"
             ".option pop"
             : "=r"(r)
             : "r"(x));
    return (uint32_t)r;
}

SPX_SHA256_ALWAYS_INLINE uint32_t spx_sha256sum1(uint32_t x)
{
    uint32_t r;
    __asm__ (".option push\n"
             ".option arch, +zknh\n"
             "sha256sum1 %0, %1\n"
             ".option pop"
             : "=r"(r)
             : "r"(x));
    return (uint32_t)r;
}

SPX_SHA256_ALWAYS_INLINE uint32_t spx_sha256sig0(uint32_t x)
{
    uint32_t r;
    __asm__ (".option push\n"
             ".option arch, +zknh\n"
             "sha256sig0 %0, %1\n"
             ".option pop"
             : "=r"(r)
             : "r"(x));
    return (uint32_t)r;
}

SPX_SHA256_ALWAYS_INLINE uint32_t spx_sha256sig1(uint32_t x)
{
    uint32_t r;
    __asm__ (".option push\n"
             ".option arch, +zknh\n"
             "sha256sig1 %0, %1\n"
             ".option pop"
             : "=r"(r)
             : "r"(x));
    return (uint32_t)r;
}

#undef SPX_SHA256_ALWAYS_INLINE

#endif
