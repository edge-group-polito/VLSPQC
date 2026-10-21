/*
 * crypto_bridge.c -- wraps xmss_variant_*() (see xmss_variant.h, sign.c --
 * copied verbatim from ../../src) into the fixed-size crypto_sign_keypair/
 * crypto_sign/crypto_sign_open convention gen_static_test.py's
 * "ds_derand_seed" shape expects, pinned to XMSS-SHA2_10_256 (see api.h).
 */
#include <string.h>
#include "api.h"
#include "xmss_variant.h"

static xmss_variant_t g_v;
static int g_v_ready = 0;

static const xmss_variant_t *variant(void)
{
    if (!g_v_ready) {
        if (xmss_variant_init(&g_v, CRYPTO_ALGNAME) != 0) {
            /* Static/bare-metal target: no stderr to report to -- fail loudly
             * and stop, since every subsequent call would be meaningless. */
            for (;;) { }
        }
        g_v_ready = 1;
    }
    return &g_v;
}

int crypto_sign_keypair(unsigned char *pk, unsigned char *sk, uint8_t *keypair_seed)
{
    return xmss_variant_keypair_seed(variant(), pk, sk, keypair_seed);
}

int crypto_sign(unsigned char *sm, unsigned long long *smlen,
                const unsigned char *m, unsigned long long mlen,
                const unsigned char *sk)
{
    return xmss_variant_sign(variant(), (uint8_t *)sk, sm, smlen, m, mlen);
}

int crypto_sign_open(unsigned char *m, unsigned long long *mlen,
                     const unsigned char *sm, unsigned long long smlen,
                     const unsigned char *pk)
{
    return xmss_variant_open(variant(), m, mlen, sm, smlen, pk);
}
