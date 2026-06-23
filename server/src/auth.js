// Authentication state: the admin passkey, the secret admin console path, and
// the set of issued API tokens. Secrets are generated once at first start and
// only their SHA-256 hashes are persisted — the plaintext is shown a single
// time so the operator can record it.
//
// The admin authenticates with the passkey at the secret path; in return they
// get a short-lived session. Admins issue API tokens, which third-party
// callers present as `Authorization: Bearer <token>` against the /v1 API.
import fs from 'node:fs';
import fsp from 'node:fs/promises';
import path from 'node:path';
import config from './config.js';
import { apiToken, passkey, sha256, urlToken, safeEqual } from './ids.js';
import { unauthorized } from './errors.js';

const authFile = path.join(config.dataDir, 'auth.json');
const accessFile = path.join(config.dataDir, 'admin-access.txt');

let state = null;                         // persisted: { adminPath, passkeyHash, tokens, createdAt }
const sessions = new Map();               // sessionToken -> expiresAt (ms)
const SESSION_TTL_MS = 12 * 60 * 60 * 1000;

let saveTimer = null;
function scheduleSave() {
  if (saveTimer) return;
  saveTimer = setTimeout(() => { saveTimer = null; saveNow(); }, 2000);
}
function saveNow() {
  fs.writeFileSync(authFile, JSON.stringify(state, null, 2), { mode: 0o600 });
}

// Initialise on startup. Returns the freshly generated credentials when the
// server is being set up for the first time, otherwise null.
export function initAuth() {
  fs.mkdirSync(config.dataDir, { recursive: true });
  if (fs.existsSync(authFile)) {
    state = JSON.parse(fs.readFileSync(authFile, 'utf8'));
    if (!Array.isArray(state.tokens)) state.tokens = [];
    return null;
  }
  const key = passkey();
  state = {
    adminPath: urlToken(18),
    passkeyHash: sha256(key),
    tokens: [],
    createdAt: Date.now(),
  };
  saveNow();
  writeAccessFile(key);
  return { passkey: key, adminPath: state.adminPath };
}

function writeAccessFile(key) {
  const body =
`pika — administrator access
============================
Generated: ${new Date().toISOString()}

Admin console path : /${state.adminPath}/
Admin passkey      : ${key}

Keep this file secret. The passkey is shown only once and is stored on the
server only as a hash. To rotate it, delete data/auth.json and restart (this
also invalidates every issued API token).
`;
  fs.writeFileSync(accessFile, body, { mode: 0o600 });
}

export const adminPath = () => state.adminPath;

// ---- admin session -------------------------------------------------------
export function verifyPasskey(input) {
  return !!input && safeEqual(sha256(String(input)), state.passkeyHash);
}

export function createSession() {
  const token = urlToken(24);
  sessions.set(token, Date.now() + SESSION_TTL_MS);
  return { token, expiresAt: Date.now() + SESSION_TTL_MS };
}

export function verifySession(token) {
  const exp = sessions.get(token);
  if (!exp) return false;
  if (exp <= Date.now()) { sessions.delete(token); return false; }
  return true;
}

export function destroySession(token) { sessions.delete(token); }

// ---- API tokens ----------------------------------------------------------
export function createApiToken(label) {
  const token = apiToken();
  const record = {
    id: urlToken(8),
    label: String(label || 'untitled').slice(0, 80),
    hash: sha256(token),
    last4: token.slice(-4),
    createdAt: Date.now(),
    lastUsedAt: null,
    revoked: false,
  };
  state.tokens.push(record);
  saveNow();
  return { token, record: publicToken(record) };
}

export function verifyApiToken(token) {
  if (!token) return null;
  const hash = sha256(token);
  const rec = state.tokens.find((t) => !t.revoked && safeEqual(t.hash, hash));
  if (!rec) return null;
  rec.lastUsedAt = Date.now();
  scheduleSave();
  return rec;
}

export function listApiTokens() {
  return state.tokens.filter((t) => !t.revoked).map(publicToken);
}

export function revokeApiToken(id) {
  const rec = state.tokens.find((t) => t.id === id && !t.revoked);
  if (!rec) return false;
  rec.revoked = true;
  rec.revokedAt = Date.now();
  saveNow();
  return true;
}

function publicToken(t) {
  return {
    id: t.id,
    label: t.label,
    last4: t.last4,
    createdAt: t.createdAt,
    lastUsedAt: t.lastUsedAt,
  };
}

// ---- middleware ----------------------------------------------------------

// Require a valid admin session (cookie or Bearer) for console API routes.
export function requireAdmin(req, _res, next) {
  const bearer = (req.get('authorization') || '').replace(/^Bearer\s+/i, '');
  const cookie = req.cookies?.pika_admin;
  const token = bearer || cookie;
  if (!token || !verifySession(token)) return next(unauthorized('admin session required'));
  next();
}

// Require a valid API token for the public /v1 API.
export function requireApiToken(req, _res, next) {
  const token = (req.get('authorization') || '').replace(/^Bearer\s+/i, '');
  const rec = verifyApiToken(token);
  if (!rec) return next(unauthorized('a valid API token is required'));
  req.apiToken = rec;
  next();
}
