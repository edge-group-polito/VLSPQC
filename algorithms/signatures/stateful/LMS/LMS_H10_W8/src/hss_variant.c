#include <string.h>
#include "hss_variant.h"
#include "hss.h"

int hss_variant_init(hss_variant_t *v, unsigned levels,
                     const param_set_t *lm_type, const param_set_t *lm_ots_type)
{
    if (levels < MIN_HSS_LEVELS || levels > MAX_HSS_LEVELS) return -1;
    v->levels = levels;
    for (unsigned i = 0; i < levels; i++) {
        v->lm_type[i] = lm_type[i];
        v->lm_ots_type[i] = lm_ots_type[i];
    }
    return 0;
}

size_t hss_variant_pk_bytes(const hss_variant_t *v)
{
    return hss_get_public_key_len(v->levels, v->lm_type, v->lm_ots_type);
}

size_t hss_variant_sk_bytes(const hss_variant_t *v)
{
    return hss_get_private_key_len(v->levels, v->lm_type, v->lm_ots_type);
}

size_t hss_variant_sig_bytes(const hss_variant_t *v)
{
    return hss_get_signature_len(v->levels, v->lm_type, v->lm_ots_type);
}

/* Serves bytes from a caller-supplied buffer instead of a real RNG --
 * hss_generate_private_key() calls this exactly once, for
 * PRIVATE_KEY_SEED_LEN (48) bytes. */
static const uint8_t *g_rnd_buf;
static size_t g_rnd_pos;

static bool shim_rbg(void *output, size_t length)
{
    memcpy(output, g_rnd_buf + g_rnd_pos, length);
    g_rnd_pos += length;
    return true;
}

int hss_variant_keypair_seed(const hss_variant_t *v, uint8_t *pk, uint8_t *sk,
                             const uint8_t *seed)
{
    g_rnd_buf = seed;
    g_rnd_pos = 0;
    size_t pkb = hss_variant_pk_bytes(v);
    bool ok = hss_generate_private_key(shim_rbg, v->levels, v->lm_type, v->lm_ots_type,
                                       NULL, sk, pk, pkb, NULL, 0, NULL);
    return ok ? 0 : -1;
}

int hss_variant_sign(const hss_variant_t *v, uint8_t *sk,
                     uint8_t *sig, size_t *siglen,
                     const uint8_t *m, size_t mlen)
{
    (void)v;
    struct hss_working_key *wk = hss_load_private_key(NULL, sk, 0, NULL, 0, NULL);
    if (!wk) return -1;

    size_t sig_cap = hss_variant_sig_bytes(v);
    bool ok = hss_generate_signature(wk, NULL, sk, m, mlen, sig, sig_cap, NULL);
    hss_free_working_key(wk);
    if (!ok) return -1;

    *siglen = sig_cap;
    return 0;
}

int hss_variant_verify(const uint8_t *m, size_t mlen,
                       const uint8_t *sig, size_t siglen,
                       const uint8_t *pk)
{
    return hss_validate_signature(pk, m, mlen, sig, siglen, NULL) ? 0 : -1;
}

/* ---- named variant tables (see hss_variant.h) ---- */

const hss_named_variant_t lms_variant_default = {
    "LMS_H10_W8", 1, { LMS_SHA256_N32_H10 }, { LMOTS_SHA256_N32_W8 }
};

const hss_named_variant_t lms_variant_fast[8] = {
    { "LMS_H5_W1",  1, { LMS_SHA256_N32_H5 },  { LMOTS_SHA256_N32_W1 } },
    { "LMS_H5_W2",  1, { LMS_SHA256_N32_H5 },  { LMOTS_SHA256_N32_W2 } },
    { "LMS_H5_W4",  1, { LMS_SHA256_N32_H5 },  { LMOTS_SHA256_N32_W4 } },
    { "LMS_H5_W8",  1, { LMS_SHA256_N32_H5 },  { LMOTS_SHA256_N32_W8 } },
    { "LMS_H10_W1", 1, { LMS_SHA256_N32_H10 }, { LMOTS_SHA256_N32_W1 } },
    { "LMS_H10_W2", 1, { LMS_SHA256_N32_H10 }, { LMOTS_SHA256_N32_W2 } },
    { "LMS_H10_W4", 1, { LMS_SHA256_N32_H10 }, { LMOTS_SHA256_N32_W4 } },
    { "LMS_H10_W8", 1, { LMS_SHA256_N32_H10 }, { LMOTS_SHA256_N32_W8 } },
};
const int lms_variant_fast_count = 8;

