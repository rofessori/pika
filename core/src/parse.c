/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pika/parse.h"

#include <ctype.h>
#include <string.h>
#include <strings.h>

/* Implemented in parse_pdf.c / parse_docx.c (external-tool backed). */
int pika_pdf_to_text(const char *data, size_t len, strbuf *out,
                     char *err, size_t errsz);
int pika_docx_to_text(const char *data, size_t len, strbuf *out,
                      char *err, size_t errsz);

pika_format pika_format_from_name(const char *name) {
    if (!name) return PIKA_FMT_AUTO;
    if (!strcasecmp(name, "auto")) return PIKA_FMT_AUTO;
    if (!strcasecmp(name, "text") || !strcasecmp(name, "txt"))
        return PIKA_FMT_TEXT;
    if (!strcasecmp(name, "md") || !strcasecmp(name, "markdown"))
        return PIKA_FMT_MARKDOWN;
    if (!strcasecmp(name, "html") || !strcasecmp(name, "htm"))
        return PIKA_FMT_HTML;
    if (!strcasecmp(name, "pdf")) return PIKA_FMT_PDF;
    if (!strcasecmp(name, "docx")) return PIKA_FMT_DOCX;
    return PIKA_FMT_AUTO;
}

const char *pika_format_name(pika_format f) {
    switch (f) {
    case PIKA_FMT_TEXT:     return "text";
    case PIKA_FMT_MARKDOWN: return "markdown";
    case PIKA_FMT_HTML:     return "html";
    case PIKA_FMT_PDF:      return "pdf";
    case PIKA_FMT_DOCX:     return "docx";
    default:                return "auto";
    }
}

static int has_ext(const char *name, const char *ext) {
    if (!name) return 0;
    size_t nl = strlen(name), el = strlen(ext);
    return nl >= el && strcasecmp(name + nl - el, ext) == 0;
}

pika_format pika_detect_format(const char *name, const char *data, size_t len) {
    if (has_ext(name, ".pdf"))  return PIKA_FMT_PDF;
    if (has_ext(name, ".docx")) return PIKA_FMT_DOCX;
    if (has_ext(name, ".html") || has_ext(name, ".htm")) return PIKA_FMT_HTML;
    if (has_ext(name, ".md") || has_ext(name, ".markdown"))
        return PIKA_FMT_MARKDOWN;
    if (has_ext(name, ".txt")) return PIKA_FMT_TEXT;

    /* content sniffing */
    if (len >= 5 && memcmp(data, "%PDF-", 5) == 0) return PIKA_FMT_PDF;
    /* docx/zip magic */
    if (len >= 4 && data[0] == 'P' && data[1] == 'K' &&
        data[2] == 3 && data[3] == 4)
        return PIKA_FMT_DOCX;

    /* Scan a prefix for an HTML/markup signature. This also catches pasted
     * snippets and page source, not just whole documents: any well-formed
     * closing tag (</x) or a recognised element opener is enough. The char
     * after an opener must be a delimiter so prose like "a < b" or code like
     * "vector<int>" is not misread as markup. */
    static const char *const TAGS[] = {
        "html","head","body","div","p","span","a","ul","ol","li","dl","dd",
        "table","thead","tbody","tr","td","th","article","section","main",
        "header","footer","nav","aside","figure","h1","h2","h3","h4","h5","h6",
        "img","br","hr","script","style","link","meta","title","blockquote",
        "pre","code","strong","em","b","i","button","form","input","label",
        "svg","picture","video","audio","iframe", NULL };
    size_t scan = len < 4096 ? len : 4096;
    for (size_t i = 0; i + 2 < scan; i++) {
        if (data[i] != '<') continue;
        unsigned char n1 = (unsigned char)data[i + 1];
        if (n1 == '/' && isalpha((unsigned char)data[i + 2]))
            return PIKA_FMT_HTML;                 /* a closing tag */
        if (n1 == '!' && !strncasecmp(data + i, "<!doctype html", 14))
            return PIKA_FMT_HTML;
        if (!isalpha(n1)) continue;
        for (int k = 0; TAGS[k]; k++) {
            size_t tl = strlen(TAGS[k]);
            if (i + 1 + tl >= scan) continue;
            if (strncasecmp(data + i + 1, TAGS[k], tl) != 0) continue;
            char after = data[i + 1 + tl];
            if (after == '>' || after == ' ' || after == '\t' ||
                after == '\n' || after == '\r' || after == '/' || after == '=')
                return PIKA_FMT_HTML;
        }
    }
    return PIKA_FMT_TEXT;
}

