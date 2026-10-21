/*
 * Haraka backend using RISC-V RV64 scalar AES encryption middle-round
 * instructions. The sponge and SPHINCS+ glue match the upstream Haraka
 * reference implementation.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "haraka.h"

#define HARAKAS_RATE 32

typedef struct {
    uint64_t lo;
    uint64_t hi;
} u128_rv;

#define U128_RC(e3, e2, e1, e0) \
    { (((uint64_t)(e1) << 32) | (uint32_t)(e0)), \
      (((uint64_t)(e3) << 32) | (uint32_t)(e2)) }

static const u128_rv haraka_rc[40] = {
    U128_RC(0x0684704c, 0xe620c00a, 0xb2c5fef0, 0x75817b9d),
    U128_RC(0x8b66b4e1, 0x88f3a06b, 0x640f6ba4, 0x2f08f717),
    U128_RC(0x3402de2d, 0x53f28498, 0xcf029d60, 0x9f029114),
    U128_RC(0x0ed6eae6, 0x2e7b4f08, 0xbbf3bcaf, 0xfd5b4f79),
    U128_RC(0xcbcfb0cb, 0x4872448b, 0x79eecd1c, 0xbe397044),
    U128_RC(0x7eeacdee, 0x6e9032b7, 0x8d5335ed, 0x2b8a057b),
    U128_RC(0x67c28f43, 0x5e2e7cd0, 0xe2412761, 0xda4fef1b),
    U128_RC(0x2924d9b0, 0xafcacc07, 0x675ffde2, 0x1fc70b3b),
    U128_RC(0xab4d63f1, 0xe6867fe9, 0xecdb8fca, 0xb9d465ee),
    U128_RC(0x1c30bf84, 0xd4b7cd64, 0x5b2a404f, 0xad037e33),
    U128_RC(0xb2cc0bb9, 0x941723bf, 0x69028b2e, 0x8df69800),
    U128_RC(0xfa0478a6, 0xde6f5572, 0x4aaa9ec8, 0x5c9d2d8a),
    U128_RC(0xdfb49f2b, 0x6b772a12, 0x0efa4f2e, 0x29129fd4),
    U128_RC(0x1ea10344, 0xf449a236, 0x32d611ae, 0xbb6a12ee),
    U128_RC(0xaf044988, 0x4b050084, 0x5f9600c9, 0x9ca8eca6),
    U128_RC(0x21025ed8, 0x9d199c4f, 0x78a2c7e3, 0x27e593ec),
    U128_RC(0xbf3aaaf8, 0xa759c9b7, 0xb9282ecd, 0x82d40173),
    U128_RC(0x6260700d, 0x6186b017, 0x37f2efd9, 0x10307d6b),
    U128_RC(0x5aca45c2, 0x21300443, 0x81c29153, 0xf6fc9ac6),
    U128_RC(0x9223973c, 0x226b68bb, 0x2caf92e8, 0x36d1943a),
    U128_RC(0xd3bf9238, 0x225886eb, 0x6cbab958, 0xe51071b4),
    U128_RC(0xdb863ce5, 0xaef0c677, 0x933dfddd, 0x24e1128d),
    U128_RC(0xbb606268, 0xffeba09c, 0x83e48de3, 0xcb2212b1),
    U128_RC(0x734bd3dc, 0xe2e4d19c, 0x2db91a4e, 0xc72bf77d),
    U128_RC(0x43bb47c3, 0x61301b43, 0x4b1415c4, 0x2cb3924e),
    U128_RC(0xdba775a8, 0xe707eff6, 0x03b231dd, 0x16eb6899),
    U128_RC(0x6df3614b, 0x3c755977, 0x8e5e2302, 0x7eca472c),
    U128_RC(0xcda75a17, 0xd6de7d77, 0x6d1be5b9, 0xb88617f9),
    U128_RC(0xec6b43f0, 0x6ba8e9aa, 0x9d6c069d, 0xa946ee5d),
    U128_RC(0xcb1e6950, 0xf957332b, 0xa2531159, 0x3bf327c1),
    U128_RC(0x2cee0c75, 0x00da619c, 0xe4ed0353, 0x600ed0d9),
    U128_RC(0xf0b1a5a1, 0x96e90cab, 0x80bbbabc, 0x63a4a350),
    U128_RC(0xae3db102, 0x5e962988, 0xab0dde30, 0x938dca39),
    U128_RC(0x17bb8f38, 0xd554a40b, 0x8814f3a8, 0x2e75b442),
    U128_RC(0x34bb8a5b, 0x5f427fd7, 0xaeb6b779, 0x360a16f6),
    U128_RC(0x26f65241, 0xcbe55438, 0x43ce5918, 0xffbaafde),
    U128_RC(0x4ce99a54, 0xb9f3026a, 0xa2ca9cf7, 0x839ec978),
    U128_RC(0xae51a51a, 0x1bdff7be, 0x40c06e28, 0x22901235),
    U128_RC(0xa0c1613c, 0xba7ed22b, 0xc173bc0f, 0x48a659cf),
    U128_RC(0x756acc03, 0x02288288, 0x4ad6bdfd, 0xe9c59da1)
};

static uint64_t load64_le(const unsigned char *src)
{
    uint64_t x = 0;

    for (size_t i = 0; i < 8; i++) {
        x |= (uint64_t)src[i] << (8U * i);
    }
    return x;
}

static void store64_le(unsigned char *dst, uint64_t x)
{
    for (size_t i = 0; i < 8; i++) {
        dst[i] = (unsigned char)(x >> (8U * i));
    }
}

static u128_rv load128(const unsigned char *src)
{
    u128_rv x;

    x.lo = load64_le(src);
    x.hi = load64_le(src + 8);
    return x;
}

static void store128(unsigned char *dst, u128_rv x)
{
    store64_le(dst, x.lo);
    store64_le(dst + 8, x.hi);
}

static u128_rv xor128(u128_rv a, u128_rv b)
{
    u128_rv x;

    x.lo = a.lo ^ b.lo;
    x.hi = a.hi ^ b.hi;
    return x;
}

static uint32_t lane32(u128_rv x, unsigned int lane)
{
    if (lane == 0) {
        return (uint32_t)x.lo;
    }
    if (lane == 1) {
        return (uint32_t)(x.lo >> 32);
    }
    if (lane == 2) {
        return (uint32_t)x.hi;
    }
    return (uint32_t)(x.hi >> 32);
}

static u128_rv make128(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    u128_rv x;

    x.lo = ((uint64_t)w1 << 32) | w0;
    x.hi = ((uint64_t)w3 << 32) | w2;
    return x;
}

static u128_rv unpacklo32(u128_rv a, u128_rv b)
{
    return make128(lane32(a, 0), lane32(b, 0), lane32(a, 1), lane32(b, 1));
}

static u128_rv unpackhi32(u128_rv a, u128_rv b)
{
    return make128(lane32(a, 2), lane32(b, 2), lane32(a, 3), lane32(b, 3));
}

static void mix2(u128_rv *s0, u128_rv *s1)
{
    u128_rv tmp = unpacklo32(*s0, *s1);

    *s1 = unpackhi32(*s0, *s1);
    *s0 = tmp;
}

static void mix4(u128_rv *s0, u128_rv *s1, u128_rv *s2, u128_rv *s3)
{
    u128_rv tmp = unpacklo32(*s0, *s1);

    *s0 = unpackhi32(*s0, *s1);
    *s1 = unpacklo32(*s2, *s3);
    *s2 = unpackhi32(*s2, *s3);
    *s3 = unpacklo32(*s0, *s2);
    *s0 = unpackhi32(*s0, *s2);
    *s2 = unpackhi32(*s1, tmp);
    *s1 = unpacklo32(*s1, tmp);
}

static uint64_t rv_aes64esm(uint64_t rs1, uint64_t rs2)
{
    uint64_t rd;

    __asm__ volatile ("aes64esm %0, %1, %2" : "=r"(rd) : "r"(rs1), "r"(rs2));
    return rd;
}

static u128_rv ctx_rc(const spx_ctx *ctx, unsigned int i)
{
    u128_rv x;

    x.lo = ctx->rc[i][0];
    x.hi = ctx->rc[i][1];
    return x;
}

static void set_ctx_rc(spx_ctx *ctx, unsigned int i, u128_rv x)
{
    ctx->rc[i][0] = x.lo;
    ctx->rc[i][1] = x.hi;
}

static u128_rv aesenc(u128_rv s, u128_rv rk)
{
    u128_rv out;

    out.lo = rv_aes64esm(s.lo, s.hi) ^ rk.lo;
    out.hi = rv_aes64esm(s.hi, s.lo) ^ rk.hi;
    return out;
}

static void aes2(u128_rv *s0, u128_rv *s1, const spx_ctx *ctx, unsigned int rci)
{
    *s0 = aesenc(*s0, ctx_rc(ctx, rci));
    *s1 = aesenc(*s1, ctx_rc(ctx, rci + 1));
    *s0 = aesenc(*s0, ctx_rc(ctx, rci + 2));
    *s1 = aesenc(*s1, ctx_rc(ctx, rci + 3));
}

static void aes4(u128_rv *s0, u128_rv *s1, u128_rv *s2, u128_rv *s3,
                 const spx_ctx *ctx, unsigned int rci)
{
    *s0 = aesenc(*s0, ctx_rc(ctx, rci));
    *s1 = aesenc(*s1, ctx_rc(ctx, rci + 1));
    *s2 = aesenc(*s2, ctx_rc(ctx, rci + 2));
    *s3 = aesenc(*s3, ctx_rc(ctx, rci + 3));
    *s0 = aesenc(*s0, ctx_rc(ctx, rci + 4));
    *s1 = aesenc(*s1, ctx_rc(ctx, rci + 5));
    *s2 = aesenc(*s2, ctx_rc(ctx, rci + 6));
    *s3 = aesenc(*s3, ctx_rc(ctx, rci + 7));
}

void tweak_constants(spx_ctx *ctx)
{
    unsigned char buf[40 * 16];

    for (unsigned int i = 0; i < 40; i++) {
        set_ctx_rc(ctx, i, haraka_rc[i]);
    }

    haraka_S(buf, sizeof(buf), ctx->pub_seed, SPX_N, ctx);

    for (unsigned int i = 0; i < 40; i++) {
        set_ctx_rc(ctx, i, load128(buf + 16 * i));
    }
}

static void haraka_S_absorb(unsigned char *s, unsigned int r,
                            const unsigned char *m, unsigned long long mlen,
                            unsigned char p, const spx_ctx *ctx)
{
    unsigned long long i;
    unsigned char t[HARAKAS_RATE];

    while (mlen >= r) {
        for (i = 0; i < r; i++) {
            s[i] ^= m[i];
        }
        haraka512_perm(s, s, ctx);
        mlen -= r;
        m += r;
    }

    memset(t, 0, r);
    for (i = 0; i < mlen; i++) {
        t[i] = m[i];
    }
    t[i] = p;
    t[r - 1] |= 128;
    for (i = 0; i < r; i++) {
        s[i] ^= t[i];
    }
}

static void haraka_S_squeezeblocks(unsigned char *h, unsigned long long nblocks,
                                   unsigned char *s, unsigned int r,
                                   const spx_ctx *ctx)
{
    while (nblocks > 0) {
        haraka512_perm(s, s, ctx);
        memcpy(h, s, HARAKAS_RATE);
        h += r;
        nblocks--;
    }
}

void haraka_S_inc_init(uint8_t *s_inc)
{
    memset(s_inc, 0, 65);
}

void haraka_S_inc_absorb(uint8_t *s_inc, const uint8_t *m, size_t mlen,
                         const spx_ctx *ctx)
{
    size_t i;

    while (mlen + s_inc[64] >= HARAKAS_RATE) {
        for (i = 0; i < (size_t)(HARAKAS_RATE - s_inc[64]); i++) {
            s_inc[s_inc[64] + i] ^= m[i];
        }
        mlen -= (size_t)(HARAKAS_RATE - s_inc[64]);
        m += HARAKAS_RATE - (uint8_t)s_inc[64];
        s_inc[64] = 0;

        haraka512_perm(s_inc, s_inc, ctx);
    }

    for (i = 0; i < mlen; i++) {
        s_inc[s_inc[64] + i] ^= m[i];
    }
    s_inc[64] += (uint8_t)mlen;
}

void haraka_S_inc_finalize(uint8_t *s_inc)
{
    s_inc[s_inc[64]] ^= 0x1F;
    s_inc[HARAKAS_RATE - 1] ^= 128;
    s_inc[64] = 0;
}

void haraka_S_inc_squeeze(uint8_t *out, size_t outlen, uint8_t *s_inc,
                          const spx_ctx *ctx)
{
    size_t i;

    for (i = 0; i < outlen && i < s_inc[64]; i++) {
        out[i] = s_inc[HARAKAS_RATE - s_inc[64] + i];
    }
    out += i;
    outlen -= i;
    s_inc[64] -= (uint8_t)i;

    while (outlen > 0) {
        haraka512_perm(s_inc, s_inc, ctx);

        for (i = 0; i < outlen && i < HARAKAS_RATE; i++) {
            out[i] = s_inc[i];
        }
        out += i;
        outlen -= i;
        s_inc[64] = (uint8_t)(HARAKAS_RATE - i);
    }
}

void haraka_S(unsigned char *out, unsigned long long outlen,
              const unsigned char *in, unsigned long long inlen,
              const spx_ctx *ctx)
{
    unsigned char s[64];
    unsigned char d[32];
    unsigned long long full_blocks;

    memset(s, 0, sizeof(s));
    haraka_S_absorb(s, HARAKAS_RATE, in, inlen, 0x1F, ctx);

    full_blocks = outlen / HARAKAS_RATE;
    haraka_S_squeezeblocks(out, full_blocks, s, HARAKAS_RATE, ctx);
    out += full_blocks * HARAKAS_RATE;

    if (outlen % HARAKAS_RATE) {
        haraka_S_squeezeblocks(d, 1, s, HARAKAS_RATE, ctx);
        memcpy(out, d, outlen % HARAKAS_RATE);
    }
}

void haraka512_perm(unsigned char *out, const unsigned char *in,
                    const spx_ctx *ctx)
{
    u128_rv s0 = load128(in);
    u128_rv s1 = load128(in + 16);
    u128_rv s2 = load128(in + 32);
    u128_rv s3 = load128(in + 48);

    for (unsigned int r = 0; r < 5; r++) {
        aes4(&s0, &s1, &s2, &s3, ctx, 8 * r);
        mix4(&s0, &s1, &s2, &s3);
    }

    store128(out, s0);
    store128(out + 16, s1);
    store128(out + 32, s2);
    store128(out + 48, s3);
}

void haraka512(unsigned char *out, const unsigned char *in, const spx_ctx *ctx)
{
    unsigned char buf[64];

    haraka512_perm(buf, in, ctx);
    for (unsigned int i = 0; i < sizeof(buf); i++) {
        buf[i] ^= in[i];
    }

    memcpy(out, buf + 8, 8);
    memcpy(out + 8, buf + 24, 8);
    memcpy(out + 16, buf + 32, 8);
    memcpy(out + 24, buf + 48, 8);
}

void haraka256(unsigned char *out, const unsigned char *in, const spx_ctx *ctx)
{
    u128_rv s0 = load128(in);
    u128_rv s1 = load128(in + 16);

    for (unsigned int r = 0; r < 5; r++) {
        aes2(&s0, &s1, ctx, 4 * r);
        mix2(&s0, &s1);
    }

    s0 = xor128(s0, load128(in));
    s1 = xor128(s1, load128(in + 16));

    store128(out, s0);
    store128(out + 16, s1);
}