const hss_named_variant_t lms_variant_slow[12] = {
    { "LMS_H15_W1", 1, { LMS_SHA256_N32_H15 }, { LMOTS_SHA256_N32_W1 } },
    { "LMS_H15_W2", 1, { LMS_SHA256_N32_H15 }, { LMOTS_SHA256_N32_W2 } },
    { "LMS_H15_W4", 1, { LMS_SHA256_N32_H15 }, { LMOTS_SHA256_N32_W4 } },
    { "LMS_H15_W8", 1, { LMS_SHA256_N32_H15 }, { LMOTS_SHA256_N32_W8 } },
    { "LMS_H20_W1", 1, { LMS_SHA256_N32_H20 }, { LMOTS_SHA256_N32_W1 } },
    { "LMS_H20_W2", 1, { LMS_SHA256_N32_H20 }, { LMOTS_SHA256_N32_W2 } },
    { "LMS_H20_W4", 1, { LMS_SHA256_N32_H20 }, { LMOTS_SHA256_N32_W4 } },
    { "LMS_H20_W8", 1, { LMS_SHA256_N32_H20 }, { LMOTS_SHA256_N32_W8 } },
    { "LMS_H25_W1", 1, { LMS_SHA256_N32_H25 }, { LMOTS_SHA256_N32_W1 } },
    { "LMS_H25_W2", 1, { LMS_SHA256_N32_H25 }, { LMOTS_SHA256_N32_W2 } },
    { "LMS_H25_W4", 1, { LMS_SHA256_N32_H25 }, { LMOTS_SHA256_N32_W4 } },
    { "LMS_H25_W8", 1, { LMS_SHA256_N32_H25 }, { LMOTS_SHA256_N32_W8 } },
};
const int lms_variant_slow_count = 12;

/* H5 (not H10) so that this stays cheap enough for the default KATNUM=100
 * used by `make test-all` -- 2-level H10 keygen+sign+verify measured at
 * ~10s/vector (~16+ minutes at KATNUM=100); 2-level H5 measured at ~0.65s. */
const hss_named_variant_t hss_variant_default = {
    "HSS_L2_H5_W8", 2,
    { LMS_SHA256_N32_H5, LMS_SHA256_N32_H5 },
    { LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8 }
};

const hss_named_variant_t hss_variant_fast[16] = {
    { "HSS_L2_H5_W1",  2, { LMS_SHA256_N32_H5, LMS_SHA256_N32_H5 },  { LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1 } },
    { "HSS_L2_H5_W2",  2, { LMS_SHA256_N32_H5, LMS_SHA256_N32_H5 },  { LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2 } },
    { "HSS_L2_H5_W4",  2, { LMS_SHA256_N32_H5, LMS_SHA256_N32_H5 },  { LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4 } },
    { "HSS_L2_H5_W8",  2, { LMS_SHA256_N32_H5, LMS_SHA256_N32_H5 },  { LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8 } },
    { "HSS_L2_H10_W1", 2, { LMS_SHA256_N32_H10, LMS_SHA256_N32_H10 }, { LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1 } },
    { "HSS_L2_H10_W2", 2, { LMS_SHA256_N32_H10, LMS_SHA256_N32_H10 }, { LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2 } },
    { "HSS_L2_H10_W4", 2, { LMS_SHA256_N32_H10, LMS_SHA256_N32_H10 }, { LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4 } },
    { "HSS_L2_H10_W8", 2, { LMS_SHA256_N32_H10, LMS_SHA256_N32_H10 }, { LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8 } },
    { "HSS_L3_H5_W1",  3, { LMS_SHA256_N32_H5, LMS_SHA256_N32_H5, LMS_SHA256_N32_H5 },  { LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1 } },
    { "HSS_L3_H5_W2",  3, { LMS_SHA256_N32_H5, LMS_SHA256_N32_H5, LMS_SHA256_N32_H5 },  { LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2 } },
    { "HSS_L3_H5_W4",  3, { LMS_SHA256_N32_H5, LMS_SHA256_N32_H5, LMS_SHA256_N32_H5 },  { LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4 } },
    { "HSS_L3_H5_W8",  3, { LMS_SHA256_N32_H5, LMS_SHA256_N32_H5, LMS_SHA256_N32_H5 },  { LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8 } },
    { "HSS_L3_H10_W1", 3, { LMS_SHA256_N32_H10, LMS_SHA256_N32_H10, LMS_SHA256_N32_H10 }, { LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1 } },
    { "HSS_L3_H10_W2", 3, { LMS_SHA256_N32_H10, LMS_SHA256_N32_H10, LMS_SHA256_N32_H10 }, { LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2 } },
    { "HSS_L3_H10_W4", 3, { LMS_SHA256_N32_H10, LMS_SHA256_N32_H10, LMS_SHA256_N32_H10 }, { LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4 } },
    { "HSS_L3_H10_W8", 3, { LMS_SHA256_N32_H10, LMS_SHA256_N32_H10, LMS_SHA256_N32_H10 }, { LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8 } },
};
const int hss_variant_fast_count = 16;

