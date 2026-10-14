/*
 * PQCgenKAT_sign_snova_r3.c
 *
 * KAT generator for original/Additional-DS/SNOVA-R3 (Round-3 SNOVA, all 18
 * variants SNOVA_{I,III,V}_{B,K,S}[_AES]). It reproduces the official
 * PQCgenKAT_sign.c of the Round-3 package exactly (standard NIST flow):
 *
 *   randombytes_init(entropy 0..47)                     -- ONE init
 *   pass 1 (the ".req" file), for count = 0 .. KATNUM-1:
 *       seed[count] = randombytes(48)
 *       mlen        = 33 * (count + 1)
 *       msg[count]  = randombytes(mlen)
 *   pass 2 (the ".rsp" file), for each count:
 *       randombytes_init(seed[count])  -- keygen/sign draw from this DRBG
 *       crypto_sign_keypair(pk, sk); crypto_sign(sm, m, sk); crypto_sign_open
 *
 * This differs from kat/gen/PQCgenKAT_sign_snova.c (older SNOVA port: entropy
 * incremented per count, constant MLEN, msg drawn AFTER the reseed), so the
 * records are byte-identical to the official KAT files and the .rsp can be
 * diffed against them. RSP fields: count, seed, mlen, msg, pk, sk, smlen, sm.
 *
 * Compile (see the SNOVA-R3 section of the top-level Makefile):
 *   gcc PQCgenKAT_sign_snova_r3.c <SNOVA-R3/common/src/all .c files> -I <common/include> \
 *       -I <SNOVA-R3/SNOVA_I_B/include> -I . -o gen
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
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    unsigned long long smlen, mlen_out;
    int ret;

    FILE *fp = fopen(RSP_PATH, "w");
    if (!fp) { perror("fopen"); return 1; }

    fprintf(fp, "# %s\n\n", ALGO_NAME);

    for (int i = 0; i < 48; i++) entropy_input[i] = (uint8_t)i;
    randombytes_init(entropy_input, NULL, 256);

    /* pass 1: draw every seed and message from the entropy-seeded DRBG */
    uint8_t (*seeds)[48] = (uint8_t (*)[48])calloc(KATNUM, 48);
    uint8_t **msgs = (uint8_t **)calloc(KATNUM, sizeof(uint8_t *));
    if (!seeds || !msgs) { perror("calloc"); fclose(fp); return 1; }
    for (int count = 0; count < KATNUM; count++) {
        unsigned long long mlen = 33ULL * (unsigned long long)(count + 1);
        randombytes(seeds[count], 48);
        msgs[count] = (uint8_t *)calloc(mlen, 1);
        if (!msgs[count]) { perror("calloc"); fclose(fp); return 1; }
        randombytes(msgs[count], mlen);
    }

    /* pass 2: keygen / sign / verify per record */
    for (int count = 0; count < KATNUM; count++) {
        unsigned long long mlen = 33ULL * (unsigned long long)(count + 1);
        uint8_t *msg    = msgs[count];
        uint8_t *sm     = (uint8_t *)calloc(mlen + CRYPTO_BYTES, 1);
        uint8_t *m_out  = (uint8_t *)calloc(mlen + CRYPTO_BYTES, 1);
        if (!sm || !m_out) { perror("calloc"); fclose(fp); return 1; }
        memcpy(seed, seeds[count], 48);

        /* keygen and sign now consume the DRBG seeded with `seed` */
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

        free(msg); msgs[count] = NULL; free(sm); free(m_out);
    }

    free(seeds); free(msgs);
    fclose(fp);
    printf("Generated %d KAT vectors -> %s\n", KATNUM, RSP_PATH);
    return 0;
}
