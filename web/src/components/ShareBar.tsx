import { useState } from 'react';

// A slim bar shown above the reader once a shareable link exists.
export function ShareBar({ url }: { url: string }) {
  const [copied, setCopied] = useState(false);

  async function copy() {
    try {
      await navigator.clipboard.writeText(url);
      setCopied(true);
      window.setTimeout(() => setCopied(false), 1500);
    } catch {
      setCopied(false);
    }
  }

  return (
    <div className="sharebar">
      <span className="dim">Shareable link</span>
      <code className="share-url">{url}</code>
      <button className="btn ghost small" onClick={copy}>
        {copied ? 'Copied ✓' : 'Copy'}
      </button>
    </div>
  );
}
