import { useApi } from '../hooks/useApi';

export function ControlAiPage() {
  const { data, error, loading, refresh } = useApi<Array<Record<string, unknown>>>('/api/clients', 12000);
  const rows = data || [];

  return (
    <div className="cp-panel">
      <div className="cp-row" style={{ justifyContent: 'space-between' }}>
        <h2 style={{ margin: 0 }}>AI · PER CLIENT (NO BLACK BOX)</h2>
        <button type="button" className="cp-btn ghost" onClick={() => refresh()}>
          REFRESH
        </button>
      </div>
      <p className="cp-muted" style={{ margin: '0.75rem 0 1rem' }}>
        Pick a client to inspect HH / HL / LL / UTT acceptance, decision timeline, risk state, and open trade
        management. Deep brain desk remains under legacy routes.
      </p>
      {loading && <div className="cp-muted">Loading…</div>}
      {error && <div className="cp-error">{error}</div>}
      <div className="cp-grid-2">
        <div>
          <h2>CLIENTS</h2>
          <ul style={{ listStyle: 'none', display: 'grid', gap: '0.45rem' }}>
            {rows.map((c) => (
              <li key={String(c.id)} className="cp-panel" style={{ margin: 0, padding: '0.75rem' }}>
                <strong>{String(c.name)}</strong>
                <div className="cp-muted">
                  #{String(c.id)} · {String(c.panel_epic || 'no market')} · risk{' '}
                  {c.risk_enabled === false ? 'OFF' : 'ON'}
                </div>
                <a className="cp-btn" href={`/brain?client=${c.id}`} style={{ marginTop: '0.5rem' }}>
                  OPEN BRAIN DESK
                </a>
              </li>
            ))}
          </ul>
        </div>
        <div className="cp-panel">
          <h2>WHAT YOU SHOULD SEE</h2>
          <ul className="cp-muted" style={{ paddingLeft: '1.1rem', lineHeight: 1.6 }}>
            <li>Structure marks: HH, HL, LL, LH, UTT</li>
            <li>Accept / reject reasons</li>
            <li>Decision → risk → open → manage → close</li>
            <li>Client-selected market + lot + risk lock</li>
          </ul>
        </div>
      </div>
    </div>
  );
}

export function ControlErrorsPage() {
  const { data, error, loading, refresh } = useApi<Array<Record<string, unknown>>>(
    '/api/system/events',
    8000
  );
  const rows = Array.isArray(data) ? data : [];

  return (
    <div className="cp-panel">
      <div className="cp-row" style={{ justifyContent: 'space-between' }}>
        <h2 style={{ margin: 0 }}>ERRORS · EVERYTHING VISIBLE</h2>
        <button type="button" className="cp-btn ghost" onClick={() => refresh()}>
          REFRESH
        </button>
      </div>
      {loading && <div className="cp-muted">Loading…</div>}
      {error && <div className="cp-error">{error}</div>}
      <table className="cp-table">
        <thead>
          <tr>
            <th>Time</th>
            <th>Type</th>
            <th>Detail</th>
          </tr>
        </thead>
        <tbody>
          {rows.slice(0, 80).map((e, i) => (
            <tr key={i}>
              <td className="cp-muted">{String(e.created_at || e.ts || '—')}</td>
              <td>{String(e.event_type || e.type || e.level || 'event')}</td>
              <td>{String(e.message || e.detail || e.payload || JSON.stringify(e)).slice(0, 220)}</td>
            </tr>
          ))}
          {!loading && rows.length === 0 && (
            <tr>
              <td colSpan={3} className="cp-muted">
                No system events yet (or API empty).
              </td>
            </tr>
          )}
        </tbody>
      </table>
    </div>
  );
}

export function ControlFeedPage() {
  return (
    <div className="cp-panel">
      <h2>FEED · CROSS-MARKET IMPACT</h2>
      <p className="cp-muted">
        Shows how correlated markets (DXY, yields, indices, BTC) may pressure clients&apos; open symbols.
        Helper only — never places orders in PAPER.
      </p>
      <div className="cp-grid-2">
        <div className="cp-panel">
          <h2>OPEN FOCUS</h2>
          <div className="cp-muted">Wired to live open epics in a follow-up pass.</div>
        </div>
        <div className="cp-panel">
          <h2>DRIVERS</h2>
          <ul className="cp-muted" style={{ paddingLeft: '1.1rem', lineHeight: 1.6 }}>
            <li>DXY ↑ → EUR pressure</li>
            <li>US10Y ↑ → XAU pressure</li>
            <li>Risk-off → indices / BTC</li>
          </ul>
        </div>
      </div>
    </div>
  );
}

export function ControlNewsPage() {
  return (
    <div className="cp-panel">
      <h2>NEWS · OPEN MARKETS</h2>
      <p className="cp-muted">
        Headlines tagged to symbols clients currently trade, with exposure reminders. Feed provider hook comes next —
        layout is ready for cyberpink desk.
      </p>
      <div className="cp-panel">
        <div className="cp-muted">No headlines loaded yet.</div>
      </div>
    </div>
  );
}

export function ControlSystemPage() {
  const { data, error, loading, refresh } = useApi<Record<string, unknown>>('/api/system/status', 5000);

  return (
    <div className="cp-panel">
      <div className="cp-row" style={{ justifyContent: 'space-between' }}>
        <h2 style={{ margin: 0 }}>SYSTEM</h2>
        <button type="button" className="cp-btn ghost" onClick={() => refresh()}>
          REFRESH
        </button>
      </div>
      {loading && <div className="cp-muted">Loading…</div>}
      {error && <div className="cp-error">{error}</div>}
      <pre
        style={{
          marginTop: '0.75rem',
          padding: '0.75rem',
          borderRadius: 12,
          border: '1px solid rgba(255,45,149,0.25)',
          overflow: 'auto',
          fontSize: '0.75rem',
          color: 'var(--cp-cyan)',
        }}
      >
        {JSON.stringify(data || {}, null, 2)}
      </pre>
      <div className="cp-row" style={{ marginTop: '1rem' }}>
        <a className="cp-btn" href="/">
          LEGACY LIVE DESK
        </a>
        <a className="cp-btn" href="/overview">
          OVERVIEW
        </a>
        <a className="cp-btn" href="/client">
          CLIENT WEB PREVIEW
        </a>
      </div>
    </div>
  );
}
