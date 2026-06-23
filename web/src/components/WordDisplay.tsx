import type { Token } from '../lib/types';

// Renders one word with its focal character pinned to the centre column and
// tinted red. A three-column grid (1fr · auto · 1fr) keeps the pivot fixed
// horizontally no matter the word, so the reader's gaze never moves.
export function WordDisplay({ token }: { token: Token | undefined }) {
  const cps = token ? Array.from(token.t) : [];
  const orp = token ? token.orp : 0;
  const head = cps.slice(0, orp).join('');
  const pivot = cps[orp] ?? '';
  const tail = cps.slice(orp + 1).join('');

  return (
    <div className="focal">
      <span className="tick tick-top" aria-hidden />
      <div className="word" aria-live="polite">
        <span className="head">{head}</span>
        <span className="pivot">{pivot}</span>
        <span className="tail">{tail}</span>
      </div>
      <span className="tick tick-bottom" aria-hidden />
    </div>
  );
}
