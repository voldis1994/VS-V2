import { FastifyInstance } from 'fastify';
import { pool } from '../db/pool.js';
import { logAudit } from '../services/audit.js';
import { generateAccessCode, hashAccessCode } from '../security/accessCode.js';
import { encrypt, maskSecret } from '../security/encryption.js';
import { revokeAllClientSessions } from '../security/clientSession.js';
import {
  getClientPanelStatus,
  stopClientRobot,
} from '../services/clientPanel.js';
import { ensureBrokerAccount, seedAccountInstruments } from './trading.js';
import {
  pullAndStoreCapitalMarkets,
  scheduleFullCapitalMarketsPull,
  countCapitalMarkets,
} from '../services/capitalMarketsSync.js';

async function hardDeleteClient(clientId: string): Promise<void> {
  const db = await pool.connect();
  try {
    await db.query('BEGIN');
    await db.query('DELETE FROM client_sessions WHERE client_id = $1', [clientId]);
    await db.query('DELETE FROM client_login_attempts WHERE 1=0'); // keep attempts global
    const accounts = await db.query(
      `SELECT ba.id
       FROM broker_accounts ba
       JOIN broker_connections bc ON bc.id = ba.broker_connection_id
       WHERE bc.client_id = $1`,
      [clientId]
    );
    const accountIds = accounts.rows.map((r) => r.id as number);
    if (accountIds.length > 0) {
      await db.query('DELETE FROM account_instrument_settings WHERE broker_account_id = ANY($1::int[])', [accountIds]);
      await db.query('DELETE FROM positions WHERE broker_account_id = ANY($1::int[])', [accountIds]);
      await db.query('DELETE FROM executions WHERE broker_account_id = ANY($1::int[])', [accountIds]);
      await db.query('DELETE FROM trades WHERE broker_account_id = ANY($1::int[])', [accountIds]);
    }
    const conns = await db.query(
      'SELECT id FROM broker_connections WHERE client_id = $1',
      [clientId]
    );
    const connIds = conns.rows.map((r) => r.id as number);
    if (connIds.length > 0) {
      await db.query(
        'DELETE FROM api_credential_metadata WHERE broker_connection_id = ANY($1::int[])',
        [connIds]
      );
      await db.query(
        'DELETE FROM broker_accounts WHERE broker_connection_id = ANY($1::int[])',
        [connIds]
      );
      await db.query('DELETE FROM broker_connections WHERE id = ANY($1::int[])', [connIds]);
    }
    await db.query('DELETE FROM clients WHERE id = $1', [clientId]);
    await db.query('COMMIT');
  } catch (err) {
    await db.query('ROLLBACK');
    throw err;
  } finally {
    db.release();
  }
}

