/*
 * api.h -- fixed sizes for the LMS_H10_W8 static test only. The dynamic flow
 * (../../src, ../../include) supports every (height, w) pair at runtime;
 * this folder pins exactly one so it can expose the same compile-time-fixed
 * crypto_sign_keypair/crypto_sign/crypto_sign_open convention
 * gen_static_test.py's "ds_derand_seed" shape expects. Sizes confirmed via
 * hss_variant_pk/sk/sig_bytes() for this parameter set.
 */
#ifndef LMS_STATIC_API_H
#define LMS_STATIC_API_H

#include <stddef.h>
#include <stdint.h>

#define CRYPTO_ALGNAME "LMS_H10_W8"

#define CRYPTO_PUBLICKEYBYTES 60
#define CRYPTO_SECRETKEYBYTES 64
#define CRYPTO_BYTES           1456

int crypto_sign_keypair(unsigned char *pk, unsigned char *sk, uint8_t *keypair_seed);

int crypto_sign(unsigned char *sm, unsigned long long *smlen,
                const unsigned char *m, unsigned long long mlen,
                const unsigned char *sk);

int crypto_sign_open(unsigned char *m, unsigned long long *mlen,
                     const unsigned char *sm, unsigned long long smlen,
                     const unsigned char *pk);

#endif
