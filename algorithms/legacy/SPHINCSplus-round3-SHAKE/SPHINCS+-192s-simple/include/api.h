#ifndef SLH_S_API_H
#define SLH_S_API_H

#include <stddef.h>
#include <stdint.h>

#define CRYPTO_ALGNAME "SLH-DSA-SHAKE-192s"

#define CRYPTO_PUBLICKEYBYTES 48
#define CRYPTO_SECRETKEYBYTES 96
#define CRYPTO_BYTES          16224

int crypto_sign_keypair(unsigned char *pk, unsigned char *sk, uint8_t *keypair_rnd);

int crypto_sign(unsigned char *sm, unsigned long long *smlen,
                const unsigned char *m, unsigned long long mlen,
                const unsigned char *sk, uint8_t *signature_rnd);

int crypto_sign_open(unsigned char *m, unsigned long long *mlen,
                     const unsigned char *sm, unsigned long long smlen,
                     const unsigned char *pk);

#endif
