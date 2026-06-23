/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A .docx is a ZIP whose word/document.xml holds the prose. We unzip just
 * that member with `unzip -p` (ubiquitous) and pull text out of <w:t> runs,
 * breaking lines on <w:p> paragraphs. No XML library required.
 */
#include "pika/parse.h"
#include "pika/proc.h"

#include <string.h>
#include <unistd.h>

static void xml_text(const char *s, size_t n, strbuf *out) {
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '&') {
            if (!strncmp(s + i, "&amp;", 5))  { sb_putc(out, '&'); i += 4; }
            else if (!strncmp(s + i, "&lt;", 4))  { sb_putc(out, '<'); i += 3; }
            else if (!strncmp(s + i, "&gt;", 4))  { sb_putc(out, '>'); i += 3; }
            else if (!strncmp(s + i, "&quot;", 6)) { sb_putc(out, '"'); i += 5; }
            else if (!strncmp(s + i, "&apos;", 6)) { sb_putc(out, '\''); i += 5; }
            else sb_putc(out, '&');
        } else {
            sb_putc(out, s[i]);
        }
    }
}

static void docx_xml_to_text(const char *xml, size_t len, strbuf *out) {
    const char *p = xml, *end = xml + len;
    while (p < end) {
        const char *lt = memchr(p, '<', (size_t)(end - p));
        if (!lt) break;
        p = lt;
        if (!strncmp(p, "<w:t>", 5) || !strncmp(p, "<w:t ", 5)) {
            const char *gt = memchr(p, '>', (size_t)(end - p));
            if (!gt) break;
            const char *close = memmem(gt, (size_t)(end - gt), "</w:t>", 6);
            if (!close) break;
            xml_text(gt + 1, (size_t)(close - gt - 1), out);
            p = close + 6;
        } else if (!strncmp(p, "</w:p>", 6)) {
            sb_putc(out, '\n');
            p += 6;
        } else if (!strncmp(p, "<w:tab", 6)) {
            sb_putc(out, '\t');
            p += 6;
        } else if (!strncmp(p, "<w:br", 5)) {
            sb_putc(out, '\n');
            p += 5;
        } else {
            p++;
        }
    }
}

int pika_docx_to_text(const char *data, size_t len, strbuf *out,
                      char *err, size_t errsz) {
    if (!pika_have_tool("unzip")) {
        if (err) snprintf(err, errsz,
            "DOCX support needs 'unzip' (install unzip)");
        return -1;
    }
    char path[1024];
    if (pika_write_temp(data, len, path, sizeof path) != 0) {
        if (err) snprintf(err, errsz, "could not create temp file");
        return -1;
    }
    const char *argv[] = { "unzip", "-p", path, "word/document.xml", NULL };
    strbuf xml; sb_init(&xml);
    int rc = pika_run_capture(argv, &xml);
    unlink(path);
    if (rc != 0 || xml.len == 0) {
        sb_free(&xml);
        if (err) snprintf(err, errsz, "could not read word/document.xml");
        return -1;
    }
    docx_xml_to_text(xml.data, xml.len, out);
    sb_free(&xml);
    return 0;
}
