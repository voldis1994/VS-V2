import { pool } from '../db/pool.js';

/** Clients service — V2 data access layer */
export async function listClient() {
  try {
    const { rows } = await pool.query('SELECT * FROM clients ORDER BY 1 DESC LIMIT 100');
    return rows;
  } catch {
    return [];
  }
}
