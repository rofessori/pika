// Thin, safe wrapper around the compiled `pika` CLI. The CLI is the single
// source of truth for parsing and tokenization, so the web layer never
// reimplements that logic — it just feeds bytes in and reads JSON out.
import { execFile } from 'node:child_process';
import config from './config.js';
import { badRequest, serverError, unprocessable } from './errors.js';

const VALID_FORMATS = new Set(['auto', 'text', 'md', 'markdown', 'html', 'pdf', 'docx']);

function clampWpm(wpm) {
  const n = parseInt(wpm, 10);
  if (!Number.isFinite(n)) return 350;
  return Math.min(1500, Math.max(50, n));
}

// Run `pika` with args, optionally piping `input` (Buffer) to stdin.
function runPika(args, input, maxBuffer) {
  return new Promise((resolve, reject) => {
    const child = execFile(
      config.pikaBin,
      args,
      { timeout: config.limits.parseTimeoutMs, maxBuffer, encoding: 'utf8' },
      (err, stdout, stderr) => {
        if (err) {
          if (err.code === 'ENOENT')
            return reject(serverError('pika binary not found; install the core'));
          if (err.killed) return reject(unprocessable('parsing timed out'));
          // Non-zero exit: surface the CLI's message as a client error.
          const msg = (stderr || '').trim().replace(/^pika:\s*/, '') || 'could not parse input';
          return reject(unprocessable(msg));
        }
        resolve(stdout);
      },
    );
    if (input) {
      child.stdin.on('error', () => {}); // ignore EPIPE if the child exits early
      child.stdin.end(input);
    }
  });
}

function parseOutput(stdout) {
  let doc;
  try {
    doc = JSON.parse(stdout);
  } catch {
    throw serverError('unexpected parser output');
  }
  if (doc.count > config.limits.maxTokens)
    throw unprocessable(`document too long (${doc.count} words; limit ${config.limits.maxTokens})`);
  return doc;
}

// Tokenize raw bytes (a Buffer) of a document.
export async function tokenizeBuffer(buf, { wpm, format = 'auto', reader = false, name } = {}) {
  if (!VALID_FORMATS.has(format)) throw badRequest(`unknown format '${format}'`);
  const args = ['--json', '--wpm', String(clampWpm(wpm)), '--format', format,
    '--max-bytes', String(config.limits.fileBytes)];
  if (reader) args.push('--reader');
  if (name) args.push('--name', name);
  const maxBuffer = config.limits.fileBytes * 4 + 1024 * 1024;
  return parseOutput(await runPika(args, buf, maxBuffer));
}

// Fetch a URL (with SSRF protection) and tokenize it.
export async function tokenizeUrl(url, { wpm, format = 'auto', reader = true } = {}) {
  if (!/^https?:\/\//i.test(url)) throw badRequest('url must start with http:// or https://');
  if (!VALID_FORMATS.has(format)) throw badRequest(`unknown format '${format}'`);
  const args = ['--json', '--wpm', String(clampWpm(wpm)), '--format', format,
    '--url', url, '--safe-fetch', '--max-bytes', String(config.limits.fileBytes)];
  if (reader) args.push('--reader');
  const maxBuffer = config.limits.fileBytes * 4 + 1024 * 1024;
  return parseOutput(await runPika(args, null, maxBuffer));
}

export { clampWpm };
