// Shared reading logic used by both the first-party app routes and the
// public /v1 API, so the two never drift apart. Routes stay thin: they handle
// auth and shape, this module does the work.
import config from './config.js';
import { tokenizeBuffer, tokenizeUrl, clampWpm } from './engine.js';
import { createReading } from './store.js';
import { badRequest, payloadTooLarge } from './errors.js';

function parseBool(v, def = false) {
  if (v === undefined || v === null || v === '') return def;
  if (typeof v === 'boolean') return v;
  return /^(1|true|yes|on)$/i.test(String(v));
}

// Turn a request (JSON body or multipart upload) into a parsed token doc.
async function parseInput(req) {
  const body = req.body || {};
  const wpm = clampWpm(body.wpm);
  const format = body.format || 'auto';

  if (req.file) {
    if (req.file.size > config.limits.fileBytes)
      throw payloadTooLarge(`file exceeds ${config.limits.fileBytes} bytes`);
    const reader = parseBool(body.reader, true);
    const doc = await tokenizeBuffer(req.file.buffer, {
      wpm, format, reader, name: req.file.originalname,
    });
    return { doc, wpm, source: { type: 'file', name: req.file.originalname || null } };
  }

  if (typeof body.url === 'string' && body.url.trim()) {
    const reader = parseBool(body.reader, true);
    const doc = await tokenizeUrl(body.url.trim(), { wpm, format, reader });
    return { doc, wpm, source: { type: 'url', name: body.url.trim() } };
  }

  if (typeof body.text === 'string' && body.text.length) {
    const buf = Buffer.from(body.text, 'utf8');
    if (buf.length > config.limits.textBytes)
      throw payloadTooLarge(`text exceeds ${config.limits.textBytes} bytes`);
    // Honour the requested format. With 'auto' the engine detects pasted HTML
    // or page source and reduces it to plain text; plain prose passes through.
    const reader = parseBool(body.reader, false);
    const doc = await tokenizeBuffer(buf, { wpm, format, reader });
    return { doc, wpm, source: { type: 'text', name: null } };
  }

  throw badRequest('provide one of: text, url, or a file upload');
}

export async function createStoredReading(req) {
  const { doc, wpm, source } = await parseInput(req);
  return createReading({ doc, wpm, source });
}

export async function parseTokenStream(req) {
  return parseInput(req);
}

// ---- response shaping ----------------------------------------------------

export function shareUrl(req, id) {
  if (config.baseUrl) return `${config.baseUrl}/${id}`;
  return `${req.protocol}://${req.get('host')}/${id}`;
}

export function readingResource(reading, req) {
  return {
    object: 'reading',
    id: reading.id,
    url: shareUrl(req, reading.id),
    title: reading.title,
    wpm: reading.wpm,
    count: reading.count,
    source: reading.source,
    created_at: new Date(reading.createdAt).toISOString(),
    expires_at: reading.expiresAt ? new Date(reading.expiresAt).toISOString() : null,
    tokens: reading.tokens,
  };
}

export function streamResource({ doc, wpm, source }) {
  return {
    object: 'token_stream',
    wpm,
    count: doc.count,
    source,
    tokens: doc.tokens,
  };
}
