/*
 * openssl_sha_shim.c -- hash.c (vendored from xmss-reference) calls OpenSSL's
 * one-shot SHA256()/SHA512() (from <openssl/sha.h>). Rather than add a
 * libcrypto dependency (this repo otherwise depends on nothing but each
 * algorithm's own source, per tools/README.md), this shim implements the
 * same two entry points on top of the self-contained sha2_256.c/sha2_512.c
 * already vendored here (same code used by DS/SLH-DSA-SHA2's "s" variants).
 */
#include <stddef.h>
#include "sha2_api.h"

unsigned char *SHA256(const unsigned char *d, size_t n, unsigned char *md)
{
    sha2_256(md, d, n);
    return md;
}

unsigned char *SHA512(const unsigned char *d, size_t n, unsigned char *md)
{
    sha2_512(md, d, n);
    return md;
}
