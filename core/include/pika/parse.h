/*
 * parse — turn arbitrary document bytes into clean plain text.
 *
 * Built-in, dependency-free decoders cover plain text, Markdown, and HTML
 * (including a "reader mode" that keeps the main article and drops navigation,
 * scripts, and other boilerplate). PDF and DOCX are decoded through small,
 * well-isolated external helpers (pdftotext / unzip) so the core stays tiny;
 * if a helper is absent the failure is reported cleanly.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PIKA_PARSE_H
#define PIKA_PARSE_H

#include "pika/strbuf.h"
#include <stddef.h>

typedef enum {
    PIKA_FMT_AUTO = 0,
    PIKA_FMT_TEXT,
    PIKA_FMT_MARKDOWN,
    PIKA_FMT_HTML,
    PIKA_FMT_PDF,
    PIKA_FMT_DOCX,
} pika_format;

typedef struct {
    pika_format fmt;       /* desired format, or PIKA_FMT_AUTO          */
    int         reader;    /* HTML: extract main article, drop chrome   */
    const char *src_name;  /* optional file name, used for auto-detect  */
} pika_parse_opts;

/* Map a format name ("text","md","html","pdf","docx","auto") to enum. */
pika_format pika_format_from_name(const char *name);
const char *pika_format_name(pika_format f);

/* Sniff a format from a file name and/or content bytes. */
pika_format pika_detect_format(const char *name, const char *data, size_t len);

/* Parse `data` (len bytes) into clean UTF-8 text appended to `out`.
 * Returns 0 on success, -1 on error (message in `err`, if non-NULL). */
int pika_parse(const char *data, size_t len, const pika_parse_opts *opts,
               strbuf *out, char *err, size_t errsz);

/* Individual decoders (exposed for testing / direct use). */
void pika_html_to_text(const char *html, size_t len, int reader, strbuf *out);
void pika_markdown_to_text(const char *md, size_t len, strbuf *out);

#endif /* PIKA_PARSE_H */
