// A single, consistent error shape for the whole API:
//   { "error": { "code": "...", "message": "...", "status": <int> } }
// plus an async route wrapper so handlers can simply `throw`.

export class ApiError extends Error {
  constructor(status, code, message) {
    super(message);
    this.status = status;
    this.code = code;
  }
}

export const badRequest   = (m) => new ApiError(400, 'invalid_request', m);
export const unauthorized = (m = 'Authentication required') =>
  new ApiError(401, 'unauthorized', m);
export const forbidden    = (m = 'Not permitted') => new ApiError(403, 'forbidden', m);
export const notFound     = (m = 'Resource not found') => new ApiError(404, 'not_found', m);
export const payloadTooLarge = (m) => new ApiError(413, 'payload_too_large', m);
export const unprocessable = (m) => new ApiError(422, 'unprocessable', m);
export const rateLimited  = (m = 'Too many requests') =>
  new ApiError(429, 'rate_limited', m);
export const serverError  = (m = 'Internal server error') =>
  new ApiError(500, 'internal_error', m);

export function sendError(res, err) {
  const status = err instanceof ApiError ? err.status : 500;
  const code = err instanceof ApiError ? err.code : 'internal_error';
  // Never leak internal failure details to clients.
  const message = status >= 500 ? 'Internal server error' : err.message;
  if (!res.headersSent) res.status(status).json({ error: { code, message, status } });
}

export const asyncHandler = (fn) => (req, res, next) =>
  Promise.resolve(fn(req, res, next)).catch(next);
