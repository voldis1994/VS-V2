import { pool } from '../db/pool.js';

/** Diagnostics service — V2 data access layer */
export async function listDiagnostics() {
  try {
    const { rows } = await pool.query('SELECT * FROM diagnostic_events ORDER BY 1 DESC LIMIT 100');
    return rows;
  } catch {
    return [];
  }
}
