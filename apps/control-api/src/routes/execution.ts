import { FastifyInstance } from 'fastify';
import { listExecution } from '../services/executionService.js';

export async function registerExecutionRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/execution', async () => ({
    items: await listExecution(),
    timestamp: new Date().toISOString(),
  }));
}
