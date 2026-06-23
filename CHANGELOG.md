# Changelog

All notable changes to this project are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/), and the project aims to follow
[Semantic Versioning](https://semver.org/).

## [1.0.0]

### Added

- **`libpika`** — a dependency‑free C11 reading engine: tokenization, optimal
  recognition point, per‑word dwell timing, JSON emission, and document parsing
  for plain text, Markdown, HTML (with reader mode), PDF (`pdftotext`), and DOCX
  (`unzip`). URL fetching via `curl`/`wget` with SSRF guards.
- **`pika`** — a pipeline‑friendly CLI that parses input to clean text or a JSON
  token stream and can fetch URLs.
- **`pika-tui`** — an optional terminal reader rendering the focal crosshair.
- **Web service** — an optional Node.js service for shareable reading links and
  a versioned, token‑authenticated HTTP API, with an admin console for issuing
  tokens. Disabled public API by default; file‑based storage; rate limiting.
- **Web app** — an optional Vite + React reader with paste/URL/file input,
  adjustable speed, and share links.
- **Tooling** — interactive `setup.sh` installer, `pkg-config` support, and
  unit tests (`make check`).

[1.0.0]: https://github.com/rofessori/pika/releases/tag/v1.0.0
