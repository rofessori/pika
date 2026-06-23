import { useEffect } from 'react';
import { useRsvp } from '../hooks/useRsvp';
import type { Token } from '../lib/types';
import { WordDisplay } from './WordDisplay';

interface Props {
  tokens: Token[];
  initialWpm: number;
  title?: string;
  onExit: () => void;
}

export function Reader({ tokens, initialWpm, title, onExit }: Props) {
  const r = useRsvp(tokens, initialWpm);

  // Keyboard control: space to play/pause, arrows to step / change speed.
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      switch (e.key) {
        case ' ': e.preventDefault(); r.toggle(); break;
        case 'ArrowLeft': e.preventDefault(); r.seek(-1); break;
        case 'ArrowRight': e.preventDefault(); r.seek(1); break;
        case 'ArrowUp': e.preventDefault(); r.setWpm(r.wpm + 25); break;
        case 'ArrowDown': e.preventDefault(); r.setWpm(r.wpm - 25); break;
        case 'r': case 'R': r.restart(); break;
        case 'Escape': onExit(); break;
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [r, onExit]);

  return (
    <div className="reader">
      <header className="reader-top">
        <button className="link" onClick={onExit}>← back</button>
        {title && <span className="reader-title" title={title}>{title}</span>}
        <span className="reader-count">{r.index + 1} / {r.count}</span>
      </header>

      <WordDisplay token={r.token} />

      <div className="reader-controls">
        <div className="progress" role="progressbar"
          aria-valuemin={0} aria-valuemax={100}
          aria-valuenow={Math.round(r.progress * 100)}>
          <div className="progress-bar" style={{ width: `${r.progress * 100}%` }} />
        </div>

        <div className="buttons">
          <button className="btn ghost" onClick={r.restart} title="Restart (R)">⏮</button>
          <button className="btn ghost" onClick={() => r.seek(-1)} title="Back (←)">−1</button>
          <button className="btn play" onClick={r.toggle} title="Play / pause (Space)">
            {r.playing ? '⏸ Pause' : r.finished ? '↺ Again' : '▶ Play'}
          </button>
          <button className="btn ghost" onClick={() => r.seek(1)} title="Forward (→)">+1</button>
        </div>

        <div className="speed">
          <label htmlFor="wpm">{r.wpm} <span className="dim">wpm</span></label>
          <input id="wpm" type="range" min={50} max={1000} step={10}
            value={r.wpm} onChange={(e) => r.setWpm(Number(e.target.value))} />
        </div>
        <p className="hint dim">Space play · ← → step · ↑ ↓ speed · Esc back</p>
      </div>
    </div>
  );
}
