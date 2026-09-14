import { useMemo } from 'react';
import { Link } from 'react-router-dom';
import { useLiveBrainFeed } from '../../hooks/useLiveBrainFeed';
import { selectActiveInstrument, useLiveBrainStore } from '../../state/liveBrainStore';
import type {
  BrainEventRecord,
  InstrumentBrainView,
  TradeAction,
} from '../../types/liveBrain';

function fmt(n: number | null | undefined, d = 4): string {
  if (n == null || !Number.isFinite(n)) return '—';
  return n.toFixed(d);
}

function actionClass(a: TradeAction): string {
  if (a === 'BUY') return 'lt-action buy';
  if (a === 'SELL') return 'lt-action sell';
  return 'lt-action wait';
}

function Zone({
  title,
  sub,
  children,
}: {
  title: string;
  sub?: string;
  children: React.ReactNode;
}) {
  return (
    <section className="lt-zone">
      <header className="lt-zone-h">
        <h2>{title}</h2>
        {sub ? <span>{sub}</span> : null}
      </header>
      <div className="lt-zone-b">{children}</div>
    </section>
  );
}

function Metric({ label, value, tone }: { label: string; value: string; tone?: string }) {
  return (
    <div className={`lt-metric${tone ? ` ${tone}` : ''}`}>
      <span className="lt-metric-l">{label}</span>
      <span className="lt-metric-v">{value}</span>
    </div>
  );
}

function DetailDrawer({ event, onClose }: { event: BrainEventRecord; onClose: () => void }) {
  return (
    <aside className="lt-drawer" role="dialog" aria-label="Event detail">
      <header className="lt-drawer-h">
        <div>
          <p className="lt-kicker">{event.kind.toUpperCase()}</p>
          <h2>{event.title}</h2>
          <p className="lt-muted">{event.summary}</p>
        </div>
        <button type="button" className="lt-btn" onClick={onClose}>
          CLOSE
        </button>
      </header>
      <div className="lt-drawer-meta">
        <span>id {event.id}</span>
        <span>ts_ns {event.ts_ns}</span>
        {event.instrument_id != null ? <span>inst {event.instrument_id}</span> : null}
        {event.epic ? <span>{event.epic}</span> : null}
      </div>
      {event.related_ids.length > 0 ? (
        <p className="lt-muted">related: {event.related_ids.join(', ')}</p>
      ) : null}
      <pre className="lt-evidence">{JSON.stringify(event.evidence, null, 2)}</pre>
    </aside>
  );
}

