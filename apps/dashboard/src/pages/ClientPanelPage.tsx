import { useCallback, useEffect, useMemo, useState } from 'react';
import { clientFetch, getClientToken, setClientToken } from '../hooks/useClientApi';
import { useClientWebSocket } from '../hooks/useClientWebSocket';
import '../styles/cyberpink.css';

type Lang = 'lv' | 'ru' | 'en';
type Gate = 'splash' | 'lang' | 'login' | 'app';
type Tab = 'home' | 'trades' | 'settings';

type Market = {
  instrument_id: number;
  epic: string;
  symbol: string;
  display_name: string;
  category: string;
  min_lot: number;
  max_lot: number;
  lot_step: number;
};

type LiveTrade = {
  market: string;
  display_name: string;
  side: 'BUY' | 'SELL';
  trade_type: string;
  regime?: string | null;
  lot_size: number;
  entry_price: number | null;
  status: 'OPEN';
} | null;

type Status = {
  client_id: number;
  client_name: string;
  connection_ok?: boolean;
  connection_status?: 'ONLINE' | 'LOST' | 'ERROR';
  robot_status: 'RUNNING' | 'STARTING' | 'STOPPED' | 'ERROR';
  requested_status?: 'RUNNING' | 'STOPPED';
  broker_status?: 'CONNECTED' | 'DEGRADED' | 'UNKNOWN';
  broker_error?: string | null;
  status_reason?: string | null;
  market: string | null;
  display_name: string | null;
  lot_size: number | null;
  risk_enabled?: boolean;
  live_trade: LiveTrade;
};

const I18N = {
  lv: {
    chooseLang: 'Izvēlies valodu',
    password: 'Parole',
    unlock: 'Atvērt',
    market: 'Tirgus',
    lot: 'Lot size',
    liveTrade: 'LIVE TRADE',
    start: 'START',
    stop: 'STOP',
    home: 'Sākums',
    analytics: 'Analītika',
    bot: 'Bots',
    settings: 'Iestatījumi',
    logout: 'Iziet',
    noTrade: 'Nav atvērta darījuma',
    riskOff: 'Risk izslēgts (admin)',
    online: 'Sistēma online',
  },
  ru: {
    chooseLang: 'Выберите язык',
    password: 'Пароль',
    unlock: 'Войти',
    market: 'Рынок',
    lot: 'Лот',
    liveTrade: 'LIVE TRADE',
    start: 'START',
    stop: 'STOP',
    home: 'Главная',
    analytics: 'Аналитика',
    bot: 'Бот',
    settings: 'Настройки',
    logout: 'Выйти',
    noTrade: 'Нет открытой сделки',
    riskOff: 'Risk выключен (admin)',
    online: 'Система online',
  },
  en: {
    chooseLang: 'Choose language',
    password: 'Password',
    unlock: 'Unlock',
    market: 'Market',
    lot: 'Lot size',
    liveTrade: 'LIVE TRADE',
    start: 'START',
    stop: 'STOP',
    home: 'Home',
    analytics: 'Analytics',
    bot: 'Bot',
    settings: 'Settings',
    logout: 'Log out',
    noTrade: 'No open trade',
    riskOff: 'Risk disabled (admin)',
    online: 'System online',
  },
} as const;

function roundLot(n: number, step: number) {
  const s = step > 0 ? step : 0.01;
  return Math.round(n / s) * s;
}

function fmtLot(n: number) {
  if (!Number.isFinite(n)) return '—';
  return n.toFixed(4).replace(/\.?0+$/, '');
}

