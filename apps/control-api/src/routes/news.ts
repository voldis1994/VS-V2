import { FastifyInstance } from 'fastify';
import { pool } from '../db/pool.js';

type NewsItem = {
  id: string;
  headline: string;
  impact: 'high' | 'medium' | 'low';
  markets: string[];
  why: string;
  source: string;
};

const MACRO_WATCH: NewsItem[] = [
  {
    id: 'dxy-watch',
    headline: 'US Dollar (DXY) direction remains a EUR / Gold driver',
    impact: 'high',
    markets: ['EURUSD', 'XAUUSD', 'GOLD'],
    why: 'DXY strength typically pressures EUR and can weigh on gold.',
    source: 'VS desk macro',
  },
  {
    id: 'yields-watch',
    headline: 'US yields / real rates watch for precious metals',
    impact: 'high',
    markets: ['XAUUSD', 'GOLD', 'XAGUSD'],
    why: 'Rising real yields often compete with non-yielding metals.',
    source: 'VS desk macro',
  },
  {
    id: 'risk-watch',
    headline: 'Risk sentiment check for indices and crypto',
    impact: 'medium',
    markets: ['US100', 'NAS100', 'BTCUSD', 'ETHUSD'],
    why: 'Risk-off flows can hit high-beta indices and crypto together.',
    source: 'VS desk macro',
  },
  {
    id: 'oil-watch',
    headline: 'Crude / energy impulse can spill into USD crosses',
    impact: 'medium',
    markets: ['USOIL', 'UKOIL', 'USDJPY'],
    why: 'Energy shocks can move USD and inflation expectations.',
    source: 'VS desk macro',
  },
  {
    id: 'ecu-watch',
    headline: 'ECB / EU data pulse for EUR majors',
    impact: 'medium',
    markets: ['EURUSD', 'EURGBP', 'EURJPY'],
    why: 'EUR pricing reacts quickly to EU growth and rate path headlines.',
    source: 'VS desk macro',
  },
];

function matchesMarket(itemMarkets: string[], open: string[]): boolean {
  if (open.length === 0) return true;
  return itemMarkets.some((m) =>
    open.some((o) => o.toUpperCase().includes(m.toUpperCase()) || m.toUpperCase().includes(o.toUpperCase()))
  );
}

/**
 * News desk for Control Panel — tags macro watch items to clients' open/selected markets.
 * Fail-soft: never blocks UI; no broker calls.
 */
export async function registerNewsRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/news/desk', async () => {
    let openMarkets: string[] = [];
    try {
      const { rows } = await pool.query(
        `SELECT DISTINCT COALESCE(panel_epic, panel_display_name) AS m
         FROM clients
         WHERE enabled = true
           AND (
             panel_epic IS NOT NULL
             OR panel_display_name IS NOT NULL
             OR UPPER(COALESCE(panel_robot_requested, '')) = 'RUNNING'
           )`
      );
      openMarkets = rows
        .map((r) => String(r.m || '').trim())
        .filter(Boolean);
    } catch {
      openMarkets = [];
    }

    const items = MACRO_WATCH.filter((item) => matchesMarket(item.markets, openMarkets)).map(
      (item) => ({
        ...item,
        markets:
          openMarkets.length === 0
            ? item.markets
            : item.markets.filter((m) =>
                openMarkets.some(
                  (o) =>
                    o.toUpperCase().includes(m.toUpperCase()) ||
                    m.toUpperCase().includes(o.toUpperCase())
                )
              ),
      })
    );

    return {
      generated_at: new Date().toISOString(),
      open_markets: openMarkets,
      items: items.length > 0 ? items : MACRO_WATCH,
      note:
        openMarkets.length === 0
          ? 'No client markets selected — showing macro watchlist.'
          : 'Filtered to markets linked to enabled clients.',
    };
  });
}
