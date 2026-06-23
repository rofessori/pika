// First-party endpoints consumed by the bundled web app. Same-origin, rate
// limited, no API token required. Mounted at /app.
import { Router } from 'express';
import multer from 'multer';
import config from '../config.js';
import { asyncHandler, notFound } from '../errors.js';
import { rateLimiter } from '../ratelimit.js';
import {
  createStoredReading, parseTokenStream, readingResource, streamResource,
} from '../service.js';
import { getReading } from '../store.js';

const upload = multer({
  storage: multer.memoryStorage(),
  limits: { fileSize: config.limits.fileBytes, files: 1 },
});

const router = Router();

router.use(rateLimiter({
  windowMs: config.rateLimit.windowMs,
  max: config.rateLimit.appMax,
  keyFn: (req) => `app:${req.ip}`,
}));

// Create and store a shareable reading.
router.post('/readings', upload.single('file'), asyncHandler(async (req, res) => {
  const reading = await createStoredReading(req);
  res.status(201).json(readingResource(reading, req));
}));

// Parse without storing — used for instant in-page preview.
router.post('/preview', upload.single('file'), asyncHandler(async (req, res) => {
  const result = await parseTokenStream(req);
  res.json(streamResource(result));
}));

// Fetch a stored reading by its share id.
router.get('/readings/:id', asyncHandler(async (req, res) => {
  const reading = await getReading(req.params.id);
  if (!reading) throw notFound('reading not found');
  res.json(readingResource(reading, req));
}));

export default router;
