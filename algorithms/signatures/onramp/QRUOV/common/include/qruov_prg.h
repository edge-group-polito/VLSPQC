// SPDX-License-Identifier: Apache-2.0
//
// Self-contained replacement for qruov.c's OpenSSL-based PRG_init_aes_ctr /
// PRG_init_shake / RejSampPRG_* / Expand_mu / Hash functions (originally
// static functions inside qruov.c, see upstream qruov.c lines ~143-247).
// Built on MAYO-C's portable AES (qruov_aes.h/aes_c.c) and a portable
// fips202-derived SHAKE (fips202.h/.c) instead of OpenSSL EVP.
//
// AES-CTR mode replicates the semantics of OpenSSL's EVP_aes_*_ctr as used
// upstream: a 128-bit big-endian counter, RejSampPRG resets it to a
// specific `index`-derived starting value and produces `tau` bytes fresh,
// while the streaming PRG2 (Sign()'s salt-retry loop in AES mode) advances
// the counter across calls. Both are implemented as ECB-encryption of
// consecutive big-endian counter blocks (CTR-mode-of-zeros == raw
// keystream), matching upstream's own `memset(dst,0,len)` +
// EVP_EncryptUpdate(...) trick.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "qruov_misc.h"
#include "qruov_tau.h"
#include "qruov_aes.h"
#include "fips202.h"

/* Self-contained replacement for OPENSSL_cleanse(): a secure (not
 * optimized-away) memory wipe. Same volatile-function-pointer trick used
 * by MAYO-C's mayo_secure_clear() (src/common/mem.c). */
static inline void QRUOV_cleanse(void *mem, size_t size) {
  typedef void *(*memset_t)(void *, int, size_t);
  static volatile memset_t memset_func = memset;
  memset_func(mem, 0, size);
#if defined(__GNUC__) || defined(__clang__)
  __asm__ volatile("" : : "r"(mem) : "memory");
#endif
}
#define OPENSSL_cleanse(p, n) QRUOV_cleanse((p), (n))

/* =====================================================================
   AES-CTR PRG (QRUOV_security_strength_category selects AES-128/192/256,
   matching upstream's EVP_AES_CTR selection)
   ===================================================================== */

#if   (QRUOV_security_strength_category == 1)
typedef aes128ctx QRUOV_AESCTX;
#  define QRUOV_AES_ECB_KEYEXP  aes128_ecb_keyexp
#  define QRUOV_AES_ECB         aes128_ecb
#  define QRUOV_AES_CTX_RELEASE aes128_ctx_release
#elif (QRUOV_security_strength_category == 3)
typedef aes192ctx QRUOV_AESCTX;
#  define QRUOV_AES_ECB_KEYEXP  aes192_ecb_keyexp
#  define QRUOV_AES_ECB         aes192_ecb
#  define QRUOV_AES_CTX_RELEASE aes192_ctx_release
#elif (QRUOV_security_strength_category == 5)
typedef aes256ctx QRUOV_AESCTX;
#  define QRUOV_AES_ECB_KEYEXP  aes256_ecb_keyexp
#  define QRUOV_AES_ECB         aes256_ecb
#  define QRUOV_AES_CTX_RELEASE aes256_ctx_release
#else
#  error "illegal QRUOV_security_strength_category"
#endif

TYPEDEF_STRUCT(PRG_CTX_aes,
  QRUOV_AESCTX aes                    ; // expanded round key
  uint8_t      seed [QRUOV_SEED_LEN]  ; // kept so PRG_copy can re-keyexp safely
                                        // (QRUOV_AESCTX.sk_exp is heap-allocated;
                                        // a raw memcpy would alias and double-free)
  uint8_t      counter [16]           ; // streaming (PRG_yield) position
  uint8_t      buffer  [16]           ; // buffered keystream block
  int          buffer_pos             ; // bytes already consumed (16 == empty)
) ;

extern PRG_CTX_aes_p PRG_init_aes_ctr (const uint8_t seed[QRUOV_SEED_LEN]) ;
extern void          PRG_yield_aes_ctr(PRG_CTX_aes ctx, int length, uint8_t * dst) ;
extern void          PRG_final_aes_ctr(PRG_CTX_aes ctx) ;
extern PRG_CTX_aes_p PRG_copy_aes_ctr (PRG_CTX_aes src) ;

extern void RejSampPRG_aes_ctr(
  PRG_CTX_aes        ctx,
  const uint64_t     index,
  const unsigned int length,
  const unsigned int tau,
        uint8_t *    dst
) ;

/* =====================================================================
   SHAKE one-shot PRG (QRUOV_SEED_LEN selects SHAKE128/256, matching
   upstream's EVP_SHAKE selection)
   ===================================================================== */

#if QRUOV_SEED_LEN == 16
#  define QRUOV_SHAKE_INIT         shake128_init
#  define QRUOV_SHAKE_ABSORB       shake128_absorb
#  define QRUOV_SHAKE_FINALIZE     shake128_finalize
#  define QRUOV_SHAKE_SQUEEZE      shake128_squeeze
#else
#  define QRUOV_SHAKE_INIT         shake256_init
#  define QRUOV_SHAKE_ABSORB       shake256_absorb
#  define QRUOV_SHAKE_FINALIZE     shake256_finalize
#  define QRUOV_SHAKE_SQUEEZE      shake256_squeeze
#endif

TYPEDEF_STRUCT(PRG_CTX_shake,
  keccak_state ctx ;
) ;

extern PRG_CTX_shake_p PRG_init_shake (const uint8_t seed[QRUOV_SEED_LEN]) ;
extern void            PRG_yield_shake(PRG_CTX_shake ctx, int length, uint8_t * dst) ;
extern void            PRG_final_shake(PRG_CTX_shake ctx) ;
extern PRG_CTX_shake_p PRG_copy_shake (PRG_CTX_shake src) ;

extern void RejSampPRG_shake(
  PRG_CTX_shake      ctx,
  const uint64_t     index,
  const unsigned int length,
  const unsigned int tau,
        uint8_t *    dst
) ;

/* =====================================================================
   PRG2 in SHAKE mode == the streaming MGF (mgf.h) directly; thin glue from
   the MGF_* API to the PRG2_* name table qruov.h switches on -- moved here
   verbatim from upstream qruov.c (originally defined inline next to
   PRG_init_shake et al).
   ===================================================================== */

#include "mgf.h"

extern MGF_CTX_s * PRG_init_mgf (const uint8_t seed[QRUOV_SEED_LEN]) ;
extern void        PRG_yield_mgf(MGF_CTX_s * ctx, int length, uint8_t * dst) ;
extern void        PRG_final_mgf(MGF_CTX_s * ctx) ;
extern MGF_CTX_s * PRG_copy_mgf (MGF_CTX_s * src) ;

/* =====================================================================
   Expand_mu(), Hash() -- shake256 only, matches upstream
   ===================================================================== */

extern void Expand_mu(
  const uint8_t seed_pk[QRUOV_SEED_LEN],
  const uint8_t message[],
  const size_t  message_length,
  uint8_t       mu[QRUOV_MU_LEN]
) ;

extern void Hash(
  const uint8_t mu  [QRUOV_MU_LEN],
  const uint8_t salt[QRUOV_SEED_LEN],
  uint8_t       dst [QRUOV_m]
) ;
