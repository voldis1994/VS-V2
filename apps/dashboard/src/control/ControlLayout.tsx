import { NavLink, Outlet, useLocation } from 'react-router-dom';
import { useCallback, useEffect, useRef, useState } from 'react';
import { apiFetch } from '../hooks/useApi';
import '../styles/cyberpink.css';

const NAV = [
  { to: '/control', end: true, label: 'MAIN' },
  { to: '/control/clients', label: 'CLIENTS' },
  { to: '/control/ai', label: 'AI' },
  { to: '/control/errors', label: 'ERRORS' },
  { to: '/control/feed', label: 'FEED' },
  { to: '/control/news', label: 'NEWS' },
  { to: '/control/system', label: 'SYSTEM' },
];

type RuntimeMode = 'PAPER' | 'SHADOW' | 'LIVE';

type RuntimeState = {
  mode?: string;
  operating_mode?: string;
  live_trading_enabled?: boolean;
};

function titleForPath(pathname: string): string {
  if (pathname.includes('/clients')) return 'CLIENTS';
  if (pathname.includes('/ai')) return 'AI';
  if (pathname.includes('/errors')) return 'ERRORS';
  if (pathname.includes('/feed')) return 'FEED';
  if (pathname.includes('/news')) return 'NEWS';
  if (pathname.includes('/system')) return 'SYSTEM';
  return 'MAIN';
}

const WIDE_KEY = 'vs-cp-wide-rail';
const NARROW_MQ = '(max-width: 1100px)';

function readFsElement(): Element | null {
  const doc = document as Document & {
    webkitFullscreenElement?: Element | null;
  };
  return document.fullscreenElement || doc.webkitFullscreenElement || null;
}

async function enterFullscreen(el: HTMLElement) {
  const anyEl = el as HTMLElement & {
    webkitRequestFullscreen?: () => Promise<void> | void;
  };
  if (el.requestFullscreen) {
    await el.requestFullscreen();
    return;
  }
  if (anyEl.webkitRequestFullscreen) {
    await anyEl.webkitRequestFullscreen();
  }
}

async function exitFullscreen() {
  const doc = document as Document & {
    webkitExitFullscreen?: () => Promise<void> | void;
  };
  if (document.exitFullscreen && document.fullscreenElement) {
    await document.exitFullscreen();
    return;
  }
  if (doc.webkitExitFullscreen) {
    await doc.webkitExitFullscreen();
  }
}

