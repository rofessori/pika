import { useRef, useState, type ClipboardEvent } from 'react';
import { ApiError, createReading, preview, type ReadInput } from '../lib/api';
import type { Reading, TokenStream } from '../lib/types';

type Tab = 'text' | 'url' | 'file';
type TextFormat = 'auto' | 'text' | 'html' | 'md';

interface Props {
  onStream: (s: TokenStream) => void;
  onReading: (r: Reading) => void;
}

export function InputPanel({ onStream, onReading }: Props) {
  const [tab, setTab] = useState<Tab>('text');
  const [text, setText] = useState('');
  const [url, setUrl] = useState('');
  const [file, setFile] = useState<File | null>(null);
  const [wpm, setWpm] = useState(350);
  const [reader, setReader] = useState(true);
  const [textFormat, setTextFormat] = useState<TextFormat>('auto');
  const [textReader, setTextReader] = useState(false);
  const [pastedRich, setPastedRich] = useState(false);
  const [busy, setBusy] = useState<'idle' | 'read' | 'share'>('idle');
  const [error, setError] = useState('');
  const fileRef = useRef<HTMLInputElement>(null);

  // When the clipboard carries a rich/HTML representation (e.g. copied from a
  // web page), capture the markup itself so the parser can strip it back down
  // to clean text — instead of silently losing it to plain-text paste.
  function onPasteText(e: ClipboardEvent<HTMLTextAreaElement>) {
    const html = e.clipboardData?.getData('text/html');
    if (!html || !html.trim()) return; // plain paste: let the browser handle it
    e.preventDefault();
    const ta = e.currentTarget;
    const start = ta.selectionStart ?? text.length;
    const end = ta.selectionEnd ?? text.length;
    setText(text.slice(0, start) + html + text.slice(end));
    setTextFormat('html');
    setPastedRich(true);
  }

  function buildInput(): ReadInput | null {
    if (tab === 'text') {
      if (!text.trim()) return null;
      const useReader = textFormat === 'html' || textFormat === 'auto' ? textReader : false;
      return { text, wpm, reader: useReader, format: textFormat };
    }
    if (tab === 'url') {
      if (!url.trim()) return null;
      return { url: url.trim(), wpm, reader };
    }
    if (!file) return null;
    return { file, wpm, reader };
  }

  async function run(kind: 'read' | 'share') {
    const input = buildInput();
    if (!input) { setError('Add some text, a URL, or a file first.'); return; }
    setError('');
    setBusy(kind);
    try {
      if (kind === 'read') onStream(await preview(input));
      else onReading(await createReading(input));
    } catch (e) {
      setError(e instanceof ApiError ? e.message : 'Something went wrong. Please try again.');
    } finally {
      setBusy('idle');
    }
  }

  return (
    <div className="panel">
      <h1 className="brand">p<span className="o">i</span>ka</h1>
      <p className="tagline">Read anything fast — one word at a time, on a fixed point.</p>

      <div className="tabs" role="tablist">
        {(['text', 'url', 'file'] as Tab[]).map((t) => (
          <button key={t} role="tab" aria-selected={tab === t}
            className={`tab ${tab === t ? 'active' : ''}`}
            onClick={() => { setTab(t); setError(''); }}>
            {t === 'text' ? 'Paste text' : t === 'url' ? 'From URL' : 'Upload file'}
          </button>
        ))}
      </div>

      {tab === 'text' && (
        <>
          <textarea className="text-input"
            placeholder="Paste or type anything — plain text, Markdown, or HTML/source code…"
            value={text} onChange={(e) => setText(e.target.value)}
            onPaste={onPasteText} rows={8} />
          <div className="text-options">
            <label className="select-label">
              Treat as
              <select value={textFormat} onChange={(e) => {
                const v = e.target.value as TextFormat;
                setTextFormat(v);
                if (v !== 'html') setPastedRich(false);
              }}>
                <option value="auto">Auto‑detect</option>
                <option value="text">Plain text</option>
                <option value="html">HTML / source code</option>
                <option value="md">Markdown</option>
              </select>
            </label>
            {(textFormat === 'html' || textFormat === 'auto') && (
              <label className="checkbox">
                <input type="checkbox" checked={textReader}
                  onChange={(e) => setTextReader(e.target.checked)} />
                Reader mode
              </label>
            )}
          </div>
          {(textFormat === 'html' || pastedRich) && (
            <p className="hint dim">Formatted/HTML content will be converted to plain text.</p>
          )}
        </>
      )}

      {tab === 'url' && (
        <>
          <input className="url-input" type="url" inputMode="url"
            placeholder="https://example.com/article"
            value={url} onChange={(e) => setUrl(e.target.value)} />
          <label className="checkbox">
            <input type="checkbox" checked={reader}
              onChange={(e) => setReader(e.target.checked)} />
            Reader mode — keep the article, drop menus and ads
          </label>
        </>
      )}

      {tab === 'file' && (
        <div className="file-drop" onClick={() => fileRef.current?.click()}>
          <input ref={fileRef} type="file" hidden
            accept=".txt,.md,.markdown,.html,.htm,.pdf,.docx,text/*"
            onChange={(e) => setFile(e.target.files?.[0] ?? null)} />
          <p>{file ? file.name : 'Choose a file (.txt, .md, .html, .pdf, .docx)'}</p>
        </div>
      )}

      <div className="speed-row">
        <label htmlFor="wpm0">{wpm} <span className="dim">words / min</span></label>
        <input id="wpm0" type="range" min={100} max={800} step={10}
          value={wpm} onChange={(e) => setWpm(Number(e.target.value))} />
      </div>

      {error && <p className="error">{error}</p>}

      <div className="actions">
        <button className="btn play" disabled={busy !== 'idle'} onClick={() => run('read')}>
          {busy === 'read' ? 'Preparing…' : '▶ Read now'}
        </button>
        <button className="btn ghost" disabled={busy !== 'idle'} onClick={() => run('share')}>
          {busy === 'share' ? 'Creating…' : '🔗 Create share link'}
        </button>
      </div>
    </div>
  );
}