export async function registerClientRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/clients', async () => {
    const { rows } = await pool.query(
      `SELECT c.id, c.name, c.enabled, c.access_enabled,
              c.access_code_hash IS NOT NULL as has_access_code,
              COALESCE(c.risk_enabled, true) as risk_enabled,
              c.preferred_broker_account_id,
              c.panel_epic, c.panel_display_name, c.panel_lot_size,
              c.panel_robot_requested, c.last_seen_at,
              c.created_at, c.updated_at,
              (
                SELECT COUNT(*)::int FROM capital_markets cm
                JOIN broker_connections bc ON bc.id = cm.broker_connection_id
                WHERE bc.client_id = c.id
              ) as capital_market_count,
              (
                SELECT ba.id FROM broker_accounts ba
                JOIN broker_connections bc ON bc.id = ba.broker_connection_id
                WHERE bc.client_id = c.id AND bc.broker_name = 'capital_com'
                ORDER BY ba.id ASC LIMIT 1
              ) as capital_account_id,
              (
                SELECT bc.id FROM broker_connections bc
                WHERE bc.client_id = c.id AND bc.broker_name = 'capital_com'
                ORDER BY bc.id ASC LIMIT 1
              ) as capital_connection_id
       FROM clients c
       ORDER BY c.created_at DESC`
    );

    const out = [];
    for (const row of rows) {
      let panel = null;
      try {
        panel = await getClientPanelStatus(row.id as number);
      } catch {
        panel = null;
      }
      out.push({
        id: row.id,
        name: row.name,
        enabled: row.enabled,
        access_enabled: row.access_enabled,
        has_access_code: row.has_access_code,
        risk_enabled: row.risk_enabled !== false,
        preferred_broker_account_id: row.preferred_broker_account_id,
        panel_epic: row.panel_epic,
        panel_display_name: row.panel_display_name,
        panel_lot_size: row.panel_lot_size != null ? Number(row.panel_lot_size) : null,
        panel_robot_requested: row.panel_robot_requested,
        last_seen_at: row.last_seen_at,
        created_at: row.created_at,
        updated_at: row.updated_at,
        robot_status: panel?.robot_status ?? 'STOPPED',
        live_trade: panel?.live_trade ?? null,
        account_id: panel?.account_id ?? row.capital_account_id ?? null,
        broker_error: panel?.broker_error ?? null,
        status_reason: panel?.status_reason ?? null,
        capital_market_count: Number(row.capital_market_count || 0),
        capital_connection_id: row.capital_connection_id ?? null,
      });
    }
    return out;
  });

  app.get('/api/clients/:id', async (request) => {
    const { id } = request.params as { id: string };
    const { rows } = await pool.query(
      `SELECT id, name, enabled, access_enabled,
              access_code_hash IS NOT NULL as has_access_code,
              preferred_broker_account_id,
              panel_epic, panel_display_name, panel_lot_size,
              panel_robot_requested, last_seen_at, created_at, updated_at
       FROM clients WHERE id = $1`,
      [id]
    );
    if (rows.length === 0) return { error: 'Not found' };
    const accounts = await pool.query(
      `SELECT ba.*, bc.broker_name, bc.environment
       FROM broker_accounts ba
       JOIN broker_connections bc ON bc.id = ba.broker_connection_id
       WHERE bc.client_id = $1`,
      [id]
    );
    let panel = null;
    try {
      panel = await getClientPanelStatus(Number(id));
    } catch {
      panel = null;
    }
    return { ...rows[0], accounts: accounts.rows, panel };
  });

  app.post('/api/clients', async (request, reply) => {
    const body = request.body as {
      name?: string;
      password?: string;
      access_enabled?: boolean;
      risk_enabled?: boolean;
      capital?: {
        environment?: string;
        identifier?: string;
        api_key?: string;
        password?: string;
      };
    };

    const name = String(body.name || '').trim();
    if (!name) {
      return reply.code(400).send({ error: 'name is required', message: 'name is required' });
    }

    const capital = body.capital;
    if (capital) {
      const identifier = String(capital.identifier || '').trim();
      const apiKey = String(capital.api_key || '').trim();
      const apiPassword = String(capital.password || '').trim();
      const environment = String(capital.environment || 'live').trim() || 'live';
      if (!identifier || !apiKey || !apiPassword) {
        return reply.code(400).send({
          error: 'Capital.com requires identifier, api_key, and password',
          message: 'Capital.com requires identifier (email), API key, and API password',
        });
      }
      if (apiKey.includes('@')) {
        return reply.code(400).send({
          error: 'API Key looks like an email',
          message:
            'API Key looks like an email. Put email in Identifier, and paste the Capital.com API Key in API Key.',
        });
      }

      const client = await pool.connect();
      try {
        await client.query('BEGIN');
        const created = await client.query(
          `INSERT INTO clients (name, enabled, access_enabled, risk_enabled)
           VALUES ($1, true, $2, $3)
           RETURNING id, name, enabled, access_enabled, risk_enabled, created_at`,
          [
            name,
            body.access_enabled !== false,
            body.risk_enabled !== false,
          ]
        );
        const row = created.rows[0] as {
          id: number;
          name: string;
          enabled: boolean;
          access_enabled: boolean;
          risk_enabled: boolean;
          created_at: string;
        };

        const plainPassword =
          String(body.password || '').trim() || generateAccessCode();
        if (plainPassword.length < 6) {
          throw new Error('Password must be at least 6 characters');
        }
        await client.query(
          `UPDATE clients SET
             access_code_hash = $2,
             access_enabled = true,
             updated_at = NOW()
           WHERE id = $1`,
          [row.id, hashAccessCode(plainPassword)]
        );

        const conn = await client.query(
          `INSERT INTO broker_connections (client_id, broker_name, environment, identifier)
           VALUES ($1, 'capital_com', $2, $3) RETURNING id`,
          [row.id, environment, identifier]
        );
        const connectionId = conn.rows[0].id as number;
        const encKey = encrypt(apiKey);
        const encPw = encrypt(apiPassword);
        await client.query(
          `INSERT INTO api_credential_metadata
           (broker_connection_id, credential_type, ciphertext, iv, tag, masked_value)
           VALUES ($1, 'api_key', $2, $3, $4, $5)`,
          [connectionId, encKey.ciphertext, encKey.iv, encKey.tag, maskSecret(apiKey)]
        );
        await client.query(
          `INSERT INTO api_credential_metadata
           (broker_connection_id, credential_type, ciphertext, iv, tag, masked_value)
           VALUES ($1, 'password', $2, $3, $4, $5)`,
          [connectionId, encPw.ciphertext, encPw.iv, encPw.tag, maskSecret(apiPassword)]
        );

        await client.query('COMMIT');

        const accountId = await ensureBrokerAccount(
          connectionId,
          `${row.name} / capital_com (${environment})`
        );

        // Auto-pull Capital markets (quick seed) so FEED/robot are not empty.
        let marketsCount = 0;
        let marketsError: string | null = null;
        let marketsSample: Array<{ epic: string; name: string }> = [];
        const pull = await pullAndStoreCapitalMarkets({
          connectionId,
          accountId,
          mode: 'quick',
          actor: 'admin',
        });
        if (pull.ok) {
          marketsCount = pull.count;
          marketsSample = pull.sample;
          scheduleFullCapitalMarketsPull(connectionId, accountId);
        } else {
          marketsError = pull.error;
          await seedAccountInstruments(accountId);
        }

        await logAudit('admin', 'client_provisioned', 'client', String(row.id), null, {
          broker_connection_id: connectionId,
          account_id: accountId,
          environment,
          capital_market_count: marketsCount,
          markets_error: marketsError,
        });

        return {
          ...row,
          access_enabled: true,
          has_access_code: true,
          access_code: plainPassword,
          broker_connection_id: connectionId,
          account_id: accountId,
          capital_market_count: marketsCount,
          capital_markets_sample: marketsSample,
          capital_markets_error: marketsError,
          message: marketsError
            ? `Client created, but Capital markets pull failed: ${marketsError}. Use PULL MARKETS on the client row.`
            : `Client created with Capital.com. Pulled ${marketsCount} markets (full catalog sync continues in background). Save the access_code now.`,
        };
      } catch (err) {
        await client.query('ROLLBACK');
        const message = err instanceof Error ? err.message : 'Provision failed';
        return reply.code(400).send({ error: message, message });
      } finally {
        client.release();
      }
    }

    const { rows } = await pool.query(
      `INSERT INTO clients (name, enabled, access_enabled, risk_enabled)
       VALUES ($1, true, $2, $3)
       RETURNING id, name, enabled, access_enabled, risk_enabled, created_at`,
      [name, body.access_enabled === true, body.risk_enabled !== false]
    );
    let accessCode: string | null = null;
    if (body.password || body.access_enabled) {
      accessCode = String(body.password || '').trim() || generateAccessCode();
      await pool.query(
        `UPDATE clients SET
           access_code_hash = $2,
           access_enabled = true,
           updated_at = NOW()
         WHERE id = $1`,
        [rows[0].id, hashAccessCode(accessCode)]
      );
    }
    await logAudit('admin', 'client_created', 'client', String(rows[0].id), null, rows[0]);
    return {
      ...rows[0],
      has_access_code: Boolean(accessCode),
      access_code: accessCode,
      message: accessCode
        ? 'Save this access_code now — it will not be shown again.'
        : 'Client created. Set password when ready.',
    };
  });

  app.put('/api/clients/:id', async (request) => {
    const { id } = request.params as { id: string };
    const body = request.body as {
      name?: string;
      enabled?: boolean;
      access_enabled?: boolean;
      risk_enabled?: boolean;
      preferred_broker_account_id?: number | null;
    };
    const prev = await pool.query('SELECT * FROM clients WHERE id = $1', [id]);
    if (!prev.rows.length) return { error: 'Not found' };

    await pool.query(
      `UPDATE clients SET
        name = COALESCE($2, name),
        enabled = COALESCE($3, enabled),
        access_enabled = COALESCE($4, access_enabled),
        risk_enabled = COALESCE($5, risk_enabled),
        updated_at = NOW()
       WHERE id = $1`,
      [
        id,
        body.name ?? null,
        body.enabled ?? null,
        body.access_enabled ?? null,
        body.risk_enabled ?? null,
      ]
    );
    if (body.preferred_broker_account_id !== undefined) {
      await pool.query(
        `UPDATE clients SET preferred_broker_account_id = $2, updated_at = NOW() WHERE id = $1`,
        [id, body.preferred_broker_account_id]
      );
    }
    const fresh = await pool.query(
      `SELECT id, name, enabled, access_enabled,
              COALESCE(risk_enabled, true) as risk_enabled,
              preferred_broker_account_id,
              access_code_hash IS NOT NULL as has_access_code,
              panel_epic, panel_display_name, panel_lot_size, last_seen_at
       FROM clients WHERE id = $1`,
      [id]
    );
    await logAudit('admin', 'client_updated', 'client', id, prev.rows[0], fresh.rows[0]);
    return fresh.rows[0];
  });

  app.post('/api/clients/:id/access-code', async (request, reply) => {
    const { id } = request.params as { id: string };
    const body = (request.body || {}) as { password?: string };
    const exists = await pool.query('SELECT id, name FROM clients WHERE id = $1', [id]);
    if (!exists.rows.length) {
      return reply.code(404).send({ error: 'Client not found' });
    }
    const custom = String(body.password || '').trim();
    if (custom && custom.length < 6) {
      return reply.code(400).send({
        error: 'Password too short',
        message: 'Password must be at least 6 characters',
      });
    }
    const code = custom || generateAccessCode();
    const hash = hashAccessCode(code);
    await pool.query(
      `UPDATE clients SET
         access_code_hash = $2,
         access_enabled = true,
         updated_at = NOW()
       WHERE id = $1`,
      [id, hash]
    );
    await revokeAllClientSessions(Number(id));
    await logAudit('admin', 'client_access_code_reset', 'client', id, null, {
      access_enabled: true,
      custom_password: Boolean(custom),
    });
    return {
      success: true,
      client_id: Number(id),
      access_code: code,
      access_enabled: true,
      message: 'Save this access code now — it will not be shown again.',
    };
  });

  /** Pull Capital.com markets for this client's Capital connection (quick + schedule full). */
  app.post('/api/clients/:id/pull-markets', async (request, reply) => {
    const { id } = request.params as { id: string };
    const body = (request.body || {}) as { mode?: string };
    const { rows } = await pool.query(
      `SELECT bc.id as connection_id, ba.id as account_id
       FROM broker_connections bc
       LEFT JOIN broker_accounts ba ON ba.broker_connection_id = bc.id
       WHERE bc.client_id = $1 AND bc.broker_name = 'capital_com'
       ORDER BY bc.id ASC, ba.id ASC
       LIMIT 1`,
      [id]
    );
    if (rows.length === 0) {
      return reply.code(404).send({
        error: 'No Capital.com connection for this client',
        message: 'No Capital.com connection for this client',
      });
    }
    const connectionId = rows[0].connection_id as number;
    let accountId = rows[0].account_id as number | null;
    if (!accountId) {
      const client = await pool.query('SELECT name FROM clients WHERE id = $1', [id]);
      const name = (client.rows[0]?.name as string) || 'Client';
      accountId = await ensureBrokerAccount(connectionId, `${name} / capital_com`);
    }
    const mode = body.mode === 'full' ? 'full' : 'quick';
    const result = await pullAndStoreCapitalMarkets({
      connectionId,
      accountId,
      mode,
      actor: 'admin',
    });
    if (!result.ok) {
      return reply.code(result.statusCode).send({ error: result.error, message: result.error });
    }
    if (mode === 'quick') {
      scheduleFullCapitalMarketsPull(result.connection_id, result.account_id);
    }
    return {
      success: true,
      client_id: Number(id),
      count: result.count,
      mode: result.mode,
      sample: result.sample,
      capital_market_count: await countCapitalMarkets(connectionId),
      full_sync_scheduled: mode === 'quick',
    };
  });

  app.post('/api/clients/:id/revoke-access', async (request, reply) => {
    const { id } = request.params as { id: string };
    const exists = await pool.query('SELECT id FROM clients WHERE id = $1', [id]);
    if (!exists.rows.length) {
      return reply.code(404).send({ error: 'Client not found' });
    }
    await pool.query(
      `UPDATE clients SET access_enabled = false, updated_at = NOW() WHERE id = $1`,
      [id]
    );
    await revokeAllClientSessions(Number(id));
    try {
      await stopClientRobot(Number(id));
    } catch {
      /* no robot */
    }
    await logAudit('admin', 'client_access_revoked', 'client', id, null, {
      access_enabled: false,
    });
    return { success: true };
  });

  app.post('/api/clients/:id/stop-robot', async (request, reply) => {
    const { id } = request.params as { id: string };
    try {
      const status = await stopClientRobot(Number(id));
      await logAudit('admin', 'client_robot_stopped', 'client', id, null, status);
      return { success: true, status };
    } catch (err) {
      const message = err instanceof Error ? err.message : 'Stop failed';
      return reply.code(400).send({ error: message, message });
    }
  });

  app.delete('/api/clients/:id', async (request, reply) => {
    const { id } = request.params as { id: string };
    const query = request.query as { hard?: string };
    const prev = await pool.query('SELECT * FROM clients WHERE id = $1', [id]);
    if (prev.rows.length === 0) {
      return reply.code(404).send({ error: 'Client not found' });
    }

    if (query.hard === '1' || query.hard === 'true') {
      await hardDeleteClient(id);
      await logAudit('admin', 'client_deleted', 'client', id, prev.rows[0], { deleted: true });
      return { success: true, hard: true };
    }

    await pool.query(
      'UPDATE clients SET enabled = false, access_enabled = false, updated_at = NOW() WHERE id = $1',
      [id]
    );
    await revokeAllClientSessions(Number(id));
    await logAudit('admin', 'client_disabled', 'client', id, prev.rows[0], { enabled: false });
    return { success: true, hard: false };
  });
}