export function ControlLayout() {
  const location = useLocation();
  const appRef = useRef<HTMLDivElement>(null);
  const [mode, setMode] = useState<RuntimeMode>('PAPER');
  const [modeBusy, setModeBusy] = useState(false);
  const [modeMsg, setModeMsg] = useState<string | null>(null);
  const [clientCount, setClientCount] = useState(0);
  const [apiOk, setApiOk] = useState<boolean | null>(null);
  const [apiDetail, setApiDetail] = useState<string>('');
  const [isFullscreen, setIsFullscreen] = useState(false);
  const [narrow, setNarrow] = useState(() => {
    try {
      return window.matchMedia(NARROW_MQ).matches;
    } catch {
      return false;
    }
  });
  const [wide, setWide] = useState(() => {
    try {
      if (window.matchMedia(NARROW_MQ).matches) return true;
      return window.localStorage.getItem(WIDE_KEY) === '1';
    } catch {
      return false;
    }
  });

  const probeApi = useCallback(async () => {
    // Only same-origin relative URLs (Vite proxy). Never probe localhost:3000 from the browser
    // (Windows localhost -> ::1 + CORS noise caused false "API online").
    try {
      const health = await fetch('/health', { headers: { Accept: 'application/json' } });
      if (!health.ok) throw new Error(`health ${health.status}`);
      // Prove admin API path works too (not only public /health).
      const clients = await fetch('/api/clients', { headers: { Accept: 'application/json' } });
      if (clients.status === 401) {
        setApiOk(false);
        setApiDetail('API up but admin token missing — restart VS-Dashboard via V2.bat');
        return;
      }
      if (!clients.ok) throw new Error(`clients ${clients.status}`);
      setApiOk(true);
      setApiDetail('Control API :3000');
    } catch {
      setApiOk(false);
      setApiDetail('API offline — Restart-ControlAPI.bat (node only, never npm)');
    }
  }, []);

  const loadMode = useCallback(async () => {
    try {
      const s = await apiFetch<RuntimeState>('/api/system/runtime-mode');
      const m = String(s.mode || s.operating_mode || 'PAPER').toUpperCase();
      if (m === 'LIVE' || m === 'SHADOW' || m === 'PAPER') setMode(m);
    } catch {
      /* keep */
    }
  }, []);

  useEffect(() => {
    void probeApi();
    const id = window.setInterval(() => void probeApi(), 5000);
    return () => window.clearInterval(id);
  }, [probeApi]);

  useEffect(() => {
    void loadMode();
    void apiFetch<Array<{ id: number }>>('/api/clients')
      .then((rows) => setClientCount(Array.isArray(rows) ? rows.length : 0))
      .catch(() => setClientCount(0));
  }, [loadMode]);

  useEffect(() => {
    const syncFs = () => {
      const fsEl = readFsElement();
      setIsFullscreen(Boolean(fsEl));
    };
    document.addEventListener('fullscreenchange', syncFs);
    document.addEventListener('webkitfullscreenchange', syncFs as EventListener);
    syncFs();
    return () => {
      document.removeEventListener('fullscreenchange', syncFs);
      document.removeEventListener('webkitfullscreenchange', syncFs as EventListener);
    };
  }, []);

  useEffect(() => {
    const mq = window.matchMedia(NARROW_MQ);
    const onChange = () => {
      const isNarrow = mq.matches;
      setNarrow(isNarrow);
      if (isNarrow) setWide(true);
    };
    onChange();
    mq.addEventListener('change', onChange);
    return () => mq.removeEventListener('change', onChange);
  }, []);

  useEffect(() => {
    if (narrow) return;
    try {
      window.localStorage.setItem(WIDE_KEY, wide ? '1' : '0');
    } catch {
      /* ignore */
    }
  }, [wide, narrow]);

  useEffect(() => {
    document.documentElement.classList.add('cp-root');
    document.body.classList.add('cp-body-root');
    return () => {
      document.documentElement.classList.remove('cp-root');
      document.body.classList.remove('cp-body-root');
    };
  }, []);

  const toggleFullscreen = async () => {
    try {
      if (readFsElement()) {
        await exitFullscreen();
        return;
      }
      const el = appRef.current || document.documentElement;
      await enterFullscreen(el);
    } catch (e) {
      setModeMsg(e instanceof Error ? e.message : 'Fullscreen blocked by browser');
    }
  };

  const switchMode = async (next: RuntimeMode) => {
    if (next === mode || modeBusy) return;
    if (next === 'LIVE') {
      const typed = window.prompt('Type LIVE to confirm real broker orders');
      if (typed !== 'LIVE') return;
    }
    setModeBusy(true);
    setModeMsg(null);
    try {
      await apiFetch('/api/system/runtime-mode', {
        method: 'POST',
        body: JSON.stringify({ mode: next, confirm: true, actor: 'control-panel' }),
      });
      setMode(next);
      setModeMsg(`Mode → ${next}`);
    } catch (e) {
      setModeMsg(e instanceof Error ? e.message : 'Mode switch failed');
    } finally {
      setModeBusy(false);
      void loadMode();
      void probeApi();
    }
  };

  const shellWide = wide || narrow;

  return (
    <div
      ref={appRef}
      className={`cp-app${isFullscreen ? ' cp-app--fs' : ''}${shellWide ? ' cp-app--wide' : ''}`}
    >
      <div className={`cp-shell${shellWide ? ' cp-shell--wide' : ''}`}>
        <aside className="cp-rail" aria-label="Control navigation">
          <div className="cp-brand">
            <img src="/logo-emblem.png" alt="VS" className="cp-brand-mark" />
            <div className="cp-brand-text">
              <strong>VS SYSTEM</strong>
              <span>CONTROL PANEL</span>
            </div>
          </div>
          <nav className="cp-nav">
            {NAV.map((item) => (
              <NavLink
                key={item.to}
                to={item.to}
                end={item.end}
                className={({ isActive }) => (isActive ? 'active' : undefined)}
              >
                <span>{item.label}</span>
                {item.to === '/control/clients' && clientCount > 0 ? (
                  <span className="badge">{clientCount}</span>
                ) : null}
              </NavLink>
            ))}
          </nav>
          <div className="cp-rail-foot">
            <div className={apiOk ? 'online' : 'offline'}>
              {apiOk === null ? '● Checking API…' : apiOk ? '● API online' : '● API offline'}
            </div>
            <div>{apiDetail || 'VS Control Panel'}</div>
          </div>
        </aside>

        <div className="cp-main">
          <header className="cp-top">
            <h1>{titleForPath(location.pathname)}</h1>
            <div className="cp-top-actions">
              <div className="cp-mode" aria-label="Operating mode">
                {(['PAPER', 'SHADOW', 'LIVE'] as RuntimeMode[]).map((m) => (
                  <button
                    key={m}
                    type="button"
                    disabled={modeBusy}
                    className={
                      mode === m
                        ? m === 'PAPER'
                          ? 'active-paper'
                          : m === 'SHADOW'
                            ? 'active-shadow'
                            : 'active-live'
                        : undefined
                    }
                    onClick={() => void switchMode(m)}
                  >
                    {m}
                  </button>
                ))}
              </div>
              <button
                type="button"
                className="cp-btn ghost"
                onClick={() => void toggleFullscreen()}
                title="Fill the whole monitor (browser fullscreen)"
              >
                {isFullscreen ? 'EXIT FULL' : 'FULLSCREEN'}
              </button>
              {!narrow && (
                <button
                  type="button"
                  className="cp-btn ghost"
                  onClick={() => setWide((v) => !v)}
                  title="Hide/show left menu for more workspace"
                >
                  {wide ? 'SHOW MENU' : 'MORE SPACE'}
                </button>
              )}
              {shellWide && (
                <details className="cp-options">
                  <summary className="cp-btn ghost">NAV</summary>
                  <div className="cp-options-panel">
                    {NAV.map((item) => (
                      <NavLink
                        key={item.to}
                        to={item.to}
                        end={item.end}
                        className="cp-btn"
                        onClick={(e) => {
                          const el = (e.currentTarget as HTMLElement).closest('details');
                          if (el) el.removeAttribute('open');
                        }}
                      >
                        {item.label}
                        {item.to === '/control/clients' && clientCount > 0
                          ? ` (${clientCount})`
                          : ''}
                      </NavLink>
                    ))}
                  </div>
                </details>
              )}
              {modeMsg && <span className="cp-muted">{modeMsg}</span>}
            </div>
          </header>
          {apiOk === false && (
            <div className="cp-api-banner" role="alert">
              <div>
                <strong>Control API nav pieejams (:3000).</strong> Bez API klientus pievienot nevar.
                <div className="cp-muted" style={{ marginTop: '0.35rem' }}>
                  1) Pārbaudi logu <code>VS-ControlAPI</code> / <code>logs\control-api.live.log</code>
                  <br />
                  2) Palaid <code>Restart-ControlAPI.bat</code> (vai visu <code>LIVE.bat</code>)
                  <br />
                  3) Docker Desktop jābūt ieslēgtam (Postgres)
                </div>
              </div>
              <button type="button" className="cp-btn primary" onClick={() => void probeApi()}>
                RETRY API
              </button>
            </div>
          )}
          <div className="cp-body">
            <Outlet />
          </div>
        </div>
      </div>
    </div>
  );
}
