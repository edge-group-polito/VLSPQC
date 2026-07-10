/*
 * PQCgenKAT_sign_xmss.c
 *
 * KAT generator for XMSS / XMSS^MT.
 *
 * Unlike every other DS family here, XMSS's pk/sk/signature sizes are
 * runtime-variable (chosen by parameter-set name / OID, not compile-time
 * constants) -- see original/Stateful-DS/XMSS/include/xmss_variant.h. Two build
 * modes select what this binary does:
 *
 *   -DXMSS_VARIANT_STR="XMSS-SHA2_10_256"   (default: build for exactly one
 *       named parameter set; this is what `make gen-xmss` uses -- the
 *       always-on variant wired into `gen-all`.)
 *
 *   -DXMSS_CHECK_ALL=1   (loop over xmss_variant_fast_names -- 49 parameter
 *       sets -- one .rsp per variant under RSP_DIR. This is what
 *       `make gen-xmss-check` uses; NOT part of `gen-all`.)
 *
 *   -DXMSS_CHECK_ALL=1 -DXMSS_CHECK_SLOW=1   (loop over xmss_variant_slow_names
 *       instead -- 28 parameter sets whose effective tree height is 16 or 20
 *       (either a single-tree XMSS set, or an XMSS^MT set whose per-layer
 *       height reduces to 20, like "40/2"/"60/3"), which cost O(2^height)
 *       leaf hashes with this non-BDS-cached reference core and can take
 *       minutes to over an hour *each*. This is what `make gen-xmss-check-slow`
 *       uses -- see DS/XMSS/README.md.)
 *
 * XMSS uses an explicit derand seed for keygen (3*n bytes) and, unlike most
 * other families here, signing needs *no* randomness at all -- so there is
 * no sig_rnd field.
 *
 * RSP fields written: count, seed, keypair_seed, mlen, msg, pk, sk, smlen, sm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#include "rng.h"
#include "xmss_variant.h"

#ifndef MLEN
#define MLEN 33
#endif

#ifndef KATNUM
#define KATNUM 10
#endif

#if !defined(XMSS_CHECK_ALL) && !defined(XMSS_VARIANT_STR)
#error "Define either -DXMSS_VARIANT_STR=\"...\" or -DXMSS_CHECK_ALL=1"
#endif

#ifdef XMSS_CHECK_ALL
#ifndef RSP_DIR
#error "RSP_DIR must be defined in XMSS_CHECK_ALL mode"
#endif
#else
#ifndef RSP_PATH
#define RSP_PATH "PQCsignKAT.rsp"
#endif
#endif

static void sanitize(char *dst, const char *src)
{
    for (; *src; src++, dst++) {
        *dst = (*src == '/' || *src == '+') ? '_' : *src;
    }
    *dst = '\0';
}

/* Generates KATNUM vectors for one parameter set into an already-open file.
 * Returns 0 on success. */
static int gen_one(const char *name, FILE *fp)
{
    xmss_variant_t v;
    if (xmss_variant_init(&v, name)) {
        fprintf(stderr, "%s: unknown parameter set\n", name);
        return 1;
    }
    size_t n = v.params.n;
    size_t pkb = xmss_variant_pk_bytes(&v);
    size_t skb = xmss_variant_sk_bytes(&v);
    size_t sigb = xmss_variant_sig_bytes(&v);

    uint8_t entropy_input[48], seed[48];
    uint8_t *keypair_seed = malloc(3 * n);
    uint8_t msg[MLEN];
    uint8_t *pk = malloc(pkb);
    uint8_t *sk = malloc(skb);
    uint8_t *sm = NULL, *m_out = NULL;
    unsigned long long smlen, mlen_out;
    int ret = 0;

    fprintf(fp, "# %s\n\n", name);
    for (int i = 0; i < 48; i++) entropy_input[i] = (uint8_t)i;

    for (int count = 0; count < KATNUM; count++) {
        randombytes_init(entropy_input, NULL, 256);
        randombytes(seed, 48);
        randombytes_init(seed, NULL, 256);

        randombytes(keypair_seed, (unsigned long long)(3 * n));
        randombytes(msg, MLEN);

        if (xmss_variant_keypair_seed(&v, pk, sk, keypair_seed) != 0) {
            fprintf(stderr, "%s count=%d: keypair failed\n", name, count);
            ret = 1; break;
        }
        /* xmss_variant_sign() mutates sk in place (advances XMSS's leaf
         * index) -- snapshot the freshly-generated (index=0) state now, so
         * the .rsp records what a fresh keypair_seed call actually produces,
         * not the post-sign state. */
        uint8_t *sk_snapshot = malloc(skb);
        memcpy(sk_snapshot, sk, skb);

        sm = malloc(sigb + MLEN);
        if (xmss_variant_sign(&v, sk, sm, &smlen, msg, MLEN) != 0) {
            fprintf(stderr, "%s count=%d: sign failed\n", name, count);
            ret = 1; break;
        }

        m_out = malloc(smlen);
        if (xmss_variant_open(&v, m_out, &mlen_out, sm, smlen, pk) != 0) {
            fprintf(stderr, "%s count=%d: verify failed\n", name, count);
            ret = 1; break;
        }

        fprintf(fp, "count = %d\n", count);
        fprintf(fp, "seed = ");
        for (int i = 0; i < 48; i++) fprintf(fp, "%02X", seed[i]);
        fprintf(fp, "\n");
        fprintf(fp, "keypair_seed = ");
        for (size_t i = 0; i < 3 * n; i++) fprintf(fp, "%02X", keypair_seed[i]);
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
        fprintf(fp, "smlen = %llu\n", smlen);
        fprintf(fp, "sm = ");
        for (unsigned long long i = 0; i < smlen; i++) fprintf(fp, "%02X", sm[i]);
        fprintf(fp, "\n\n");

        free(sk_snapshot);
        free(sm); sm = NULL;
        free(m_out); m_out = NULL;

        for (int i = 47; i >= 0; i--) { if (++entropy_input[i]) break; }
    }

    free(keypair_seed); free(pk); free(sk);
    free(sm); free(m_out);
    return ret;
}

int main(void)
{
#ifdef XMSS_CHECK_ALL
#ifdef XMSS_CHECK_SLOW
    const char *const *names = xmss_variant_slow_names;
    int count = xmss_variant_slow_count;
#else
    const char *const *names = xmss_variant_fast_names;
    int count = xmss_variant_fast_count;
#endif
    mkdir(RSP_DIR, 0755);
    int failures = 0;
    for (int i = 0; i < count; i++) {
        const char *name = names[i];
        char safe[64];
        sanitize(safe, name);
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.rsp", RSP_DIR, safe);
        FILE *fp = fopen(path, "w");
        if (!fp) { perror(path); failures++; continue; }
        printf("[%d/%d] generating %s -> %s\n", i + 1, count, name, path);
        if (gen_one(name, fp) != 0) failures++;
        fclose(fp);
    }
    printf("Generated %d/%d XMSS variants (KATNUM=%d each)\n",
           count - failures, count, KATNUM);
    return failures ? 1 : 0;
#else
    FILE *fp = fopen(RSP_PATH, "w");
    if (!fp) { perror("fopen"); return 1; }
    int ret = gen_one(XMSS_VARIANT_STR, fp);
    fclose(fp);
    if (ret == 0) {
        printf("Generated %d KAT vectors -> %s\n", KATNUM, RSP_PATH);
    }
    return ret;
#endif
}
