/*
 * fetch — retrieve a remote document using curl or wget (whichever exists).
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PIKA_FETCH_H
#define PIKA_FETCH_H

#include "pika/strbuf.h"
#include <stddef.h>

typedef struct {
    size_t      max_bytes;    /* cap response size (0 = tool default)      */
    int         timeout_s;    /* per-request timeout (default 30)          */
    int         deny_private; /* refuse localhost / private/link-local IPs */
    const char *user_agent;   /* override UA (default "pika/<ver>")        */
} pika_fetch_opts;

void pika_fetch_opts_default(pika_fetch_opts *o);

/* Fetch http(s) `url` into `out`. Returns 0 on success, -1 on error. */
int  pika_fetch(const char *url, const pika_fetch_opts *opts, strbuf *out,
                char *err, size_t errsz);

/* True if a usable downloader (curl or wget) is present. */
int  pika_fetch_available(void);

#endif /* PIKA_FETCH_H */
