import { pool } from '../db/pool.js';

/** Positions service — V2 data access layer */
export async function listPosition() {
  try {
    const { rows } = await pool.query('SELECT * FROM positions ORDER BY 1 DESC LIMIT 100');
    return rows;
  } catch {
    return [];
  }
}
