/*
 * PQCgenKAT_sign_cross.c
 *
 * KAT generator for CROSS.
 * CROSS uses a SHAKE-based internal CSPRNG initialized with a 48-byte seed.
 *
 * Seed/message derivation: use a private SHAKE CSPRNG state seeded with the
 * standard {0,...,47} sequence (avoids conflict with CROSS's own randombytes()).
 * Platform CSPRNG is initialized separately for each keygen+sign pair.
 *
 * RSP fields: count, seed (48 B CROSS CSPRNG seed), mlen, msg, pk, sk, smlen, sm
 *
 * Compile with CROSS sources only (no harness/gen/rng.c needed).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "csprng_hash.h"
#include "parameters.h"

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
    uint8_t master_seed[48];
    CSPRNG_STATE_T master_state;
    uint8_t seed[48];
    uint8_t msg[MLEN];
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t *sm    = NULL;
    uint8_t *m_out = NULL;
    unsigned long long smlen, mlen_out;
    int ret;

    FILE *fp = fopen(RSP_PATH, "w");
    if (!fp) { perror("fopen"); return 1; }
    fprintf(fp, "# %s\n\n", ALGO_NAME);

    /* Seed master SHAKE DRBG with {0,...,47} — standard NIST KAT entropy */
    for (int i = 0; i < 48; i++) master_seed[i] = (uint8_t)i;
    /* Use domain sep 0xFFFF (distinct from algorithm's domain sep) */
    csprng_initialize(&master_state, master_seed, 48, (uint16_t)0xFFFF);

    for (int count = 0; count < KATNUM; count++) {

        /* Derive deterministic seed and message for this vector */
        csprng_randombytes(seed, 48,   &master_state);
        csprng_randombytes(msg,  MLEN, &master_state);

        /* Initialize CROSS platform CSPRNG for this keygen+sign pair */
        csprng_initialize(&platform_csprng_state, seed, 48, CSPRNG_DOMAIN_SEP_CONST);

        ret = crypto_sign_keypair(pk, sk);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign_keypair failed\n", count);
            fclose(fp); return 1;
        }

        size_t sm_max = MLEN + CRYPTO_BYTES;
        sm = (uint8_t *)malloc(sm_max);
        if (!sm) { perror("malloc"); fclose(fp); return 1; }

        ret = crypto_sign(sm, &smlen, msg, MLEN, sk);
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
    }

    fclose(fp);
    printf("Generated %d KAT vectors -> %s\n", KATNUM, RSP_PATH);
    return 0;
}
