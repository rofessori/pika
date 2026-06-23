// pika web service entrypoint.
//
// Responsibilities, in order: security headers and request ids; body parsing
// with limits; first-party app routes (/app); public developer API (/v1);
// the secret admin console; static hosting of the built frontend with an SPA
// fallback so share links like /A1B2C3 resolve. Heavy lifting is delegated to
// the `pika` CLI and the modules in this directory.
import express from 'express';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';

import config from './config.js';
import { ApiError, sendError, badRequest, notFound, payloadTooLarge } from './errors.js';
import { initStore } from './store.js';
import { initAuth, adminPath } from './auth.js';
import appRoutes from './routes/app.js';
import v1Routes from './routes/v1.js';
import { adminRouter } from './routes/admin.js';

const VERSION = '1.0.0';

// ---- bootstrap state -----------------------------------------------------
initStore();
const bootstrap = initAuth();
const ADMIN_PATH = adminPath();

const app = express();
app.disable('x-powered-by');
app.set('trust proxy', config.trustProxy);

// ---- cross-cutting middleware -------------------------------------------
app.use((req, res, next) => {
  req.id = crypto.randomUUID();
  res.set('X-Request-Id', req.id);
  res.set('X-Content-Type-Options', 'nosniff');
  res.set('X-Frame-Options', 'DENY');
  res.set('Referrer-Policy', 'no-referrer');
  res.set('Content-Security-Policy',
    "default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; " +
    "script-src 'self' 'unsafe-inline'; connect-src 'self'; base-uri 'none'; " +
    "form-action 'self'; frame-ancestors 'none'");
  next();
});

// Minimal cookie parsing (admin session may also travel as a cookie).
app.use((req, _res, next) => {
  req.cookies = {};
  const header = req.headers.cookie;
  if (header) for (const part of header.split(';')) {
    const i = part.indexOf('=');
    if (i > 0) req.cookies[part.slice(0, i).trim()] = decodeURIComponent(part.slice(i + 1).trim());
  }
  next();
});

const jsonLimit = config.limits.textBytes + 1024 * 1024;
app.use(express.json({ limit: jsonLimit }));
app.use(express.urlencoded({ extended: false, limit: jsonLimit }));

// CORS for the public developer API only (the first-party app is same-origin).
app.use('/v1', (req, res, next) => {
  res.set('Access-Control-Allow-Origin', '*');
  res.set('Access-Control-Allow-Methods', 'GET,POST,DELETE,OPTIONS');
  res.set('Access-Control-Allow-Headers', 'Authorization,Content-Type');
  res.set('Access-Control-Max-Age', '600');
  if (req.method === 'OPTIONS') return res.sendStatus(204);
  next();
});

// ---- health --------------------------------------------------------------
app.get('/healthz', (_req, res) => {
  res.json({ status: 'ok', version: VERSION, public_api: config.publicApi });
});

// ---- routes --------------------------------------------------------------
app.use('/app', appRoutes);
app.use('/v1', v1Routes);
app.use(`/${ADMIN_PATH}`, adminRouter(ADMIN_PATH));

// JSON 404 for unmatched API paths (must precede the SPA fallback).
app.use(['/app', '/v1', `/${ADMIN_PATH}`], (_req, _res, next) =>
  next(notFound('endpoint not found')));

// ---- static frontend + SPA fallback -------------------------------------
const indexHtml = path.join(config.webDir, 'index.html');
const hasWeb = fs.existsSync(indexHtml);
if (hasWeb) {
  app.use(express.static(config.webDir, { index: false, maxAge: '1h' }));
}

app.get('*', (req, res, next) => {
  if (req.method !== 'GET') return next();
  if (hasWeb) return res.sendFile(indexHtml);
  res.status(200).type('html').send(FALLBACK_PAGE);
});

// ---- error handling ------------------------------------------------------
app.use((err, _req, res, _next) => {
  let e = err;
  if (!(e instanceof ApiError)) {
    if (e?.type === 'entity.too.large' || e?.status === 413) e = payloadTooLarge('request body too large');
    else if (e?.code === 'LIMIT_FILE_SIZE') e = payloadTooLarge('uploaded file is too large');
    else if (e?.code === 'LIMIT_FILE_COUNT') e = badRequest('only one file may be uploaded');
    else if (e?.type === 'entity.parse.failed' || e?.status === 400) e = badRequest('invalid request body');
    else console.error('[pika] unhandled error:', err);
  }
  sendError(res, e);
});

// ---- start ---------------------------------------------------------------
const FALLBACK_PAGE = `<!doctype html><meta charset="utf-8">
<title>pika</title><body style="background:#0a0a0b;color:#e8e8ea;font-family:system-ui;
display:grid;place-items:center;height:100vh;margin:0">
<div style="text-align:center"><h1 style="font-weight:600">p<span style="color:#d62828">i</span>ka</h1>
<p style="color:#8a8a92">API is running. Build the web frontend (<code>cd web &amp;&amp; npm run build</code>)
to serve the reader here.</p></div></body>`;

const server = app.listen(config.port, config.host, () => {
  banner();
});
server.on('error', (e) => {
  console.error('[pika] failed to start:', e.message);
  process.exit(1);
});

for (const sig of ['SIGINT', 'SIGTERM'])
  process.on(sig, () => server.close(() => process.exit(0)));

function banner() {
  const base = config.baseUrl || `http://${config.host}:${config.port}`;
  console.log(`\n  pika ${VERSION} — listening on http://${config.host}:${config.port}`);
  console.log(`  public API: ${config.publicApi ? 'enabled' : 'disabled'}  ·  web app: ${hasWeb ? 'served' : 'not built'}`);
  if (bootstrap) {
    console.log('\n  ───────────────────────────────────────────────────────────');
    console.log('  Administrator access (shown once — also saved to data/admin-access.txt):');
    console.log(`    console : ${base}/${bootstrap.adminPath}/`);
    console.log(`    passkey : ${bootstrap.passkey}`);
    console.log('  ───────────────────────────────────────────────────────────\n');
  } else {
    console.log(`  admin console: ${base}/${ADMIN_PATH}/  (passkey in data/admin-access.txt)\n`);
  }
}
