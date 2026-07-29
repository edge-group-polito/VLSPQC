/*
 * PQCgenKAT_sign_hawk.c
 *
 * KAT generator for HAWK.
 *
 * HAWK's keygen has no explicit-randomness ("derand") entry point (its
 * Gaussian/NTRU sampling draws a data-dependent, variable amount of
 * randomness via an rng callback, with no seed parameter), so it's still
 * driven by reseeding the NIST AES-256-CTR DRBG with a deterministic
 * 48-byte entropy input (count-based) and calling the ordinary API.
 *
 * Sign, however, is driven entirely by whatever bytes its rng callback
 * hands back -- HAWK's own hawk_sign_finish() takes an arbitrary hawk_rng
 * callback, so instead of calling crypto_sign() (which wires that callback
 * straight to randombytes()), this generator calls hawk_sign_start()/
 * shake_inject()/hawk_sign_finish() directly with a *recording* callback:
 * it still forwards to randombytes() (preserving the exact DRBG stream
 * position, so `sm` matches what the old crypto_sign()-based generator
 * produced bit-for-bit), but also copies every dispensed byte into a
 * capture buffer. That captured trace (`rho`) is written to the .rsp, and
 * crypto_sign_with_randomness() can later replay it against a *playback*
 * callback to reproduce the exact same signature -- without needing
 * keygen to run first purely to advance the DRBG.
 *
 *   crypto_sign_keypair(pk, sk)
 *   crypto_sign_with_randomness(sm, &smlen, m, mlen, sk, rho, rho_len)
 *   crypto_sign_open(m, &mlen, sm, smlen, pk)
 *
 * RSP fields written:
 *   count, seed (48 B DRBG seed), mlen, msg, pk, sk, rho, smlen, sm
 *
 * Compile with the algorithm's sources and rng.c, e.g.:
 *   gcc PQCgenKAT_sign_hawk.c rng.c <algo_srcs> -I <algo_include> -I . -o gen
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "rng.h"
#include "api.h"
#include "hawk.h"

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

/* Generous, variant-independent upper bound on how much randomness a single
 * hawk_sign_finish() call can draw (real usage is a small fraction of this
 * for both hawk-512 and hawk-1024) and on the temporary buffer it needs. */
#define RHO_CAP       65536
#define SIGN_TMP_SIZE (1u << 18)

typedef struct {
    unsigned char *buf;
    size_t cap;
    size_t pos;
} record_rng_ctx;

static void
hrng_record(void *ctx, void *dst, size_t len)
{
    record_rng_ctx *rc = (record_rng_ctx *)ctx;
    randombytes(dst, (unsigned long long)len);
    if (rc->pos + len > rc->cap) {
        fprintf(stderr, "hrng_record: capture buffer overflow (RHO_CAP too small)\n");
        exit(1);
    }
    memcpy(rc->buf + rc->pos, dst, len);
    rc->pos += len;
}

int main(void)
{
    uint8_t entropy_input[48];
    uint8_t seed[48];
    uint8_t msg[MLEN];
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t rho[RHO_CAP];
    record_rng_ctx rc;
    shake_context sc;
    unsigned char sign_tmp[SIGN_TMP_SIZE];
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

        /* Signing — allocate max possible size */
        size_t sm_max = MLEN + CRYPTO_BYTES;
        sm = (uint8_t *)malloc(sm_max);
        if (!sm) { perror("malloc"); fclose(fp); return 1; }

        /* Sign via the raw hawk_sign_finish() (same steps crypto_sign() takes
         * internally) with a recording rng, capturing the exact randomness
         * trace `hawk_sign_finish` consumes so it can be replayed later. */
        rc.buf = rho; rc.cap = RHO_CAP; rc.pos = 0;
        if (msg != sm) {
            memmove(sm, msg, MLEN);
        }
        hawk_sign_start(&sc);
        shake_inject(&sc, sm, MLEN);
        if (!hawk_sign_finish(HAWK_LOGN, &hrng_record, &rc,
                sm + MLEN, &sc, sk, sign_tmp, sizeof sign_tmp))
        {
            fprintf(stderr, "count=%d: hawk_sign_finish failed\n", count);
            free(sm); fclose(fp); return 1;
        }
        smlen = MLEN + CRYPTO_BYTES;

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
        fprintf(fp, "rho_len = %zu\n", rc.pos);
        fprintf(fp, "rho = ");
        for (size_t i = 0; i < rc.pos; i++) fprintf(fp, "%02X", rho[i]);
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
    printf("Generated %d KAT vectors → %s\n", KATNUM, RSP_PATH);
    return 0;
}