function InstrumentPanel({ inst }: { inst: InstrumentBrainView }) {
  const longSide = inst.prediction.long_side;
  const shortSide = inst.prediction.short_side;
  return (
    <>
      <Zone title="MARKETS / CHART" sub={inst.symbol || inst.epic || `ID ${inst.instrument_id}`}>
        <div className="lt-quote-plane">
          <div className="lt-mid">{fmt(inst.quote.mid, 5)}</div>
          <div className="lt-quote-row">
            <span>BID {fmt(inst.quote.bid, 5)}</span>
            <span>ASK {fmt(inst.quote.ask, 5)}</span>
            <span>SPR {fmt(inst.quote.spread, 5)}</span>
          </div>
          <div className="lt-spark" aria-hidden>
            <div
              className="lt-spark-fill"
              style={{
                width: `${Math.min(100, Math.max(8, Math.abs(inst.micro.momentum) * 100))}%`,
              }}
            />
          </div>
        </div>
      </Zone>

      <Zone title="BRAIN STATE · 1m+ STRUCTURE" sub={inst.structure.trend_direction}>
        <div className="lt-grid-2">
          <Metric label="SWING" value={fmt(inst.structure.swing_state, 3)} />
          <Metric label="TREND STR" value={fmt(inst.structure.trend_strength, 3)} />
          <Metric label="VOL" value={fmt(inst.structure.volatility, 4)} />
          <Metric label="QUALITY" value={fmt(inst.structure.structure_quality, 3)} />
          <Metric label="COMPRESS" value={fmt(inst.structure.compression, 3)} />
          <Metric label="EXPAND" value={fmt(inst.structure.expansion, 3)} />
          <Metric label="RANGE POS" value={fmt(inst.structure.range_position, 3)} />
          <Metric
            label="AUTHORITY"
            value={inst.has_structure_authority ? 'YES' : 'NO'}
            tone={inst.has_structure_authority ? 'ok' : 'warn'}
          />
        </div>
      </Zone>

      <Zone title="10s MICROSTRUCTURE" sub={inst.has_micro_authority ? 'AUTHORITY' : 'WAITING'}>
        <div className="lt-grid-2">
          <Metric label="MOMENTUM" value={fmt(inst.micro.momentum, 3)} />
          <Metric label="ACCEL" value={fmt(inst.micro.acceleration, 3)} />
          <Metric label="ACCEPT" value={fmt(inst.micro.acceptance, 3)} />
          <Metric label="REJECT" value={fmt(inst.micro.rejection, 3)} />
          <Metric label="RECLAIM" value={fmt(inst.micro.reclaim, 3)} />
          <Metric label="BODY%" value={fmt(inst.micro.body_pct, 3)} />
          <Metric label="BUY PRESS" value={fmt(inst.micro.aggressive_buy_pressure, 3)} />
          <Metric label="SELL PRESS" value={fmt(inst.micro.aggressive_sell_pressure, 3)} />
        </div>
      </Zone>

      <Zone title="LONG / SHORT PREDICTION + EV">
        <div className="lt-pred">
          <div className="lt-pred-col long">
            <h3>LONG</h3>
            <Metric label="EV" value={fmt(longSide.expected_value, 4)} tone="ok" />
            <Metric label="P" value={fmt(longSide.probability, 3)} />
            <Metric label="CONF" value={fmt(longSide.confidence, 3)} />
            <Metric label="MOVE" value={fmt(longSide.expected_move, 4)} />
          </div>
          <div className="lt-pred-col short">
            <h3>SHORT</h3>
            <Metric label="EV" value={fmt(shortSide.expected_value, 4)} tone="danger" />
            <Metric label="P" value={fmt(shortSide.probability, 3)} />
            <Metric label="CONF" value={fmt(shortSide.confidence, 3)} />
            <Metric label="MOVE" value={fmt(shortSide.expected_move, 4)} />
          </div>
        </div>
      </Zone>

      <Zone title="BUY / SELL / WAIT" sub="DecisionEngine sole source">
        <div className={actionClass(inst.decision_action)}>{inst.decision_action}</div>
        <div className="lt-grid-2" style={{ marginTop: '0.75rem' }}>
          <Metric label="DIR" value={inst.decision.direction} />
          <Metric label="EV" value={fmt(inst.decision.expected_value, 4)} />
          <Metric label="P" value={fmt(inst.decision.probability, 3)} />
          <Metric label="SPREAD COST" value={fmt(inst.decision.spread_cost, 5)} />
        </div>
        {inst.decision.reason_codes.length > 0 ? (
          <p className="lt-reasons">{inst.decision.reason_codes.join(' · ')}</p>
        ) : null}
      </Zone>

      <Zone title="POSITIONS + P/L">
        <div className="lt-grid-2">
          <Metric label="SIDE" value={inst.position.direction} />
          <Metric label="QTY" value={fmt(inst.position.quantity, 2)} />
          <Metric label="ENTRY" value={fmt(inst.position.entry_price, 5)} />
          <Metric label="MARK" value={fmt(inst.position.current_price, 5)} />
          <Metric
            label="uPnL"
            value={fmt(inst.position.unrealized_pnl, 2)}
            tone={inst.position.unrealized_pnl >= 0 ? 'ok' : 'danger'}
          />
          <Metric label="rPnL" value={fmt(inst.position.realized_pnl, 2)} />
          <Metric
            label="MFE/MAE"
            value={`${fmt(inst.position.mfe, 2)} / ${fmt(inst.position.mae, 2)}`}
          />
          <Metric label="ACTION" value={inst.position.position_action} />
          <Metric label="DEAL" value={inst.position.deal_id || '—'} />
        </div>
      </Zone>

      <Zone title="RISK / EXECUTION">
        <div className="lt-grid-2">
          <Metric
            label="RISK"
            value={inst.risk.approved ? 'APPROVED' : 'VETO'}
            tone={inst.risk.approved ? 'ok' : 'danger'}
          />
          <Metric label="QTY" value={fmt(inst.risk.approved_quantity, 2)} />
          <Metric label="BUDGET" value={fmt(inst.risk.risk_budget_used, 3)} />
          <Metric label="EXPOSURE" value={fmt(inst.risk.exposure, 2)} />
          <Metric label="DAILY PnL" value={fmt(inst.risk.daily_pnl, 2)} />
          <Metric label="DRAWDOWN" value={fmt(inst.risk.max_drawdown, 2)} />
          <Metric label="EXEC" value={inst.execution.status} />
          <Metric label="FILL" value={fmt(inst.execution.fill_price, 5)} />
          <Metric label="FILLED QTY" value={fmt(inst.execution.filled_quantity, 2)} />
          <Metric label="MSG" value={inst.execution.message || '—'} />
        </div>
      </Zone>
    </>
  );
}

