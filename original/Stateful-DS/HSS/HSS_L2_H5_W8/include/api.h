/*
 * api.h -- fixed sizes for the HSS_L2_H5_W8 static test only. The dynamic
 * flow (../../src, ../../include) supports every (height, w, levels)
 * combination at runtime; this folder pins exactly one (2 levels, each
 * LMS_SHA256_N32_H5/LMOTS_SHA256_N32_W8) so it can expose the same
 * compile-time-fixed crypto_sign_keypair/crypto_sign/crypto_sign_open
 * convention gen_static_test.py's "ds_derand_seed" shape expects. Sizes
 * confirmed via hss_variant_pk/sk/sig_bytes() for this parameter set.
 */
#ifndef HSS_STATIC_API_H
#define HSS_STATIC_API_H

#include <stddef.h>
#include <stdint.h>

#define CRYPTO_ALGNAME "HSS_L2_H5_W8"

#define CRYPTO_PUBLICKEYBYTES 60
#define CRYPTO_SECRETKEYBYTES 64
#define CRYPTO_BYTES           2644

int crypto_sign_keypair(unsigned char *pk, unsigned char *sk, uint8_t *keypair_seed);

int crypto_sign(unsigned char *sm, unsigned long long *smlen,
                const unsigned char *m, unsigned long long mlen,
                const unsigned char *sk);

int crypto_sign_open(unsigned char *m, unsigned long long *mlen,
                     const unsigned char *sm, unsigned long long smlen,
                     const unsigned char *pk);

#endif
