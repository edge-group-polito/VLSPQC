#ifndef API_H
#define API_H

/* Compatibility layer: maps the harness' explicit-randomness crypto_sign*
 * calls onto slhdsa-c (FIPS 205, SLH-DSA-SHAKE-256f, pure SLH-DSA with an empty context
 * string). The signed-message layout is sm = sig || m.
 *
 * slh_keygen_internal() takes SK.seed, SK.prf and PK.seed, i.e. the 3*n bytes
 * of keypair_rnd; slh_sign() takes the n bytes of optional randomness
 * (addrnd) directly. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "slh_dsa.h"

#define CRYPTO_ALGNAME        "SLH-DSA-SHAKE-256f"
#define CRYPTO_PUBLICKEYBYTES SPX_PK_BYTES
#define CRYPTO_SECRETKEYBYTES SPX_SK_BYTES
#define CRYPTO_BYTES          SPX_BYTES

#define SLH_PARAM slh_dsa_shake_256f

static inline int crypto_sign_keypair(unsigned char *pk, unsigned char *sk,
                                      uint8_t *keypair_rnd)
{
    return slh_keygen_internal(sk, pk, keypair_rnd, keypair_rnd + SPX_N,
                               keypair_rnd + 2 * SPX_N, &SLH_PARAM) == 0 ? 0 : -1;
}

static inline int crypto_sign(unsigned char *sm, unsigned long long *smlen,
                              const unsigned char *m, unsigned long long mlen,
                              const unsigned char *sk, uint8_t *signature_rnd)
{
    size_t siglen;

    memmove(sm + CRYPTO_BYTES, m, (size_t)mlen);
    siglen = slh_sign(sm, sm + CRYPTO_BYTES, (size_t)mlen, NULL, 0, sk,
                      signature_rnd, &SLH_PARAM);
    if (siglen != CRYPTO_BYTES) {
        return -1;
    }
    *smlen = (unsigned long long)siglen + mlen;
    return 0;
}

static inline int crypto_sign_open(unsigned char *m, unsigned long long *mlen,
                                   const unsigned char *sm, unsigned long long smlen,
                                   const unsigned char *pk)
{
    if (smlen < CRYPTO_BYTES) {
        return -1;
    }
    if (!slh_verify(sm + CRYPTO_BYTES, (size_t)(smlen - CRYPTO_BYTES), sm,
                    CRYPTO_BYTES, NULL, 0, pk, &SLH_PARAM)) {
        return -1;
    }
    *mlen = smlen - CRYPTO_BYTES;
    memmove(m, sm + CRYPTO_BYTES, (size_t)*mlen);
    return 0;
}

#endif
