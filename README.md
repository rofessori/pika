# pika

[![CI](https://github.com/rofessori/pika/actions/workflows/ci.yml/badge.svg)](https://github.com/rofessori/pika/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)

pika is a speed reader. It flashes text one word at a time and keeps each word's
focal letter pinned to the same spot, so your eyes stop hopping across the line.
You pick the pace, in words per minute.

It's a C library first. The command-line tool, the terminal reader, and the
small web app are all built on top of `libpika` — but the library is the point.

## Features

- Reads plain text, Markdown, HTML, PDF, and DOCX.
- Reader mode lifts the article out of a web page and drops the menus, scripts,
  and ads.
- No required dependencies in the core. PDF, DOCX, and URL fetching shell out to
  tools you probably already have (`pdftotext`, `unzip`, `curl`/`wget`), and say
  so plainly when one is missing.
- A CLI that behaves itself in a pipe.
- An optional web app: shareable links and a token-authenticated HTTP API.

## Install

You need a C compiler and `make`. Then:

```sh
git clone https://github.com/rofessori/pika.git
cd pika
./setup.sh
```

The installer asks what you want and does the rest. To skip the questions:

```sh
./setup.sh --essentials      # library + CLI
./setup.sh --reader          # + terminal reader
./setup.sh --everything      # + web app
```

Or drive `make` yourself:

```sh
make && sudo make install    # library, headers, CLI
make install-tui             # the terminal reader
make check                   # run the tests
```

On macOS, `xcode-select --install` gets you the compiler and `make`, and
`brew install poppler` adds PDF support. On Debian/Ubuntu, `apt install
build-essential poppler-utils unzip`.

## Usage

```sh
pika notes.txt                                       # read a file in the terminal
pika --url https://example.com/post --reader --text  # just the article, as text
pika paper.pdf --text | grep method                  # it's an ordinary filter
```

The terminal reader takes a file or a pipe. Space plays and pauses, the arrows
steer and change speed, `q` quits:

```sh
pika-tui article.txt
```

From C, the whole engine is three calls:

```c
pika_tokens toks;
pika_tokenize("Read this fast.", NULL, &toks);
/* each toks.items[i] has the word, its focal letter, and how long to show it */
pika_tokens_free(&toks);
```

## Documentation

- [Manual](docs/manual.md) — install options, the CLI, the library, the web app.
- [HTTP API](docs/API.md) — endpoints, authentication, the developer API.
- [Architecture](docs/ARCHITECTURE.md) — how the pieces fit together.

## Contributing

Bug reports and patches are welcome. [CONTRIBUTING.md](CONTRIBUTING.md) covers
building and testing; [SECURITY.md](SECURITY.md) is for reporting something
privately.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
