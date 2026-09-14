import { pool } from '../db/pool.js';

/** Accounts service — V2 data access layer */
export async function listAccount() {
  try {
    const { rows } = await pool.query('SELECT * FROM accounts ORDER BY 1 DESC LIMIT 100');
    return rows;
  } catch {
    return [];
  }
}
