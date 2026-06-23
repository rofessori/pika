/*
 * json — minimal JSON emission for the token stream.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PIKA_JSON_H
#define PIKA_JSON_H

#include "pika/strbuf.h"
#include "pika/token.h"

/* Append a JSON-escaped string (without surrounding quotes) to sb. */
void pika_json_escape(strbuf *sb, const char *s);

/* Emit a complete document:
 * {"version","wpm","count","tokens":[{"t","orp","ncp","w"}...]} */
void pika_json_tokens(strbuf *sb, const pika_tokens *t, int wpm);

#endif /* PIKA_JSON_H */