const hss_named_variant_t hss_variant_slow[24] = {
    { "HSS_L2_H15_W1", 2, { LMS_SHA256_N32_H15, LMS_SHA256_N32_H15 }, { LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1 } },
    { "HSS_L2_H15_W2", 2, { LMS_SHA256_N32_H15, LMS_SHA256_N32_H15 }, { LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2 } },
    { "HSS_L2_H15_W4", 2, { LMS_SHA256_N32_H15, LMS_SHA256_N32_H15 }, { LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4 } },
    { "HSS_L2_H15_W8", 2, { LMS_SHA256_N32_H15, LMS_SHA256_N32_H15 }, { LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8 } },
    { "HSS_L2_H20_W1", 2, { LMS_SHA256_N32_H20, LMS_SHA256_N32_H20 }, { LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1 } },
    { "HSS_L2_H20_W2", 2, { LMS_SHA256_N32_H20, LMS_SHA256_N32_H20 }, { LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2 } },
    { "HSS_L2_H20_W4", 2, { LMS_SHA256_N32_H20, LMS_SHA256_N32_H20 }, { LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4 } },
    { "HSS_L2_H20_W8", 2, { LMS_SHA256_N32_H20, LMS_SHA256_N32_H20 }, { LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8 } },
    { "HSS_L2_H25_W1", 2, { LMS_SHA256_N32_H25, LMS_SHA256_N32_H25 }, { LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1 } },
    { "HSS_L2_H25_W2", 2, { LMS_SHA256_N32_H25, LMS_SHA256_N32_H25 }, { LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2 } },
    { "HSS_L2_H25_W4", 2, { LMS_SHA256_N32_H25, LMS_SHA256_N32_H25 }, { LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4 } },
    { "HSS_L2_H25_W8", 2, { LMS_SHA256_N32_H25, LMS_SHA256_N32_H25 }, { LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8 } },
    { "HSS_L3_H15_W1", 3, { LMS_SHA256_N32_H15, LMS_SHA256_N32_H15, LMS_SHA256_N32_H15 }, { LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1 } },
    { "HSS_L3_H15_W2", 3, { LMS_SHA256_N32_H15, LMS_SHA256_N32_H15, LMS_SHA256_N32_H15 }, { LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2 } },
    { "HSS_L3_H15_W4", 3, { LMS_SHA256_N32_H15, LMS_SHA256_N32_H15, LMS_SHA256_N32_H15 }, { LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4 } },
    { "HSS_L3_H15_W8", 3, { LMS_SHA256_N32_H15, LMS_SHA256_N32_H15, LMS_SHA256_N32_H15 }, { LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8 } },
    { "HSS_L3_H20_W1", 3, { LMS_SHA256_N32_H20, LMS_SHA256_N32_H20, LMS_SHA256_N32_H20 }, { LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1 } },
    { "HSS_L3_H20_W2", 3, { LMS_SHA256_N32_H20, LMS_SHA256_N32_H20, LMS_SHA256_N32_H20 }, { LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2 } },
    { "HSS_L3_H20_W4", 3, { LMS_SHA256_N32_H20, LMS_SHA256_N32_H20, LMS_SHA256_N32_H20 }, { LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4 } },
    { "HSS_L3_H20_W8", 3, { LMS_SHA256_N32_H20, LMS_SHA256_N32_H20, LMS_SHA256_N32_H20 }, { LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8 } },
    { "HSS_L3_H25_W1", 3, { LMS_SHA256_N32_H25, LMS_SHA256_N32_H25, LMS_SHA256_N32_H25 }, { LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1, LMOTS_SHA256_N32_W1 } },
    { "HSS_L3_H25_W2", 3, { LMS_SHA256_N32_H25, LMS_SHA256_N32_H25, LMS_SHA256_N32_H25 }, { LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2, LMOTS_SHA256_N32_W2 } },
    { "HSS_L3_H25_W4", 3, { LMS_SHA256_N32_H25, LMS_SHA256_N32_H25, LMS_SHA256_N32_H25 }, { LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4, LMOTS_SHA256_N32_W4 } },
    { "HSS_L3_H25_W8", 3, { LMS_SHA256_N32_H25, LMS_SHA256_N32_H25, LMS_SHA256_N32_H25 }, { LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8, LMOTS_SHA256_N32_W8 } },
};
const int hss_variant_slow_count = 24;
