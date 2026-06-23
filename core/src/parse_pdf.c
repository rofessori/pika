/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * PDF decoding is delegated to `pdftotext` (poppler-utils) — a battle-tested,
 * widely packaged tool. Keeping it external means libpika has zero heavy
 * dependencies; if the tool is missing we say so plainly.
 */
#include "pika/parse.h"
#include "pika/proc.h"

#include <string.h>
#include <unistd.h>

int pika_pdf_to_text(const char *data, size_t len, strbuf *out,
                     char *err, size_t errsz) {
    if (!pika_have_tool("pdftotext")) {
        if (err) snprintf(err, errsz,
            "PDF support needs 'pdftotext' (install poppler-utils)");
        return -1;
    }
    char path[1024];
    if (pika_write_temp(data, len, path, sizeof path) != 0) {
        if (err) snprintf(err, errsz, "could not create temp file");
        return -1;
    }
    const char *argv[] = { "pdftotext", "-q", "-enc", "UTF-8", "-nopgbrk",
                           path, "-", NULL };
    int rc = pika_run_capture(argv, out);
    unlink(path);
    if (rc != 0) {
        if (err) snprintf(err, errsz, "pdftotext failed (status %d)", rc);
        return -1;
    }
    return 0;
}
