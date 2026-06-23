// Administrator console, served from the secret path printed at first start.
// The operator signs in with the passkey to manage API tokens. Everything
// here lives under that unguessable path; the management API additionally
// requires a valid admin session.
import { Router } from 'express';
import { asyncHandler, badRequest, unauthorized } from '../errors.js';
import {
  requireAdmin, verifyPasskey, createSession, destroySession,
  createApiToken, listApiTokens, revokeApiToken,
} from '../auth.js';
import config from '../config.js';
import { stats as storeStats } from '../store.js';
import { ADMIN_PAGE } from './adminPage.js';

export function adminRouter(basePath) {
  const router = Router();

  // The console UI (self-contained HTML, no build step).
  router.get('/', (_req, res) => {
    res.type('html').send(ADMIN_PAGE.replace(/__BASE__/g, basePath));
  });

  // Exchange the passkey for a session token.
  router.post('/session', asyncHandler(async (req, res) => {
    if (!verifyPasskey(req.body?.passkey)) throw unauthorized('invalid passkey');
    const session = createSession();
    res.json({ session_token: session.token, expires_at: new Date(session.expiresAt).toISOString() });
  }));

  router.post('/session/logout', requireAdmin, (req, res) => {
    const token = (req.get('authorization') || '').replace(/^Bearer\s+/i, '');
    destroySession(token);
    res.json({ ok: true });
  });

  // Everything below requires an authenticated admin session.
  router.use('/api', requireAdmin);

  router.get('/api/stats', (_req, res) => {
    res.json({
      object: 'stats',
      public_api: config.publicApi,
      tokens: listApiTokens().length,
      ...storeStats(),
    });
  });

  router.get('/api/tokens', (_req, res) => {
    res.json({ object: 'list', data: listApiTokens() });
  });

  router.post('/api/tokens', asyncHandler(async (req, res) => {
    const label = req.body?.label;
    if (label && String(label).length > 80) throw badRequest('label too long');
    const { token, record } = createApiToken(label);
    // `token` is returned exactly once and never stored in plaintext.
    res.status(201).json({ object: 'api_token', token, ...record });
  }));

  router.delete('/api/tokens/:id', asyncHandler(async (req, res) => {
    if (!revokeApiToken(req.params.id)) throw badRequest('unknown token');
    res.json({ object: 'api_token', id: req.params.id, revoked: true });
  }));

  return router;
}
