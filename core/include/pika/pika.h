/*
 * pika — a fast, focal-point (RSVP) reading engine.
 *
 * This umbrella header exposes the full public API of libpika. The library is
 * deliberately dependency-free (C11 + libc only) so the same core can power a
 * unix pipeline filter, a terminal TUI, and a web backend.
 *
 * Pipeline: raw bytes -> pika_parse() -> plain text -> pika_tokenize() ->
 * token stream (word + optimal-recognition-point + suggested delay).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PIKA_H
#define PIKA_H

#include "pika/strbuf.h"
#include "pika/token.h"
#include "pika/parse.h"
#include "pika/fetch.h"
#include "pika/json.h"

#define PIKA_VERSION "1.0.0"

#endif /* PIKA_H */