/* ---- Markdown -> text ---------------------------------------------------- */

static int line_is_rule(const char *s, size_t n) {
    int c = 0; char ch = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == ' ' || s[i] == '\t') continue;
        if (s[i] == '-' || s[i] == '*' || s[i] == '_') {
            if (ch && s[i] != ch) return 0;
            ch = s[i]; c++;
        } else return 0;
    }
    return c >= 3;
}

void pika_markdown_to_text(const char *md, size_t len, strbuf *out) {
    const char *p = md, *end = md + len;
    int in_fence = 0;
    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        const char *le = nl ? nl : end;
        const char *ls = p;
        size_t llen = (size_t)(le - ls);

        /* fenced code blocks: keep contents, drop the ``` lines */
        const char *t = ls;
        while (t < le && (*t == ' ' || *t == '\t')) t++;
        if (le - t >= 3 && (strncmp(t, "```", 3) == 0 ||
                            strncmp(t, "~~~", 3) == 0)) {
            in_fence = !in_fence;
            p = nl ? nl + 1 : end;
            continue;
        }
        if (in_fence) {
            sb_append(out, ls, llen);
            sb_putc(out, '\n');
            p = nl ? nl + 1 : end;
            continue;
        }
        if (line_is_rule(ls, llen)) { p = nl ? nl + 1 : end; continue; }

        /* strip leading blockquote / heading / list markers */
        const char *q = ls;
        while (q < le && (*q == ' ' || *q == '\t')) q++;
        while (q < le && *q == '>') { q++; while (q < le && *q == ' ') q++; }
        while (q < le && *q == '#') q++;
        if (q < le && (*q == '-' || *q == '*' || *q == '+') &&
            q + 1 < le && q[1] == ' ') {
            sb_append(out, "\xe2\x80\xa2 ", 4); /* bullet */
            q += 2;
        }
        if (q < le && (*q == ' ' || *q == '\t')) q++;

        /* inline: drop emphasis/backticks/tags, unwrap links */
        for (const char *r = q; r < le; r++) {
            char c = *r;
            if (c == '*' || c == '_' || c == '`' || c == '#') continue;
            if (c == '!' && r + 1 < le && r[1] == '[') continue;
            if (c == '[') {                       /* [text](url) -> text */
                const char *close = memchr(r, ']', (size_t)(le - r));
                if (close && close + 1 < le && close[1] == '(') {
                    const char *paren = memchr(close, ')', (size_t)(le - close));
                    sb_append(out, r + 1, (size_t)(close - r - 1));
                    r = paren ? paren : close;
                    continue;
                }
            }
            if (c == '<') {                       /* drop inline html tag */
                const char *gt = memchr(r, '>', (size_t)(le - r));
                if (gt) { r = gt; continue; }
            }
            sb_putc(out, c);
        }
        sb_putc(out, '\n');
        p = nl ? nl + 1 : end;
    }
}

/* ---- dispatch ------------------------------------------------------------ */

int pika_parse(const char *data, size_t len, const pika_parse_opts *opts,
               strbuf *out, char *err, size_t errsz) {
    pika_parse_opts def = {0};
    if (!opts) opts = &def;

    pika_format fmt = opts->fmt;
    if (fmt == PIKA_FMT_AUTO)
        fmt = pika_detect_format(opts->src_name, data, len);

    switch (fmt) {
    case PIKA_FMT_HTML:
        pika_html_to_text(data, len, opts->reader, out);
        return 0;
    case PIKA_FMT_MARKDOWN:
        pika_markdown_to_text(data, len, out);
        return 0;
    case PIKA_FMT_PDF:
        return pika_pdf_to_text(data, len, out, err, errsz);
    case PIKA_FMT_DOCX:
        return pika_docx_to_text(data, len, out, err, errsz);
    case PIKA_FMT_TEXT:
    default:
        sb_append(out, data, len);
        return 0;
    }
}
