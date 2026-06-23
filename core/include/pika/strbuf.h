/*
 * strbuf — a tiny growable byte buffer.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PIKA_STRBUF_H
#define PIKA_STRBUF_H

#include <stddef.h>
#include <stdio.h>

typedef struct {
    char  *data; /* NUL-terminated; never NULL after first append */
    size_t len;  /* bytes used, excluding the NUL */
    size_t cap;  /* bytes allocated */
} strbuf;

void   sb_init(strbuf *sb);
void   sb_free(strbuf *sb);
void   sb_reset(strbuf *sb);

/* All appends abort() on allocation failure — callers stay simple. */
void   sb_putc(strbuf *sb, char c);
void   sb_append(strbuf *sb, const char *s, size_t n);
void   sb_puts(strbuf *sb, const char *s);
void   sb_printf(strbuf *sb, const char *fmt, ...);

/* Detach the owned buffer; caller must free(). Buffer is re-initialised. */
char  *sb_detach(strbuf *sb, size_t *len_out);

/* Read an entire stream into sb (appended). Returns 0 on success. */
int    sb_read_stream(strbuf *sb, FILE *fp, size_t limit);
/* Read a whole file by path. Returns 0 on success, -1 on error. */
int    sb_read_file(strbuf *sb, const char *path, size_t limit);

#endif /* PIKA_STRBUF_H */
