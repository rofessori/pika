/*
 * token — splits plain text into a timed RSVP stream.
 *
 * Each token is one displayed "chunk" (usually a word). The Optimal
 * Recognition Point (ORP) is the codepoint the eye should fixate on; renderers
 * align that codepoint to a fixed focal column so the gaze never moves.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PIKA_TOKEN_H
#define PIKA_TOKEN_H

#include <stddef.h>

typedef struct {
    char  *text;     /* UTF-8 chunk, NUL-terminated (owned)        */
    int    orp;      /* index (in codepoints) of the focal char    */
    int    ncp;      /* number of codepoints in text               */
    double weight;   /* dwell multiplier relative to the base rate */
} pika_token;

typedef struct {
    pika_token *items;
    size_t      count;
    size_t      cap;
} pika_tokens;

typedef struct {
    int    wpm;            /* base words-per-minute (default 350)   */
    double punct_factor;   /* extra dwell after . ! ? (default 2.2) */
    double comma_factor;   /* extra dwell after , ; : (default 1.7) */
    double long_factor;    /* extra dwell for long words (def 1.4)  */
    int    long_len;       /* codepoints considered "long" (def 8)  */
    int    max_chunk;      /* >1 groups short words together (def 1)*/
} pika_tok_opts;

void pika_tok_opts_default(pika_tok_opts *o);

/* Tokenize NUL-terminated UTF-8 text. Result must be freed with
 * pika_tokens_free(). Returns 0 on success. */
int  pika_tokenize(const char *text, const pika_tok_opts *opts,
                   pika_tokens *out);

void pika_tokens_free(pika_tokens *t);

/* Suggested on-screen time for a token, in milliseconds, at the base wpm. */
double pika_token_delay_ms(const pika_token *t, int wpm);

/* Compute the ORP codepoint index for a chunk of `ncp` codepoints. */
int  pika_orp_for_length(int ncp);

#endif /* PIKA_TOKEN_H */
