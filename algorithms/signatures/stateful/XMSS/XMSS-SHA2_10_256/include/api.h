/*
 * api.h -- fixed sizes for the XMSS-SHA2_10_256 static test only. The
 * dynamic flow (../../src, ../../include) supports all 77 XMSS/XMSS^MT
 * parameter sets at runtime; this folder pins exactly one so it can expose
 * the same compile-time-fixed crypto_sign_keypair/crypto_sign/crypto_sign_open
 * convention gen_static_test.py's "ds_derand_seed" shape expects. Sizes
 * confirmed via xmss_variant_pk/sk/sig_bytes() for this parameter set.
 */
#ifndef XMSS_STATIC_API_H
#define XMSS_STATIC_API_H

#include <stddef.h>
#include <stdint.h>

#define CRYPTO_ALGNAME "XMSS-SHA2_10_256"

#define CRYPTO_PUBLICKEYBYTES 68
#define CRYPTO_SECRETKEYBYTES 136
#define CRYPTO_BYTES           2500

int crypto_sign_keypair(unsigned char *pk, unsigned char *sk, uint8_t *keypair_seed);

int crypto_sign(unsigned char *sm, unsigned long long *smlen,
                const unsigned char *m, unsigned long long mlen,
                const unsigned char *sk);

int crypto_sign_open(unsigned char *m, unsigned long long *mlen,
                     const unsigned char *sm, unsigned long long smlen,
                     const unsigned char *pk);

#endif
