// SPDX-License-Identifier: Apache-2.0
//
// Self-contained replacement for upstream's mgf.h/mgf.c. Upstream's default
// (non-QRUOV_HASH_LEGACY) MGF reaches directly into OpenSSL's *private*
// EVP_MD_CTX/EVP_MD struct layouts and calls internal (non-public) SHA3_absorb/
// SHA3_squeeze symbols to get a streaming SHAKE XOF -- see the original
// mgf.c for the vendored struct definitions copied from openssl-1.1.1t/
// openssl-3.1.0 internals. Replaced here with the portable fips202-derived
// SHAKE (fips202.h/.c) already used elsewhere in this repo (pqov, MAYO-C
// ports): its keccak_state naturally supports "absorb, finalize once, then
// squeeze repeatedly" -- exactly the MGF_init/update/yield contract below,
// no OpenSSL internals required. Public interface (MGF_CTX, MGF_init,
// MGF_update, MGF_yield, MGF_final, MGF_CTX_copy) is unchanged so qruov.h/
// qruov.c/Fql.h need no further edits beyond what removing OpenSSL already
// required.
#pragma once

#include <inttypes.h>
#include <string.h>
#include "qruov_misc.h"
#include "fips202.h"

#if QRUOV_SEED_LEN == 16
#  define QRUOV_MGF_SHAKE_INIT     shake128_init
#  define QRUOV_MGF_SHAKE_ABSORB   shake128_absorb
#  define QRUOV_MGF_SHAKE_FINALIZE shake128_finalize
#  define QRUOV_MGF_SHAKE_SQUEEZE  shake128_squeeze
#else
#  define QRUOV_MGF_SHAKE_INIT     shake256_init
#  define QRUOV_MGF_SHAKE_ABSORB   shake256_absorb
#  define QRUOV_MGF_SHAKE_FINALIZE shake256_finalize
#  define QRUOV_MGF_SHAKE_SQUEEZE  shake256_squeeze
#endif

TYPEDEF_STRUCT(MGF_CTX,
  keccak_state ctx       ;
  int          finalized ;
) ;

extern MGF_CTX_s * MGF_init  (const uint8_t * seed, const size_t n0, MGF_CTX ctx) ;
extern MGF_CTX_s * MGF_update(const uint8_t * seed, const size_t n0, MGF_CTX ctx) ;
extern uint8_t   * MGF_yield (MGF_CTX ctx, uint8_t * dest, const size_t n1) ;
extern void MGF_final(MGF_CTX ctx) ;
extern void MGF_CTX_copy(MGF_CTX src, MGF_CTX dst) ;
