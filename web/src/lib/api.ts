import type { Reading, TokenStream, ApiErrorBody } from './types';

export class ApiError extends Error {
  code: string;
  status: number;
  constructor(status: number, code: string, message: string) {
    super(message);
    this.status = status;
    this.code = code;
  }
}

async function parse<T>(res: Response): Promise<T> {
  const data = await res.json().catch(() => null);
  if (!res.ok) {
    const body = data as ApiErrorBody | null;
    throw new ApiError(
      res.status,
      body?.error?.code ?? 'error',
      body?.error?.message ?? res.statusText,
    );
  }
  return data as T;
}

export interface ReadInput {
  text?: string;
  url?: string;
  file?: File;
  wpm: number;
  reader: boolean;
  format?: string;
}

function toBody(input: ReadInput): BodyInit {
  if (input.file) {
    const fd = new FormData();
    fd.append('file', input.file);
    fd.append('wpm', String(input.wpm));
    fd.append('reader', String(input.reader));
    if (input.format) fd.append('format', input.format);
    return fd;
  }
  return JSON.stringify(input);
}

function headers(input: ReadInput): HeadersInit {
  return input.file ? {} : { 'content-type': 'application/json' };
}

// Parse without storing — for instant preview.
export function preview(input: ReadInput): Promise<TokenStream> {
  return fetch('/app/preview', {
    method: 'POST',
    headers: headers(input),
    body: toBody(input),
  }).then((r) => parse<TokenStream>(r));
}

// Parse and store, returning a shareable reading.
export function createReading(input: ReadInput): Promise<Reading> {
  return fetch('/app/readings', {
    method: 'POST',
    headers: headers(input),
    body: toBody(input),
  }).then((r) => parse<Reading>(r));
}

export function getReading(id: string): Promise<Reading> {
  return fetch(`/app/readings/${encodeURIComponent(id)}`).then((r) =>
    parse<Reading>(r),
  );
}
