/*
 * PQCgenKAT_sign_sdith_r3.c
 *
 * KAT generator for SDITH-R3 (NIST Round-3 VOLE-in-the-Head SDitH, 12 parameter sets:
 * cat{1,3,5} x {fast,short} x {-,cipherpow}), reproducing the flow of the
 * official Round-3 PQCgenKAT_sign.c bit-for-bit:
 *
 *   1. entropy_input = 0..47; randombytes_init(entropy_input, NULL, 256);
 *   2. for every count: seed = randombytes(48); mlen = 33*(count+1);
 *      msg = randombytes(mlen)           (all from ONE continuous DRBG stream)
 *   3. for every count: randombytes_init(seed, NULL, 256);
 *      crypto_sign_keypair(pk, sk); crypto_sign(sm, &smlen, msg, mlen, sk);
 *      crypto_sign_open() check.
 *
 * (The Round-2 style generator PQCgenKAT_sign_sdith.c re-seeds per count and
 * draws the message from the reseeded stream, which does NOT reproduce the
 * official KAT; hence this separate file.)
 *
 * RSP fields written: count, seed (48 B DRBG seed), mlen, msg, pk, sk, smlen, sm.
 * KATNUM records are produced (official: 100).
 *
 * Compile with all of SDITH-R3/common/src (incl. rng.c, the NIST DRBG) plus
 * this file and one variant's include dir.
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

#define MLEN_OF(i) (33ULL * ((unsigned long long)(i) + 1))

static void fprint_hex(FILE *fp, const char *label, const uint8_t *b, unsigned long long len)
{
    fprintf(fp, "%s", label);
    for (unsigned long long i = 0; i < len; i++) fprintf(fp, "%02X", b[i]);
    fprintf(fp, "\n");
}

int main(void)
{
    uint8_t entropy_input[48];
    uint8_t (*seeds)[48] = malloc((size_t)KATNUM * 48);
    uint8_t **msgs = calloc((size_t)KATNUM, sizeof(uint8_t *));
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    unsigned long long smlen, mlen_out;
    int ret;

    if (!seeds || !msgs) { perror("malloc"); return 1; }

    FILE *fp = fopen(RSP_PATH, "w");
    if (!fp) { perror("fopen"); return 1; }

    fprintf(fp, "# %s\n\n", ALGO_NAME);

    for (int i = 0; i < 48; i++) entropy_input[i] = (uint8_t)i;
    randombytes_init(entropy_input, NULL, 256);

    /* Phase 1: seeds and messages from one continuous DRBG stream */
    for (int count = 0; count < KATNUM; count++) {
        unsigned long long mlen = MLEN_OF(count);
        randombytes(seeds[count], 48);
        msgs[count] = malloc((size_t)mlen);
        if (!msgs[count]) { perror("malloc"); fclose(fp); return 1; }
        randombytes(msgs[count], mlen);
    }

    /* Phase 2: keygen + sign + verify, DRBG reseeded with each record's seed */
    for (int count = 0; count < KATNUM; count++) {
        unsigned long long mlen = MLEN_OF(count);
        uint8_t *sm    = malloc((size_t)mlen + CRYPTO_BYTES);
        uint8_t *m_out = malloc((size_t)mlen + CRYPTO_BYTES);
        if (!sm || !m_out) { perror("malloc"); fclose(fp); return 1; }

        randombytes_init(seeds[count], NULL, 256);

        ret = crypto_sign_keypair(pk, sk);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign_keypair failed (%d)\n", count, ret);
            fclose(fp); return 1;
        }
        ret = crypto_sign(sm, &smlen, msgs[count], mlen, sk);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign failed (%d)\n", count, ret);
            fclose(fp); return 1;
        }
        ret = crypto_sign_open(m_out, &mlen_out, sm, smlen, pk);
        if (ret != 0 || mlen_out != mlen || memcmp(m_out, msgs[count], (size_t)mlen) != 0) {
            fprintf(stderr, "count=%d: crypto_sign_open failed (%d)\n", count, ret);
            fclose(fp); return 1;
        }

        fprintf(fp, "count = %d\n", count);
        fprint_hex(fp, "seed = ", seeds[count], 48);
        fprintf(fp, "mlen = %llu\n", mlen);
        fprint_hex(fp, "msg = ", msgs[count], mlen);
        fprint_hex(fp, "pk = ", pk, CRYPTO_PUBLICKEYBYTES);
        fprint_hex(fp, "sk = ", sk, CRYPTO_SECRETKEYBYTES);
        fprintf(fp, "smlen = %llu\n", smlen);
        fprint_hex(fp, "sm = ", sm, smlen);
        fprintf(fp, "\n");

        free(sm); free(m_out);
    }

    for (int count = 0; count < KATNUM; count++) free(msgs[count]);
    free(msgs); free(seeds);
    fclose(fp);
    printf("Generated %d KAT vectors -> %s\n", KATNUM, RSP_PATH);
    return 0;
}
