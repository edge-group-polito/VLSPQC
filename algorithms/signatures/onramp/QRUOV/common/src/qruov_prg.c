// SPDX-License-Identifier: Apache-2.0
#include <stdlib.h>
#include <string.h>
#include "qruov_prg.h"

/* =====================================================================
   RejSamp -- moved here verbatim from upstream qruov.c (the only callers,
   RejSampPRG_aes_ctr / RejSampPRG_shake / Hash, all live in this file too).
   ===================================================================== */

static inline void rejsamp_and_q(const unsigned int tau, uint8_t *dst) {
  for (unsigned int i = 0; i < tau; i++) { dst[i] &= QRUOV_q; }
}

static inline void rejsamp_rejection_with_aux(
  const unsigned int length, const uint8_t *aux, uint8_t *dst
) {
  while (*aux == QRUOV_q) aux++;
  for (unsigned int i = 0; i < length; i++) {
    if (*dst == QRUOV_q) {
      *dst = *aux++;
      while (*aux == QRUOV_q) aux++;
    }
    *dst++;
  }
}

static inline void RejSamp(
  const unsigned int length, const unsigned int tau, uint8_t *dst
) {
  rejsamp_and_q(tau, dst);
  rejsamp_rejection_with_aux(length, dst + length, dst);
}

/* =====================================================================
   128-bit big-endian counter helpers (shared AES-CTR plumbing)
   ===================================================================== */

static inline void ctr_inc(uint8_t ctr[16]) {
  for (int i = 15; i >= 0; i--) {
    if (++ctr[i]) break;
  }
}

/* Set ctr[16] to the big-endian 128-bit value (index << 64), matching
 * upstream's `save64(index, iv)` (index occupies the *upper* 64 bits, low
 * 64 bits start at zero -- see qruov.c's RejSampPRG_aes_ctr). */
static inline void ctr_set_index(uint8_t ctr[16], uint64_t index) {
  for (int i = 0; i < 8; i++) ctr[i] = (uint8_t)(index >> (8 * (7 - i)));
  memset(ctr + 8, 0, 8);
}

/* =====================================================================
   AES-CTR PRG
   ===================================================================== */

PRG_CTX_aes_p PRG_init_aes_ctr(const uint8_t seed[QRUOV_SEED_LEN]) {
  PRG_CTX_aes_p ctx = malloc(sizeof(PRG_CTX_aes));
  memcpy(ctx->seed, seed, QRUOV_SEED_LEN);
  QRUOV_AES_ECB_KEYEXP(&ctx->aes, seed);
  memset(ctx->counter, 0, 16);
  ctx->buffer_pos = 16; /* empty: force a refill on first yield */
  return ctx;
}

void PRG_yield_aes_ctr(PRG_CTX_aes ctx, int length, uint8_t *dst) {
  while (length > 0) {
    if (ctx->buffer_pos == 16) {
      QRUOV_AES_ECB(ctx->buffer, ctx->counter, 1, &ctx->aes);
      ctr_inc(ctx->counter);
      ctx->buffer_pos = 0;
    }
    int take = 16 - ctx->buffer_pos;
    if (take > length) take = length;
    memcpy(dst, ctx->buffer + ctx->buffer_pos, (size_t)take);
    ctx->buffer_pos += take;
    dst    += take;
    length -= take;
  }
}

void PRG_final_aes_ctr(PRG_CTX_aes ctx) {
  QRUOV_AES_CTX_RELEASE(&ctx->aes);
  free(ctx);
}

PRG_CTX_aes_p PRG_copy_aes_ctr(PRG_CTX_aes src) {
  /* QRUOV_AESCTX.sk_exp is heap-allocated -- re-derive the key schedule
   * rather than aliasing the pointer via a raw struct memcpy (which would
   * double-free once either copy is released). */
  PRG_CTX_aes_p dst = malloc(sizeof(PRG_CTX_aes));
  memcpy(dst, src, sizeof(PRG_CTX_aes));
  QRUOV_AES_ECB_KEYEXP(&dst->aes, src->seed);
  return dst;
}

void RejSampPRG_aes_ctr(
  PRG_CTX_aes ctx, const uint64_t index,
  const unsigned int length, const unsigned int tau, uint8_t *dst
) {
  size_t nblocks = ((size_t)tau + 15) / 16;
  uint8_t *blocks = malloc(nblocks * 16);
  uint8_t  ctr[16];
  ctr_set_index(ctr, index);
  for (size_t i = 0; i < nblocks; i++) {
    memcpy(blocks + i * 16, ctr, 16);
    ctr_inc(ctr);
  }
  uint8_t *out = malloc(nblocks * 16);
  QRUOV_AES_ECB(out, blocks, nblocks, &ctx->aes);
  memcpy(dst, out, tau);
  free(blocks);
  free(out);
  RejSamp(length, tau, dst);
}

