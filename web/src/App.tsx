import { useCallback, useEffect, useState } from 'react';
import { InputPanel } from './components/InputPanel';
import { Reader } from './components/Reader';
import { ShareBar } from './components/ShareBar';
import { ApiError, getReading } from './lib/api';
import type { Reading, Token, TokenStream } from './lib/types';

const SHARE_RE = /^\/([0-9A-Z]{4,12})$/;

interface ReaderState {
  tokens: Token[];
  wpm: number;
  title?: string;
  shareUrl?: string;
}

export default function App() {
  const [reader, setReader] = useState<ReaderState | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  const openReading = useCallback((r: Reading, shareUrl?: string) => {
    setReader({
      tokens: r.tokens,
      wpm: r.wpm,
      title: r.title || r.source.name || undefined,
      shareUrl: shareUrl ?? r.url,
    });
  }, []);

  // Resolve the current URL: a share id loads that reading, anything else is
  // the input screen.
  const loadFromPath = useCallback(async () => {
    const m = window.location.pathname.match(SHARE_RE);
    if (!m) { setReader(null); return; }
    setLoading(true);
    setError('');
    try {
      const r = await getReading(m[1]);
      openReading(r, window.location.origin + '/' + r.id);
    } catch (e) {
      setError(e instanceof ApiError ? e.message : 'Could not load that reading.');
      setReader(null);
      window.history.replaceState({}, '', '/');
    } finally {
      setLoading(false);
    }
  }, [openReading]);

  useEffect(() => {
    loadFromPath();
    const onPop = () => loadFromPath();
    window.addEventListener('popstate', onPop);
    return () => window.removeEventListener('popstate', onPop);
  }, [loadFromPath]);

  const onStream = (s: TokenStream) => {
    setReader({
      tokens: s.tokens,
      wpm: s.wpm,
      title: s.source.name || undefined,
    });
  };

  const onReading = (r: Reading) => {
    window.history.pushState({}, '', '/' + r.id);
    openReading(r);
  };

  const exit = () => {
    setReader(null);
    setError('');
    if (window.location.pathname !== '/') window.history.pushState({}, '', '/');
  };

  if (loading) {
    return <main className="stage"><div className="loading">Loading…</div></main>;
  }

  if (reader) {
    return (
      <main className="stage">
        {reader.shareUrl && <ShareBar url={reader.shareUrl} />}
        <Reader tokens={reader.tokens} initialWpm={reader.wpm}
          title={reader.title} onExit={exit} />
      </main>
    );
  }

  return (
    <main className="stage">
      {error && <p className="error toast">{error}</p>}
      <InputPanel onStream={onStream} onReading={onReading} />
      <footer className="footer dim">
        pika · free software (GPL-3.0) · works in your terminal too
      </footer>
    </main>
  );
}
