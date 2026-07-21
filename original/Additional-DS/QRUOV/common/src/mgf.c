// SPDX-License-Identifier: Apache-2.0
//
// Self-contained replacement for upstream's mgf.c -- see mgf.h for why.
#include "mgf.h"

MGF_CTX_s * MGF_init(const uint8_t * seed, const size_t n0, MGF_CTX ctx) {
  QRUOV_MGF_SHAKE_INIT(&ctx->ctx);
  QRUOV_MGF_SHAKE_ABSORB(&ctx->ctx, seed, n0);
  ctx->finalized = 0;
  return ctx;
}

MGF_CTX_s * MGF_update(const uint8_t * seed, const size_t n0, MGF_CTX ctx) {
  QRUOV_MGF_SHAKE_ABSORB(&ctx->ctx, seed, n0);
  return ctx;
}

uint8_t * MGF_yield(MGF_CTX ctx, uint8_t * dest, const size_t n1) {
  if (!ctx->finalized) {
    QRUOV_MGF_SHAKE_FINALIZE(&ctx->ctx);
    ctx->finalized = 1;
  }
  QRUOV_MGF_SHAKE_SQUEEZE(dest, n1, &ctx->ctx);
  return dest;
}

void MGF_final(MGF_CTX ctx) {
  (void)ctx;
  return;
}

void MGF_CTX_copy(MGF_CTX src, MGF_CTX dst) {
  memcpy(dst, src, sizeof(MGF_CTX_s));
  return;
}
