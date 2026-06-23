# Security

## Reporting a vulnerability

Please report security issues privately rather than opening a public issue. Use
GitHub's **"Report a vulnerability"** feature (Security ▸ Advisories) on the
repository. We aim to acknowledge reports within a few days and will coordinate
a fix and disclosure with you.

## Security model

The library and CLI process untrusted documents. The optional web service is
the part exposed to a network, and is designed defensively:

- **Authentication.** The public developer API requires a bearer token issued
  by an administrator and is disabled by default (`PUBLIC_API`). The admin
  console lives at an unguessable path generated at first start and is protected
  by a passkey. The passkey and all API tokens are stored only as SHA‑256
  hashes; plaintext is shown once. Comparisons are constant‑time.
- **Request limits.** Inline text, uploaded files, fetched pages, and the number
  of tokens per reading are all capped (configurable). Both API surfaces are
  rate‑limited.
- **SSRF protection.** Server‑side URL fetching refuses `file://` and other
  non‑HTTP schemes and rejects loopback, private, link‑local, and
  carrier‑grade‑NAT addresses.
- **Subprocess safety.** External tools (`pdftotext`, `unzip`, `curl`/`wget`)
  are executed without a shell and with argument vectors, so document content
  cannot be interpreted as shell commands.
- **HTTP hardening.** Security headers (`Content-Security-Policy`,
  `X-Content-Type-Options`, `X-Frame-Options`, `Referrer-Policy`) are sent on
  every response, and error responses never leak internal details.

## Hardening checklist for operators

- Keep `PUBLIC_API=false` unless you intend to expose the developer API.
- Run the service as an unprivileged user.
- Restrict access to the data directory; it contains `auth.json` (secret
  hashes) and `admin-access.txt`.
- Put the service behind a TLS‑terminating proxy or a Cloudflare Tunnel, and set
  `TRUST_PROXY` correctly so rate limiting sees real client addresses.
