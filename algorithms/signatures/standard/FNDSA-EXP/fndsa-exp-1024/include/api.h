#ifndef API_H
#define API_H

/* Compatibility layer (EXPERIMENTAL, pre-standard): maps the harness'
 * explicit-randomness Falcon-style crypto_sign* calls onto c-fn-dsa
 * (Thomas Pornin's best-guess FN-DSA / FIPS 206 draft, logn = 10), using the
 * "raw" (no pre-hash) interface with an empty context string.
 *
 *   keypair_rnd (48 B): its first 32 bytes are the key-generation seed;
 *   nonce       (40 B): the 40-byte signing seed (FN-DSA uses 40-byte seeds);
 *   sign_rnd    (48 B): unused (FN-DSA draws all signing randomness from the
 *                       seeded PRNG).
 * The signed-message layout is sm = sig || m (fixed-size signature). */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "fndsa.h"

#define FNDSA_LOGN 10

#define CRYPTO_ALGNAME        "FN-DSA-1024 (c-fn-dsa, experimental)"
#define CRYPTO_SECRETKEYBYTES FNDSA_SIGN_KEY_SIZE(FNDSA_LOGN)
#define CRYPTO_PUBLICKEYBYTES FNDSA_VRFY_KEY_SIZE(FNDSA_LOGN)
#define CRYPTO_BYTES          FNDSA_SIGNATURE_SIZE(FNDSA_LOGN)

static inline int crypto_sign_keypair(unsigned char *pk, unsigned char *sk,
                                      unsigned char *keypair_rnd)
{
    fndsa_keygen_seeded(FNDSA_LOGN, keypair_rnd, 32, sk, pk);
    return 0;
}

static inline int crypto_sign(unsigned char *sm, unsigned long long *smlen,
                              const unsigned char *m, unsigned long long mlen,
                              const unsigned char *sk,
                              const unsigned char *nonce,
                              const unsigned char *sign_rnd)
{
    size_t siglen;

    (void)sign_rnd;
    memmove(sm + CRYPTO_BYTES, m, (size_t)mlen);
    siglen = fndsa_sign_seeded(sk, CRYPTO_SECRETKEYBYTES, "", 0,
                               FNDSA_HASH_ID_RAW, sm + CRYPTO_BYTES,
                               (size_t)mlen, nonce, 40, sm, CRYPTO_BYTES);
    if (siglen != CRYPTO_BYTES) {
        return -1;
    }
    *smlen = (unsigned long long)siglen + mlen;
    return 0;
}

static inline int crypto_sign_open(unsigned char *m, unsigned long long *mlen,
                                   const unsigned char *sm,
                                   unsigned long long smlen,
                                   const unsigned char *pk)
{
    if (smlen < CRYPTO_BYTES) {
        return -1;
    }
    if (!fndsa_verify(sm, CRYPTO_BYTES, pk, CRYPTO_PUBLICKEYBYTES, "", 0,
                      FNDSA_HASH_ID_RAW, sm + CRYPTO_BYTES,
                      (size_t)(smlen - CRYPTO_BYTES))) {
        return -1;
    }
    *mlen = smlen - CRYPTO_BYTES;
    memmove(m, sm + CRYPTO_BYTES, (size_t)*mlen);
    return 0;
}

#endif