export function LiveTerminalPage() {
  useLiveBrainFeed();
  const snapshot = useLiveBrainStore((s) => s.snapshot);
  const status = useLiveBrainStore((s) => s.status);
  const events = useLiveBrainStore((s) => s.events);
  const selectedEvent = useLiveBrainStore((s) => s.selectedEvent);
  const awaiting = useLiveBrainStore((s) => s.awaitingMarketCore);
  const wsConnected = useLiveBrainStore((s) => s.wsConnected);
  const openEvent = useLiveBrainStore((s) => s.openEvent);
  const closeEvent = useLiveBrainStore((s) => s.closeEvent);
  const selectInstrument = useLiveBrainStore((s) => s.selectInstrument);
  const selectedInstrumentId = useLiveBrainStore((s) => s.selectedInstrumentId);
  const inst = useLiveBrainStore(selectActiveInstrument);

  const instruments = snapshot?.instruments ?? [];

  const healthLine = useMemo(() => {
    const h = snapshot?.health;
    if (!h) return 'NO SNAPSHOT';
    return `CORE ${h.market_core} · FEEDS ${h.feeds} · EXEC ${h.execution} · DATA ${h.data}`;
  }, [snapshot]);

  return (
    <div className="lt-root">
      <header className="lt-top">
        <div className="lt-brand">
          <span className="lt-live-dot" data-on={wsConnected || status?.connected ? '1' : '0'} />
          <div>
            <h1>VS-V2 LIVE BRAIN</h1>
            <p className="lt-muted">
              Authoritative C++ market-core · UI does not invent decisions
            </p>
          </div>
        </div>
        <div className="lt-top-meta">
          <span>mode {snapshot?.operating_mode ?? '—'}</span>
          <span>brain {snapshot?.brain_version ?? status?.brain_version ?? '—'}</span>
          <span>
            model {(snapshot?.model_id ?? status?.model_id) || '—'}@
            {(snapshot?.model_version ?? status?.model_version) || '—'}
          </span>
          <span>{healthLine}</span>
          <Link className="lt-link" to="/overview">
            DESK
          </Link>
        </div>
      </header>

      <div className="lt-inst-bar">
        {instruments.length === 0 ? (
          <span className="lt-muted">
            {awaiting
              ? 'Awaiting market-core POST /api/pipeline/brain-snapshot…'
              : 'No instruments in last snapshot'}
          </span>
        ) : (
          instruments.map((i) => (
            <button
              key={i.instrument_id}
              type="button"
              className={`lt-inst-tab${selectedInstrumentId === i.instrument_id ? ' on' : ''}`}
              onClick={() => selectInstrument(i.instrument_id)}
            >
              {i.symbol || i.epic || `#${i.instrument_id}`}
              <em className={actionClass(i.decision_action)}>{i.decision_action}</em>
            </button>
          ))
        )}
      </div>

      <div className="lt-body">
        <main className="lt-main">
          {inst ? (
            <InstrumentPanel inst={inst} />
          ) : (
            <Zone title="AWAITING AUTHORITATIVE BRAIN">
              <p className="lt-muted">
                Control API relays BrainSnapshot from market-core. Dashboard never synthesizes
                BUY/SELL/WAIT.
              </p>
            </Zone>
          )}

          <Zone title="SYSTEM / DATA HEALTH · LEARNING">
            <div className="lt-grid-2">
              <Metric
                label="FEED"
                value={status?.connected ? 'LIVE' : 'STALE'}
                tone={status?.connected ? 'ok' : 'warn'}
              />
              <Metric label="WS" value={wsConnected ? 'ON' : 'OFF'} />
              <Metric label="INGESTS" value={String(status?.ingest_count ?? 0)} />
              <Metric label="AGE ms" value={status?.age_ms != null ? String(status.age_ms) : '—'} />
              <Metric label="AUTHORITATIVE" value={status?.authoritative ? 'YES' : 'NO'} />
              <Metric
                label="INVENTS?"
                value={status?.invents_decisions ? 'YES' : 'NO'}
                tone="ok"
              />
              <Metric
                label="MODEL"
                value={`${status?.model_id ?? '—'}@${status?.model_version ?? '—'}`}
              />
              <Metric label="ERROR" value={status?.last_error || 'none'} />
            </div>
          </Zone>
        </main>

        <aside className="lt-log">
          <header className="lt-zone-h">
            <h2>DETAIL LOG</h2>
            <span>trades · decisions · predictions · positions · market · errors</span>
          </header>
          <div className="lt-log-list">
            {events.map((ev) => (
              <button
                key={ev.id}
                type="button"
                className="lt-log-row"
                onClick={() => void openEvent(ev.id)}
              >
                <span className="lt-log-kind">{ev.kind}</span>
                <span className="lt-log-title">{ev.title}</span>
                <span className="lt-log-sum">{ev.summary}</span>
              </button>
            ))}
            {events.length === 0 ? <p className="lt-muted">No events yet</p> : null}
          </div>
        </aside>
      </div>

      {selectedEvent ? <DetailDrawer event={selectedEvent} onClose={closeEvent} /> : null}
    </div>
  );
}

export default LiveTerminalPage;
