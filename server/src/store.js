// Persistent store for shareable readings. Each reading is one JSON file
// under <dataDir>/readings/<id>.json — no database to run or back up, which
// keeps the deployment tiny. An in-memory index tracks expiry and counts.
import fs from 'node:fs';
import fsp from 'node:fs/promises';
import path from 'node:path';
import config from './config.js';
import { shareId } from './ids.js';
import { notFound, payloadTooLarge, serverError } from './errors.js';

const dir = path.join(config.dataDir, 'readings');
const index = new Map(); // id -> expiresAt (ms)

export function initStore() {
  fs.mkdirSync(dir, { recursive: true });
  for (const file of fs.readdirSync(dir)) {
    if (!file.endsWith('.json')) continue;
    try {
      const r = JSON.parse(fs.readFileSync(path.join(dir, file), 'utf8'));
      index.set(r.id, r.expiresAt || 0);
    } catch { /* skip corrupt entry */ }
  }
  pruneExpired();
  setInterval(pruneExpired, 6 * 60 * 60 * 1000).unref(); // every 6h
}

function fileFor(id) {
  return path.join(dir, `${id}.json`);
}

function deriveTitle(tokens) {
  return tokens.slice(0, 9).map((t) => t.t).join(' ').slice(0, 120);
}

export async function createReading({ doc, wpm, source }) {
  if (index.size >= config.limits.maxReadings) {
    pruneExpired();
    if (index.size >= config.limits.maxReadings)
      throw serverError('storage is full; please try again later');
  }
  // Generate a unique id, retrying on the rare collision.
  let id;
  for (let i = 0; i < 6; i++) {
    id = shareId(6 + Math.floor(i / 2)); // grow length if we keep colliding
    if (!index.has(id) && !fs.existsSync(fileFor(id))) break;
  }
  const now = Date.now();
  const reading = {
    id,
    version: 1,
    createdAt: now,
    expiresAt: now + config.limits.readingTtlDays * 86400_000,
    wpm,
    title: deriveTitle(doc.tokens),
    source, // { type: 'text'|'url'|'file', name }
    count: doc.count,
    tokens: doc.tokens,
  };
  const json = JSON.stringify(reading);
  if (json.length > config.limits.fileBytes * 6)
    throw payloadTooLarge('reading is too large to store');
  await fsp.writeFile(fileFor(id), json, { flag: 'wx' });
  index.set(id, reading.expiresAt);
  return reading;
}

export async function getReading(id) {
  if (!/^[0-9A-Z]{4,12}$/.test(id)) throw notFound('reading not found');
  let raw;
  try {
    raw = await fsp.readFile(fileFor(id), 'utf8');
  } catch {
    throw notFound('reading not found');
  }
  const reading = JSON.parse(raw);
  if (reading.expiresAt && reading.expiresAt <= Date.now()) {
    await removeReading(id);
    throw notFound('reading has expired');
  }
  return reading;
}

export async function removeReading(id) {
  index.delete(id);
  await fsp.rm(fileFor(id), { force: true });
}

export function stats() {
  return { readings: index.size, maxReadings: config.limits.maxReadings };
}

function pruneExpired() {
  const now = Date.now();
  for (const [id, expiresAt] of index) {
    if (expiresAt && expiresAt <= now) {
      index.delete(id);
      fs.rmSync(fileFor(id), { force: true });
    }
  }
}
