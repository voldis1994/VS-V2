import { pool } from '../db/pool.js';

/** Executions service — V2 data access layer */
export async function listExecution() {
  try {
    const { rows } = await pool.query('SELECT * FROM executions ORDER BY 1 DESC LIMIT 100');
    return rows;
  } catch {
    return [];
  }
}
