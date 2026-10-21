/*
 * PQCgenKAT_sign_faest_r3.c
 *
 * KAT generator for algorithms/signatures/onramp/FAEST-R3 (Round-3 FAEST v3, lambda=128, all 4
 * variants FAEST_{128F,128S,EM_128F,EM_128S}). It reproduces the official
 * PQCgenKAT_sign.c of the Round-3 package exactly (standard NIST flow):
 *
 *   randombytes_init(entropy 0..47)                     -- ONE init
 *   pass 1, for count = 0 .. KATNUM-1:
 *       seed = randombytes(48); mlen = 33 * (count + 1); msg = randombytes(mlen)
 *   pass 2, for each record:
 *       randombytes_init(seed)       -- keygen/sign draw from the seeded DRBG
 *       crypto_sign_keypair(pk, sk); crypto_sign(sm, m, sk); crypto_sign_open
 *
 * This differs from harness/gen/PQCgenKAT_sign_faest.c (older FAEST port: entropy
 * incremented per count, constant MLEN, msg drawn AFTER the reseed), so the
 * records are byte-identical to the official KAT files and the .rsp can be
 * diffed against them. RSP fields: count, seed, mlen, msg, pk, sk, smlen, sm.
 *
 * Compile (see the FAEST-R3 section of the top-level Makefile):
 *   gcc PQCgenKAT_sign_faest_r3.c <FAEST-R3/common/src sources> -I <common/include> \
 *       -I <FAEST-R3/FAEST_128F/include> -I . -o gen
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "rng.h"
#include "api.h"

#ifndef KATNUM
#define KATNUM 10
#endif

#ifndef RSP_PATH
#define RSP_PATH "PQCsignKAT.rsp"
#endif

#ifndef ALGO_NAME
#define ALGO_NAME CRYPTO_ALGNAME
#endif

static void fprint_hex(FILE *fp, const char *label, const uint8_t *a, unsigned long long len)
{
    fprintf(fp, "%s", label);
    for (unsigned long long i = 0; i < len; i++) fprintf(fp, "%02X", a[i]);
    fprintf(fp, "\n");
}

int main(void)
{
    uint8_t entropy_input[48];
    uint8_t seed[48];
    uint8_t *pk = (uint8_t *)malloc(CRYPTO_PUBLICKEYBYTES);
    uint8_t *sk = (uint8_t *)malloc(CRYPTO_SECRETKEYBYTES);
    if (!pk || !sk) { perror("malloc"); free(pk); free(sk); return 1; }
    unsigned long long smlen, mlen_out;
    int ret;

    FILE *fp = fopen(RSP_PATH, "w");
    if (!fp) { perror("fopen"); return 1; }

    fprintf(fp, "# %s\n\n", ALGO_NAME);

    for (int i = 0; i < 48; i++) entropy_input[i] = (uint8_t)i;
    randombytes_init(entropy_input, NULL, 256);

    /* Phase 1 (as in the official generator's .req pass): draw every seed and
     * message from the single entropy-seeded DRBG. Must be completed before
     * any randombytes_init(seed) below, which replaces the global DRBG state. */
    static uint8_t seeds[KATNUM][48];
    uint8_t *msgs[KATNUM];
    for (int count = 0; count < KATNUM; count++) {
        unsigned long long mlen = 33ULL * (unsigned long long)(count + 1);
        randombytes(seeds[count], 48);
        msgs[count] = (uint8_t *)calloc(mlen, 1);
        if (!msgs[count]) { perror("calloc"); fclose(fp); return 1; }
        randombytes(msgs[count], mlen);
    }

    /* Phase 2: replay each record with the DRBG seeded from its own seed. */
    for (int count = 0; count < KATNUM; count++) {
        unsigned long long mlen = 33ULL * (unsigned long long)(count + 1);
        uint8_t *msg    = msgs[count];
        uint8_t *sm     = (uint8_t *)calloc(mlen + CRYPTO_BYTES, 1);
        uint8_t *m_out  = (uint8_t *)calloc(mlen + CRYPTO_BYTES, 1);
        if (!sm || !m_out) { perror("calloc"); fclose(fp); return 1; }
        memcpy(seed, seeds[count], 48);

        /* keygen and sign consume the DRBG seeded with `seed` */
        randombytes_init(seed, NULL, 256);

        ret = crypto_sign_keypair(pk, sk);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign_keypair failed (%d)\n", count, ret);
            fclose(fp); return 1;
        }
        ret = crypto_sign(sm, &smlen, msg, mlen, sk);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign failed (%d)\n", count, ret);
            fclose(fp); return 1;
        }
        ret = crypto_sign_open(m_out, &mlen_out, sm, smlen, pk);
        if (ret != 0 || mlen_out != mlen || memcmp(m_out, msg, mlen) != 0) {
            fprintf(stderr, "count=%d: crypto_sign_open failed (%d)\n", count, ret);
            fclose(fp); return 1;
        }

        fprintf(fp, "count = %d\n", count);
        fprint_hex(fp, "seed = ", seed, 48);
        fprintf(fp, "mlen = %llu\n", mlen);
        fprint_hex(fp, "msg = ", msg, mlen);
        fprint_hex(fp, "pk = ", pk, CRYPTO_PUBLICKEYBYTES);
        fprint_hex(fp, "sk = ", sk, CRYPTO_SECRETKEYBYTES);
        fprintf(fp, "smlen = %llu\n", smlen);
        fprint_hex(fp, "sm = ", sm, smlen);
        fprintf(fp, "\n");

        free(msg); free(sm); free(m_out);
    }

    fclose(fp);
    free(pk); free(sk);
    printf("Generated %d KAT vectors -> %s\n", KATNUM, RSP_PATH);
    return 0;
}
