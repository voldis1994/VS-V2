import { FormEvent, useCallback, useEffect, useMemo, useState } from 'react';
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
  robot_status?: string | null;
  capital_market_count?: number | null;
  capital_connection_id?: number | null;
  account_id?: number | null;
  live_trade?: {
    market: string;
    display_name: string;
    trade_type?: string;
    lot_size: number;
    entry_price: number | null;
  } | null;
  last_seen_at?: string | null;
  broker_error?: string | null;
  status_reason?: string | null;
};

type ProvisionResult = ClientRow & {
  access_code?: string;
  broker_connection_id?: number;
  account_id?: number;
  capital_market_count?: number;
  capital_markets_error?: string | null;
  message?: string;
};

type ClientWebState = {
  url?: string;
  source?: string;
  local_gateway?: string;
};

const emptyForm = {
  name: '',
  password: '',
  environment: 'live',
  identifier: '',
  api_key: '',
  api_password: '',
};

export function ControlClientsPage() {
  const { data, loading, error, refresh } = useApi<ClientRow[]>('/api/clients', 10000);
  const [form, setForm] = useState(emptyForm);
  const [busy, setBusy] = useState(false);
  const [msg, setMsg] = useState<string | null>(null);
  const [issued, setIssued] = useState<{ id: number; code: string } | null>(null);
  const [clientWeb, setClientWeb] = useState<ClientWebState | null>(null);
  const [urlDraft, setUrlDraft] = useState('');
  const [urlBusy, setUrlBusy] = useState(false);
  const [urlMsg, setUrlMsg] = useState<string | null>(null);
  const [copied, setCopied] = useState<string | null>(null);

  const rows = useMemo(() => data || [], [data]);
  const publicUrl = (clientWeb?.url || '').trim();

  const loadClientWeb = useCallback(async () => {
    try {
      const s = await apiFetch<ClientWebState>('/api/system/client-web');
      setClientWeb(s);
      setUrlDraft(String(s.url || ''));
    } catch {
      setClientWeb(null);
    }
  }, []);

  useEffect(() => {
    void loadClientWeb();
  }, [loadClientWeb]);

  const flashCopied = (label: string) => {
    setCopied(label);
    window.setTimeout(() => setCopied(null), 1800);
  };

  const copyText = async (text: string, label: string) => {
    const t = text.trim();
    if (!t) return;
    try {
      await navigator.clipboard.writeText(t);
      flashCopied(label);
    } catch {
      setMsg('Clipboard blocked — select and copy manually');
    }
  };

  const savePublicUrl = async () => {
    if (urlBusy) return;
    setUrlBusy(true);
    setUrlMsg(null);
    try {
      const s = await apiFetch<ClientWebState>('/api/system/client-web', {
        method: 'PUT',
        body: JSON.stringify({ url: urlDraft.trim() }),
      });
      setClientWeb(s);
      setUrlDraft(String(s.url || ''));
      setUrlMsg('Saglabāts — šo URL sūti klientiem');
    } catch (err) {
      setUrlMsg(err instanceof Error ? err.message : 'Save failed');
    } finally {
      setUrlBusy(false);
    }
  };

  const createClient = async (e: FormEvent) => {
    e.preventDefault();
    if (!form.name.trim() || busy) return;
    setBusy(true);
    setMsg(null);
    try {
      const body: Record<string, unknown> = {
        name: form.name.trim(),
        access_enabled: true,
        risk_enabled: true,
      };
      if (form.password.trim()) body.password = form.password.trim();

      const hasAnyCapital =
        Boolean(form.identifier.trim()) ||
        Boolean(form.api_key.trim()) ||
        Boolean(form.api_password.trim());
      if (hasAnyCapital) {
        if (!form.identifier.trim() || !form.api_key.trim() || !form.api_password.trim()) {
          throw new Error(
            'Capital: aizpildi identifier (email) + API key + API password (vai atstāj visus tukšus).'
          );
        }
        if (form.api_key.includes('@')) {
          throw new Error('API Key izskatās pēc email — email liec Identifier laukā.');
        }
        body.capital = {
          environment: form.environment,
          identifier: form.identifier.trim(),
          api_key: form.api_key.trim(),
          password: form.api_password.trim(),
        };
      }

      const created = await apiFetch<ProvisionResult>('/api/clients', {
        method: 'POST',
        body: JSON.stringify(body),
      });
      if (created.access_code) {
        setIssued({ id: created.id, code: created.access_code });
      } else {
        setIssued(null);
      }
      setForm(emptyForm);
      setMsg(created.message || `Created #${created.id} ${created.name}`);
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
    const custom = window.prompt(
      `Password for ${client.name} (leave empty to auto-generate)`,
      ''
    );
    if (custom === null) return;
    try {
      const res = await apiFetch<{ access_code: string; client_id: number }>(
        `/api/clients/${client.id}/access-code`,
        {
          method: 'POST',
          body: JSON.stringify(custom.trim() ? { password: custom.trim() } : {}),
        }
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

  const pullMarkets = async (client: ClientRow) => {
    setMsg(null);
    setBusy(true);
    try {
      const res = await apiFetch<{
        count?: number;
        capital_market_count?: number;
        message?: string;
        sample?: Array<{ epic: string; name: string }>;
      }>(`/api/clients/${client.id}/pull-markets`, {
        method: 'POST',
        body: JSON.stringify({ mode: 'quick' }),
      });
      const n = res.capital_market_count ?? res.count ?? 0;
      const sample = (res.sample || [])
        .slice(0, 3)
        .map((s) => s.epic)
        .join(', ');
      setMsg(
        `Pulled ${n} Capital markets for ${client.name}${sample ? ` (e.g. ${sample})` : ''}. Full catalog sync continues in background.`
      );
      refresh();
    } catch (err) {
      setMsg(err instanceof Error ? err.message : 'Pull markets failed');
    } finally {
      setBusy(false);
    }
  };

  return (
    <div>
      <section className="cp-panel cp-issued" style={{ marginBottom: '1rem' }}>
        <div className="cp-row" style={{ justifyContent: 'space-between', alignItems: 'flex-start' }}>
          <div>
            <h2 style={{ margin: 0 }}>KLIENTU WEB MĀJASLAPA</h2>
            <p className="cp-muted" style={{ margin: '0.35rem 0 0' }}>
              Publiska adrese klientiem. Kad Cloudflare / domens mainas — ielime jauno URL,
              saglabā un nokope klientam.
            </p>
          </div>
          {copied && <span className="cp-ok">Copied: {copied}</span>}
        </div>
        <div className="cp-issued-code" style={{ wordBreak: 'break-all', fontSize: '1.1rem' }}>
          {publicUrl || '—'}
        </div>
        <div className="cp-row" style={{ flexWrap: 'wrap', gap: '0.5rem' }}>
          <button
            type="button"
            className="cp-btn primary"
            disabled={!publicUrl}
            onClick={() => void copyText(publicUrl, 'URL')}
          >
            COPY URL
          </button>
          {publicUrl && (
            <a className="cp-btn ghost" href={publicUrl} target="_blank" rel="noreferrer">
              OPEN
            </a>
          )}
          <span className="cp-muted">
            {clientWeb?.source ? `source: ${clientWeb.source}` : ''}
            {clientWeb?.local_gateway ? ` · local ${clientWeb.local_gateway}` : ''}
          </span>
        </div>
        <label style={{ display: 'block', marginTop: '0.85rem' }}>
          Update public URL
          <div className="cp-row" style={{ marginTop: '0.35rem', flexWrap: 'wrap' }}>
            <input
              style={{ flex: '1 1 16rem', minWidth: '12rem' }}
              value={urlDraft}
              onChange={(e) => setUrlDraft(e.target.value)}
              placeholder="https://xxxx.trycloudflare.com"
              autoComplete="off"
            />
            <button
              type="button"
              className="cp-btn"
              disabled={urlBusy || !urlDraft.trim()}
              onClick={() => void savePublicUrl()}
            >
              {urlBusy ? 'SAVING…' : 'SAVE URL'}
            </button>
          </div>
        </label>
        {urlMsg && (
          <p className={/fail|error|invalid|required/i.test(urlMsg) ? 'cp-error' : 'cp-ok'}>
            {urlMsg}
          </p>
        )}
      </section>

      <section className="cp-panel cp-hero-panel">
        <div className="cp-clients-hero">
          <img src="/logo-full.png" alt="VS" className="cp-clients-hero-logo" />
          <div>
            <h2>ADD CLIENT</h2>
            <p className="cp-muted">
              Vards + web parole + Capital API. Pec saglabasanas sistema automaticki velk Capital
              tirgus (quick). Ja Markets = 0 — spied PULL MARKETS.
            </p>
          </div>
        </div>
        <form className="cp-form" onSubmit={(e) => void createClient(e)}>
          <div className="cp-grid-2">
            <label>
              Client name
              <input
                value={form.name}
                onChange={(e) => setForm((f) => ({ ...f, name: e.target.value }))}
                placeholder="B.O.S.S"
                required
              />
            </label>
            <label>
              Web password (optional — auto if empty)
              <input
                type="text"
                value={form.password}
                onChange={(e) => setForm((f) => ({ ...f, password: e.target.value }))}
                placeholder="Client login password"
                autoComplete="off"
              />
            </label>
            <label>
              Capital environment
              <select
                value={form.environment}
                onChange={(e) => setForm((f) => ({ ...f, environment: e.target.value }))}
              >
                <option value="live">live</option>
                <option value="demo">demo</option>
              </select>
            </label>
            <label>
              Capital identifier (email)
              <input
                value={form.identifier}
                onChange={(e) => setForm((f) => ({ ...f, identifier: e.target.value }))}
                placeholder="client@email.com"
                autoComplete="off"
              />
            </label>
            <label>
              Capital API key
              <input
                value={form.api_key}
                onChange={(e) => setForm((f) => ({ ...f, api_key: e.target.value }))}
                placeholder="API key from Capital Settings → API"
                autoComplete="off"
              />
            </label>
            <label>
              Capital API password
              <input
                type="password"
                value={form.api_password}
                onChange={(e) => setForm((f) => ({ ...f, api_password: e.target.value }))}
                placeholder="API password (not login password)"
                autoComplete="new-password"
              />
            </label>
          </div>
          <div className="cp-row">
            <button className="cp-btn primary" type="submit" disabled={busy}>
              {busy ? 'CONNECTING…' : 'CREATE + CONNECT'}
            </button>
            <span className="cp-muted">
              Ja Capital lauki aizpildīti — brokeris + web access uzreiz.
            </span>
          </div>
        </form>
        {msg && (
          <p className={/fail|error|unreachable|nav |aizpildi|izskatās/i.test(msg) ? 'cp-error' : 'cp-ok'}>
            {msg}
          </p>
        )}
        {issued && (
          <div className="cp-panel cp-issued" style={{ marginTop: '0.75rem' }}>
            <div className="cp-muted">One-time password for client #{issued.id}</div>
            <div className="cp-issued-code">{issued.code}</div>
            <div className="cp-row" style={{ flexWrap: 'wrap', gap: '0.5rem' }}>
              <button
                type="button"
                className="cp-btn ghost"
                onClick={() => void copyText(issued.code, 'password')}
              >
                COPY PASSWORD
              </button>
              <button
                type="button"
                className="cp-btn primary"
                disabled={!publicUrl}
                onClick={() =>
                  void copyText(
                    `Klienta web: ${publicUrl}\nParole: ${issued.code}`,
                    'URL + password'
                  )
                }
              >
                COPY URL + PASSWORD
              </button>
            </div>
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
                <th>Markets</th>
                <th>Robot</th>
                <th>Market / Lot</th>
                <th>Actions</th>
              </tr>
            </thead>
            <tbody>
              {rows.map((c) => {
                const riskOn = c.risk_enabled !== false;
                const running =
                  String(c.robot_status || c.panel_robot_requested || '').toUpperCase() ===
                  'RUNNING';
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
                        title="Admin risk lock — client cannot start when OFF"
                      >
                        {riskOn ? 'RISK ON' : 'RISK OFF'}
                      </button>
                    </td>
                    <td>
                      <div
                        className={
                          Number(c.capital_market_count || 0) > 0 ? 'cp-ok' : 'cp-error'
                        }
                      >
                        {Number(c.capital_market_count || 0)} markets
                      </div>
                      {Number(c.capital_market_count || 0) === 0 && (
                        <div className="cp-muted">Pull needed</div>
                      )}
                    </td>
                    <td>
                      <span className={`cp-pill ${running ? 'on' : 'off'}`}>
                        {String(c.robot_status || c.panel_robot_requested || 'STOPPED').toUpperCase()}
                      </span>
                      {c.live_trade && (
                        <div className="cp-muted">
                          {c.live_trade.display_name || c.live_trade.market} ·{' '}
                          {Number(c.live_trade.lot_size).toFixed(2)}
                        </div>
                      )}
                      {(c.broker_error || c.status_reason) && (
                        <div className="cp-error">{c.broker_error || c.status_reason}</div>
                      )}
                    </td>
                    <td>
                      <div>{c.panel_display_name || c.panel_epic || '—'}</div>
                      <div className="cp-muted">
                        {c.panel_lot_size != null ? Number(c.panel_lot_size).toFixed(2) : '—'} lot
                      </div>
                    </td>
                    <td>
                      <div className="cp-row">
                        <button
                          type="button"
                          className="cp-btn primary"
                          disabled={busy}
                          onClick={() => void pullMarkets(c)}
                          title="Fetch Capital.com market catalog via API"
                        >
                          PULL MARKETS
                        </button>
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
                  <td colSpan={7} className="cp-muted">
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
