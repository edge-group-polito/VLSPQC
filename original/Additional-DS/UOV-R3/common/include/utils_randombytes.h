// SPDX-License-Identifier: CC0 OR Apache-2.0
/// @file utils_randombytes.h
/// @brief wrappers for randombytes().
///
///

#ifndef _UTILS_RANDOMBYTES_H_
#define _UTILS_RANDOMBYTES_H_


#ifdef  __cplusplus
extern  "C" {
#endif


#include "config.h"
#include "params.h"


#if defined(_UTILS_SUPERCOP_)||defined(_UTILS_PQM4_)

#include "randombytes.h"

/* Originally gated on _NIST_KAT_ && _UTILS_OPENSSL_ (rng.h's AES256_ECB used
 * OpenSSL). rng.h now ships a self-contained software AES-256 fallback, so
 * this no longer implies an OpenSSL dependency -- decoupled from
 * _UTILS_OPENSSL_ (which independently selects the SHAKE backend, and stays
 * unset by default -- see config.h). */
#elif defined(_NIST_KAT_)

#include "rng.h"

#else
#define randombytes PQOV_NAMESPACE(randombytes)
void randombytes(unsigned char *x, unsigned long long xlen);

#endif


#ifdef  __cplusplus
}
#endif

#endif // _UTILS_RANDOMBYTES_H_


