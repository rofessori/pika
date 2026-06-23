// Shapes returned by the pika web API.

export interface Token {
  t: string;   // the word/chunk
  orp: number; // index (in codepoints) of the focal character
  ncp: number; // number of codepoints
  w: number;   // dwell weight relative to the base rate
}

export interface ReadingSource {
  type: 'text' | 'url' | 'file';
  name: string | null;
}

export interface TokenStream {
  object: 'token_stream';
  wpm: number;
  count: number;
  source: ReadingSource;
  tokens: Token[];
}

export interface Reading {
  object: 'reading';
  id: string;
  url: string;
  title: string;
  wpm: number;
  count: number;
  source: ReadingSource;
  created_at: string;
  expires_at: string | null;
  tokens: Token[];
}

export interface ApiErrorBody {
  error: { code: string; message: string; status: number };
}
