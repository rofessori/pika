import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { Token } from '../lib/types';

const clamp = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, n));

export interface Rsvp {
  index: number;
  count: number;
  token: Token | undefined;
  playing: boolean;
  wpm: number;
  progress: number; // 0..1
  finished: boolean;
  play: () => void;
  pause: () => void;
  toggle: () => void;
  restart: () => void;
  seek: (delta: number) => void;
  setIndex: (i: number) => void;
  setWpm: (w: number) => void;
}

// Drives the rapid-serial-visual-presentation loop: advances through tokens
// on a timer whose interval is the base words-per-minute rate scaled by each
// token's dwell weight (punctuation and long words linger a little).
export function useRsvp(tokens: Token[], initialWpm = 350): Rsvp {
  const [index, setIndex] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [wpm, setWpmState] = useState(initialWpm);
  const count = tokens.length;
  const lastIndex = Math.max(0, count - 1);

  // Reset whenever a new document is loaded.
  useEffect(() => {
    setIndex(0);
    setPlaying(false);
  }, [tokens]);

  // Keep the latest values available to the timer without re-subscribing.
  const wpmRef = useRef(wpm);
  wpmRef.current = wpm;

  useEffect(() => {
    if (!playing || count === 0) return;
    if (index >= lastIndex) {
      setPlaying(false);
      return;
    }
    const tok = tokens[index];
    const base = 60000 / wpmRef.current;
    const delay = Math.max(40, base * (tok?.w ?? 1));
    const id = window.setTimeout(() => setIndex((i) => Math.min(i + 1, lastIndex)), delay);
    return () => window.clearTimeout(id);
  }, [playing, index, wpm, tokens, count, lastIndex]);

  const play = useCallback(() => {
    setIndex((i) => (i >= lastIndex ? 0 : i));
    setPlaying(true);
  }, [lastIndex]);
  const pause = useCallback(() => setPlaying(false), []);
  const toggle = useCallback(() => (playing ? pause() : play()), [playing, play, pause]);
  const restart = useCallback(() => setIndex(0), []);
  const seek = useCallback((delta: number) => {
    setPlaying(false);
    setIndex((i) => clamp(i + delta, 0, lastIndex));
  }, [lastIndex]);
  const setWpm = useCallback((w: number) => setWpmState(clamp(Math.round(w), 50, 1500)), []);
  const setIndexClamped = useCallback((i: number) => setIndex(clamp(i, 0, lastIndex)), [lastIndex]);

  return useMemo<Rsvp>(() => ({
    index,
    count,
    token: tokens[index],
    playing,
    wpm,
    progress: count > 1 ? index / lastIndex : count === 1 ? 1 : 0,
    finished: count > 0 && index >= lastIndex && !playing,
    play, pause, toggle, restart, seek, setIndex: setIndexClamped, setWpm,
  }), [index, count, tokens, playing, wpm, lastIndex, play, pause, toggle, restart, seek, setIndexClamped, setWpm]);
}
