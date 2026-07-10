/*
 * PQCgenKAT_sign_mldsa.c
 *
 * KAT generator for ML-DSA (Dilithium).
 * ML-DSA uses explicit seed parameters:
 *   crypto_sign_keypair(pk, sk, keypair_seed)  -- keypair_seed: SEEDBYTES
 *   crypto_sign(sm, &smlen, m, mlen, NULL, 0, sk, sign_rnd)
 *                                              -- sign_rnd: RNDBYTES
 *   crypto_sign_open(m, &mlen, sm, smlen, NULL, 0, pk)
 *
 * RSP fields written:
 *   count, seed, keypair_seed, sign_rnd, mlen, msg, pk, sk, smlen, sm
 *
 * Compile with ML-DSA sources and rng.c.
 * Needs: -I <ml-dsa-variant>/include/
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "rng.h"
#include "api.h"
#include "params.h"   /* defines SEEDBYTES, RNDBYTES */
#include "sign.h"

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

int main(void)
{
    uint8_t entropy_input[48];
    uint8_t seed[48];
    uint8_t keypair_seed[SEEDBYTES];
    uint8_t sign_rnd[RNDBYTES];
    uint8_t msg[MLEN];
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t *sm    = NULL;
    uint8_t *m_out = NULL;
    size_t smlen, mlen_out;
    int ret;

    FILE *fp = fopen(RSP_PATH, "w");
    if (!fp) { perror("fopen"); return 1; }

    fprintf(fp, "# %s\n\n", ALGO_NAME);

    for (int i = 0; i < 48; i++) entropy_input[i] = (uint8_t)i;

    for (int count = 0; count < KATNUM; count++) {

        randombytes_init(entropy_input, NULL, 256);
        randombytes(seed, 48);
        randombytes_init(seed, NULL, 256);

        randombytes(keypair_seed, SEEDBYTES);
        randombytes(msg,          MLEN);
        randombytes(sign_rnd,     RNDBYTES);

        ret = crypto_sign_keypair(pk, sk, keypair_seed);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign_keypair failed\n", count);
            fclose(fp); return 1;
        }

        size_t sm_max = MLEN + CRYPTO_BYTES;
        sm = (uint8_t *)malloc(sm_max);
        if (!sm) { perror("malloc"); fclose(fp); return 1; }

        ret = crypto_sign(sm, &smlen, msg, MLEN, NULL, 0, sk, sign_rnd);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign failed\n", count);
            free(sm); fclose(fp); return 1;
        }

        m_out = (uint8_t *)malloc(smlen);
        if (!m_out) { perror("malloc"); free(sm); fclose(fp); return 1; }
        ret = crypto_sign_open(m_out, &mlen_out, sm, smlen, NULL, 0, pk);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign_open failed\n", count);
            free(sm); free(m_out); fclose(fp); return 1;
        }

        fprintf(fp, "count = %d\n", count);
        fprintf(fp, "seed = ");
        for (int i = 0; i < 48; i++) fprintf(fp, "%02X", seed[i]);
        fprintf(fp, "\n");
        fprintf(fp, "keypair_seed = ");
        for (int i = 0; i < SEEDBYTES; i++) fprintf(fp, "%02X", keypair_seed[i]);
        fprintf(fp, "\n");
        fprintf(fp, "sign_rnd = ");
        for (int i = 0; i < RNDBYTES; i++) fprintf(fp, "%02X", sign_rnd[i]);
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
        fprintf(fp, "smlen = %zu\n", smlen);
        fprintf(fp, "sm = ");
        for (size_t i = 0; i < smlen; i++) fprintf(fp, "%02X", sm[i]);
        fprintf(fp, "\n\n");

        free(sm); sm = NULL;
        free(m_out); m_out = NULL;

        for (int i = 47; i >= 0; i--) { if (++entropy_input[i]) break; }
    }

    fclose(fp);
    printf("Generated %d KAT vectors → %s\n", KATNUM, RSP_PATH);
    return 0;
}
