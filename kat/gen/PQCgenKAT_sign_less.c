/*
 * PQCgenKAT_sign_less.c
 *
 * KAT generator for LESS.
 * LESS uses a SHAKE-based internal CSPRNG initialized with:
 *   initialize_csprng(&platform_csprng_state, seed, 48)
 *
 * Seed/message generation: NIST AES-256-CTR DRBG (from kat/gen/rng.h).
 * No randombytes() symbol conflict: LESS's randombytes is static-inline in rng.h
 * and only visible to LESS source TUs that include it; PQCgenKAT_sign_less.c
 * does not include LESS's rng.h, so the NIST randombytes() from kat/gen/rng.c
 * is used for seed/message derivation without conflict.
 *
 * RSP fields: count, seed (48 B LESS CSPRNG seed), mlen, msg, pk, sk, smlen, sm
 *
 * Compile with LESS sources (including src/rng.c) and kat/gen/rng.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "rng.h"        /* NIST DRBG from kat/gen/ */
#include "api.h"        /* LESS API */
#include "sha3.h"       /* defines SHAKE_STATE_STRUCT */
#include "parameters.h"

/* These are defined in LESS src/rng.c */
extern SHAKE_STATE_STRUCT platform_csprng_state;
void initialize_csprng(SHAKE_STATE_STRUCT *shake_state,
                       const unsigned char *seed,
                       const uint32_t seed_len_bytes);

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

    for (int i = 0; i < 48; i++) entropy_input[i] = (uint8_t)i;

    for (int count = 0; count < KATNUM; count++) {

        /* Derive seed and message using NIST DRBG */
        randombytes_init(entropy_input, NULL, 256);
        randombytes(seed, 48);
        randombytes(msg,  MLEN);

        /* Initialize LESS internal SHAKE CSPRNG with derived seed */
        initialize_csprng(&platform_csprng_state, seed, 48);

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

        for (int i = 47; i >= 0; i--) { if (++entropy_input[i]) break; }
    }

    fclose(fp);
    printf("Generated %d KAT vectors -> %s\n", KATNUM, RSP_PATH);
    return 0;
}
