# HTTP API

The optional web service exposes two HTTP surfaces:

- **First‑party app endpoints** under `/app`, used by the bundled web reader.
  They are same‑origin and rate‑limited, and require no token.
- **The developer API** under `/v1`, authenticated with API tokens. It is
  **disabled by default** and enabled by setting `PUBLIC_API=true`.

Both are thin layers over the same engine, so they behave identically.

## Conventions

- All requests and responses are JSON, except file uploads (`multipart/form-data`).
- Every response includes an `X-Request-Id` header.
- Rate‑limited responses include `X-RateLimit-Limit`, `X-RateLimit-Remaining`,
  and `X-RateLimit-Reset` headers.
- Errors share one shape:

  ```json
  { "error": { "code": "invalid_request", "message": "…", "status": 400 } }
  ```

  Common codes: `invalid_request` (400), `unauthorized` (401), `forbidden`
  (403), `not_found` (404), `payload_too_large` (413), `unprocessable` (422),
  `rate_limited` (429), `internal_error` (500).

## Objects

A **token** is one displayed chunk:

```json
{ "t": "reading", "orp": 2, "ncp": 7, "w": 1.0 }
```

`t` is the text, `orp` is the focal character index (in codepoints), `ncp` is
the codepoint count, and `w` is the dwell weight relative to the base rate.

A **reading** is a stored, shareable document:

```json
{
  "object": "reading",
  "id": "A1B2C3",
  "url": "https://your-host.example/A1B2C3",
  "title": "The quick brown fox …",
  "wpm": 350,
  "count": 9,
  "source": { "type": "text", "name": null },
  "created_at": "2026-01-01T00:00:00.000Z",
  "expires_at": "2026-01-31T00:00:00.000Z",
  "tokens": [ /* token objects */ ]
}
```

A **token stream** is the same data without being stored (no `id`/`url`):

```json
{ "object": "token_stream", "wpm": 350, "count": 9,
  "source": { "type": "text", "name": null }, "tokens": [ /* … */ ] }
```

## Input

Endpoints that create readings accept exactly one source, either as a JSON body
or as `multipart/form-data`:

| Field    | Type    | Notes                                                        |
| -------- | ------- | ------------------------------------------------------------ |
| `text`   | string  | Inline text. Capped by `MAX_TEXT_BYTES` (default 1 MiB).     |
| `url`    | string  | `http(s)` URL; fetched with SSRF protection.                 |
| `file`   | file    | Multipart upload. Capped by `MAX_FILE_BYTES` (default 8 MiB).|
| `wpm`    | number  | Base words per minute (50–1500, default 350).                |
| `reader` | boolean | HTML reader mode (default `true` for URLs and files).        |
| `format` | string  | `auto` (default), `text`, `md`, `html`, `pdf`, `docx`.       |

## First‑party app endpoints

| Method | Path                 | Description                                  |
| ------ | -------------------- | -------------------------------------------- |
| POST   | `/app/readings`      | Parse, store, and return a shareable reading |
| POST   | `/app/preview`       | Parse and return a token stream (not stored) |
| GET    | `/app/readings/{id}` | Fetch a stored reading                       |

```sh
curl -s -X POST http://localhost:8787/app/readings \
  -H 'content-type: application/json' \
  -d '{"text":"Reading on a fixed point is fast.","wpm":400}'
```

## Developer API (`/v1`)

Requires `Authorization: Bearer <token>` and `PUBLIC_API=true`. Tokens are
issued from the admin console (see below).

| Method | Path                | Description                                            |
| ------ | ------------------- | ------------------------------------------------------ |
| POST   | `/v1/readings`      | Create a reading. `store=false` returns a stream only. |
| POST   | `/v1/tokenize`      | Parse and return a token stream (never stored).        |
| GET    | `/v1/readings/{id}` | Fetch a stored reading.                                |
| DELETE | `/v1/readings/{id}` | Delete a stored reading.                               |

```sh
curl -s -X POST https://your-host.example/v1/tokenize \
  -H 'authorization: Bearer pk_xxxxxxxxxxxxxxxxxxxxxxxx' \
  -H 'content-type: application/json' \
  -d '{"url":"https://example.com/article","reader":true}'
```

The API is versioned: breaking changes will ship under a new prefix (`/v2`).

## Admin console

The console is served from a secret, unguessable path generated at first start
and printed to the log (and saved to `server/data/admin-access.txt`). Sign in
with the one‑time passkey to manage tokens.

| Method | Path                          | Description                          |
| ------ | ----------------------------- | ------------------------------------ |
| POST   | `/{adminPath}/session`        | Exchange the passkey for a session   |
| GET    | `/{adminPath}/api/tokens`     | List active API tokens               |
| POST   | `/{adminPath}/api/tokens`     | Create a token (returned once)       |
| DELETE | `/{adminPath}/api/tokens/{id}`| Revoke a token                       |
| GET    | `/{adminPath}/api/stats`      | Counts and public‑API status         |

Tokens are stored only as SHA‑256 hashes; the plaintext is shown a single time
on creation. Revoking a token takes effect immediately.

## Health

`GET /healthz` → `{ "status": "ok", "version": "1.0.0", "public_api": false }`.
