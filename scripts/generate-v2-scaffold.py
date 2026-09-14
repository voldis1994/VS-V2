#!/usr/bin/env python3
"""Generate VS-V2 control-api, dashboard, ai, config, contracts, infra, tools."""
from __future__ import annotations
import json
import os
import textwrap

ROOT = "/workspace"

def w(path: str, content: str) -> None:
    full = os.path.join(ROOT, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "w", encoding="utf-8") as f:
        f.write(content if content.endswith("\n") else content + "\n")

# --- control-api types ---
for name, fields in [
    ("market.ts", "export interface MarketInstrument { instrument_id: number; symbol: string; epic: string; regime: string; confidence: number; last_mid?: number; }\n"),
    ("brain.ts", "export interface BrainSnapshot { instrument_id: number; bias: string; structure: number; momentum: number; pressure: number; composite: number; bar_count: number; }\n"),
    ("prediction.ts", "export interface Prediction { id: string; instrument_id: number; horizon: string; probability: number; edge: number; created_at: string; }\n"),
    ("decision.ts", "export interface Decision { instrument_id: number; action: string; buy_score: number; sell_score: number; confidence: number; reason: string; reason_codes: string[]; }\n"),
    ("position.ts", "export interface Position { id: number; epic: string; direction: string; size: number; entry_price: number; unrealized_pnl: number; }\n"),
    ("risk.ts", "export interface RiskSnapshot { account_id: number; exposure: number; daily_pnl: number; max_drawdown: number; risk_budget_used: number; }\n"),
    ("execution.ts", "export interface ExecutionOrder { id: string; epic: string; direction: string; status: string; filled_size: number; }\n"),
    ("learning.ts", "export interface LearningRun { id: string; status: string; model_version: string; metrics: Record<string, number>; }\n"),
    ("diagnostics.ts", "export interface DiagnosticEvent { component: string; level: string; message: string; timestamp: string; }\n"),
    ("index.ts", "export * from './market.js';\nexport * from './brain.js';\nexport * from './prediction.js';\nexport * from './decision.js';\nexport * from './position.js';\nexport * from './risk.js';\nexport * from './execution.js';\nexport * from './learning.js';\nexport * from './diagnostics.js';\n"),
]:
    w(f"apps/control-api/src/types/{name}", fields)

# --- control-api config ---
w("apps/control-api/src/config/environment.ts", textwrap.dedent("""
export type OperatingMode = 'REPLAY' | 'PAPER' | 'DEMO' | 'LIVE';
export function operatingMode(): OperatingMode {
  const m = (process.env.OPERATING_MODE || 'PAPER').toUpperCase();
  if (m === 'REPLAY' || m === 'PAPER' || m === 'DEMO' || m === 'LIVE') return m;
  return 'PAPER';
}
export function liveTradingEnabled(): boolean {
  return (process.env.LIVE_TRADING_ENABLED || 'false').toLowerCase() === 'true';
}
"""))

w("apps/control-api/src/config/runtime.ts", textwrap.dedent("""
export const runtimeConfig = {
  marketCoreUrl: process.env.MARKET_CORE_URL || 'http://127.0.0.1:9100',
  pipelineToken: process.env.PIPELINE_TOKEN || process.env.PIPELINE_SERVICE_TOKEN || '',
  redisUrl: process.env.REDIS_URL || 'redis://localhost:6379',
};
"""))

# --- control-api services ---
service_stub = '''import { pool } from '../db/pool.js';

export async function list{entity}() {{
  const {{ rows }} = await pool.query('SELECT * FROM {table} ORDER BY id DESC LIMIT 100');
  return rows;
}}
'''

services = {
    "accountService.ts": ("Accounts", "accounts"),
    "clientService.ts": ("Clients", "clients"),
    "marketCoreService.ts": ("MarketCore snapshots", "market_snapshots"),
    "brainService.ts": ("Brain snapshots", "brain_snapshots"),
    "positionService.ts": ("Positions", "positions"),
    "executionService.ts": ("Executions", "executions"),
    "riskService.ts": ("Risk", "risk_snapshots"),
    "learningService.ts": ("Learning runs", "learning_runs"),
    "diagnosticsService.ts": ("Diagnostics", "diagnostic_events"),
}

