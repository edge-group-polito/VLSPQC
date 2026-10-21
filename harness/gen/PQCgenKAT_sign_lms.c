/*
 * PQCgenKAT_sign_lms.c
 *
 * KAT generator for LMS (Leighton-Micali Signatures) -- Cisco's hash-sigs
 * library with levels=1, see algorithms/signatures/stateful/LMS/README.md.
 *
 * Like HSS, pk/sk/sig sizes are chosen by parameter set (height, Winternitz
 * w), not compile-time constants -- see algorithms/signatures/stateful/HSS/include/hss_variant.h
 * (shared by DS/LMS and DS/HSS). Two build modes:
 *
 *   (default)             build for lms_variant_default (LMS_H10_W8) --
 *       what `make gen-lms` uses, wired into `gen-all`.
 *
 *   -DLMS_CHECK_ALL=1     loop over lms_variant_fast[] (8 parameter sets:
 *       heights 5/10 x all 4 Winternitz values) -- one .rsp per variant
 *       under RSP_DIR. What `make gen-lms-check` uses; NOT part of `gen-all`.
 *
 *   -DLMS_CHECK_ALL=1 -DLMS_CHECK_SLOW=1   loop over lms_variant_slow[] (12
 *       parameter sets: heights 15/20/25) instead -- keygen alone measured
 *       over a minute at height 15 and grows ~32x per +5 height, so this can
 *       take a very long time. What `make gen-lms-check-slow` uses.
 *
 * LMS uses an explicit derand seed for keygen (48 bytes -- SEED_LEN+I_LEN)
 * and, like XMSS, signing needs no randomness at all -- so there is no
 * sig_rnd field. Unlike XMSS, the derand seed produces a full private key in
 * one call and signing does not need re-deriving anything from it, but
 * signing does mutate sk in place (advances an 8-byte counter) -- the .rsp
 * must record the pre-sign sk, so it's snapshotted before calling sign.
 *
 * RSP fields written: count, seed, keypair_seed, mlen, msg, pk, sk, smlen, sm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#include "rng.h"
#include "hss_variant.h"

#ifndef MLEN
#define MLEN 33
#endif

#ifndef KATNUM
#define KATNUM 10
#endif

#ifdef LMS_CHECK_ALL
#ifndef RSP_DIR
#error "RSP_DIR must be defined in LMS_CHECK_ALL mode"
#endif
#else
#ifndef RSP_PATH
#define RSP_PATH "PQCsignKAT.rsp"
#endif
#endif

/* Generates KATNUM vectors for one named variant into an already-open file.
 * Returns 0 on success. */
