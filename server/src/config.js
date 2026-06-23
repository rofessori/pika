// Central, environment-driven configuration. Every value has a sensible
// default so the server runs out of the box; override via environment
// variables (see .env.example) for production.
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const env = process.env;

const int = (name, def) => {
  const v = parseInt(env[name], 10);
  return Number.isFinite(v) ? v : def;
};
const bool = (name, def) => {
  if (env[name] == null) return def;
  return /^(1|true|yes|on)$/i.test(env[name]);
};

export const config = {
  host: env.HOST || '127.0.0.1',
  port: int('PORT', 8787),

  // Path to the compiled `pika` CLI (installed from core/).
  pikaBin: env.PIKA_BIN || 'pika',

  // Where readings and auth state are persisted.
  dataDir: env.DATA_DIR || path.join(here, '..', 'data'),

  // Built frontend (vite output). Served as static files when present.
  webDir: env.WEB_DIR || path.join(here, '..', '..', 'web', 'dist'),

  // Absolute base URL used when building shareable links. When empty it is
  // derived from the incoming request, which is correct behind most proxies
  // that set X-Forwarded-* headers.
  baseUrl: (env.BASE_URL || '').replace(/\/$/, ''),

  // When false (default), the public /v1 developer API is disabled and only
  // the first-party web app endpoints work. Tokens can still be managed.
  publicApi: bool('PUBLIC_API', false),

  limits: {
    textBytes: int('MAX_TEXT_BYTES', 1024 * 1024),       // 1 MiB pasted text
    fileBytes: int('MAX_FILE_BYTES', 8 * 1024 * 1024),   // 8 MiB upload / URL
    maxTokens: int('MAX_TOKENS', 200000),                // cap stored stream
    readingTtlDays: int('READING_TTL_DAYS', 30),
    maxReadings: int('MAX_READINGS', 50000),
    fetchTimeoutMs: int('FETCH_TIMEOUT_MS', 20000),
    parseTimeoutMs: int('PARSE_TIMEOUT_MS', 15000),
  },

  rateLimit: {
    windowMs: int('RATE_WINDOW_MS', 60000),
    appMax: int('RATE_APP_MAX', 60),    // per IP per window, first-party app
    apiMax: int('RATE_API_MAX', 120),   // per token per window, /v1
  },

  // Trust X-Forwarded-* from this many proxy hops (nginx/cloudflared = 1).
  trustProxy: int('TRUST_PROXY', 1),
};

export default config;
