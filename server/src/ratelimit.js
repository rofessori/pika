// A small in-memory fixed-window rate limiter. No external dependency; fine
// for a single-process deployment. Behind a load balancer you would move this
// to a shared store, but the interface stays the same.
import { rateLimited } from './errors.js';

export function rateLimiter({ windowMs, max, keyFn }) {
  const buckets = new Map(); // key -> { count, resetAt }

  // Opportunistic cleanup so the map cannot grow without bound.
  setInterval(() => {
    const now = Date.now();
    for (const [k, b] of buckets) if (b.resetAt <= now) buckets.delete(k);
  }, windowMs).unref();

  return (req, res, next) => {
    const key = keyFn(req);
    const now = Date.now();
    let b = buckets.get(key);
    if (!b || b.resetAt <= now) {
      b = { count: 0, resetAt: now + windowMs };
      buckets.set(key, b);
    }
    b.count++;
    const remaining = Math.max(0, max - b.count);
    res.set('X-RateLimit-Limit', String(max));
    res.set('X-RateLimit-Remaining', String(remaining));
    res.set('X-RateLimit-Reset', String(Math.ceil(b.resetAt / 1000)));
    if (b.count > max) {
      res.set('Retry-After', String(Math.ceil((b.resetAt - now) / 1000)));
      return next(rateLimited());
    }
    next();
  };
}
