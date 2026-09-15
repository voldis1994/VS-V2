import { useEffect, useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { apiFetch, useApi } from '../hooks/useApi';

type ClientRow = {
  id: number;
  name: string;
  enabled: boolean;
  access_enabled?: boolean;
  has_access_code?: boolean;
  risk_enabled?: boolean;
  panel_epic?: string | null;
  panel_lot_size?: number | null;
  panel_robot_requested?: string | null;
  last_seen_at?: string | null;
};

type Status = {
  mode?: string;
  operating_mode?: string;
  clients?: number | { active?: number };
};

export function ControlMainPage() {
  const { data: clients, loading, error, refresh } = useApi<ClientRow[]>('/api/clients', 8000);
  const [status, setStatus] = useState<Status | null>(null);

  useEffect(() => {
    void apiFetch<Status>('/api/system/status')
      .then(setStatus)
      .catch(() => setStatus(null));
  }, []);

  const rows = clients || [];
  const stats = useMemo(() => {
    const total = rows.length;
    const running = rows.filter((c) => String(c.panel_robot_requested || '').toUpperCase() === 'RUNNING').length;
    const riskOff = rows.filter((c) => c.risk_enabled === false).length;
    const accessOn = rows.filter((c) => c.access_enabled).length;
    return { total, running, riskOff, accessOn };
  }, [rows]);

  const bars = [40, 55, 48, 62, 70, 66, 78, 72, 84, 80, 88, 92];

  return (
    <div>
      <div className="cp-kpis">
        <div className="cp-kpi">
          <div className="label">Clients</div>
          <div className="value">{stats.total}</div>
          <div className="delta">{stats.accessOn} with web access</div>
        </div>
        <div className="cp-kpi">
          <div className="label">Robots running</div>
          <div className="value">{stats.running}</div>
          <div className="delta">requested RUNNING</div>
        </div>
        <div className="cp-kpi">
          <div className="label">Risk locked</div>
          <div className="value">{stats.riskOff}</div>
          <div className="delta neg">admin override OFF</div>
        </div>
        <div className="cp-kpi">
          <div className="label">Mode</div>
          <div className="value">{String(status?.mode || status?.operating_mode || 'PAPER').toUpperCase()}</div>
          <div className="delta">fail-closed default PAPER</div>
        </div>
      </div>

      <div className="cp-grid-2">
        <section className="cp-panel">
          <h2>PORTFOLIO SNAPSHOT</h2>
          <div className="cp-chart-placeholder" aria-hidden>
            {bars.map((h, i) => (
              <span key={i} style={{ height: `${h}%` }} />
            ))}
          </div>
          <p className="cp-muted" style={{ marginTop: '0.75rem' }}>
            Live equity/P&amp;L wiring stays on trading accounts — this board is the multi-client command surface.
          </p>
        </section>

        <section className="cp-panel">
          <h2>QUICK ACTIONS</h2>
          <div className="cp-row" style={{ marginBottom: '0.75rem' }}>
            <Link className="cp-btn primary" to="/control/clients">
              + ADD / MANAGE CLIENTS
            </Link>
            <button type="button" className="cp-btn ghost" onClick={() => refresh()}>
              REFRESH
            </button>
          </div>
          {loading && <div className="cp-muted">Loading clients…</div>}
          {error && <div className="cp-error">{error}</div>}
          <ul className="cp-muted" style={{ listStyle: 'none', display: 'grid', gap: '0.45rem' }}>
            {rows.slice(0, 6).map((c) => (
              <li key={c.id} style={{ display: 'flex', justifyContent: 'space-between', gap: '0.5rem' }}>
                <span>{c.name}</span>
                <span>
                  {String(c.panel_robot_requested || 'STOPPED').toUpperCase() === 'RUNNING' ? (
                    <span className="cp-pill on">RUN</span>
                  ) : (
                    <span className="cp-pill off">STOP</span>
                  )}
                </span>
              </li>
            ))}
            {!loading && rows.length === 0 && <li>No clients yet — create one in Clients.</li>}
          </ul>
        </section>
      </div>
    </div>
  );
}
