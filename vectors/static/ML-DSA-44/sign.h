#ifndef SIGN_H
#define SIGN_H

/* Compatibility layer: maps the harness' explicit-seed / explicit-randomness
 * crypto_sign* calls onto mldsa-native (FIPS 204, ML-DSA-44, pure ML-DSA with
 * a caller-supplied context string). The signature layout is sm = sig || m. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "mldsa_native.h"
#include "api.h"
#include "params.h"

static inline int crypto_sign_keypair(uint8_t *pk, uint8_t *sk,
                                      const uint8_t *seed)
{
    return PQCP_MLDSA_NATIVE_MLDSA44_keypair_internal(pk, sk, seed);
}

static inline int crypto_sign(uint8_t *sm, size_t *smlen,
                              const uint8_t *m, size_t mlen,
                              const uint8_t *ctx, size_t ctxlen,
                              const uint8_t *sk, const uint8_t *rnd)
{
    uint8_t pre[MLD_DOMAIN_SEPARATION_MAX_BYTES];
    size_t prelen;
    int ret;

    prelen = PQCP_MLDSA_NATIVE_MLDSA44_prepare_domain_separation_prefix(pre, NULL, 0, ctx, ctxlen,
                                                   MLD_PREHASH_NONE);
    if (prelen == 0) {
        return -1;
    }
    memmove(sm + CRYPTO_BYTES, m, mlen);
    ret = PQCP_MLDSA_NATIVE_MLDSA44_signature_internal(sm, sm + CRYPTO_BYTES, mlen, pre, prelen,
                                  rnd, sk, 0);
    if (ret != 0) {
        return ret;
    }
    *smlen = CRYPTO_BYTES + mlen;
    return 0;
}

static inline int crypto_sign_open(uint8_t *m, size_t *mlen,
                                   const uint8_t *sm, size_t smlen,
                                   const uint8_t *ctx, size_t ctxlen,
                                   const uint8_t *pk)
{
    if (smlen < CRYPTO_BYTES) {
        return -1;
    }
    if (PQCP_MLDSA_NATIVE_MLDSA44_verify(sm, sm + CRYPTO_BYTES, smlen - CRYPTO_BYTES, ctx, ctxlen,
                    pk) != 0) {
        return -1;
    }
    *mlen = smlen - CRYPTO_BYTES;
    memmove(m, sm + CRYPTO_BYTES, *mlen);
    return 0;
}

#endif
