/*
 * kat_parser.h
 * Parser for NIST PQC KAT response (.rsp) files.
 * Supports all standard and extended field types used across
 * signature schemes and KEMs.
 */
#ifndef KAT_PARSER_H
#define KAT_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Maximum sizes */
#define KAT_MAX_FIELDS     32
#define KAT_NAME_MAX       64
#define KAT_DATA_MAX       (256 * 1024)  /* 256 KB per field (covers LESS pk ~197 KB) */

/* One field in an .rsp entry: either an integer or hex-decoded bytes */
typedef struct {
    char    name[KAT_NAME_MAX];
    uint8_t *data;      /* hex-decoded bytes (NULL if is_int) */
    size_t   len;       /* byte length of data                */
    long     ival;      /* integer value (if is_int != 0)     */
    int      is_int;
} kat_field_t;

/* One test-vector block (between blank lines) */
typedef struct {
    kat_field_t fields[KAT_MAX_FIELDS];
    int         num_fields;
    int         count;     /* convenience: value of "count" field */
} kat_entry_t;

/* Parser context */
typedef struct {
    FILE *fp;
    char  algo_name[128];  /* from first comment line "# <algo>" */
} kat_parser_t;

/*
 * Open the .rsp file.  Returns 0 on success, -1 on error.
 */
int kat_parser_open(kat_parser_t *p, const char *filename);

/*
 * Read the next test-vector block into *entry.
 * Returns  0 – entry filled successfully
 *          1 – end-of-file (no more entries)
 *         -1 – parse error
 * The caller must call kat_entry_free() after each successful call.
 */
int kat_parser_next(kat_parser_t *p, kat_entry_t *entry);

/*
 * Free all heap memory owned by *entry.
 */
void kat_entry_free(kat_entry_t *entry);

/*
 * Close the parser.
 */
void kat_parser_close(kat_parser_t *p);

/*
 * Look up a field by name.  Returns pointer to the field or NULL.
 */
kat_field_t *kat_entry_get(kat_entry_t *entry, const char *name);

/*
 * Convenience: get binary data from a named field.
 * Writes into *out (must be pre-allocated with at least expected_len bytes).
 * Returns 0 on success, -1 if field missing or length mismatch.
 * Pass expected_len=0 to skip length check.
 */
int kat_get_bytes(kat_entry_t *entry, const char *name,
                  uint8_t *out, size_t expected_len);

/*
 * Convenience: get integer value from a named field.
 * Returns 0 on success, -1 if field missing.
 */
int kat_get_long(kat_entry_t *entry, const char *name, long *out);

/*
 * Write helpers used by the KAT generators.
 */
void kat_fprint_comment(FILE *fp, const char *algo_name);
void kat_fprint_int(FILE *fp, const char *name, long value);
void kat_fprint_hex(FILE *fp, const char *name,
                    const uint8_t *data, size_t len);

#endif /* KAT_PARSER_H */
