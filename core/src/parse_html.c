/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A small, forgiving HTML-to-text converter with an optional "reader mode".
 * It is not a conformant parser; it is a robust extractor tuned for turning
 * real-world articles into clean prose for reading.
 */
#include "pika/parse.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ---- UTF-8 --------------------------------------------------------------- */

/* Encode codepoint `cp` into `buf` (>=4 bytes); return the byte count. */
static int utf8_encode(unsigned cp, char *buf) {
    if (cp < 0x80) { buf[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    buf[0] = (char)(0xF0 | (cp >> 18));
    buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* ---- whitespace-collapsing emitter --------------------------------------
 * All text — literal runs and decoded entities alike — flows through here so
 * spacing stays consistent. Runs of whitespace collapse to a single space; a
 * block boundary collapses to a single newline. */

typedef struct { strbuf *out; int pending_nl; int pending_sp; int started; } emit;

static void emit_text(emit *e, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
            c == '\f' || c == '\v') {
            if (e->started) e->pending_sp = 1;
        } else {
            if (e->started) {
                if (e->pending_nl)      sb_putc(e->out, '\n');
                else if (e->pending_sp) sb_putc(e->out, ' ');
            }
            e->pending_nl = e->pending_sp = 0;
            sb_putc(e->out, (char)c);
            e->started = 1;
        }
    }
}

static void emit_break(emit *e) { if (e->started) e->pending_nl = 1; }

/* ---- entities ------------------------------------------------------------ */

struct entity { const char *name; unsigned cp; };
static const struct entity ENTITIES[] = {
    {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'},
    {"apos", '\''}, {"nbsp", ' '}, {"mdash", 0x2014}, {"ndash", 0x2013},
    {"hellip", 0x2026}, {"lsquo", 0x2018}, {"rsquo", 0x2019},
    {"ldquo", 0x201C}, {"rdquo", 0x201D}, {"copy", 0x00A9},
    {"reg", 0x00AE}, {"trade", 0x2122}, {"deg", 0x00B0},
    {"middot", 0x00B7}, {"bull", 0x2022}, {"eacute", 0x00E9},
    {"egrave", 0x00E8}, {"agrave", 0x00E0}, {"uuml", 0x00FC},
    {"ouml", 0x00F6}, {"auml", 0x00E4}, {"szlig", 0x00DF}, {NULL, 0},
};

/* Decode an entity starting just after '&'. On success advance *pp past the
 * ';', emit the character(s) through `e`, and return 1; else return 0. */
static int decode_entity(const char **pp, const char *end, emit *e) {
    const char *p = *pp;
    const char *semi = memchr(p, ';', (size_t)(end - p));
    if (!semi || semi - p > 12 || semi == p) return 0;
    size_t n = (size_t)(semi - p);
    char buf[4];
    if (p[0] == '#') {
        unsigned cp; char *q;
        if (p[1] == 'x' || p[1] == 'X') cp = (unsigned)strtoul(p + 2, &q, 16);
        else                            cp = (unsigned)strtoul(p + 1, &q, 10);
        if (q != semi || cp == 0) return 0;
        emit_text(e, buf, (size_t)utf8_encode(cp, buf));
        *pp = semi + 1;
        return 1;
    }
    for (const struct entity *en = ENTITIES; en->name; en++) {
        if (strlen(en->name) == n && strncmp(en->name, p, n) == 0) {
            emit_text(e, buf, (size_t)utf8_encode(en->cp, buf));
            *pp = semi + 1;
            return 1;
        }
    }
    return 0;
}

/* ---- tag classification -------------------------------------------------- */

static int tag_in(const char *name, const char *const *set) {
    for (int i = 0; set[i]; i++)
        if (strcasecmp(set[i], name) == 0) return 1;
    return 0;
}

/* Tags whose content stays on the same line (everything else breaks). */
static int is_inline_tag(const char *name) {
    static const char *inl[] = {
        "a","span","b","i","em","strong","code","small","sub","sup","u",
        "mark","abbr","q","cite","time","label","font","wbr","s","del",
        "ins","kbd","samp","var","big","tt","nobr",NULL};
    return tag_in(name, inl);
}

/* Tags dropped (with their entire content) in reader mode. */
static int is_chrome_tag(const char *name) {
    static const char *cr[] = {
        "nav","header","footer","aside","form","menu","button","figure",
        "figcaption","iframe","fieldset","dialog",NULL};
    return tag_in(name, cr);
}

/* Tags whose raw content must be skipped entirely, always. */
static int is_raw_tag(const char *name) {
    static const char *raw[] = {
        "script","style","head","svg","noscript","template",NULL};
    return tag_in(name, raw);
}

/* ---- tag scanning -------------------------------------------------------- */

/* Read a tag name into buf (lowercased). p points just after '<'. */
static const char *read_tag_name(const char *p, const char *end,
                                  char *buf, size_t bufsz, int *closing) {
    *closing = 0;
    if (p < end && *p == '/') { *closing = 1; p++; }
    size_t i = 0;
    while (p < end && (isalnum((unsigned char)*p) || *p == '-')) {
        if (i + 1 < bufsz) buf[i++] = (char)tolower((unsigned char)*p);
        p++;
    }
    buf[i] = '\0';
    return p;
}

/* Find the end '>' of a tag, respecting quoted attribute values. */
static const char *skip_to_tag_end(const char *p, const char *end) {
    char quote = 0;
    while (p < end) {
        if (quote) { if (*p == quote) quote = 0; }
        else if (*p == '"' || *p == '\'') quote = *p;
        else if (*p == '>') return p + 1;
        p++;
    }
    return end;
}

/* Skip raw element content up to and including </name>. */
static const char *skip_raw_element(const char *p, const char *end,
                                    const char *name) {
    size_t nlen = strlen(name);
    while (p < end) {
        const char *lt = memchr(p, '<', (size_t)(end - p));
        if (!lt) return end;
        if (lt + 1 < end && lt[1] == '/' &&
            strncasecmp(lt + 2, name, nlen) == 0)
            return skip_to_tag_end(lt + 2 + nlen, end);
        p = lt + 1;
    }
    return end;
}

/* ---- main converter ------------------------------------------------------ */

void pika_html_to_text(const char *html, size_t len, int reader, strbuf *out) {
    const char *p = html, *end = html + len;
    emit e = { out, 0, 0, 0 };
    /* In reader mode we suppress output while inside a chrome container; depth
     * counts nested copies of the outermost chrome tag we entered. */
    int chrome_depth = 0;
    char chrome_name[24] = {0};

    while (p < end) {
        if (*p == '<') {
            if (end - p >= 4 && strncmp(p, "<!--", 4) == 0) {    /* comment */
                const char *c = memmem(p + 4, (size_t)(end - p - 4), "-->", 3);
                p = c ? c + 3 : end;
                continue;
            }
            if (p + 1 < end && (p[1] == '!' || p[1] == '?')) {   /* decl/PI */
                p = skip_to_tag_end(p + 1, end);
                continue;
            }
            char name[24]; int closing;
            const char *after = read_tag_name(p + 1, end, name, sizeof name,
                                              &closing);
            if (name[0] == '\0') {                               /* stray '<' */
                if (!chrome_depth) emit_text(&e, "<", 1);
                p++;
                continue;
            }
            const char *tag_end = skip_to_tag_end(after, end);

            if (!closing && is_raw_tag(name)) {
                p = skip_raw_element(tag_end, end, name);
                continue;
            }

            if (reader) {
                if (chrome_depth) {
                    if (strcasecmp(name, chrome_name) == 0)
                        chrome_depth += closing ? -1 : 1;
                    p = tag_end;
                    continue;
                }
                if (!closing && is_chrome_tag(name)) {
                    if (tag_end[-2] != '/') {     /* not self-closing */
                        snprintf(chrome_name, sizeof chrome_name, "%s", name);
                        chrome_depth = 1;
                    }
                    p = tag_end;
                    continue;
                }
            }

            if (strcasecmp(name, "br") == 0 || !is_inline_tag(name))
                emit_break(&e);
            if (!closing && strcasecmp(name, "li") == 0)
                emit_text(&e, "\xe2\x80\xa2 ", 4);   /* bullet */

            p = tag_end;
            continue;
        }

        if (chrome_depth) { p++; continue; }

        if (*p == '&') {
            const char *q = p + 1;
            if (decode_entity(&q, end, &e)) { p = q; continue; }
            emit_text(&e, "&", 1);
            p++;
            continue;
        }

        const char *next = p;
        while (next < end && *next != '<' && *next != '&') next++;
        emit_text(&e, p, (size_t)(next - p));
        p = next;
    }
}