/* =====================================================================
   SHAKE one-shot PRG
   ===================================================================== */

PRG_CTX_shake_p PRG_init_shake(const uint8_t seed[QRUOV_SEED_LEN]) {
  PRG_CTX_shake_p ctx = malloc(sizeof(PRG_CTX_shake));
  QRUOV_SHAKE_INIT(&ctx->ctx);
  QRUOV_SHAKE_ABSORB(&ctx->ctx, seed, QRUOV_SEED_LEN);
  return ctx;
}

/* May be called just once per context (matches upstream's own contract --
 * see qruov.c's comment on PRG_yield_shake): finalizes then squeezes. */
void PRG_yield_shake(PRG_CTX_shake ctx, int length, uint8_t *dst) {
  QRUOV_SHAKE_FINALIZE(&ctx->ctx);
  QRUOV_SHAKE_SQUEEZE(dst, (size_t)length, &ctx->ctx);
}

void PRG_final_shake(PRG_CTX_shake ctx) {
  free(ctx);
}

PRG_CTX_shake_p PRG_copy_shake(PRG_CTX_shake src) {
  /* keccak_state is plain POD (uint64_t[25] + pos), no heap pointers --
   * unlike the AES case, a raw memcpy is a perfectly safe deep copy. */
  PRG_CTX_shake_p dst = malloc(sizeof(PRG_CTX_shake));
  memcpy(dst, src, sizeof(PRG_CTX_shake));
  return dst;
}

void RejSampPRG_shake(
  PRG_CTX_shake ctx, const uint64_t index,
  const unsigned int length, const unsigned int tau, uint8_t *dst
) {
  uint8_t iv[2];
  iv[0] = (uint8_t)(index >> 8);
  iv[1] = (uint8_t)(index & 0xFF);
  QRUOV_SHAKE_ABSORB(&ctx->ctx, iv, 2);
  QRUOV_SHAKE_FINALIZE(&ctx->ctx);
  QRUOV_SHAKE_SQUEEZE(dst, tau, &ctx->ctx);
  RejSamp(length, tau, dst);
}

/* =====================================================================
   PRG2 in SHAKE mode == the streaming MGF directly (moved here verbatim
   from upstream qruov.c, see qruov_prg.h)
   ===================================================================== */

MGF_CTX_s * PRG_init_mgf(const uint8_t seed[QRUOV_SEED_LEN]) {
  return MGF_init(seed, QRUOV_SEED_LEN, malloc(sizeof(MGF_CTX)));
}
void PRG_yield_mgf(MGF_CTX_s * ctx, int length, uint8_t * dst) {
  MGF_yield(ctx, dst, (size_t)length);
}
void PRG_final_mgf(MGF_CTX_s * ctx) {
  MGF_final(ctx);
  free(ctx);
}
MGF_CTX_s * PRG_copy_mgf(MGF_CTX_s * src) {
  MGF_CTX_s * dst = malloc(sizeof(MGF_CTX));
  MGF_CTX_copy(src, dst);
  return dst;
}

/* =====================================================================
   Expand_mu(), Hash() -- shake256 only, matches upstream
   ===================================================================== */

void Expand_mu(
  const uint8_t seed_pk[QRUOV_SEED_LEN],
  const uint8_t message[],
  const size_t  message_length,
  uint8_t       mu[QRUOV_MU_LEN]
) {
  keccak_state ctx;
  shake256_init(&ctx);
  shake256_absorb(&ctx, seed_pk, QRUOV_SEED_LEN);
  shake256_absorb(&ctx, message, message_length);
  shake256_finalize(&ctx);
  shake256_squeeze(mu, QRUOV_MU_LEN, &ctx);
}

void Hash(
  const uint8_t mu  [QRUOV_MU_LEN],
  const uint8_t salt[QRUOV_SEED_LEN],
  uint8_t       dst [QRUOV_m]
) {
  uint8_t tmp[QRUOV_tau4];
  keccak_state ctx;
  shake256_init(&ctx);
  shake256_absorb(&ctx, mu, QRUOV_MU_LEN);
  shake256_absorb(&ctx, salt, QRUOV_SALT_LEN);
  shake256_finalize(&ctx);
  shake256_squeeze(tmp, QRUOV_tau4, &ctx);
  RejSamp(QRUOV_m, QRUOV_tau4, tmp);
  memcpy(dst, tmp, QRUOV_m);
}