static int gen_one(const hss_named_variant_t *nv, FILE *fp)
{
    hss_variant_t v;
    if (hss_variant_init(&v, nv->levels, nv->lm_type, nv->lm_ots_type)) {
        fprintf(stderr, "%s: bad parameter set\n", nv->name);
        return 1;
    }
    size_t pkb = hss_variant_pk_bytes(&v);
    size_t skb = hss_variant_sk_bytes(&v);
    size_t sigb = hss_variant_sig_bytes(&v);

    uint8_t entropy_input[48], seed[48], keypair_seed[48];
    uint8_t msg[MLEN];
    uint8_t *pk = malloc(pkb);
    uint8_t *sk = malloc(skb);
    uint8_t *sk_snapshot = malloc(skb);
    uint8_t *sig = malloc(sigb);
    int ret = 0;

    fprintf(fp, "# %s\n\n", nv->name);
    for (int i = 0; i < 48; i++) entropy_input[i] = (uint8_t)i;

    for (int count = 0; count < KATNUM; count++) {
        randombytes_init(entropy_input, NULL, 256);
        randombytes(seed, 48);
        randombytes_init(seed, NULL, 256);

        randombytes(keypair_seed, 48);
        randombytes(msg, MLEN);

        if (hss_variant_keypair_seed(&v, pk, sk, keypair_seed) != 0) {
            fprintf(stderr, "%s count=%d: keypair failed\n", nv->name, count);
            ret = 1; break;
        }
        /* hss_variant_sign() mutates sk (advances its counter) -- snapshot
         * the freshly-generated state first, see file header comment. */
        memcpy(sk_snapshot, sk, skb);

        size_t siglen;
        if (hss_variant_sign(&v, sk, sig, &siglen, msg, MLEN) != 0) {
            fprintf(stderr, "%s count=%d: sign failed\n", nv->name, count);
            ret = 1; break;
        }

        if (hss_variant_verify(msg, MLEN, sig, siglen, pk) != 0) {
            fprintf(stderr, "%s count=%d: verify failed\n", nv->name, count);
            ret = 1; break;
        }

        fprintf(fp, "count = %d\n", count);
        fprintf(fp, "seed = ");
        for (int i = 0; i < 48; i++) fprintf(fp, "%02X", seed[i]);
        fprintf(fp, "\n");
        fprintf(fp, "keypair_seed = ");
        for (int i = 0; i < 48; i++) fprintf(fp, "%02X", keypair_seed[i]);
        fprintf(fp, "\n");
        fprintf(fp, "mlen = %d\n", MLEN);
        fprintf(fp, "msg = ");
        for (int i = 0; i < MLEN; i++) fprintf(fp, "%02X", msg[i]);
        fprintf(fp, "\n");
        fprintf(fp, "pk = ");
        for (size_t i = 0; i < pkb; i++) fprintf(fp, "%02X", pk[i]);
        fprintf(fp, "\n");
        fprintf(fp, "sk = ");
        for (size_t i = 0; i < skb; i++) fprintf(fp, "%02X", sk_snapshot[i]);
        fprintf(fp, "\n");
        fprintf(fp, "smlen = %zu\n", siglen + MLEN);
        fprintf(fp, "sm = ");
        for (size_t i = 0; i < siglen; i++) fprintf(fp, "%02X", sig[i]);
        for (int i = 0; i < MLEN; i++) fprintf(fp, "%02X", msg[i]);
        fprintf(fp, "\n\n");

        for (int i = 47; i >= 0; i--) { if (++entropy_input[i]) break; }
    }

    free(pk); free(sk); free(sk_snapshot); free(sig);
    return ret;
}

static void sanitize(char *dst, const char *src)
{
    for (; *src; src++, dst++) *dst = *src;
    *dst = '\0';
}

int main(void)
{
#ifdef LMS_CHECK_ALL
#ifdef LMS_CHECK_SLOW
    const hss_named_variant_t *tbl = lms_variant_slow;
    int count = lms_variant_slow_count;
#else
    const hss_named_variant_t *tbl = lms_variant_fast;
    int count = lms_variant_fast_count;
#endif
    mkdir(RSP_DIR, 0755);
    int failures = 0;
    for (int i = 0; i < count; i++) {
        char safe[64];
        sanitize(safe, tbl[i].name);
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.rsp", RSP_DIR, safe);
        FILE *fp = fopen(path, "w");
        if (!fp) { perror(path); failures++; continue; }
        printf("[%d/%d] generating %s -> %s\n", i + 1, count, tbl[i].name, path);
        if (gen_one(&tbl[i], fp) != 0) failures++;
        fclose(fp);
    }
    printf("Generated %d/%d LMS variants (KATNUM=%d each)\n",
           count - failures, count, KATNUM);
    return failures ? 1 : 0;
#else
    FILE *fp = fopen(RSP_PATH, "w");
    if (!fp) { perror("fopen"); return 1; }
    int ret = gen_one(&lms_variant_default, fp);
    fclose(fp);
    if (ret == 0) {
        printf("Generated %d KAT vectors -> %s\n", KATNUM, RSP_PATH);
    }
    return ret;
#endif
}
