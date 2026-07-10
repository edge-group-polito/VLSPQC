/*
 * PQCgenKAT_sign_falcon.c
 *
 * KAT generator for FALCON.
 * FALCON uses explicit randomness parameters:
 *   crypto_sign_keypair(pk, sk, keypair_rnd)   -- keypair_rnd: 48 bytes
 *   crypto_sign(sm, &smlen, m, mlen, sk, nonce, sign_rnd)
 *                                              -- nonce: 40 B, sign_rnd: 48 B
 *   crypto_sign_open(m, &mlen, sm, smlen, pk)
 *
 * All randomness is derived from the NIST AES-256-CTR DRBG seeded per-count.
 *
 * RSP fields written:
 *   count, seed, keypair_rnd, nonce, sign_rnd, mlen, msg, pk, sk, smlen, sm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "rng.h"
#include "api.h"

#ifndef MLEN
#define MLEN 33
#endif

#ifndef KATNUM
#define KATNUM 10
#endif

#ifndef RSP_PATH
#define RSP_PATH "PQCsignKAT.rsp"
#endif

#ifndef ALGO_NAME
#define ALGO_NAME CRYPTO_ALGNAME
#endif

#define FALCON_KEYPAIR_RND_BYTES 48
#define FALCON_NONCE_BYTES       40
#define FALCON_SIGN_RND_BYTES    48

int main(void)
{
    uint8_t entropy_input[48];
    uint8_t seed[48];
    uint8_t keypair_rnd[FALCON_KEYPAIR_RND_BYTES];
    uint8_t nonce[FALCON_NONCE_BYTES];
    uint8_t sign_rnd[FALCON_SIGN_RND_BYTES];
    uint8_t msg[MLEN];
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t *sm   = NULL;
    uint8_t *m_out = NULL;
    unsigned long long smlen, mlen_out;
    int ret;

    FILE *fp = fopen(RSP_PATH, "w");
    if (!fp) { perror("fopen"); return 1; }

    fprintf(fp, "# %s\n\n", ALGO_NAME);

    for (int i = 0; i < 48; i++) entropy_input[i] = (uint8_t)i;

    for (int count = 0; count < KATNUM; count++) {

        randombytes_init(entropy_input, NULL, 256);

        /* First DRBG output = the seed stored in .rsp */
        randombytes(seed, 48);
        /* Re-seed with that seed so tests can replay */
        randombytes_init(seed, NULL, 256);

        /* Derive explicit randomness from DRBG */
        randombytes(keypair_rnd, FALCON_KEYPAIR_RND_BYTES);
        randombytes(msg,         MLEN);
        randombytes(nonce,       FALCON_NONCE_BYTES);
        randombytes(sign_rnd,    FALCON_SIGN_RND_BYTES);

        ret = crypto_sign_keypair(pk, sk, keypair_rnd);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign_keypair failed\n", count);
            fclose(fp); return 1;
        }

        size_t sm_max = MLEN + CRYPTO_BYTES;
        sm = (uint8_t *)malloc(sm_max);
        if (!sm) { perror("malloc"); fclose(fp); return 1; }

        ret = crypto_sign(sm, &smlen, msg, MLEN, sk, nonce, sign_rnd);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign failed\n", count);
            free(sm); fclose(fp); return 1;
        }

        m_out = (uint8_t *)malloc(smlen);
        if (!m_out) { perror("malloc"); free(sm); fclose(fp); return 1; }
        ret = crypto_sign_open(m_out, &mlen_out, sm, smlen, pk);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign_open failed\n", count);
            free(sm); free(m_out); fclose(fp); return 1;
        }

        fprintf(fp, "count = %d\n", count);
        fprintf(fp, "seed = ");
        for (int i = 0; i < 48; i++) fprintf(fp, "%02X", seed[i]);
        fprintf(fp, "\n");
        fprintf(fp, "keypair_rnd = ");
        for (int i = 0; i < FALCON_KEYPAIR_RND_BYTES; i++) fprintf(fp, "%02X", keypair_rnd[i]);
        fprintf(fp, "\n");
        fprintf(fp, "nonce = ");
        for (int i = 0; i < FALCON_NONCE_BYTES; i++) fprintf(fp, "%02X", nonce[i]);
        fprintf(fp, "\n");
        fprintf(fp, "sign_rnd = ");
        for (int i = 0; i < FALCON_SIGN_RND_BYTES; i++) fprintf(fp, "%02X", sign_rnd[i]);
        fprintf(fp, "\n");
        fprintf(fp, "mlen = %d\n", MLEN);
        fprintf(fp, "msg = ");
        for (int i = 0; i < MLEN; i++) fprintf(fp, "%02X", msg[i]);
        fprintf(fp, "\n");
        fprintf(fp, "pk = ");
        for (int i = 0; i < CRYPTO_PUBLICKEYBYTES; i++) fprintf(fp, "%02X", pk[i]);
        fprintf(fp, "\n");
        fprintf(fp, "sk = ");
        for (int i = 0; i < CRYPTO_SECRETKEYBYTES; i++) fprintf(fp, "%02X", sk[i]);
        fprintf(fp, "\n");
        fprintf(fp, "smlen = %llu\n", smlen);
        fprintf(fp, "sm = ");
        for (unsigned long long i = 0; i < smlen; i++) fprintf(fp, "%02X", sm[i]);
        fprintf(fp, "\n\n");

        free(sm); sm = NULL;
        free(m_out); m_out = NULL;

        for (int i = 47; i >= 0; i--) { if (++entropy_input[i]) break; }
    }

    fclose(fp);
    printf("Generated %d KAT vectors → %s\n", KATNUM, RSP_PATH);
    return 0;
}
