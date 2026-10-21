/*
 * hss_variant.h -- pqrv adapter over Cisco's hash-sigs HSS/LMS library.
 *
 * hash-sigs has no crypto_sign_*-style API: keygen takes an RNG *callback*
 * (called exactly once, for PRIVATE_KEY_SEED_LEN=48 bytes -- SEED_LEN(32) +
 * I_LEN(16), see hss_keygen.c:121 and hss_internal.h under SECRET_METHOD==2),
 * and signing is fully deterministic (no RNG at all -- SECRET_METHOD 2
 * derives the per-signature randomizer from the private key's SEED/I via a
 * PRF, per hss/ACVP Definition.txt) but *mutates* the 64-byte private key in
 * place (advances an 8-byte counter) as a side effect of hss_generate_signature.
 *
 * LMS is just HSS with levels=1 -- same library, same adapter.
 */
#ifndef HSS_VARIANT_H
#define HSS_VARIANT_H

#include <stdint.h>
#include <stddef.h>
#include "common_defs.h"

typedef struct {
    unsigned    levels;
    param_set_t lm_type[MAX_HSS_LEVELS];
    param_set_t lm_ots_type[MAX_HSS_LEVELS];
} hss_variant_t;

/* Fills in a variant from `levels` (lm_type[i], lm_ots_type[i]) pairs. */
int hss_variant_init(hss_variant_t *v, unsigned levels,
                     const param_set_t *lm_type, const param_set_t *lm_ots_type);

size_t hss_variant_pk_bytes(const hss_variant_t *v);
size_t hss_variant_sk_bytes(const hss_variant_t *v);
size_t hss_variant_sig_bytes(const hss_variant_t *v);

/* seed: PRIVATE_KEY_SEED_LEN (48) bytes of explicit randomness. Writes the
 * full pk and the full (64-byte, fixed regardless of parameter set) private
 * key to pk/sk. */
int hss_variant_keypair_seed(const hss_variant_t *v, uint8_t *pk, uint8_t *sk,
                             const uint8_t *seed);

/* Deterministic given sk -- but MUTATES sk in place (advances its signature
 * counter), same caveat as XMSS: snapshot sk before calling this if the
 * pre-sign state needs to be recorded. */
int hss_variant_sign(const hss_variant_t *v, uint8_t *sk,
                     uint8_t *sig, size_t *siglen,
                     const uint8_t *m, size_t mlen);

int hss_variant_verify(const uint8_t *m, size_t mlen,
                       const uint8_t *sig, size_t siglen,
                       const uint8_t *pk);

/*
 * Named parameter-set tables for the LMS/HSS KAT generators' "-check" sweeps.
 * "Fast" = LMS heights 5/10 (<=1024 leaves/tree, keygen near-instant);
 * "slow" = heights 15/20/25 (>=32768 leaves/tree -- keygen alone measured at
 * 1+ minute at height 15, growing ~32x per +5 height, so height 20/25 single
 * trees are minutes-to-hours). See algorithms/signatures/stateful/LMS/README.md and
 * algorithms/signatures/stateful/HSS/README.md. HSS tables repeat each (height, w) pair
 * uniformly across L=2 and L=3 levels.
 */
typedef struct {
    const char  *name;
    unsigned    levels;
    param_set_t lm_type[MAX_HSS_LEVELS];
    param_set_t lm_ots_type[MAX_HSS_LEVELS];
} hss_named_variant_t;

extern const hss_named_variant_t lms_variant_default;
extern const hss_named_variant_t lms_variant_fast[];
extern const int lms_variant_fast_count;
extern const hss_named_variant_t lms_variant_slow[];
extern const int lms_variant_slow_count;

extern const hss_named_variant_t hss_variant_default;
extern const hss_named_variant_t hss_variant_fast[];
extern const int hss_variant_fast_count;
extern const hss_named_variant_t hss_variant_slow[];
extern const int hss_variant_slow_count;

#endif
