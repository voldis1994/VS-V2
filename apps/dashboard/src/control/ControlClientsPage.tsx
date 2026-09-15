import { FormEvent, useMemo, useState } from 'react';
import { apiFetch, useApi } from '../hooks/useApi';

type ClientRow = {
  id: number;
  name: string;
  enabled: boolean;
  access_enabled?: boolean;
  has_access_code?: boolean;
  risk_enabled?: boolean;
  panel_epic?: string | null;
  panel_display_name?: string | null;
  panel_lot_size?: number | null;
  panel_robot_requested?: string | null;
  last_seen_at?: string | null;
};

export function ControlClientsPage() {
  const { data, loading, error, refresh } = useApi<ClientRow[]>('/api/clients', 10000);
  const [name, setName] = useState('');
  const [busy, setBusy] = useState(false);
  const [msg, setMsg] = useState<string | null>(null);
  const [issued, setIssued] = useState<{ id: number; code: string } | null>(null);

  const rows = useMemo(() => data || [], [data]);

  const createClient = async (e: FormEvent) => {
    e.preventDefault();
    if (!name.trim() || busy) return;
    setBusy(true);
    setMsg(null);
    try {
      const created = await apiFetch<ClientRow>('/api/clients', {
        method: 'POST',
        body: JSON.stringify({ name: name.trim() }),
      });
      setName('');
      setMsg(`Created #${created.id} ${created.name}`);
      refresh();
    } catch (err) {
      setMsg(err instanceof Error ? err.message : 'Create failed');
    } finally {
      setBusy(false);
    }
  };

  const patch = async (id: number, body: Record<string, unknown>) => {
    setMsg(null);
    try {
      await apiFetch(`/api/clients/${id}`, { method: 'PUT', body: JSON.stringify(body) });
      refresh();
    } catch (err) {
      setMsg(err instanceof Error ? err.message : 'Update failed');
    }
  };

  const issuePassword = async (client: ClientRow) => {
    setMsg(null);
    try {
      const res = await apiFetch<{ access_code: string; client_id: number }>(
        `/api/clients/${client.id}/access-code`,
        { method: 'POST', body: JSON.stringify({}) }
      );
      setIssued({ id: res.client_id, code: res.access_code });
      setMsg(`Password issued for #${client.id} — copy now, shown once.`);
      refresh();
    } catch (err) {
      setMsg(err instanceof Error ? err.message : 'Password issue failed');
    }
  };

  const stopRobot = async (client: ClientRow) => {
    try {
      await apiFetch(`/api/clients/${client.id}/stop-robot`, {
        method: 'POST',
        body: JSON.stringify({}),
      });
      setMsg(`Force-stopped robot for ${client.name}`);
      refresh();
    } catch (err) {
      setMsg(err instanceof Error ? err.message : 'Stop failed');
    }
  };

  return (
    <div>
      <section className="cp-panel">
        <h2>+ ADD CLIENT</h2>
        <form className="cp-form" onSubmit={(e) => void createClient(e)}>
          <label>
            Client name
            <input
              value={name}
              onChange={(e) => setName(e.target.value)}
              placeholder="Alpha Capital"
              required
            />
          </label>
          <div className="cp-row">
            <button className="cp-btn primary" type="submit" disabled={busy}>
              CREATE CLIENT
            </button>
            <span className="cp-muted">Then set password + Capital broker on Brokers / Trading.</span>
          </div>
        </form>
        {msg && <p className={msg.toLowerCase().includes('fail') ? 'cp-error' : 'cp-ok'}>{msg}</p>}
        {issued && (
          <div className="cp-panel" style={{ marginTop: '0.75rem', borderColor: 'rgba(45,226,230,0.45)' }}>
            <div className="cp-muted">One-time password for client #{issued.id}</div>
            <div style={{ fontFamily: 'var(--cp-mono)', fontSize: '1.4rem', letterSpacing: '0.12em' }}>
              {issued.code}
            </div>
            <button
              type="button"
              className="cp-btn ghost"
              onClick={() => void navigator.clipboard.writeText(issued.code)}
            >
              COPY
            </button>
          </div>
        )}
      </section>

      <section className="cp-panel">
        <div className="cp-row" style={{ justifyContent: 'space-between', marginBottom: '0.75rem' }}>
          <h2 style={{ margin: 0 }}>CLIENTS</h2>
          <button type="button" className="cp-btn ghost" onClick={() => refresh()}>
            REFRESH
          </button>
        </div>
        {loading && <div className="cp-muted">Loading…</div>}
        {error && <div className="cp-error">{error}</div>}
        <div style={{ overflowX: 'auto' }}>
          <table className="cp-table">
            <thead>
              <tr>
                <th>Client</th>
                <th>Access</th>
                <th>Risk</th>
                <th>Robot</th>
                <th>Market / Lot</th>
                <th>Actions</th>
              </tr>
            </thead>
            <tbody>
              {rows.map((c) => {
                const riskOn = c.risk_enabled !== false;
                const running = String(c.panel_robot_requested || '').toUpperCase() === 'RUNNING';
                return (
                  <tr key={c.id}>
                    <td>
                      <strong>{c.name}</strong>
                      <div className="cp-muted">#{c.id}</div>
                    </td>
                    <td>
                      <button
                        type="button"
                        className={`cp-pill ${c.access_enabled ? 'on' : 'off'}`}
                        onClick={() => void patch(c.id, { access_enabled: !c.access_enabled })}
                      >
                        {c.access_enabled ? 'WEB ON' : 'WEB OFF'}
                      </button>
                    </td>
                    <td>
                      <button
                        type="button"
                        className={`cp-pill ${riskOn ? 'on' : 'off'}`}
                        onClick={() => void patch(c.id, { risk_enabled: !riskOn })}
                        title="Admin risk lock — client cannot trade risk when OFF"
                      >
                        {riskOn ? 'RISK ON' : 'RISK OFF'}
                      </button>
                    </td>
                    <td>
                      <span className={`cp-pill ${running ? 'on' : 'off'}`}>
                        {String(c.panel_robot_requested || 'STOPPED').toUpperCase()}
                      </span>
                    </td>
                    <td>
                      <div>{c.panel_display_name || c.panel_epic || '—'}</div>
                      <div className="cp-muted">
                        {c.panel_lot_size != null ? Number(c.panel_lot_size).toFixed(2) : '—'} lot
                      </div>
                    </td>
                    <td>
                      <div className="cp-row">
                        <button type="button" className="cp-btn" onClick={() => void issuePassword(c)}>
                          SET PASSWORD
                        </button>
                        <button type="button" className="cp-btn danger" onClick={() => void stopRobot(c)}>
                          FORCE STOP
                        </button>
                      </div>
                    </td>
                  </tr>
                );
              })}
              {!loading && rows.length === 0 && (
                <tr>
                  <td colSpan={6} className="cp-muted">
                    No clients yet.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </section>
    </div>
  );
}
