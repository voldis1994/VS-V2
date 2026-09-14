import { pool } from '../db/pool.js';

/** Learning runs service — V2 data access layer */
export async function listLearning() {
  try {
    const { rows } = await pool.query('SELECT * FROM learning_runs ORDER BY 1 DESC LIMIT 100');
    return rows;
  } catch {
    return [];
  }
}
