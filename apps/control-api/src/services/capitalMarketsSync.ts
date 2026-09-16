/**
 * Pull Capital.com market catalog into capital_markets and seed account instruments.
 * Used after client/broker provision and by trading pull endpoint.
 */
import { pool } from '../db/pool.js';
import { decrypt } from '../security/encryption.js';
import { logAudit } from './audit.js';
import { acquireCapitalSession, fetchAllCapitalMarkets, type CapitalMarket } from './capitalCom.js';

async function tradingHelpers() {
  const mod = await import('../routes/trading.js');
  return {
    ensureBrokerAccount: mod.ensureBrokerAccount,
    seedAccountInstruments: mod.seedAccountInstruments,
  };
}

async function loadCredentialMap(brokerConnectionId: number): Promise<Record<string, string>> {
  const { rows } = await pool.query(
    `SELECT credential_type, ciphertext, iv, tag
     FROM api_credential_metadata
     WHERE broker_connection_id = $1`,
    [brokerConnectionId]
  );
  const out: Record<string, string> = {};
  for (const row of rows) {
    out[row.credential_type as string] = decrypt(
      row.ciphertext as string,
      row.iv as string,
      row.tag as string
    );
  }
  return out;
}

export type PullMarketsResult =
  | {
      ok: true;
      count: number;
      connection_id: number;
      account_id: number;
      mode: 'quick' | 'full';
      sample: Array<{ epic: string; name: string }>;
    }
  | { ok: false; error: string; statusCode: number };

async function upsertMarkets(connectionId: number, markets: CapitalMarket[]): Promise<void> {
  for (const m of markets) {
    await pool.query(
      `INSERT INTO capital_markets
       (broker_connection_id, epic, symbol, display_name, instrument_type, category, min_lot, max_lot, lot_step)
       VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9)
       ON CONFLICT (broker_connection_id, epic) DO UPDATE SET
         symbol = EXCLUDED.symbol,
         display_name = EXCLUDED.display_name,
         instrument_type = EXCLUDED.instrument_type,
         category = EXCLUDED.category,
         min_lot = EXCLUDED.min_lot,
         max_lot = EXCLUDED.max_lot,
         lot_step = EXCLUDED.lot_step,
         updated_at = NOW()`,
      [
        connectionId,
        m.epic,
        m.symbol,
        m.display_name,
        m.instrument_type,
        m.category,
        m.min_lot,
        m.max_lot,
        m.lot_step,
      ]
    );
  }
}

async function orphanCleanup(
  connectionId: number,
  accountId: number,
  seenEpics: string[]
): Promise<void> {
  if (seenEpics.length === 0) return;
  const orphan = await pool.query(
    `SELECT id FROM capital_markets
     WHERE broker_connection_id = $1 AND NOT (epic = ANY($2::text[]))`,
    [connectionId, seenEpics]
  );
  const orphanIds = orphan.rows.map((r) => r.id as number);
  if (orphanIds.length === 0) return;
  await pool.query(
    `DELETE FROM account_instrument_settings
     WHERE broker_account_id = $1 AND instrument_id = ANY($2::int[])`,
    [accountId, orphanIds]
  );
  await pool.query('DELETE FROM capital_markets WHERE id = ANY($1::int[])', [orphanIds]);
}

/**
 * Pull + store Capital markets for a broker connection / trading account.
 * mode=quick: search sweep (~seconds). mode=full: navigation tree + search (1–3 min).
 */
export async function pullAndStoreCapitalMarkets(opts: {
  connectionId: number;
  accountId?: number;
  mode?: 'quick' | 'full';
  actor?: string;
}): Promise<PullMarketsResult> {
  const mode = opts.mode || 'quick';
  const { rows } = await pool.query(
    `SELECT bc.id as connection_id, bc.broker_name, bc.environment, bc.identifier, bc.client_id,
            c.name as client_name
     FROM broker_connections bc
     JOIN clients c ON c.id = bc.client_id
     WHERE bc.id = $1`,
    [opts.connectionId]
  );
  if (rows.length === 0) {
    return { ok: false, error: 'Broker connection not found', statusCode: 404 };
  }
  const conn = rows[0] as {
    connection_id: number;
    broker_name: string;
    environment: string;
    identifier: string | null;
    client_id: number;
    client_name: string;
  };
  if (conn.broker_name !== 'capital_com') {
    return { ok: false, error: 'Only Capital.com connections can pull markets', statusCode: 400 };
  }

  const { ensureBrokerAccount, seedAccountInstruments } = await tradingHelpers();
  const accountId =
    opts.accountId ??
    (await ensureBrokerAccount(
      conn.connection_id,
      `${conn.client_name} / capital_com (${conn.environment})`
    ));

  const creds = await loadCredentialMap(conn.connection_id);
  const apiKey = creds.api_key || '';
  const password = creds.password || '';
  const identifier = (conn.identifier || '').trim();
  if (!apiKey || !password || !identifier) {
    return {
      ok: false,
      error: 'Missing Capital.com credentials (api_key / password / identifier)',
      statusCode: 400,
    };
  }

  const opened = await acquireCapitalSession({
    environment: conn.environment,
    apiKey,
    identifier,
    password,
    connectionId: conn.connection_id,
  });
  if (!opened.ok) {
    return { ok: false, error: opened.result.detail, statusCode: 400 };
  }

  const markets = await fetchAllCapitalMarkets(opened.session, { mode });
  if (markets.length === 0) {
    return {
      ok: false,
      error:
        'Capital.com returned 0 markets. Check Live/Demo environment matches the API key, and key has market-data permission.',
      statusCode: 502,
    };
  }

  await upsertMarkets(conn.connection_id, markets);
  if (mode === 'full') {
    await orphanCleanup(
      conn.connection_id,
      accountId,
      markets.map((m) => m.epic)
    );
  }
  await seedAccountInstruments(accountId);
  await logAudit(
    opts.actor || 'admin',
    'capital_markets_pulled',
    'broker_connection',
    String(conn.connection_id),
    null,
    { count: markets.length, environment: conn.environment, mode }
  );

  return {
    ok: true,
    count: markets.length,
    connection_id: conn.connection_id,
    account_id: accountId,
    mode,
    sample: markets.slice(0, 8).map((m) => ({ epic: m.epic, name: m.display_name })),
  };
}

/** Fire-and-forget full catalog sync after quick seed (does not block HTTP). */
export function scheduleFullCapitalMarketsPull(connectionId: number, accountId: number): void {
  setImmediate(() => {
    void pullAndStoreCapitalMarkets({
      connectionId,
      accountId,
      mode: 'full',
      actor: 'background',
    }).catch((err) => {
      console.warn(
        `[capitalMarketsSync] background full pull failed connection=${connectionId}:`,
        err instanceof Error ? err.message : err
      );
    });
  });
}

export async function countCapitalMarkets(connectionId: number): Promise<number> {
  const { rows } = await pool.query(
    `SELECT COUNT(*)::int AS n FROM capital_markets WHERE broker_connection_id = $1`,
    [connectionId]
  );
  return Number(rows[0]?.n || 0);
}