export function ClientPanelPage() {
  const [gate, setGate] = useState<Gate>(() => (getClientToken() ? 'app' : 'splash'));
  const [tab, setTab] = useState<Tab>('home');
  const [lang, setLang] = useState<Lang>(
    () => (localStorage.getItem('vs_client_lang') as Lang) || 'lv'
  );
  const [token, setToken] = useState<string | null>(() => getClientToken());
  const [password, setPassword] = useState('');
  const [loginError, setLoginError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [status, setStatus] = useState<Status | null>(null);
  const [markets, setMarkets] = useState<Market[]>([]);
  const [epic, setEpic] = useState('');
  const [lot, setLot] = useState(0.1);
  const [error, setError] = useState<string | null>(null);

  const t = I18N[lang];
  const selected = useMemo(() => markets.find((m) => m.epic === epic) || null, [markets, epic]);
  const running = status?.robot_status === 'RUNNING';
  const starting = status?.robot_status === 'STARTING';
  const errorState = status?.robot_status === 'ERROR';
  const active =
    status?.requested_status === 'RUNNING' || running || starting || errorState;
  const riskOn = status?.risk_enabled !== false;

  useEffect(() => {
    if (gate !== 'splash') return;
    const id = window.setTimeout(() => setGate('lang'), 1500);
    return () => window.clearTimeout(id);
  }, [gate]);

  const refresh = useCallback(async () => {
    const st = await clientFetch<Status>('/api/client/status');
    setStatus(st);
    if (st.market) setEpic(st.market);
    if (st.lot_size != null) setLot(Number(st.lot_size));
    return st;
  }, []);

  const loadMarkets = useCallback(async () => {
    const res = await clientFetch<{ markets: Market[] }>('/api/client/markets');
    setMarkets(res.markets || []);
    return res.markets || [];
  }, []);

  useEffect(() => {
    if (!token || gate !== 'app') return;
    setBusy(true);
    Promise.all([refresh(), loadMarkets()])
      .then(([st, mk]) => {
        if (!st.market && mk[0]) {
          setEpic(mk[0].epic);
          setLot(mk[0].min_lot);
        }
      })
      .catch((e) => {
        setError(e instanceof Error ? e.message : 'Session error');
        if (String(e).toLowerCase().includes('unauthorized')) {
          setClientToken(null);
          setToken(null);
          setGate('login');
        }
      })
      .finally(() => setBusy(false));
  }, [token, gate, refresh, loadMarkets]);

  useEffect(() => {
    if (!token || !active || gate !== 'app') return;
    const id = window.setInterval(() => void refresh().catch(() => undefined), 3000);
    return () => window.clearInterval(id);
  }, [token, active, gate, refresh]);

  useClientWebSocket(Boolean(token) && gate === 'app', () => {
    void refresh().catch(() => undefined);
  });

  const pickLang = (l: Lang) => {
    setLang(l);
    localStorage.setItem('vs_client_lang', l);
    setGate(token ? 'app' : 'login');
  };

  const login = async () => {
    setLoginError(null);
    setBusy(true);
    try {
      const res = await clientFetch<{ token: string }>('/api/client-auth/login', {
        method: 'POST',
        body: JSON.stringify({ access_code: password.trim() }),
      });
      setClientToken(res.token);
      setToken(res.token);
      setPassword('');
      setGate('app');
      setTab('home');
    } catch (e) {
      setLoginError(e instanceof Error ? e.message : 'Login failed');
    } finally {
      setBusy(false);
    }
  };

  const logout = async () => {
    try {
      await clientFetch('/api/client-auth/logout', { method: 'POST' });
    } catch {
      /* ignore */
    }
    setClientToken(null);
    setToken(null);
    setStatus(null);
    setGate('lang');
  };

  const persist = async (nextEpic: string, nextLot: number) => {
    await clientFetch('/api/client/config', {
      method: 'PUT',
      body: JSON.stringify({ epic: nextEpic, lot_size: nextLot }),
    });
    await refresh();
  };

  const bumpLot = async (dir: -1 | 1) => {
    if (!selected || active || !riskOn) return;
    const step = selected.lot_step || 0.01;
    const next = Math.min(
      selected.max_lot,
      Math.max(selected.min_lot, roundLot(lot + dir * step, step))
    );
    setLot(next);
    try {
      await persist(selected.epic, next);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Lot update failed');
    }
  };

  const onMarket = async (value: string) => {
    if (active || !riskOn) return;
    const m = markets.find((x) => x.epic === value);
    if (!m) return;
    setEpic(m.epic);
    setLot(m.min_lot);
    try {
      await persist(m.epic, m.min_lot);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Market update failed');
    }
  };

  const toggleRobot = async () => {
    if (busy) return;
    if (!active && !riskOn) {
      setError(t.riskOff);
      return;
    }
    setBusy(true);
    setError(null);
    try {
      if (!active) {
        if (!epic) throw new Error('Select market');
        await persist(epic, lot);
        const res = await clientFetch<{ status: Status }>('/api/client/start', {
          method: 'POST',
          body: JSON.stringify({}),
        });
        setStatus(res.status);
      } else {
        const res = await clientFetch<{ status: Status }>('/api/client/stop', {
          method: 'POST',
          body: JSON.stringify({}),
        });
        setStatus(res.status);
      }
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Action failed');
      try {
        await refresh();
      } catch {
        /* ignore */
      }
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="cw-app">
      <div className="cw-phone">
        {gate === 'splash' && (
          <div className="cw-splash">
            <img className="cw-logo-full" src="/logo-full.png" alt="VS" />
            <div className="cw-title">VS SYSTEM</div>
          </div>
        )}

        {gate === 'lang' && (
          <div className="cw-lang">
            <img className="cw-logo-sm" src="/logo-full.png" alt="VS" />
            <div className="cw-title">{t.chooseLang}</div>
            <div className="cw-btn-stack">
              <button type="button" className="cw-btn lang" onClick={() => pickLang('lv')}>
                Latviešu
              </button>
              <button type="button" className="cw-btn lang" onClick={() => pickLang('ru')}>
                Русский
              </button>
              <button type="button" className="cw-btn lang" onClick={() => pickLang('en')}>
                English
              </button>
            </div>
          </div>
        )}

        {gate === 'login' && (
          <div className="cw-login">
            <img className="cw-logo-sm" src="/logo-full.png" alt="VS" />
            <div className="cw-title">VS SYSTEM</div>
            <div className="cw-field">
              <label htmlFor="cw-pass">{t.password}</label>
              <input
                id="cw-pass"
                className="cw-input"
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === 'Enter') void login();
                }}
                autoComplete="current-password"
              />
            </div>
            {loginError && <div className="cp-error">{loginError}</div>}
            <div className="cw-btn-stack">
              <button type="button" className="cw-btn primary" disabled={busy} onClick={() => void login()}>
                {t.unlock}
              </button>
            </div>
          </div>
        )}

        {gate === 'app' && (
          <>
            <div className="cw-header">
              <img className="cw-logo-sm" src="/logo-full.png" alt="VS" />
              <div className="cw-client-name">{status?.client_name || '…'}</div>
              <div className="cp-muted">{t.online}</div>
            </div>

            <div className="cw-main">
              {tab === 'home' && (
                <>
                  <button
                    type="button"
                    className={`cw-robot ${running ? 'running' : ''}`}
                    disabled={busy}
                    onClick={() => void toggleRobot()}
                    aria-label={active ? t.stop : t.start}
                  >
                    <img className="bot-img" src="/logo-emblem.png" alt="" />
                  </button>
                  <div className="cw-robot-meta">
                    <span className="start">{t.start}</span>
                    <span className="stop">{t.stop}</span>
                  </div>
                  {!riskOn && <div className="cp-error">{t.riskOff}</div>}

                  <div className="cw-card">
                    <div className="cap">{t.market}</div>
                    <select
                      className="cw-select"
                      value={epic}
                      disabled={active || busy || !riskOn || markets.length === 0}
                      onChange={(e) => void onMarket(e.target.value)}
                    >
                      {markets.length === 0 && <option value="">—</option>}
                      {markets.map((m) => (
                        <option key={m.instrument_id} value={m.epic}>
                          {m.display_name}
                        </option>
                      ))}
                    </select>
                    <div className="cap" style={{ marginTop: '0.85rem' }}>
                      {t.lot}
                    </div>
                    <div className="cw-lot-row">
                      <button
                        type="button"
                        className="cw-lot-btn"
                        disabled={active || !riskOn}
                        onClick={() => void bumpLot(-1)}
                      >
                        −
                      </button>
                      <div className="cw-lot-val">{fmtLot(lot)}</div>
                      <button
                        type="button"
                        className="cw-lot-btn"
                        disabled={active || !riskOn}
                        onClick={() => void bumpLot(1)}
                      >
                        +
                      </button>
                    </div>
                  </div>

                  <div className="cw-card cw-live">
                    <div className="cap">{t.liveTrade}</div>
                    {status?.live_trade ? (
                      <div>
                        <div>
                          {status.live_trade.side} ·{' '}
                          {status.live_trade.display_name || status.live_trade.market}
                        </div>
                        <div className="cp-muted">
                          {fmtLot(status.live_trade.lot_size)} · entry{' '}
                          {status.live_trade.entry_price ?? '—'}
                        </div>
                      </div>
                    ) : (
                      <div className="cp-muted">{t.noTrade}</div>
                    )}
                  </div>
                  {error && <div className="cp-error">{error}</div>}
                </>
              )}

              {tab === 'trades' && (
                <div className="cw-card">
                  <div className="cap">{t.analytics}</div>
                  <div className="cp-muted">{status?.display_name || status?.market || '—'}</div>
                  <div className="cp-muted">Robot: {status?.robot_status || '—'}</div>
                  <div className="cp-muted">Broker: {status?.broker_status || '—'}</div>
                </div>
              )}

              {tab === 'settings' && (
                <div className="cw-card">
                  <div className="cap">{t.settings}</div>
                  <div className="cw-btn-stack">
                    <button type="button" className="cw-btn lang" onClick={() => setGate('lang')}>
                      {t.chooseLang}
                    </button>
                    <button type="button" className="cw-btn" onClick={() => void logout()}>
                      {t.logout}
                    </button>
                  </div>
                </div>
              )}
            </div>

            <nav className="cw-nav" aria-label="Client navigation">
              <button type="button" className={tab === 'home' ? 'active' : ''} onClick={() => setTab('home')}>
                <span className="ico">⌂</span>
                {t.home}
              </button>
              <button
                type="button"
                className={tab === 'trades' ? 'active' : ''}
                onClick={() => setTab('trades')}
              >
                <span className="ico">▣</span>
                {t.analytics}
              </button>
              <button
                type="button"
                className={tab === 'settings' ? 'active' : ''}
                onClick={() => setTab('settings')}
              >
                <span className="ico">⚙</span>
                {t.settings}
              </button>
            </nav>
          </>
        )}
      </div>
    </div>
  );
}