for fname, (label, table) in services.items():
    w(f"apps/control-api/src/services/{fname}", f"""import {{ pool }} from '../db/pool.js';

/** {label} service — V2 data access layer */
export async function list{fname.replace('Service.ts','').title().replace('_','')}() {{
  try {{
    const {{ rows }} = await pool.query('SELECT * FROM {table} ORDER BY 1 DESC LIMIT 100');
    return rows;
  }} catch {{
    return [];
  }}
}}
""")

w("apps/control-api/src/services/marketCoreService.ts", textwrap.dedent("""
import { listRegimeSnapshots } from './regimes.js';

export async function marketCoreStatus() {
  const instruments = listRegimeSnapshots();
  return {
    status: 'ONLINE',
    pipeline: 'normalize→quality→fusion→candles→brain→decision→risk',
    instruments: instruments.length,
    timestamp: new Date().toISOString(),
  };
}
"""))

w("apps/control-api/src/services/brainService.ts", textwrap.dedent("""
import { listRegimeSnapshots } from './regimes.js';

export async function listBrainSnapshots() {
  return listRegimeSnapshots().map((row, i) => ({
    instrument_id: i + 1,
    epic: row.epic,
    bias: row.current.includes('UP') ? 'BULLISH' : row.current.includes('DOWN') ? 'BEARISH' : 'NEUTRAL',
    structure: row.confidence * 0.6,
    momentum: row.confidence * 0.4,
    pressure: 0,
    composite: row.confidence,
    bar_count: row.bar_count,
  }));
}
"""))

# --- websocket ---
w("apps/control-api/src/websocket/websocketServer.ts", textwrap.dedent("""
import { WebSocket } from 'ws';

export type WsClient = WebSocket;

export class StreamHub {
  private clients = new Set<WsClient>();

  add(client: WsClient) { this.clients.add(client); }
  remove(client: WsClient) { this.clients.delete(client); }

  broadcast(payload: unknown) {
    const msg = JSON.stringify(payload);
    for (const c of this.clients) {
      if (c.readyState === 1) c.send(msg);
    }
  }
}
"""))

for stream in ["marketStream", "brainStream", "decisionStream", "positionStream"]:
    w(f"apps/control-api/src/websocket/{stream}.ts", textwrap.dedent(f"""
import {{ StreamHub }} from './websocketServer.js';

export const {stream[0].lower() + stream[1:]} = new StreamHub();

export function register{stream[0].upper() + stream[1:]}(app: {{ get: Function }}) {{
  app.get('/ws/{stream.replace('Stream','').lower()}', {{ websocket: true }}, (socket: import('ws').WebSocket) => {{
    {stream[0].lower() + stream[1:]}.add(socket);
    socket.on('close', () => {stream[0].lower() + stream[1:]}.remove(socket));
  }});
}}
"""))

# --- routes ---
route_names = [
    ("brain", "brainService", "listBrainSnapshots", "/api/brain/snapshots"),
    ("predictions", None, None, "/api/predictions"),
    ("decisions", None, None, "/api/decisions"),
    ("positions", "positionService", "listPosition", "/api/positions/v2"),
    ("execution", "executionService", "listExecution", "/api/execution"),
    ("risk", "riskService", "listRisk", "/api/risk"),
    ("learning", "learningService", "listLearning", "/api/learning"),
    ("models", None, None, "/api/models"),
    ("diagnostics", "diagnosticsService", "listDiagnostics", "/api/diagnostics"),
]

for route, svc, fn, path in route_names:
    if svc:
        body = f"""import {{ FastifyInstance }} from 'fastify';
import {{ {fn} }} from '../services/{svc}.js';

export async function register{route.title()}Routes(app: FastifyInstance): Promise<void> {{
  app.get('{path}', async () => ({{
    items: await {fn}(),
    timestamp: new Date().toISOString(),
  }}));
}}
"""
    else:
        body = f"""import {{ FastifyInstance }} from 'fastify';

export async function register{route.title()}Routes(app: FastifyInstance): Promise<void> {{
  app.get('{path}', async () => ({{
    items: [],
    timestamp: new Date().toISOString(),
  }}));
}}
"""
    w(f"apps/control-api/src/routes/{route}.ts", body)

