import { useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { useApi } from '../hooks/useApi';

type ClientRow = {
  id: number;
  name: string;
  risk_enabled?: boolean;
  panel_epic?: string | null;
  panel_display_name?: string | null;
  panel_lot_size?: number | null;
  panel_robot_requested?: string | null;
  robot_status?: string | null;
  live_trade?: {
    market: string;
    display_name: string;
    side?: string;
    trade_type?: string;
    lot_size: number;
    entry_price: number | null;
  } | null;
  status_reason?: string | null;
  broker_error?: string | null;
};

type BrainLive = {
  snapshot?: {
    instruments?: Array<Record<string, unknown>>;
    source?: string;
  } | null;
  status?: Record<string, unknown>;
  awaiting_market_core?: boolean;
  note?: string;
};

type FeedRow = {
  id?: string;
  name?: string;
  status?: string;
  kind?: string;
  last_ok_at?: string | null;
  last_error?: string | null;
  latency_ms?: number | null;
};

type NewsDesk = {
  generated_at: string;
  open_markets: string[];
  note?: string;
  items: Array<{
    id: string;
    headline: string;
    impact: 'high' | 'medium' | 'low';
    markets: string[];
    why: string;
    source: string;
  }>;
};

export function ControlAiPage() {
  const clientsApi = useApi<ClientRow[]>('/api/clients', 8000);
  const brainApi = useApi<BrainLive>('/api/brain/live', 5000);
  const eventsApi = useApi<{ items?: Array<Record<string, unknown>> }>(
    '/api/brain/events?kind=all&limit=40',
    8000
  );
  const [selectedId, setSelectedId] = useState<number | null>(null);

  const rows = clientsApi.data || [];
  const selected =
    rows.find((c) => c.id === selectedId) || rows[0] || null;
  const instruments = brainApi.data?.snapshot?.instruments || [];
  const events = eventsApi.data?.items || [];

  return (
    <div>
      <section className="cp-panel">
        <div className="cp-row" style={{ justifyContent: 'space-between' }}>
          <h2 style={{ margin: 0 }}>AI · PER CLIENT (NO BLACK BOX)</h2>
          <button
            type="button"
            className="cp-btn ghost"
            onClick={() => {
              clientsApi.refresh();
              brainApi.refresh();
              eventsApi.refresh();
            }}
          >
            REFRESH
          </button>
        </div>
        <p className="cp-muted">
          Live brain snapshot from market-core + each client&apos;s robot/market/risk state. No invented
          decisions.
        </p>
        {brainApi.data?.awaiting_market_core && (
          <div className="cp-error">{brainApi.data.note || 'Waiting for market-core brain snapshot'}</div>
        )}
      </section>

      <div className="cp-grid-2">
        <section className="cp-panel">
          <h2>CLIENTS</h2>
          {(clientsApi.loading && <div className="cp-muted">Loading…</div>) || null}
          {clientsApi.error && <div className="cp-error">{clientsApi.error}</div>}
          <ul style={{ listStyle: 'none', display: 'grid', gap: '0.45rem' }}>
            {rows.map((c) => {
              const active = (selected?.id || null) === c.id;
              const running = String(c.robot_status || c.panel_robot_requested || '').toUpperCase() === 'RUNNING';
              return (
                <li key={c.id}>
                  <button
                    type="button"
                    className="cp-btn"
                    style={{
                      width: '100%',
                      justifyContent: 'space-between',
                      background: active ? 'rgba(255,45,149,0.22)' : undefined,
                    }}
                    onClick={() => setSelectedId(c.id)}
                  >
                    <span>
                      {c.name}{' '}
                      <span className="cp-muted">
                        #{c.id} · {c.panel_display_name || c.panel_epic || 'no market'}
                      </span>
                    </span>
                    <span className={`cp-pill ${running ? 'on' : 'off'}`}>
                      {String(c.robot_status || c.panel_robot_requested || 'STOPPED').toUpperCase()}
                    </span>
                  </button>
                </li>
              );
            })}
            {!clientsApi.loading && rows.length === 0 && (
              <li className="cp-muted">No clients yet.</li>
            )}
          </ul>
        </section>

        <section className="cp-panel">
          <h2>SELECTED CLIENT</h2>
          {!selected && <div className="cp-muted">Select a client.</div>}
          {selected && (
            <div style={{ display: 'grid', gap: '0.55rem' }}>
              <div>
                <strong>{selected.name}</strong>
                <div className="cp-muted">#{selected.id}</div>
              </div>
              <div>
                Risk:{' '}
                <span className={`cp-pill ${selected.risk_enabled === false ? 'off' : 'on'}`}>
                  {selected.risk_enabled === false ? 'OFF' : 'ON'}
                </span>
              </div>
              <div>
                Market: {selected.panel_display_name || selected.panel_epic || '—'} · lot{' '}
                {selected.panel_lot_size != null ? Number(selected.panel_lot_size).toFixed(2) : '—'}
              </div>
              <div>
                Robot: {String(selected.robot_status || selected.panel_robot_requested || 'STOPPED')}
              </div>
              {selected.live_trade ? (
                <div>
                  Live: {selected.live_trade.side || selected.live_trade.trade_type || 'OPEN'} ·{' '}
                  {selected.live_trade.display_name || selected.live_trade.market} ·{' '}
                  {Number(selected.live_trade.lot_size).toFixed(2)} @{' '}
                  {selected.live_trade.entry_price ?? '—'}
                </div>
              ) : (
                <div className="cp-muted">No open live trade</div>
              )}
              {(selected.broker_error || selected.status_reason) && (
                <div className="cp-error">
                  {selected.broker_error || selected.status_reason}
                </div>
              )}
              <Link className="cp-btn" to="/control/ai">
                AI · THIS CLIENT
              </Link>
            </div>
          )}
        </section>
      </div>

      <div className="cp-grid-2">
        <section className="cp-panel">
          <h2>BRAIN INSTRUMENTS</h2>
          {brainApi.error && <div className="cp-error">{brainApi.error}</div>}
          <table className="cp-table">
            <thead>
              <tr>
                <th>Symbol</th>
                <th>Side / Decision</th>
                <th>Detail</th>
              </tr>
            </thead>
            <tbody>
              {instruments.slice(0, 20).map((inst, i) => (
                <tr key={i}>
                  <td>{String(inst.symbol || inst.epic || inst.instrument_id || '—')}</td>
                  <td>{String(inst.decision || inst.side || inst.bias || '—')}</td>
                  <td className="cp-muted">
                    {String(inst.reason || inst.regime || inst.status || '').slice(0, 120) || '—'}
                  </td>
                </tr>
              ))}
              {instruments.length === 0 && (
                <tr>
                  <td colSpan={3} className="cp-muted">
                    No live instruments in brain snapshot yet.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </section>

        <section className="cp-panel">
          <h2>RECENT BRAIN EVENTS</h2>
          {eventsApi.error && <div className="cp-error">{eventsApi.error}</div>}
          <table className="cp-table">
            <thead>
              <tr>
                <th>Time</th>
                <th>Kind</th>
                <th>Detail</th>
              </tr>
            </thead>
            <tbody>
              {events.slice(0, 30).map((ev, i) => (
                <tr key={i}>
                  <td className="cp-muted">{String(ev.ts || ev.created_at || '—')}</td>
                  <td>{String(ev.kind || ev.type || 'event')}</td>
                  <td>{String(ev.summary || ev.message || JSON.stringify(ev)).slice(0, 160)}</td>
                </tr>
              ))}
              {events.length === 0 && (
                <tr>
                  <td colSpan={3} className="cp-muted">
                    No brain events yet.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </section>
      </div>
    </div>
  );
}

export function ControlErrorsPage() {
  const eventsApi = useApi<Array<Record<string, unknown>> | { items?: Array<Record<string, unknown>> }>(
    '/api/system/events',
    8000
  );
  const brainErrors = useApi<{ items?: Array<Record<string, unknown>> }>(
    '/api/brain/events?kind=error&limit=50',
    8000
  );
  const clientsApi = useApi<ClientRow[]>('/api/clients', 10000);

  const systemRows = Array.isArray(eventsApi.data)
    ? eventsApi.data
    : eventsApi.data?.items || [];
  const brainRows = brainErrors.data?.items || [];
  const clientIssues = (clientsApi.data || []).filter(
    (c) =>
      String(c.robot_status || '').toUpperCase() === 'ERROR' ||
      Boolean(c.broker_error) ||
      Boolean(c.status_reason)
  );

  return (
    <div>
      <section className="cp-panel">
        <div className="cp-row" style={{ justifyContent: 'space-between' }}>
          <h2 style={{ margin: 0 }}>ERRORS · EVERYTHING VISIBLE</h2>
          <button
            type="button"
            className="cp-btn ghost"
            onClick={() => {
              eventsApi.refresh();
              brainErrors.refresh();
              clientsApi.refresh();
            }}
          >
            REFRESH
          </button>
        </div>
      </section>

      <section className="cp-panel">
        <h2>CLIENT / ROBOT ISSUES</h2>
        {clientIssues.length === 0 ? (
          <div className="cp-muted">No client robot errors right now.</div>
        ) : (
          <table className="cp-table">
            <thead>
              <tr>
                <th>Client</th>
                <th>Status</th>
                <th>Detail</th>
              </tr>
            </thead>
            <tbody>
              {clientIssues.map((c) => (
                <tr key={c.id}>
                  <td>
                    {c.name} <span className="cp-muted">#{c.id}</span>
                  </td>
                  <td>{String(c.robot_status || 'ERROR')}</td>
                  <td className="cp-error">{c.broker_error || c.status_reason || '—'}</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </section>

      <div className="cp-grid-2">
        <section className="cp-panel">
          <h2>SYSTEM EVENTS</h2>
          {eventsApi.error && <div className="cp-error">{eventsApi.error}</div>}
          <table className="cp-table">
            <thead>
              <tr>
                <th>Time</th>
                <th>Type</th>
                <th>Detail</th>
              </tr>
            </thead>
            <tbody>
              {systemRows.slice(0, 80).map((e, i) => (
                <tr key={i}>
                  <td className="cp-muted">{String(e.created_at || e.ts || '—')}</td>
                  <td>{String(e.event_type || e.type || e.level || 'event')}</td>
                  <td>{String(e.message || e.detail || e.payload || JSON.stringify(e)).slice(0, 220)}</td>
                </tr>
              ))}
              {systemRows.length === 0 && (
                <tr>
                  <td colSpan={3} className="cp-muted">
                    No system events yet.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </section>

        <section className="cp-panel">
          <h2>BRAIN ERRORS</h2>
          {brainErrors.error && <div className="cp-error">{brainErrors.error}</div>}
          <table className="cp-table">
            <thead>
              <tr>
                <th>Time</th>
                <th>Detail</th>
              </tr>
            </thead>
            <tbody>
              {brainRows.slice(0, 50).map((e, i) => (
                <tr key={i}>
                  <td className="cp-muted">{String(e.ts || e.created_at || '—')}</td>
                  <td className="cp-error">
                    {String(e.message || e.summary || JSON.stringify(e)).slice(0, 220)}
                  </td>
                </tr>
              ))}
              {brainRows.length === 0 && (
                <tr>
                  <td colSpan={2} className="cp-muted">
                    No brain error events.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </section>
      </div>
    </div>
  );
}

export function ControlFeedPage() {
  const feedsApi = useApi<FeedRow[] | { items?: FeedRow[] }>('/api/feeds', 7000);
  const clientsApi = useApi<ClientRow[]>('/api/clients', 10000);

  const feeds = Array.isArray(feedsApi.data) ? feedsApi.data : feedsApi.data?.items || [];
  const openMarkets = useMemo(() => {
    const set = new Set<string>();
    for (const c of clientsApi.data || []) {
      if (c.panel_epic) set.add(String(c.panel_epic));
      if (c.live_trade?.market) set.add(String(c.live_trade.market));
    }
    return [...set];
  }, [clientsApi.data]);

  const drivers = [
    { driver: 'DXY ↑', affects: 'EURUSD / EUR*', bias: 'EUR soft', strength: openMarkets.some((m) => m.includes('EUR')) ? 'HIGH' : 'WATCH' },
    { driver: 'US10Y ↑', affects: 'XAUUSD / GOLD', bias: 'Gold soft', strength: openMarkets.some((m) => /XAU|GOLD/i.test(m)) ? 'HIGH' : 'WATCH' },
    { driver: 'Risk-off', affects: 'US100 / BTC', bias: 'Risk assets soft', strength: openMarkets.some((m) => /US100|NAS|BTC/i.test(m)) ? 'HIGH' : 'WATCH' },
  ];

  return (
    <div>
      <section className="cp-panel">
        <div className="cp-row" style={{ justifyContent: 'space-between' }}>
          <h2 style={{ margin: 0 }}>FEED · CROSS-MARKET IMPACT</h2>
          <button
            type="button"
            className="cp-btn ghost"
            onClick={() => {
              feedsApi.refresh();
              clientsApi.refresh();
            }}
          >
            REFRESH
          </button>
        </div>
        <p className="cp-muted">Helper only — never places orders in PAPER.</p>
      </section>

      <div className="cp-grid-2">
        <section className="cp-panel">
          <h2>OPEN FOCUS</h2>
          {openMarkets.length === 0 ? (
            <div className="cp-muted">No client markets selected / open.</div>
          ) : (
            <div className="cp-row">
              {openMarkets.map((m) => (
                <span key={m} className="cp-pill on">
                  {m}
                </span>
              ))}
            </div>
          )}
          <h2 style={{ marginTop: '1rem' }}>DRIVERS</h2>
          <table className="cp-table">
            <thead>
              <tr>
                <th>Driver</th>
                <th>Affects</th>
                <th>Bias</th>
                <th>Strength</th>
              </tr>
            </thead>
            <tbody>
              {drivers.map((d) => (
                <tr key={d.driver}>
                  <td>{d.driver}</td>
                  <td>{d.affects}</td>
                  <td>{d.bias}</td>
                  <td>
                    <span className={`cp-pill ${d.strength === 'HIGH' ? 'warn' : 'on'}`}>{d.strength}</span>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </section>

        <section className="cp-panel">
          <h2>LIVE FEED HEALTH</h2>
          {feedsApi.error && <div className="cp-error">{feedsApi.error}</div>}
          <table className="cp-table">
            <thead>
              <tr>
                <th>Feed</th>
                <th>Status</th>
                <th>Latency</th>
                <th>Error</th>
              </tr>
            </thead>
            <tbody>
              {feeds.map((f, i) => (
                <tr key={String(f.id || f.name || i)}>
                  <td>
                    {f.name || f.id || 'feed'}
                    <div className="cp-muted">{f.kind || ''}</div>
                  </td>
                  <td>
                    <span
                      className={`cp-pill ${
                        ['LIVE', 'OK', 'HEALTHY', 'ONLINE'].includes(String(f.status || '').toUpperCase())
                          ? 'on'
                          : 'off'
                      }`}
                    >
                      {String(f.status || '—')}
                    </span>
                  </td>
                  <td className="cp-muted">{f.latency_ms != null ? `${f.latency_ms}ms` : '—'}</td>
                  <td className="cp-error">{f.last_error || '—'}</td>
                </tr>
              ))}
              {feeds.length === 0 && (
                <tr>
                  <td colSpan={4} className="cp-muted">
                    No feed rows yet.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </section>
      </div>
    </div>
  );
}

export function ControlNewsPage() {
  const newsApi = useApi<NewsDesk>('/api/news/desk', 15000);
  const data = newsApi.data;

  return (
    <div className="cp-panel">
      <div className="cp-row" style={{ justifyContent: 'space-between' }}>
        <h2 style={{ margin: 0 }}>NEWS · OPEN MARKETS</h2>
        <button type="button" className="cp-btn ghost" onClick={() => newsApi.refresh()}>
          REFRESH
        </button>
      </div>
      <p className="cp-muted">
        Headlines tagged to markets clients currently trade. Generated at{' '}
        {data?.generated_at || '—'}.
        {data?.note ? ` ${data.note}` : ''}
      </p>
      {newsApi.loading && <div className="cp-muted">Loading…</div>}
      {newsApi.error && <div className="cp-error">{newsApi.error}</div>}
      <div className="cp-row" style={{ marginBottom: '0.75rem' }}>
        {(data?.open_markets || []).map((m) => (
          <span key={m} className="cp-pill on">
            {m}
          </span>
        ))}
        {data && data.open_markets.length === 0 && (
          <span className="cp-muted">No open/selected client markets — showing macro watchlist.</span>
        )}
      </div>
      <div style={{ display: 'grid', gap: '0.65rem' }}>
        {(data?.items || []).map((item) => (
          <article key={item.id} className="cp-panel" style={{ margin: 0 }}>
            <div className="cp-row" style={{ justifyContent: 'space-between' }}>
              <strong>{item.headline}</strong>
              <span className={`cp-pill ${item.impact === 'high' ? 'off' : item.impact === 'medium' ? 'warn' : 'on'}`}>
                {item.impact.toUpperCase()}
              </span>
            </div>
            <div className="cp-muted">{item.why}</div>
            <div className="cp-row">
              {item.markets.map((m) => (
                <span key={m} className="cp-pill on">
                  {m}
                </span>
              ))}
              <span className="cp-muted">{item.source}</span>
            </div>
          </article>
        ))}
        {!newsApi.loading && (data?.items || []).length === 0 && (
          <div className="cp-muted">No news items.</div>
        )}
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
        <Link className="cp-btn primary" to="/control/clients">
          CLIENTS
        </Link>
        <Link className="cp-btn" to="/control/ai">
          AI
        </Link>
        <a className="cp-btn" href="/client" target="_blank" rel="noreferrer">
          CLIENT WEB
        </a>
      </div>
    </div>
  );
}
