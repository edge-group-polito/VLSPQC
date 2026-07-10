/*
 * crypto_bridge.c -- wraps hss_variant_*() (see hss_variant.h, hss_variant.c
 * -- copied verbatim from ../../src) into the fixed-size crypto_sign_keypair/
 * crypto_sign/crypto_sign_open convention gen_static_test.py's
 * "ds_derand_seed" shape expects, pinned to LMS_H10_W8 (levels=1,
 * LMS_SHA256_N32_H10/LMOTS_SHA256_N32_W8 -- see api.h for sizes).
 *
 * hss_variant_sign() writes only the detached signature; crypto_sign()
 * appends the message after it to match every other family's sm = sig||msg
 * convention here.
 */
#include <string.h>
#include "api.h"
#include "hss_variant.h"

static hss_variant_t g_v;
static int g_v_ready = 0;

static const hss_variant_t *variant(void)
{
    if (!g_v_ready) {
        param_set_t lm[1]  = { LMS_SHA256_N32_H10 };
        param_set_t ots[1] = { LMOTS_SHA256_N32_W8 };
        if (hss_variant_init(&g_v, 1, lm, ots) != 0) {
            for (;;) { } /* static/bare-metal: no stderr, fail loudly and stop */
        }
        g_v_ready = 1;
    }
    return &g_v;
}

int crypto_sign_keypair(unsigned char *pk, unsigned char *sk, uint8_t *keypair_seed)
{
    return hss_variant_keypair_seed(variant(), pk, sk, keypair_seed);
}

int crypto_sign(unsigned char *sm, unsigned long long *smlen,
                const unsigned char *m, unsigned long long mlen,
                const unsigned char *sk)
{
    size_t siglen;
    int ret = hss_variant_sign(variant(), (uint8_t *)sk, sm, &siglen, m, (size_t)mlen);
    if (ret != 0) return ret;
    memmove(sm + siglen, m, (size_t)mlen);
    *smlen = (unsigned long long)siglen + mlen;
    return 0;
}

int crypto_sign_open(unsigned char *m, unsigned long long *mlen,
                     const unsigned char *sm, unsigned long long smlen,
                     const unsigned char *pk)
{
    size_t siglen = CRYPTO_BYTES;
    if (smlen < siglen) return -1;
    unsigned long long msglen = smlen - siglen;
    if (hss_variant_verify(sm + siglen, (size_t)msglen, sm, siglen, pk) != 0) return -1;
    memmove(m, sm + siglen, (size_t)msglen);
    *mlen = msglen;
    return 0;
}
