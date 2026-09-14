import { pool } from '../db/pool.js';

/** Risk service — V2 data access layer */
export async function listRisk() {
  try {
    const { rows } = await pool.query('SELECT * FROM risk_snapshots ORDER BY 1 DESC LIMIT 100');
    return rows;
  } catch {
    return [];
  }
}
