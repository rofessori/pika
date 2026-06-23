/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pika/fetch.h"
#include "pika/proc.h"
#include "pika/pika.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void pika_fetch_opts_default(pika_fetch_opts *o) {
    o->max_bytes    = 8u * 1024 * 1024;
    o->timeout_s    = 30;
    o->deny_private = 0;
    o->user_agent   = "pika/" PIKA_VERSION;
}

int pika_fetch_available(void) {
    return pika_have_tool("curl") || pika_have_tool("wget");
}

/* Copy the host portion of `url` into host[] (lowercased, no userinfo/port). */
static int extract_host(const char *url, char *host, size_t hostsz) {
    const char *p = strstr(url, "://");
    if (!p) return -1;
    p += 3;
    const char *at = strchr(p, '@');
    const char *slash = strpbrk(p, "/?#");
    if (at && (!slash || at < slash)) p = at + 1;  /* skip userinfo */
    size_t i = 0;
    while (*p && *p != '/' && *p != ':' && *p != '?' && *p != '#') {
        if (i + 1 < hostsz) host[i++] = (char)tolower((unsigned char)*p);
        p++;
    }
    host[i] = '\0';
    return i ? 0 : -1;
}

static int is_private_host(const char *host) {
    if (!strcmp(host, "localhost") || !strcmp(host, "::1") ||
        !strcmp(host, "0.0.0.0")) return 1;
    if (strstr(host, ".localhost")) return 1;
    /* IPv4 literal ranges commonly used internally */
    unsigned a, b, c, d;
    if (sscanf(host, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        if (a == 127 || a == 10 || a == 0) return 1;
        if (a == 192 && b == 168) return 1;
        if (a == 169 && b == 254) return 1;          /* link-local */
        if (a == 172 && b >= 16 && b <= 31) return 1;
        if (a == 100 && b >= 64 && b <= 127) return 1;/* CGNAT */
    }
    if (!strncmp(host, "fe80", 4) || !strncmp(host, "fc", 2) ||
        !strncmp(host, "fd", 2)) return 1;            /* IPv6 local */
    return 0;
}

int pika_fetch(const char *url, const pika_fetch_opts *opts, strbuf *out,
               char *err, size_t errsz) {
    pika_fetch_opts def;
    if (!opts) { pika_fetch_opts_default(&def); opts = &def; }

    if (strncasecmp(url, "http://", 7) != 0 &&
        strncasecmp(url, "https://", 8) != 0) {
        if (err) snprintf(err, errsz, "only http and https URLs are allowed");
        return -1;
    }
    char host[256];
    if (extract_host(url, host, sizeof host) != 0) {
        if (err) snprintf(err, errsz, "could not parse host from URL");
        return -1;
    }
    if (opts->deny_private && is_private_host(host)) {
        if (err) snprintf(err, errsz, "refusing to fetch private address '%s'",
                          host);
        return -1;
    }

    char timeout[16], maxsz[32];
    snprintf(timeout, sizeof timeout, "%d", opts->timeout_s > 0
             ? opts->timeout_s : 30);

    int rc;
    if (pika_have_tool("curl")) {
        const char *argv[20]; int n = 0;
        argv[n++] = "curl";
        argv[n++] = "-fsSL";              /* fail on errors, follow redirects */
        argv[n++] = "--max-time";   argv[n++] = timeout;
        argv[n++] = "--max-redirs"; argv[n++] = "5";
        argv[n++] = "--proto";      argv[n++] = "=http,https";
        argv[n++] = "-A";           argv[n++] = opts->user_agent;
        if (opts->max_bytes) {
            snprintf(maxsz, sizeof maxsz, "%zu", opts->max_bytes);
            argv[n++] = "--max-filesize"; argv[n++] = maxsz;
        }
        argv[n++] = url;
        argv[n] = NULL;
        rc = pika_run_capture(argv, out);
    } else if (pika_have_tool("wget")) {
        const char *argv[16]; int n = 0;
        argv[n++] = "wget";
        argv[n++] = "-qO-";
        argv[n++] = "--timeout"; argv[n++] = timeout;
        argv[n++] = "--tries";   argv[n++] = "2";
        argv[n++] = "-U";        argv[n++] = opts->user_agent;
        argv[n++] = url;
        argv[n] = NULL;
        rc = pika_run_capture(argv, out);
    } else {
        if (err) snprintf(err, errsz, "neither curl nor wget is installed");
        return -1;
    }

    if (rc != 0) {
        if (err) snprintf(err, errsz, "download failed (status %d)", rc);
        return -1;
    }
    if (opts->max_bytes && out->len > opts->max_bytes)
        out->len = opts->max_bytes, out->data[out->len] = '\0';
    return 0;
}
