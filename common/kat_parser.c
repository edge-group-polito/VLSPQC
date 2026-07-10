/*
 * kat_parser.c
 * Parser and writer for NIST PQC KAT .rsp files.
 */
#include "kat_parser.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* ---------- internal helpers ---------- */

static int hex_char(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode hex string (even number of chars) into *out.
 * Returns number of bytes written, or -1 on error. */
static long hex_decode(const char *hex, uint8_t *out, size_t max_out)
{
    size_t n = strlen(hex);
    if (n % 2 != 0) return -1;
    size_t bytes = n / 2;
    if (bytes > max_out) return -1;
    for (size_t i = 0; i < bytes; i++) {
        int hi = hex_char(hex[2*i]);
        int lo = hex_char(hex[2*i+1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return (long)bytes;
}

/* Trim trailing whitespace in place */
static void rtrim(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n-1]))
        s[--n] = '\0';
}

/* ---------- public API ---------- */

int kat_parser_open(kat_parser_t *p, const char *filename)
{
    memset(p, 0, sizeof(*p));
    p->fp = fopen(filename, "r");
    if (!p->fp) {
        fprintf(stderr, "kat_parser: cannot open '%s': %s\n",
                filename, strerror(errno));
        return -1;
    }
    /* Read optional first comment line "# <algo name>" */
    char line[256];
    long pos = ftell(p->fp);
    if (fgets(line, sizeof(line), p->fp)) {
        rtrim(line);
        if (line[0] == '#') {
            const char *name = line + 1;
            while (*name == ' ') name++;
            size_t alen = strlen(name);
            if (alen >= sizeof(p->algo_name)) alen = sizeof(p->algo_name) - 1;
            memcpy(p->algo_name, name, alen);
            p->algo_name[alen] = '\0';
        } else {
            fseek(p->fp, pos, SEEK_SET);  /* put back */
        }
    }
    return 0;
}

void kat_parser_close(kat_parser_t *p)
{
    if (p->fp) { fclose(p->fp); p->fp = NULL; }
}

void kat_entry_free(kat_entry_t *entry)
{
    for (int i = 0; i < entry->num_fields; i++) {
        free(entry->fields[i].data);
        entry->fields[i].data = NULL;
    }
    memset(entry, 0, sizeof(*entry));
}

int kat_parser_next(kat_parser_t *p, kat_entry_t *entry)
{
    memset(entry, 0, sizeof(*entry));
    /* Heap-allocate: KAT_DATA_MAX * 2 hex chars + field name + " = " + newline */
    size_t line_cap = KAT_DATA_MAX * 2 + 64;
    char *line = (char *)malloc(line_cap);
    if (!line) { fprintf(stderr, "kat_parser: out of memory for line buffer\n"); return -1; }
    int  got_any = 0;

    while (fgets(line, (int)line_cap, p->fp)) {
        rtrim(line);

        /* Skip comment lines */
        if (line[0] == '#') continue;

        /* Blank line after at least one field → end of this entry */
        if (line[0] == '\0') {
            if (got_any) break;
            continue;
        }

        /* Parse "name = value" */
        char *eq = strchr(line, '=');
        if (!eq) continue;

        char name[KAT_NAME_MAX];
        size_t nlen = (size_t)(eq - line);
        while (nlen > 0 && isspace((unsigned char)line[nlen-1])) nlen--;
        if (nlen == 0 || nlen >= KAT_NAME_MAX) continue;
        memcpy(name, line, nlen);
        name[nlen] = '\0';

        const char *val = eq + 1;
        while (*val == ' ' || *val == '\t') val++;

        if (entry->num_fields >= KAT_MAX_FIELDS) {
            fprintf(stderr, "kat_parser: too many fields in entry\n");
            free(line);
            return -1;
        }

        kat_field_t *f = &entry->fields[entry->num_fields];
        snprintf(f->name, KAT_NAME_MAX, "%s", name);

        /* Check if the value is purely decimal (integer field) */
        int is_decimal = 1;
        for (const char *c = val; *c; c++) {
            if (!isdigit((unsigned char)*c)) { is_decimal = 0; break; }
        }

        if (is_decimal && strlen(val) > 0 && strlen(val) <= 18) {
            f->is_int = 1;
            f->ival   = atol(val);
            f->data   = NULL;
            f->len    = 0;
        } else {
            /* Hex field */
            size_t hexlen = strlen(val);
            if (hexlen % 2 != 0 || hexlen > KAT_DATA_MAX * 2) {
                /* Could be empty (allowed in .req files) */
                if (hexlen == 0) {
                    f->data = NULL;
                    f->len  = 0;
                    f->is_int = 0;
                } else {
                    fprintf(stderr,
                        "kat_parser: bad hex length %zu for field '%s'\n",
                        hexlen, name);
                    free(line);
                    return -1;
                }
            } else {
                size_t bytes = hexlen / 2;
                f->data = (uint8_t *)malloc(bytes);
                if (!f->data) { free(line); return -1; }
                long n = hex_decode(val, f->data, bytes);
                if (n < 0) {
                    fprintf(stderr,
                        "kat_parser: bad hex data for field '%s'\n", name);
                    free(f->data); f->data = NULL;
                    free(line);
                    return -1;
                }
                f->len = (size_t)n;
            }
        }

        /* Update convenience field */
        if (strcmp(name, "count") == 0 && f->is_int)
            entry->count = (int)f->ival;

        entry->num_fields++;
        got_any = 1;
    }

    free(line);
    if (!got_any) return 1;  /* EOF */
    return 0;
}

kat_field_t *kat_entry_get(kat_entry_t *entry, const char *name)
{
    for (int i = 0; i < entry->num_fields; i++) {
        if (strcmp(entry->fields[i].name, name) == 0)
            return &entry->fields[i];
    }
    return NULL;
}

int kat_get_bytes(kat_entry_t *entry, const char *name,
                  uint8_t *out, size_t expected_len)
{
    kat_field_t *f = kat_entry_get(entry, name);
    if (!f || f->is_int || !f->data) {
        fprintf(stderr, "kat_parser: field '%s' missing or not bytes\n", name);
        return -1;
    }
    if (expected_len != 0 && f->len != expected_len) {
        fprintf(stderr,
            "kat_parser: field '%s' length %zu, expected %zu\n",
            name, f->len, expected_len);
        return -1;
    }
    memcpy(out, f->data, f->len);
    return 0;
}

int kat_get_long(kat_entry_t *entry, const char *name, long *out)
{
    kat_field_t *f = kat_entry_get(entry, name);
    if (!f || !f->is_int) {
        fprintf(stderr, "kat_parser: integer field '%s' missing\n", name);
        return -1;
    }
    *out = f->ival;
    return 0;
}

/* ---------- write helpers ---------- */

void kat_fprint_comment(FILE *fp, const char *algo_name)
{
    fprintf(fp, "# %s\n\n", algo_name);
}

void kat_fprint_int(FILE *fp, const char *name, long value)
{
    fprintf(fp, "%s = %ld\n", name, value);
}

void kat_fprint_hex(FILE *fp, const char *name,
                    const uint8_t *data, size_t len)
{
    fprintf(fp, "%s = ", name);
    for (size_t i = 0; i < len; i++)
        fprintf(fp, "%02X", data[i]);
    fprintf(fp, "\n");
}
