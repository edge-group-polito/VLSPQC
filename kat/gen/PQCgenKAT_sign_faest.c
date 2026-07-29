/*
 * PQCgenKAT_sign_faest.c
 *
 * KAT generator for FAEST (all 12 variants: faest_{128,192,256}{f,s} and
 * faest_em_{128,192,256}{f,s}).
 *
 * FAEST's keygen has no explicit-randomness ("derand") entry point:
 * crypto_sign_keypair() draws randomness internally via rand_bytes() in a
 * data-dependent rejection-sampling loop, which -- when compiled with
 * -DHAVE_RANDOMBYTES (see randomness.c) -- forwards straight to the NIST
 * AES-256-CTR DRBG's randombytes(). This is the same "reseed the global DRBG
 * once per count, then call the ordinary API" shape as HAWK; unlike CROSS/LESS
 * there is no separate internal CSPRNG object to re-seed.
 *
 * Sign, however, DOES have a derand entry point that upstream faest-ref
 * exposes but nothing in this repo used until now: faest_<variant>_sign_with_
 * randomness(sk, msg, mlen, rho, rho_len, sig, siglen). We draw `rho`
 * ourselves (same pattern as ML-DSA's sign_rnd) and sign through
 * crypto_sign_with_randomness() instead of crypto_sign() -- this lets a
 * consumer replay sign deterministically from a static (sk, rho) pair alone,
 * without needing to run keygen first purely to advance the DRBG.
 *
 *   crypto_sign_keypair(pk, sk)
 *   crypto_sign_with_randomness(sm, &smlen, m, mlen, sk, rho)
 *   crypto_sign_open(m, &mlen, sm, smlen, pk)
 *
 * RSP fields written: count, seed (48 B DRBG seed), mlen, msg, pk, sk, rho,
 *                      smlen, sm
 *
 * Compile with the variant's sources (incl. kat/gen/rng.c for the DRBG) and
 * -DHAVE_RANDOMBYTES, e.g.:
 *   gcc -DHAVE_RANDOMBYTES PQCgenKAT_sign_faest.c rng.c <variant_srcs> -I <variant_include> -I . -o gen
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

int main(void)
{
    uint8_t entropy_input[48];
    uint8_t seed[48];
    uint8_t msg[MLEN];
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t rho[CRYPTO_RHOBYTES];
    uint8_t *sm   = NULL;
    uint8_t *m_out = NULL;
    unsigned long long smlen, mlen_out;
    int ret;

    FILE *fp = fopen(RSP_PATH, "w");
    if (!fp) { perror("fopen"); return 1; }

    fprintf(fp, "# %s\n\n", ALGO_NAME);

    /* The NIST KAT procedure uses count-based entropy:
     * entropy_input[i] = i for count=0,
     * incremented by 1 for each successive count.            */
    for (int i = 0; i < 48; i++) entropy_input[i] = (uint8_t)i;

    for (int count = 0; count < KATNUM; count++) {

        /* Seed the DRBG with entropy_input for this count */
        randombytes_init(entropy_input, NULL, 256);

        /* The seed stored in the .rsp is the first 48 bytes the DRBG outputs.
         * The test replays this to get identical randomness.  */
        randombytes(seed, 48);

        /* Re-seed DRBG with that seed so keygen sees it */
        randombytes_init(seed, NULL, 256);

        /* Get random message */
        randombytes(msg, MLEN);

        /* Key generation */
        ret = crypto_sign_keypair(pk, sk);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign_keypair failed (%d)\n", count, ret);
            fclose(fp);
            return 1;
        }

        /* Draw rho explicitly (same pattern as ML-DSA's sign_rnd) so sign can
         * later be replayed from (sk, rho) alone, without needing keygen. */
        randombytes(rho, CRYPTO_RHOBYTES);

        /* Signing -- allocate max possible size */
        size_t sm_max = MLEN + CRYPTO_BYTES;
        sm = (uint8_t *)malloc(sm_max);
        if (!sm) { perror("malloc"); fclose(fp); return 1; }

        ret = crypto_sign_with_randomness(sm, &smlen, msg, MLEN, sk, rho);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign_with_randomness failed (%d)\n", count, ret);
            free(sm); fclose(fp); return 1;
        }

        /* Verify */
        m_out = (uint8_t *)malloc(smlen);
        if (!m_out) { perror("malloc"); free(sm); fclose(fp); return 1; }
        ret = crypto_sign_open(m_out, &mlen_out, sm, smlen, pk);
        if (ret != 0) {
            fprintf(stderr, "count=%d: crypto_sign_open failed (%d)\n", count, ret);
            free(sm); free(m_out); fclose(fp); return 1;
        }

        /* Write entry */
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
        fprintf(fp, "rho = ");
        for (int i = 0; i < CRYPTO_RHOBYTES; i++) fprintf(fp, "%02X", rho[i]);
        fprintf(fp, "\n");
        fprintf(fp, "smlen = %llu\n", smlen);
        fprintf(fp, "sm = ");
        for (unsigned long long i = 0; i < smlen; i++) fprintf(fp, "%02X", sm[i]);
        fprintf(fp, "\n\n");

        free(sm); sm = NULL;
        free(m_out); m_out = NULL;

        /* Advance entropy for next count */
        for (int i = 47; i >= 0; i--) {
            if (++entropy_input[i]) break;
        }
    }

    fclose(fp);
    printf("Generated %d KAT vectors -> %s\n", KATNUM, RSP_PATH);
    return 0;
}
