# Architecture

`pika` is organised around a single principle: **the C library is the source of
truth**, and everything else is a thin presentation layer over it. Parsing,
tokenization, focal‑point computation, and timing are implemented once, in C,
and reused everywhere.

```
            ┌────────────────────────────────────────────┐
            │                 libpika (C)                  │
            │  parse → tokenize → ORP + dwell → JSON/text  │
            └────────────────────────────────────────────┘
                 ▲              ▲                  ▲
                 │ links        │ links            │ runs as a subprocess
        ┌────────┴───────┐ ┌────┴───────┐   ┌──────┴───────────────┐
        │   pika (CLI)   │ │  pika-tui  │   │  Node web service     │
        │  pipelines     │ │  terminal  │   │  sharing + HTTP API   │
        └────────────────┘ └────────────┘   └──────────┬───────────┘
                                                        │ serves
                                                 ┌──────┴───────┐
                                                 │ React web app │
                                                 └──────────────┘
```

## The library (`core/`)

`libpika` is C11 with no required third‑party dependencies. Modules:

| Module        | Responsibility                                                       |
| ------------- | -------------------------------------------------------------------- |
| `strbuf`      | A small growable byte buffer used throughout.                        |
| `token`       | Tokenization, optimal‑recognition‑point, and dwell‑time weighting.   |
| `parse`       | Format detection and dispatch to the decoders below.                 |
| `parse_html`  | HTML → text, with an optional reader mode and entity decoding.       |
| `parse` (md)  | Markdown → text.                                                     |
| `parse_pdf`   | PDF → text via the external `pdftotext` tool.                        |
| `parse_docx`  | DOCX → text via `unzip` and a small XML text extractor.              |
| `fetch`       | URL retrieval via `curl`/`wget`, with SSRF guards.                   |
| `proc`        | Safe subprocess execution (no shell) used by the decoders/fetcher.   |
| `json`        | JSON emission for the token stream.                                  |

External tools (`pdftotext`, `unzip`, `curl`/`wget`) are invoked only when the
corresponding format/feature is used, and their absence is reported as a clean
error rather than a crash. This keeps the core tiny and portable while still
supporting heavy formats.

### Data flow

1. **Acquire bytes** — from a file, standard input, or a fetched URL.
2. **Parse** — `pika_parse()` detects the format (by name and content) and
   produces clean UTF‑8 text.
3. **Tokenize** — `pika_tokenize()` splits the text into chunks, computes each
   chunk's focal character and dwell weight.
4. **Render** — the CLI prints text or JSON; the TUI and web app animate the
   stream.

The focal character (ORP) is chosen by chunk length; the dwell weight increases
for long words, numbers, and chunks ending a clause or sentence.

## The CLI and TUI (`core/cli`, `core/tui`)

Both link the static library directly, so they have no runtime dependency on the
shared object. The CLI is a stateless filter (text/JSON in, text/JSON out). The
TUI is a self‑contained terminal renderer using raw mode and ANSI escapes — no
`ncurses` dependency.

## The web service (`server/`)

A small Node.js (Express) service that **shells out to the `pika` CLI** for all
parsing, so it never reimplements the engine. Responsibilities:

- **Sharing**: readings are stored as one JSON file each under the data
  directory and addressed by a short id; no database is required.
- **Two API surfaces**: first‑party `/app` endpoints for the bundled web app,
  and a token‑authenticated `/v1` developer API. Both call a shared service
  layer (`service.js`) so they cannot drift apart.
- **Administration**: a console served from a secret path, protected by a
  passkey generated at first start. Admins issue and revoke API tokens.
- **Safety**: request size limits, per‑client rate limiting, SSRF‑guarded URL
  fetching, security headers, and a strict error envelope.

Configuration is entirely environment‑driven (`config.js`,
`server/.env.example`).

## The web app (`web/`)

A Vite + React + TypeScript single‑page app. It renders the token stream
returned by the service, pinning each token's focal character to a fixed centre
column. It talks only to the first‑party `/app` endpoints. The build is a static
bundle the Node service hosts directly; share links (`/<id>`) are resolved by an
SPA fallback.

## Why this shape

- **One engine, many front‑ends.** Behaviour is identical in a pipeline, a
  terminal, and a browser because they share the same C code.
- **Small surface, few dependencies.** The essential install is just a compiler
  and `make`; heavy formats and the web stack are opt‑in.
- **Easy to host.** No database, no build step for the admin console, and a
  single long‑running process.
