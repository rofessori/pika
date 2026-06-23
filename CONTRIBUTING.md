# Contributing

Thanks for your interest in improving `pika`. This document covers how to build,
test, and submit changes.

## Building

The library and CLI need only a C11 compiler and `make`:

```sh
make            # build the library, CLI, and TUI
make check      # run the unit tests (must pass)
make clean
```

The optional web service and frontend need Node.js 18+:

```sh
cd server && npm install
cd ../web && npm install && npm run build
```

To run the stack locally:

```sh
# terminal 1 — the API (point it at your built CLI)
cd server && PIKA_BIN=../core/pika npm run dev

# terminal 2 — the frontend with hot reload (proxies to the API)
cd web && npm run dev
```

## Coding style

- **C:** C11. Keep the code warning‑clean under `-Wall -Wextra -Wpedantic`
  (the default flags). No required third‑party libraries in the core — features
  that need heavy dependencies shell out to well‑known tools and degrade
  gracefully when they are absent. Keep functions small and modules focused.
- **JavaScript/TypeScript:** ES modules, small focused files. The server has no
  build step; the frontend is type‑checked (`tsc`) as part of `npm run build`.
- Match the style of the surrounding code. Prefer clarity over cleverness.

## Tests

Add or update tests in `core/tests/` for any change to the library, and make
sure `make check` passes. When changing parsing or tokenization, include a case
that demonstrates the new behaviour.

## Submitting changes

1. Fork the repository and create a topic branch.
2. Make your change with clear, focused commits.
3. Ensure `make check` passes and the build is warning‑clean.
4. Open a pull request describing the change and the motivation.

By contributing you agree that your contributions are licensed under the
project's GPL‑3.0‑or‑later license.
