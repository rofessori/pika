# pika manual

The full guide to installing and using `pika` — the command-line tool, the C
library, the terminal reader, and the optional web app. For a quick overview,
see the [README](../README.md).

## Contents

- [How it works](#how-it-works)
- [Quick start](#quick-start)
- [Requirements](#requirements)
- [The command-line tool](#the-command-line-tool)
- [Using the library](#using-the-library)
- [The terminal reader](#the-terminal-reader)
- [The optional web app](#the-optional-web-app)
- [Project layout](#project-layout)
- [Building and testing](#building-and-testing)

## How it works

`pika` turns a document into a stream of **tokens** (usually words). For each
token it computes two things:

1. **The focal character (ORP).** The token is aligned so this character sits
   on a fixed centre column. In the name `p-i-ka`, the red `i` is the focal
   point.
2. **A dwell time.** Longer words, numbers, and words that end a sentence or
   clause are shown slightly longer, because they take longer to absorb.

Renderers (the terminal reader, the web app, or your own program) just display
each token for its dwell time. This technique is known as Rapid Serial Visual
Presentation (RSVP).

Input can be plain text, Markdown, HTML, PDF, or DOCX. For web pages there is a
**reader mode** that keeps the article and removes navigation, scripts, and
other boilerplate.

## Quick start

```sh
git clone https://github.com/rofessori/pika.git
cd pika
./setup.sh            # interactive installer (choose "Essentials")
```

The installer builds the library and CLI, runs the tests, and installs them
(by default to `/usr/local`). It also offers to install the terminal reader and
to set up the web app. To install without prompts:

```sh
./setup.sh --essentials          # library + CLI only
./setup.sh --reader              # also the terminal reader
./setup.sh --everything          # also the web app
./setup.sh --essentials --prefix "$HOME/.local" --yes
```

Prefer `make`? See [Building and testing](#building-and-testing).

## Requirements

| Feature                     | Needs                                             |
| --------------------------- | ------------------------------------------------- |
| Library + CLI + reader      | A C11 compiler (`gcc`/`clang`) and `make`         |
| Reading PDFs                | `pdftotext` (the `poppler-utils` package)         |
| Reading DOCX                | `unzip`                                           |
| Fetching URLs               | `curl` **or** `wget`                              |
| Web app (optional)          | Node.js 18+                                       |

Everything except the compiler and `make` is optional. The library detects what
is available and reports clearly when a feature is unavailable.

**macOS:** install the compiler and `make` with `xcode-select --install`. The
optional tools are available via [Homebrew](https://brew.sh):
`brew install poppler` (PDF); `unzip`/`curl` ship with macOS already.

**Debian/Ubuntu:** `sudo apt install build-essential` for the compiler and
`make`; `sudo apt install poppler-utils unzip curl` for the optional features.

The installer falls back to plain text prompts when `whiptail`/`dialog` is not
present (as on a stock macOS), so it works the same everywhere.

## The command-line tool

`pika` reads a file (or standard input) and prints either clean text or a JSON
token stream. It is built to compose with other tools.

```sh
# Read a file in the terminal reader (when stdout is a terminal)
pika notes.txt

# Clean a web page down to just the article text
pika --url https://example.com/post --reader --text

# Strip a PDF to plain text and search it
pika paper.pdf --text | grep -i "method"

# Emit the timed token stream as JSON (for your own tools or the web app)
pika README.md --json | jq '.count'

# Works in a pipeline
curl -s https://example.com | pika --format html --reader --text | fold -s
```

Useful options (`pika --help` lists them all):

| Option              | Meaning                                                   |
| ------------------- | --------------------------------------------------------- |
| `-u, --url URL`     | Fetch a URL with `curl`/`wget` first                      |
| `-f, --format FMT`  | `auto` (default), `text`, `md`, `html`, `pdf`, `docx`     |
| `-R, --reader`      | Reader mode for HTML (keep article, drop chrome)          |
| `-t, --text`        | Output cleaned plain text                                 |
| `-j, --json`        | Output the RSVP token stream as JSON                      |
| `-w, --wpm N`       | Base words per minute (default 350)                       |
| `--safe-fetch`      | Refuse to fetch private/loopback addresses                |

## Using the library

`libpika` is a plain C11 library with no required dependencies. After
installing, build against it with `pkg-config`:

```c
#include <pika/pika.h>
#include <stdio.h>

int main(void) {
    const char *text = "Reading on a fixed point is fast.";

    pika_tokens toks;
    pika_tokenize(text, NULL, &toks);          /* NULL = default options */

    for (size_t i = 0; i < toks.count; i++) {
        pika_token *t = &toks.items[i];
        printf("%-12s focal=%d  dwell=%.0fms\n",
               t->text, t->orp, pika_token_delay_ms(t, 350));
    }
    pika_tokens_free(&toks);
    return 0;
}
```

```sh
cc example.c $(pkg-config --cflags --libs pika) -o example
```

The public headers live under `pika/` (`token.h`, `parse.h`, `fetch.h`,
`json.h`, `strbuf.h`). See [ARCHITECTURE.md](ARCHITECTURE.md) for an overview of
the API surface.

## The terminal reader

`pika-tui` is an optional terminal program that reads a file (or standard
input) one word at a time, with the focal character highlighted.

```sh
pika-tui article.txt
pika paper.pdf --text | pika-tui --wpm 450
```

Controls: **Space** play/pause, **← →** step a word, **↑ ↓** adjust speed,
**r** restart, **q** quit.

## The optional web app

The `server/` directory contains a small Node.js service that wraps the `pika`
CLI to provide:

- a clean web reader (the `web/` frontend),
- **shareable links**: a reading is saved and given a short URL such as
  `https://your-host/A1B2C3` that anyone can open,
- a **token-authenticated HTTP API** for developers.

Run it locally:

```sh
cd server
npm install
PIKA_BIN=/usr/local/bin/pika npm start
# open http://localhost:8787
```

(For the in-page reader, also build the frontend once: `cd web && npm install &&
npm run build`.)

On first start the server prints — and saves to `server/data/admin-access.txt`
— a secret **admin console URL** and a one-time **passkey**. Sign in there to
issue and revoke API tokens.

The public developer API is **disabled by default**; enable it with
`PUBLIC_API=true`. Full reference: [API.md](API.md). All limits (text size, file
size, link expiry, rate limits) are configurable — see
[`server/.env.example`](../server/.env.example).

## Project layout

```
core/      The C library (libpika), the `pika` CLI, and the `pika-tui` reader
server/    Optional Node.js web service (sharing + HTTP API)
web/       Optional Vite + React frontend
docs/      Architecture and HTTP API reference
setup.sh   Interactive installer
```

## Building and testing

```sh
make            # build the library, CLI, and TUI
make check      # run the unit tests
make lib        # just the library (static + shared)
make cli        # just the CLI
make install            # install lib + headers + CLI (PREFIX=/usr/local)
make install-tui        # install the optional terminal reader
make clean
```

Override the install location with `PREFIX`, e.g.
`make install PREFIX="$HOME/.local"`.

The web service is a standard Node.js application. It listens on `127.0.0.1`
by default and is configured entirely through environment variables (see
[`server/.env.example`](../server/.env.example)), so it can run behind any
reverse proxy you prefer.
