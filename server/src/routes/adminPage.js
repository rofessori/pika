// Self-contained admin console page. No build step, no external assets.
// `__BASE__` is replaced with the secret base path at serve time.
export const ADMIN_PAGE = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="robots" content="noindex, nofollow">
<title>pika · admin</title>
<style>
  :root { --bg:#0a0a0b; --fg:#e8e8ea; --dim:#8a8a92; --accent:#d62828;
          --line:#26262b; --card:#141417; }
  * { box-sizing:border-box; }
  body { margin:0; background:var(--bg); color:var(--fg);
         font:15px/1.5 ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,sans-serif; }
  main { max-width:760px; margin:0 auto; padding:48px 20px; }
  h1 { font-weight:600; letter-spacing:.02em; margin:0 0 4px; }
  h1 .o { color:var(--accent); }
  p.sub { color:var(--dim); margin:0 0 32px; }
  .card { background:var(--card); border:1px solid var(--line);
          border-radius:12px; padding:20px; margin:0 0 20px; }
  label { display:block; color:var(--dim); font-size:13px; margin:0 0 6px; }
  input { width:100%; padding:10px 12px; background:#000; color:var(--fg);
          border:1px solid var(--line); border-radius:8px; font-size:15px; }
  button { padding:10px 16px; background:var(--fg); color:#000; border:0;
           border-radius:8px; font-weight:600; cursor:pointer; }
  button.ghost { background:transparent; color:var(--dim);
                 border:1px solid var(--line); }
  button:hover { opacity:.9; }
  .row { display:flex; gap:10px; align-items:flex-end; }
  .row > div { flex:1; }
  table { width:100%; border-collapse:collapse; font-size:14px; }
  th,td { text-align:left; padding:10px 8px; border-bottom:1px solid var(--line); }
  th { color:var(--dim); font-weight:500; }
  code { font-family:ui-monospace,SFMono-Regular,Menlo,monospace; }
  .token { background:#000; border:1px dashed var(--accent); padding:12px;
           border-radius:8px; word-break:break-all; margin-top:12px; }
  .muted { color:var(--dim); }
  .err { color:var(--accent); min-height:20px; margin-top:8px; }
  .hidden { display:none; }
  .pill { font-size:12px; padding:2px 8px; border-radius:999px;
          border:1px solid var(--line); color:var(--dim); }
</style>
</head>
<body>
<main>
  <h1>p<span class="o">i</span>ka · admin</h1>
  <p class="sub">Issue and revoke API tokens for the developer API.</p>

  <section id="login" class="card">
    <label for="pk">Admin passkey</label>
    <div class="row">
      <div><input id="pk" type="password" placeholder="XXXXX-XXXXX-XXXXX-XXXXX-XXXXX" autocomplete="off"></div>
      <button id="signin">Sign in</button>
    </div>
    <div class="err" id="loginErr"></div>
  </section>

  <div id="console" class="hidden">
    <section class="card">
      <div style="display:flex;justify-content:space-between;align-items:center">
        <div><span class="muted">Public API</span> <span id="apiState" class="pill">—</span></div>
        <div class="muted" id="counts"></div>
      </div>
    </section>

    <section class="card">
      <label for="label">Create a new API token</label>
      <div class="row">
        <div><input id="label" placeholder="e.g. mobile app, CI pipeline"></div>
        <button id="create">Create token</button>
      </div>
      <div class="err" id="createErr"></div>
      <div id="newToken" class="hidden">
        <div class="token"><code id="tokenValue"></code></div>
        <p class="muted">Copy it now — it is shown only once.</p>
      </div>
    </section>

    <section class="card">
      <table>
        <thead><tr><th>Label</th><th>Token</th><th>Created</th><th>Last used</th><th></th></tr></thead>
        <tbody id="tokens"></tbody>
      </table>
    </section>
    <button class="ghost" id="logout">Sign out</button>
  </div>
</main>
<script>
const BASE = '/__BASE__';
let session = sessionStorage.getItem('pika_admin_session') || '';
const $ = (id) => document.getElementById(id);

async function api(path, opts = {}) {
  const headers = { 'content-type': 'application/json', ...(opts.headers || {}) };
  if (session) headers.authorization = 'Bearer ' + session;
  const res = await fetch(BASE + path, { ...opts, headers });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data?.error?.message || res.statusText);
  return data;
}

function fmt(ts) { return ts ? new Date(ts).toLocaleString() : '—'; }

async function refresh() {
  const stats = await api('/api/stats');
  $('apiState').textContent = stats.public_api ? 'enabled' : 'disabled';
  $('counts').textContent = stats.readings + ' readings · ' + stats.tokens + ' tokens';
  const list = await api('/api/tokens');
  $('tokens').innerHTML = list.data.map((t) =>
    '<tr><td>' + escapeHtml(t.label) + '</td>' +
    '<td><code>pk_…' + t.last4 + '</code></td>' +
    '<td>' + fmt(t.createdAt) + '</td>' +
    '<td>' + fmt(t.lastUsedAt) + '</td>' +
    '<td><button class="ghost" data-id="' + t.id + '">Revoke</button></td></tr>'
  ).join('') || '<tr><td colspan="5" class="muted">No active tokens.</td></tr>';
  document.querySelectorAll('#tokens button').forEach((b) =>
    b.onclick = async () => {
      if (!confirm('Revoke this token? Apps using it will stop working.')) return;
      await api('/api/tokens/' + b.dataset.id, { method: 'DELETE' });
      refresh();
    });
}

function showConsole() { $('login').classList.add('hidden'); $('console').classList.remove('hidden'); refresh(); }

$('signin').onclick = async () => {
  $('loginErr').textContent = '';
  try {
    const r = await api('/session', { method: 'POST', body: JSON.stringify({ passkey: $('pk').value.trim() }) });
    session = r.session_token;
    sessionStorage.setItem('pika_admin_session', session);
    showConsole();
  } catch (e) { $('loginErr').textContent = e.message; }
};
$('pk').addEventListener('keydown', (e) => { if (e.key === 'Enter') $('signin').click(); });

$('create').onclick = async () => {
  $('createErr').textContent = '';
  try {
    const r = await api('/api/tokens', { method: 'POST', body: JSON.stringify({ label: $('label').value.trim() }) });
    $('tokenValue').textContent = r.token;
    $('newToken').classList.remove('hidden');
    $('label').value = '';
    refresh();
  } catch (e) { $('createErr').textContent = e.message; }
};

$('logout').onclick = async () => {
  try { await api('/session/logout', { method: 'POST' }); } catch {}
  session = ''; sessionStorage.removeItem('pika_admin_session');
  location.reload();
};

function escapeHtml(s) { return String(s).replace(/[&<>"]/g, (c) => ({ '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;' }[c])); }

if (session) { showConsole(); }
</script>
</body>
</html>`;