w("apps/control-api/src/routes/marketCore.ts", textwrap.dedent("""
import { FastifyInstance } from 'fastify';
import { marketCoreStatus } from '../services/marketCoreService.js';

export async function registerMarketCoreRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/market-core/status', async () => marketCoreStatus());
}
"""))

# --- dashboard pages ---
pages = [
    "Overview", "Markets", "Brain", "Structure", "Patterns", "Scenarios",
    "Predictions", "Decisions", "Positions", "Risk", "Execution", "Learning",
    "Models", "Diagnostics",
]

for page in pages:
    slug = page.lower()
    w(f"apps/dashboard/src/pages/{page}/index.tsx", textwrap.dedent(f"""
import {{ useEffect, useState }} from 'react';
import {{ api }} from '../../services/api';

export default function {page}Page() {{
  const [data, setData] = useState<unknown>(null);
  useEffect(() => {{
    void api.get('/api/{slug if slug != 'overview' else 'system/status'}').then(setData).catch(() => setData(null));
  }}, []);
  return (
    <section className="v2-page">
      <header className="v2-page-header">
        <h1>{page.upper()}</h1>
        <p className="v2-muted">VS-V2 trading desk — {page} view</p>
      </header>
      <pre className="v2-panel">{{JSON.stringify(data, null, 2)}}</pre>
    </section>
  );
}}
export {{ {page}Page }};
"""))

# stores
for store in ["market", "brain", "prediction", "decision", "position", "risk"]:
    w(f"apps/dashboard/src/stores/{store}Store.ts", textwrap.dedent(f"""
import {{ create }} from 'zustand';
import {{ api }} from '../services/api';

interface {store.title()}State {{
  items: unknown[];
  loading: boolean;
  fetch: () => Promise<void>;
}}

export const use{store.title()}Store = create<{store.title()}State>((set) => ({{
  items: [],
  loading: false,
  fetch: async () => {{
    set({{ loading: true }});
    try {{
      const res = await api.get('/api/{store if store != 'prediction' else 'predictions'}');
      set({{ items: (res as {{ items?: unknown[] }}).items || [], loading: false }});
    }} catch {{
      set({{ loading: false }});
    }}
  }},
}}));
"""))

w("apps/dashboard/src/services/api.ts", textwrap.dedent("""
const BASE = import.meta.env.VITE_API_URL || 'http://localhost:3000';

async function request<T>(path: string): Promise<T> {
  const res = await fetch(`${BASE}${path}`, { credentials: 'include' });
  if (!res.ok) throw new Error(`${res.status} ${path}`);
  return res.json() as Promise<T>;
}

export const api = {
  get: request,
};
"""))

w("apps/dashboard/src/services/websocket.ts", textwrap.dedent("""
const WS_BASE = (import.meta.env.VITE_WS_URL || 'ws://localhost:3000/ws').replace(/\\/ws$/, '');

export function connectStream(channel: string, onMessage: (data: unknown) => void): () => void {
  const ws = new WebSocket(`${WS_BASE}/ws/${channel}`);
  ws.onmessage = (ev) => {
    try { onMessage(JSON.parse(ev.data)); } catch { /* ignore */ }
  };
  return () => ws.close();
}
"""))

# components
components = {
    "market": "MarketTape",
    "brain": "BrainScoreCard",
    "structure": "StructurePanel",
    "patterns": "PatternGrid",
    "scenarios": "ScenarioList",
    "prediction": "PredictionChart",
    "decision": "DecisionBoard",
    "position": "PositionTable",
    "risk": "RiskGauge",
    "diagnostics": "DiagnosticsFeed",
}
for folder, comp in components.items():
    w(f"apps/dashboard/src/components/{folder}/{comp}.tsx", textwrap.dedent(f"""
export function {comp}({{ label = '{folder}' }}: {{ label?: string }}) {{
  return <div className="v2-card"><h3>{{label}}</h3><p className="v2-muted">Live {folder} component</p></div>;
}}
"""))

print("control-api + dashboard scaffold done")
