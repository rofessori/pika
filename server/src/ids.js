// Identifier and secret generation.
import crypto from 'node:crypto';

// Share ids: short, uppercase, unambiguous (no 0/O/1/I). Looks like /A1B2C3.
const SHARE_ALPHABET = '23456789ABCDEFGHJKLMNPQRSTUVWXYZ';

export function shareId(len = 6) {
  const bytes = crypto.randomBytes(len);
  let out = '';
  for (let i = 0; i < len; i++) out += SHARE_ALPHABET[bytes[i] % SHARE_ALPHABET.length];
  return out;
}

// URL-safe random string for secret paths and session tokens.
export function urlToken(bytes = 24) {
  return crypto.randomBytes(bytes).toString('base64url');
}

// Developer API token. The "pk_" prefix makes it greppable and easy to spot
// in logs so it can be revoked if leaked.
export function apiToken() {
  return 'pk_' + crypto.randomBytes(24).toString('base64url');
}

// Human-typeable passkey: five groups of five Crockford-base32 chars.
export function passkey() {
  const A = '0123456789ABCDEFGHJKMNPQRSTVWXYZ';
  const bytes = crypto.randomBytes(25);
  let s = '';
  for (let i = 0; i < 25; i++) {
    if (i > 0 && i % 5 === 0) s += '-';
    s += A[bytes[i] % A.length];
  }
  return s;
}

export function sha256(input) {
  return crypto.createHash('sha256').update(input).digest('hex');
}

// Constant-time string comparison that tolerates differing lengths.
export function safeEqual(a, b) {
  const ba = Buffer.from(String(a));
  const bb = Buffer.from(String(b));
  if (ba.length !== bb.length) return false;
  return crypto.timingSafeEqual(ba, bb);
}
