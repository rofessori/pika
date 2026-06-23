// Public, versioned developer API. Every request must present a valid API
// token issued by an administrator: `Authorization: Bearer pk_...`.
// Disabled entirely unless PUBLIC_API is enabled. Mounted at /v1.
import { Router } from 'express';
import multer from 'multer';
import config from '../config.js';
import { asyncHandler, forbidden } from '../errors.js';
import { rateLimiter } from '../ratelimit.js';
import { requireApiToken } from '../auth.js';
import {
  createStoredReading, parseTokenStream, readingResource, streamResource,
} from '../service.js';
import { getReading, removeReading } from '../store.js';

const upload = multer({
  storage: multer.memoryStorage(),
  limits: { fileSize: config.limits.fileBytes, files: 1 },
});

const router = Router();

// Gate the whole surface behind the public-API switch.
router.use((req, res, next) => {
  if (!config.publicApi)
    return next(forbidden('the public API is disabled on this server'));
  next();
});

router.use(requireApiToken);

router.use(rateLimiter({
  windowMs: config.rateLimit.windowMs,
  max: config.rateLimit.apiMax,
  keyFn: (req) => `v1:${req.apiToken.id}`,
}));

// Create a reading. By default it is stored and gets a shareable URL; pass
// store=false to receive the token stream without persisting it.
router.post('/readings', upload.single('file'), asyncHandler(async (req, res) => {
  const store = req.body?.store === undefined || /^(1|true|yes|on)$/i.test(String(req.body.store));
  if (!store) {
    const result = await parseTokenStream(req);
    return res.json(streamResource(result));
  }
  const reading = await createStoredReading(req);
  res.status(201).json(readingResource(reading, req));
}));

// Stateless tokenization: parse input and return the stream, never stored.
router.post('/tokenize', upload.single('file'), asyncHandler(async (req, res) => {
  const result = await parseTokenStream(req);
  res.json(streamResource(result));
}));

router.get('/readings/:id', asyncHandler(async (req, res) => {
  const reading = await getReading(req.params.id);
  res.json(readingResource(reading, req));
}));

router.delete('/readings/:id', asyncHandler(async (req, res) => {
  await getReading(req.params.id); // 404s if missing/expired
  await removeReading(req.params.id);
  res.json({ object: 'reading', id: req.params.id, deleted: true });
}));

export default router;
