/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pika/token.h"

#include <stdlib.h>
#include <string.h>

/* --- UTF-8 helpers -------------------------------------------------------- */

/* Length in bytes of the UTF-8 sequence starting at byte `b` (1..4). */
static int utf8_seq_len(unsigned char b) {
    if (b < 0x80) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 1; /* invalid lead byte: treat as single byte */
}

/* Count codepoints in a NUL-terminated UTF-8 string. */
static int utf8_count(const char *s) {
    int n = 0;
    while (*s) { s += utf8_seq_len((unsigned char)*s); n++; }
    return n;
}

/* --- classification ------------------------------------------------------- */

/* A "word boundary" is whitespace. Everything else (letters, digits,
 * punctuation glued to a word) rides along with the chunk. */
static int is_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\f' || c == '\v';
}

static int ends_sentence(const char *s, size_t len) {
    /* look at the final ASCII punctuation char, skipping closing quotes */
    while (len) {
        char c = s[len - 1];
        if (c == '"' || c == '\'' || c == ')' || c == ']' || c == '}')
            { len--; continue; }
        return c == '.' || c == '!' || c == '?';
    }
    return 0;
}

static int ends_clause(const char *s, size_t len) {
    while (len) {
        char c = s[len - 1];
        if (c == '"' || c == '\'' || c == ')' || c == ']' || c == '}')
            { len--; continue; }
        return c == ',' || c == ';' || c == ':';
    }
    return 0;
}

/* --- ORP ------------------------------------------------------------------ */

int pika_orp_for_length(int ncp) {
    if (ncp <= 1)  return 0;
    if (ncp <= 5)  return 1;
    if (ncp <= 9)  return 2;
    if (ncp <= 13) return 3;
    return 4;
}

/* --- options -------------------------------------------------------------- */

void pika_tok_opts_default(pika_tok_opts *o) {
    o->wpm          = 350;
    o->punct_factor = 2.2;
    o->comma_factor = 1.7;
    o->long_factor  = 1.4;
    o->long_len     = 8;
    o->max_chunk    = 1;
}

/* --- token vector --------------------------------------------------------- */

static void tokens_push(pika_tokens *t, char *text, int ncp, double weight) {
    if (t->count == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 256;
        pika_token *p = realloc(t->items, t->cap * sizeof *p);
        if (!p) { free(text); return; }
        t->items = p;
    }
    pika_token *tok = &t->items[t->count++];
    tok->text   = text;
    tok->ncp    = ncp;
    tok->orp    = pika_orp_for_length(ncp);
    tok->weight = weight;
}

void pika_tokens_free(pika_tokens *t) {
    for (size_t i = 0; i < t->count; i++) free(t->items[i].text);
    free(t->items);
    t->items = NULL;
    t->count = t->cap = 0;
}

/* --- tokenizer ------------------------------------------------------------ */

int pika_tokenize(const char *text, const pika_tok_opts *opts,
                  pika_tokens *out) {
    pika_tok_opts def;
    if (!opts) { pika_tok_opts_default(&def); opts = &def; }

    out->items = NULL;
    out->count = out->cap = 0;
    if (!text) return 0;

    const char *p = text;
    while (*p) {
        while (is_space((unsigned char)*p)) p++;   /* skip gap */
        if (!*p) break;

        const char *start = p;
        int merged = 0;                            /* words in this chunk */
        int maxc = opts->max_chunk > 0 ? opts->max_chunk : 1;
        while (*p && merged < maxc) {
            const char *wstart = p;
            while (*p && !is_space((unsigned char)*p)) p++;
            size_t wlen = (size_t)(p - wstart);
            merged++;
            /* stop merging at clause/sentence punctuation so pauses land
             * where a reader naturally breathes */
            if (ends_sentence(wstart, wlen) || ends_clause(wstart, wlen))
                break;
            if (merged >= maxc) break;             /* don't consume the gap */
            const char *q = p;
            while (is_space((unsigned char)*q)) q++;
            if (!*q) break;
            p = q;
        }

        size_t len = (size_t)(p - start);
        char *chunk = malloc(len + 1);
        if (!chunk) return -1;
        memcpy(chunk, start, len);
        chunk[len] = '\0';

        int ncp = utf8_count(chunk);

        double w = 1.0;
        if (ncp >= opts->long_len) w *= opts->long_factor;
        if (ends_sentence(chunk, len))      w *= opts->punct_factor;
        else if (ends_clause(chunk, len))   w *= opts->comma_factor;
        /* numbers take longer to parse */
        for (size_t i = 0; i < len; i++)
            if (chunk[i] >= '0' && chunk[i] <= '9') { w *= 1.25; break; }

        tokens_push(out, chunk, ncp, w);
    }
    return 0;
}

double pika_token_delay_ms(const pika_token *t, int wpm) {
    if (wpm <= 0) wpm = 350;
    double base = 60000.0 / (double)wpm;
    return base * t->weight;
}
