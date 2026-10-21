/* Minimal stand-in for <openssl/sha.h>'s one-shot API -- see
 * src/openssl_sha_shim.c for the self-contained implementation. */
#ifndef XMSS_OPENSSL_SHA_SHIM_H
#define XMSS_OPENSSL_SHA_SHIM_H

#include <stddef.h>

unsigned char *SHA256(const unsigned char *d, size_t n, unsigned char *md);
unsigned char *SHA512(const unsigned char *d, size_t n, unsigned char *md);

#endif
