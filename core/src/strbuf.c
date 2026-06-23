/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pika/strbuf.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static void sb_die(void) {
    fputs("pika: out of memory\n", stderr);
    abort();
}

static void sb_grow(strbuf *sb, size_t need) {
    if (sb->len + need + 1 <= sb->cap) return;
    size_t cap = sb->cap ? sb->cap : 64;
    while (cap < sb->len + need + 1) cap *= 2;
    char *p = realloc(sb->data, cap);
    if (!p) sb_die();
    sb->data = p;
    sb->cap = cap;
}

void sb_init(strbuf *sb) {
    sb->data = NULL;
    sb->len = sb->cap = 0;
    sb_grow(sb, 0);
    sb->data[0] = '\0';
}

void sb_free(strbuf *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = sb->cap = 0;
}

void sb_reset(strbuf *sb) {
    sb->len = 0;
    if (sb->data) sb->data[0] = '\0';
}

void sb_putc(strbuf *sb, char c) {
    sb_grow(sb, 1);
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

void sb_append(strbuf *sb, const char *s, size_t n) {
    if (n == 0) return;
    sb_grow(sb, n);
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void sb_puts(strbuf *sb, const char *s) {
    sb_append(sb, s, strlen(s));
}

void sb_printf(strbuf *sb, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return; }
    sb_grow(sb, (size_t)n);
    vsnprintf(sb->data + sb->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    sb->len += (size_t)n;
}

char *sb_detach(strbuf *sb, size_t *len_out) {
    char *d = sb->data;
    if (len_out) *len_out = sb->len;
    sb->data = NULL;
    sb->len = sb->cap = 0;
    sb_init(sb);
    return d;
}

int sb_read_stream(strbuf *sb, FILE *fp, size_t limit) {
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, fp)) > 0) {
        if (limit && sb->len + n > limit) {
            sb_append(sb, buf, limit - sb->len);
            return 1; /* truncated */
        }
        sb_append(sb, buf, n);
    }
    return ferror(fp) ? -1 : 0;
}

int sb_read_file(strbuf *sb, const char *path, size_t limit) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    int rc = sb_read_stream(sb, fp, limit);
    fclose(fp);
    return rc < 0 ? -1 : 0;
}
