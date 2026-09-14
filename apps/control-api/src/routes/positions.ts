import { FastifyInstance } from 'fastify';
import { listPosition } from '../services/positionService.js';

export async function registerPositionsRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/positions/v2', async () => ({
    items: await listPosition(),
    timestamp: new Date().toISOString(),
  }));
}
