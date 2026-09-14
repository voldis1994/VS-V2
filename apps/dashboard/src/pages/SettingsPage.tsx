import { useState } from 'react';
import { useApi, apiFetch } from '../hooks/useApi';

export function SettingsPage() {
  const { data, refresh } = useApi<Record<string, unknown>>('/api/settings');
  const [newMode, setNewMode] = useState('');
  const [msg, setMsg] = useState<string | null>(null);
  const [err, setErr] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  const goLive = async () => {
    setErr(null);
    setMsg(null);
    const ok = window.confirm(
      'Arm LIVE trading?\n\nRequires healthy broker/data/model/risk.\nEnables real execution.',
    );
    if (!ok) return;
    setBusy(true);
    try {
      await apiFetch('/api/system/runtime-mode', {
        method: 'POST',
        body: JSON.stringify({ mode: 'LIVE', confirm: true, actor: 'dashboard' }),
      });
      setNewMode('LIVE');
      setMsg('LIVE armed (fail-closed health + confirmation)');
      refresh();
    } catch (e) {
      setErr(e instanceof Error ? e.message : 'Failed');
    } finally {
      setBusy(false);
    }
  };

  const changeMode = async () => {
    setErr(null);
    setMsg(null);
    setBusy(true);
    try {
      const mode = newMode || String(data?.operating_mode || 'PAPER');
      let confirm = false;
      if (mode === 'LIVE') {
        confirm = window.confirm(
          'Arm LIVE trading?\n\nRequires healthy broker/data/model/risk.\nEnables real execution.',
        );
        if (!confirm) {
          setMsg('LIVE arm cancelled');
          return;
        }
      }
      await apiFetch('/api/system/runtime-mode', {
        method: 'POST',
        body: JSON.stringify({ mode, confirm, actor: 'dashboard' }),
      });
      setMsg(`Mode → ${mode}`);
      refresh();
    } catch (e) {
      setErr(e instanceof Error ? e.message : 'Mode change failed');
    } finally {
      setBusy(false);
    }
  };

  return (
    <div>
      <h1 className="page-title">Settings</h1>
      <p className="page-subtitle">Fail-closed defaults · LIVE trading requires explicit arming</p>

      <div className="card" style={{ marginBottom: 16 }}>
        <div className="section-title">LIVE</div>
        <p style={{ marginBottom: 12 }}>
          Status:{' '}
          <span className="badge badge-unhealthy">
            {data?.live_trading_enabled === true ? 'ON' : 'OFF'} ·{' '}
            {String(data?.operating_mode ?? 'PAPER')}
          </span>
        </p>
        <div className="actions">
          <button className="btn btn-primary" onClick={goLive} disabled={busy}>
            Switch to LIVE now
          </button>
        </div>
        {msg && <p style={{ marginTop: 8, color: 'var(--accent)' }}>{msg}</p>}
        {err && <p className="error-state" style={{ marginTop: 8 }}>{err}</p>}
      </div>

      <div className="card" style={{ marginBottom: 16 }}>
        <div className="section-title">Operating Mode</div>
        <div className="actions">
          <select
            className="input"
            style={{ maxWidth: 200 }}
            value={newMode || String(data?.operating_mode || 'PAPER')}
            onChange={(e) => setNewMode(e.target.value)}
          >
            
            <option value="PAPER">PAPER</option>
            <option value="SHADOW">SHADOW</option>
            <option value="LIVE">LIVE</option>
          </select>
          <button className="btn btn-primary" onClick={changeMode} disabled={busy}>
            Apply mode
          </button>
        </div>
      </div>

      <div className="card">
        <div className="section-title">System Parameters</div>
        <div className="grid grid-2" style={{ gap: 8 }}>
          <div>Primary Horizon: <strong>{String(data?.primary_horizon_ms)}ms</strong></div>
          <div>Entry TTL: <strong>{String(data?.entry_ttl_ms)}ms</strong></div>
          <div>Log Level: <strong>{String(data?.log_level)}</strong></div>
          <div>Live Enabled: <strong>{String(data?.live_trading_enabled === true)}</strong></div>
        </div>
      </div>
    </div>
  );
}
